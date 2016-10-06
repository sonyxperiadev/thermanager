#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/wait.h>

#include "log.h"
#include "list.h"
#include "watch.h"
#include "thermal_zone.h"
#include "cpufreq.h"
#include "util.h"
#include "resource.h"

static LIST(g_resource_manager_list);

#define ABS(x) (((x)<0)?-(x):(x))
#define MIN(x,y) (((x)<(y))?(x):(y))

struct resource *resource_manager_find(const char *name)
{
	struct list_node *iter;

	for_list_node(&g_resource_manager_list, iter) {
		struct resource *res =
				list_entry(iter, struct resource, list_node);
		if (!strcmp(res->name, name))
			return res;
	}
	return NULL;
}

void resource_manager_add(struct resource *res)
{
	list_append(&g_resource_manager_list, &res->list_node);
}

void resource_manager_remove(struct resource *res)
{
	list_remove(&g_resource_manager_list, &res->list_node);
}

void resource_manager_prepare(void)
{
	struct list_node *safe;
	struct list_node *iter;

	for_list_node_safe(&g_resource_manager_list, iter, safe) {
		struct resource *res =
				list_entry(iter, struct resource, list_node);
		if (resource_prepare(res)) {
			LOGI("preparation of resource \"%s\" failed,"
					" removing\n", res->name);
			list_remove(&g_resource_manager_list, &res->list_node);
			resource_close(res);
		}
	}
}

void resource_close(struct resource *res)
{
	if (res->close == NULL)
		return;
	res->close(res);
}

int resource_prepare(struct resource *res)
{
	if (res->prepare == NULL)
		return 0;
	return res->prepare(res);
}

void resource_enable(struct resource *res)
{
	if (res->enable == NULL)
		return;
	res->enable(res);
}

void resource_disable(struct resource *res)
{
	if (res->disable == NULL)
		return;
	res->disable(res);
}

int resource_read_value(struct resource *res, char *buf, unsigned int len)
{
	if (res->read_value == NULL)
		return -1;
	return res->read_value(res, buf, len);
}

int resource_read_int(struct resource *res)
{
	char buf[13];
	int rc;

	if (res->read_value == NULL)
		return -1;

	rc = res->read_value(res, buf, sizeof(buf) - 1);
	if (rc <= 0)
		return -1;

	buf[rc] = 0;
	return strtol(buf, 0, 0);
}

int resource_write_int(struct resource *res, int value)
{
	char buf[13];
	int rc;

	if (res->write_value == NULL)
		return -1;

	rc = snprintf(buf, sizeof(buf), "%d", value);
	return res->write_value(res, buf, rc);
}

int resource_write_value(struct resource *res, const char *val, unsigned int len)
{
	if (res->write_value == NULL)
		return -1;
	return res->write_value(res, val, len);
}

void resource_set_edges(struct resource *res, int lower, int upper)
{
	if (res->set_edges == NULL)
		return;
	res->set_edges(res, lower, upper);
}

struct tz_resource {
	struct resource resource;
	struct thermal_zone *zone;
	int low_edge;
	int high_edge;
	int value;
};

static void resource_tz_enable(struct resource *res)
{
	struct tz_resource *tres =
			container_of(res, struct tz_resource, resource);
	thermal_zone_enable(tres->zone);
}

static void resource_tz_disable(struct resource *res)
{
	struct tz_resource *tres =
			container_of(res, struct tz_resource, resource);
	thermal_zone_disable(tres->zone);
}

static void resource_tz_close(struct resource *res)
{
	struct tz_resource *tres =
			container_of(res, struct tz_resource, resource);
	thermal_zone_close(tres->zone);
	free(tres);
}

static void resource_tz_set_edges(struct resource *res, int lower, int upper)
{
	struct tz_resource *tres =
			container_of(res, struct tz_resource, resource);
	if (lower >= tres->value || lower < tres->low_edge)
		lower = tres->low_edge;

	if (upper <= tres->value || upper > tres->high_edge)
		upper = tres->high_edge;

	if (lower == tres->low_edge && upper == tres->high_edge)
		return;

	tres->low_edge = lower;
	tres->high_edge = upper;
	thermal_zone_set_trip(tres->zone, lower, upper);
}

static int resource_tz_read_value(struct resource *res, char *buf, unsigned int len)
{
	struct tz_resource *tres =
			container_of(res, struct tz_resource, resource);
	int rc;

	rc = thermal_zone_read(tres->zone, buf, len);
	if (rc <= 0)
		return rc;

	buf[rc] = 0;
	tres->value = strtol(buf, 0, 0);
	return rc;
}

struct resource *resource_tz_open(const char *name, const char *file)
{
	struct tz_resource *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->zone = thermal_zone_open(file);
	if (res->zone == NULL) {
		free(res);
		return NULL;
	}
	res->resource.read_value = resource_tz_read_value;
	res->resource.close = resource_tz_close;
	res->resource.enable = resource_tz_enable;
	res->resource.disable = resource_tz_disable;
	res->resource.set_edges = resource_tz_set_edges;

	res->low_edge = INT_MIN;
	res->high_edge = INT_MAX;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}

struct sysfs_resource {
	struct resource resource;
	struct watch_ticket *ticket;
	int fd;
};

static void resource_sysfs_enable(struct resource *res)
{
	struct sysfs_resource *sres =
			container_of(res, struct sysfs_resource, resource);
	sres->ticket = watch_manager_add_timeout(5000);
}

static void resource_sysfs_disable(struct resource *res)
{
	struct sysfs_resource *sres =
			container_of(res, struct sysfs_resource, resource);
	if (sres->ticket != NULL) {
		watch_ticket_delete(sres->ticket);
		sres->ticket = NULL;
	}
}

static void resource_sysfs_close(struct resource *res)
{
	struct sysfs_resource *sres =
			container_of(res, struct sysfs_resource, resource);

	if (sres->ticket != NULL)
		watch_ticket_delete(sres->ticket);
	close(sres->fd);
	free(sres);
}

static int resource_sysfs_read_value(struct resource *res,
		char *buf, unsigned int len)
{
	struct sysfs_resource *sres =
			container_of(res, struct sysfs_resource, resource);
	int rc;

#if 0
	if (sres->ticket != NULL) {
		rc = watch_ticket_clear(sres->ticket);
		if (rc)
			return -1;
	}
#endif
	rc = read(sres->fd, buf, len);
	lseek(sres->fd, 0, SEEK_SET);
	return rc;
}

static int resource_sysfs_write_value(struct resource *res,
		const char *val, unsigned int len)
{
	struct sysfs_resource *sres =
			container_of(res, struct sysfs_resource, resource);
	int rc;
	rc = write(sres->fd, val, len);
	return -(rc <= 0);
}

struct resource *resource_sysfs_open(const char *name, const char *file)
{
	struct sysfs_resource *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->fd = open(file, O_RDWR);
	if (res->fd == -1) {
		res->fd = open(file, O_RDONLY);
		if (res->fd == -1) {
			free(res);
			return NULL;
		}
	} else {
		res->resource.write_value = resource_sysfs_write_value;
		res->resource.enable = resource_sysfs_enable;
		res->resource.disable = resource_sysfs_disable;
	}
	res->resource.read_value = resource_sysfs_read_value;
	res->resource.close = resource_sysfs_close;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}

struct union_resource {
	struct resource resource;
	struct resource **members;
	char **member_names;
	int nmembers;
};

static int resource_union_prepare(struct resource *res)
{
	struct union_resource *ures =
			container_of(res, struct union_resource, resource);
	int i;

	if (ures->members != NULL || ures->nmembers == 0)
		return -1;

	ures->members = calloc(1, sizeof(ures->members[0]) * ures->nmembers);
	if (ures->members == NULL)
		return -1;

	for (i = 0; i < ures->nmembers; ++i) {
		ures->members[i] = resource_manager_find(ures->member_names[i]);
		if (ures->members[i] == 0) {
			memmove(ures->member_names[i], ures->member_names[i + 1],
					(ures->nmembers - (i + 1)) * sizeof(ures->member_names[0]));
			--i;
			--ures->nmembers;
		}
	}
	return -(ures->members == 0);
}

static void resource_union_set_edges(struct resource *res, int lo, int hi)
{
	struct union_resource *ures =
			container_of(res, struct union_resource, resource);
	int i;

	for (i = 0; i < ures->nmembers; ++i) {
		resource_set_edges(ures->members[i], lo, hi);
	}
}

static void resource_union_enable(struct resource *res)
{
	struct union_resource *ures =
			container_of(res, struct union_resource, resource);
	int i;

	for (i = 0; i < ures->nmembers; ++i) {
		resource_enable(ures->members[i]);
	}
}

static void resource_union_disable(struct resource *res)
{
	struct union_resource *ures =
			container_of(res, struct union_resource, resource);
	int i;

	for (i = 0; i < ures->nmembers; ++i) {
		resource_disable(ures->members[i]);
	}
}

static void resource_union_close(struct resource *res)
{
	struct union_resource *ures =
			container_of(res, struct union_resource, resource);
	int i;

	if (ures->members)
		free(ures->members);
	for (i = 0; i < ures->nmembers; ++i)
		free(ures->member_names[i]);
	free(ures->member_names);

	free(ures);
}

static int resource_union_read_value(struct resource *res, char *buf, unsigned int len)
{
	struct union_resource *ures =
			container_of(res, struct union_resource, resource);
	int max = INT_MIN;
	int i;

	for (i = 0; i < ures->nmembers; ++i) {
		int value;
		value = resource_read_int(ures->members[i]);
		if (value > max)
			max = value;
	}

	return snprintf(buf, len, "%d", max);
}

static int resource_union_write_value(struct resource *res, const char *val, unsigned int len)
{
	struct union_resource *ures =
			container_of(res, struct union_resource, resource);
	int i;

	for (i = 0; i < ures->nmembers; ++i) {
		resource_write_value(ures->members[i], val, len);
	}
	return 0;
}

struct resource *resource_union_open(const char *name, int count, const char **names)
{
	struct union_resource *res;
	int i;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->resource.prepare = resource_union_prepare;
	res->resource.enable = resource_union_enable;
	res->resource.disable = resource_union_disable;
	res->resource.set_edges = resource_union_set_edges;
	res->resource.close = resource_union_close;
	res->resource.read_value = resource_union_read_value;
	res->resource.write_value = resource_union_write_value;
	res->nmembers = count;

	res->member_names = calloc(1, sizeof(res->member_names[0]) * count);
	if (res->member_names == NULL) {
		free(res);
		return NULL;
	}

	for (i = 0; i < res->nmembers; ++i) {
		res->member_names[i] = strdup(names[i]);
		if (res->member_names[i] == 0)
			break;

	}
	if (i != res->nmembers) {
		for (--i; i >= 0; --i)
			free(res->member_names[i]);
		free(res->member_names);
		free(res);
		return NULL;
	}

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}

struct alias_resource {
	struct resource resource;
	struct resource *aliased;
	char alias_name[256];
};

static int resource_alias_prepare(struct resource *res)
{
	struct alias_resource *ares =
			container_of(res, struct alias_resource, resource);
	ares->aliased = resource_manager_find(ares->alias_name);
	return -(ares->aliased == NULL);
}

static void resource_alias_set_edges(struct resource *res, int lo, int hi)
{
	struct alias_resource *ares =
			container_of(res, struct alias_resource, resource);
	resource_set_edges(ares->aliased, lo, hi);
}

static void resource_alias_enable(struct resource *res)
{
	struct alias_resource *ares =
			container_of(res, struct alias_resource, resource);
	resource_enable(ares->aliased);
}

static void resource_alias_disable(struct resource *res)
{
	struct alias_resource *ares =
			container_of(res, struct alias_resource, resource);
	resource_disable(ares->aliased);
}

static void resource_alias_close(struct resource *res)
{
	struct alias_resource *ares =
			container_of(res, struct alias_resource, resource);
	free(ares);
}

static int resource_alias_read_value(struct resource *res, char *buf, unsigned int len)
{
	struct alias_resource *ares =
			container_of(res, struct alias_resource, resource);
	return resource_read_value(ares->aliased, buf, len);
}

static int resource_alias_write_value(struct resource *res, const char *val, unsigned int len)
{
	struct alias_resource *ares =
			container_of(res, struct alias_resource, resource);
	return resource_write_value(ares->aliased, val, len);
}

struct resource *resource_alias_open(const char *name, const char *aliased)
{
	struct alias_resource *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->resource.prepare = resource_alias_prepare;
	res->resource.enable = resource_alias_enable;
	res->resource.disable = resource_alias_disable;
	res->resource.set_edges = resource_alias_set_edges;
	res->resource.close = resource_alias_close;
	res->resource.read_value = resource_alias_read_value;
	res->resource.write_value = resource_alias_write_value;
	strncpy(res->alias_name, aliased, sizeof(res->alias_name));
	res->alias_name[sizeof(res->alias_name) - 1] = 0;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}

struct halt_resource {
	struct resource resource;
	struct watch_ticket *ticket;
	int delay;
};

static void resource_halt_cb(void *data, struct watch_ticket *ticket)
{
	//struct halt_resource *ares = (struct halt_resource)data;
	watch_ticket_delete(ticket);
	KLOGE("halting\n");
	util_halt();
}

static void resource_halt_close(struct resource *res)
{
	struct halt_resource *ares =
			container_of(res, struct halt_resource, resource);
	if (ares->ticket)
		watch_ticket_delete(ares->ticket);
	free(ares);
}

static void resource_halt_enable(struct resource *res)
{
	struct halt_resource *ares =
			container_of(res, struct halt_resource, resource);
	KLOGE("will halt in %d seconds\n", ares->delay);
	if (!ares->ticket) {
		ares->ticket = watch_manager_add_null();
		if (!ares->ticket)
			return;
		watch_ticket_callback(ares->ticket, resource_halt_cb, ares);
	}
	if (ares->ticket)
		watch_ticket_set_timeout(ares->ticket, ares->delay * 1000);
}

static void resource_halt_disable(struct resource *res)
{
	struct halt_resource *ares =
			container_of(res, struct halt_resource, resource);
	if (ares->ticket)
		watch_ticket_set_null(ares->ticket);
	LOG("halt canceled\n");
}

struct resource *resource_halt_open(const char *name, int delay)
{
	struct halt_resource *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->delay = delay;

	res->resource.close = resource_halt_close;
	res->resource.enable = resource_halt_enable;
	res->resource.disable = resource_halt_disable;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}

struct echo_resource {
	struct resource resource;
};

static void resource_echo_close(struct resource *res)
{
	struct echo_resource *ares =
			container_of(res, struct echo_resource, resource);
	free(ares);
}

static int resource_echo_write_value(struct resource *res, const char *val, unsigned int len)
{
	LOG("%s: %s\n", res->name, val);
	return len;
}

struct resource *resource_echo_open(const char *name)
{
	struct echo_resource *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->resource.close = resource_echo_close;
	res->resource.write_value = resource_echo_write_value;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}

struct deadband_resource {
	struct resource resource;
	struct resource *aliased;
	char alias_name[256];
	int deadband;
	int lwv, lrv;
};

static int resource_deadband_prepare(struct resource *res)
{
	struct deadband_resource *ares =
			container_of(res, struct deadband_resource, resource);
	ares->aliased = resource_manager_find(ares->alias_name);
	return -(ares->aliased == NULL);
}

static void resource_deadband_set_edges(struct resource *res, int lo, int hi)
{
	struct deadband_resource *ares =
			container_of(res, struct deadband_resource, resource);
	resource_set_edges(ares->aliased, lo, hi);
}

static void resource_deadband_enable(struct resource *res)
{
	struct deadband_resource *ares =
			container_of(res, struct deadband_resource, resource);
	resource_enable(ares->aliased);
}

static void resource_deadband_disable(struct resource *res)
{
	struct deadband_resource *ares =
			container_of(res, struct deadband_resource, resource);
	resource_disable(ares->aliased);
}

static void resource_deadband_close(struct resource *res)
{
	struct deadband_resource *ares =
			container_of(res, struct deadband_resource, resource);
	free(ares);
}

static int resource_deadband_read_value(struct resource *res, char *buf, unsigned int len)
{
	struct deadband_resource *ares =
			container_of(res, struct deadband_resource, resource);
	int ival;

	ival = resource_read_int(ares->aliased);
	if (ABS(ival - ares->lrv) <= ares->deadband)
		return snprintf(buf, len, "%d", ares->lrv);

	ares->lrv = ival;
	return snprintf(buf, len, "%d", ival);
}

static int resource_deadband_write_value(struct resource *res, const char *val, unsigned int len)
{
	struct deadband_resource *ares =
			container_of(res, struct deadband_resource, resource);
	int ival;

	ival = strtol(val, 0, 0);
	if (ABS(ival - ares->lwv) <= ares->deadband)
		return len;

	ares->lwv = ival;
	return resource_write_int(ares->aliased, ival);
}

struct resource *resource_deadband_open(const char *name,
		const char *resource, unsigned int deadband)
{
	struct deadband_resource *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->resource.prepare = resource_deadband_prepare;
	res->resource.enable = resource_deadband_enable;
	res->resource.disable = resource_deadband_disable;
	res->resource.set_edges = resource_deadband_set_edges;
	res->resource.close = resource_deadband_close;
	res->resource.read_value = resource_deadband_read_value;
	res->resource.write_value = resource_deadband_write_value;
	strncpy(res->alias_name, resource, sizeof(res->alias_name));
	res->alias_name[sizeof(res->alias_name) - 1] = 0;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	res->deadband = deadband;
	res->lwv = INT_MIN;
	res->lrv = INT_MIN;

	return &res->resource;
}

struct msmadc_resource {
	struct resource resource;
	struct resource *sysfs;
};

static int resource_msmadc_prepare(struct resource *res)
{
	struct msmadc_resource *ares =
			container_of(res, struct msmadc_resource, resource);
	return resource_prepare(ares->sysfs);
}

static void resource_msmadc_set_edges(struct resource *res, int lo, int hi)
{
	struct msmadc_resource *ares =
			container_of(res, struct msmadc_resource, resource);
	resource_set_edges(ares->sysfs, lo, hi);
}

static void resource_msmadc_enable(struct resource *res)
{
	struct msmadc_resource *ares =
			container_of(res, struct msmadc_resource, resource);
	resource_enable(ares->sysfs);
}

static void resource_msmadc_disable(struct resource *res)
{
	struct msmadc_resource *ares =
			container_of(res, struct msmadc_resource, resource);
	resource_disable(ares->sysfs);
}

static void resource_msmadc_close(struct resource *res)
{
	struct msmadc_resource *ares =
			container_of(res, struct msmadc_resource, resource);
	resource_close(ares->sysfs);
	free(ares);
}

static int resource_msmadc_read_value(struct resource *res, char *buf, unsigned int len)
{
	struct msmadc_resource *ares =
			container_of(res, struct msmadc_resource, resource);
	char lbuf[256];
	int val;
	int rc;

	rc = resource_read_value(ares->sysfs, lbuf, sizeof(lbuf) - 1);
	if (rc <= 0)
		return rc;
	if (rc <= 7 || strncmp(lbuf, "Result:", 7))
		return -1;
	lbuf[rc] = 0;

	val = (int)strtoul(lbuf + 7, 0, 0);

	return snprintf(buf, len, "%d", val);
}

struct resource *resource_msmadc_open(const char *name, const char *file)
{
	struct msmadc_resource *res;
	struct resource *sysfs;

	sysfs = resource_sysfs_open("", file);
	if (sysfs == NULL)
		return NULL;

	res = calloc(1, sizeof(*res));
	if (res == NULL) {
		resource_sysfs_close(sysfs);
		return NULL;
	}

	res->sysfs = sysfs;
	res->resource.prepare = resource_msmadc_prepare;
	res->resource.enable = resource_msmadc_enable;
	res->resource.disable = resource_msmadc_disable;
	res->resource.set_edges = resource_msmadc_set_edges;
	res->resource.close = resource_msmadc_close;
	res->resource.read_value = resource_msmadc_read_value;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}

struct intent_resource {
	struct resource resource;
	char intent[256];
};

static int resource_intent_write_value(struct resource *res,
		const char *val, unsigned int len)
{
	struct intent_resource *ares =
			container_of(res, struct intent_resource, resource);
	char buf[len + 1];
	pid_t pid;

	memcpy(buf, val, len);
	buf[len] = 0;

	pid = fork();
	switch (pid) {
	case -1: return -1;
	case  0:
		setenv("CLASSPATH", "/system/framework/am.jar", 1);
		execlp("app_process", "app_process",
				"/system/bin/",
				"com.android.commands.am.Am",
				"broadcast", "-a",
				ares->intent,
				len ? "-e" : NULL,
				len ? "notice" : NULL,
				len ? buf : NULL,
				NULL);
		LOGE("failed to launch am with intent \"%s\"\n", ares->intent);
		_exit(1);
	default:
		waitpid(pid, NULL, 0);
		break;
	}
	return 0;
}

static void resource_intent_close(struct resource *res)
{
	struct intent_resource *ares =
			container_of(res, struct intent_resource, resource);
	free(ares);
}

struct resource *resource_intent_open(const char *name, const char *intent)
{
	struct intent_resource *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->resource.close = resource_intent_close;
	res->resource.write_value = resource_intent_write_value;

	strncpy(res->intent, intent, sizeof(res->intent));
	res->intent[sizeof(res->intent) - 1] = 0;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}


struct cpufreq_resource {
	struct resource resource;
	struct watch_ticket *ticket;
	struct cpufreq *cpufreq;
};

static void resource_cpufreq_enable(struct resource *res)
{
	struct cpufreq_resource *sres =
			container_of(res, struct cpufreq_resource, resource);
	sres->ticket = watch_manager_add_timeout(5000);
}

static void resource_cpufreq_disable(struct resource *res)
{
	struct cpufreq_resource *sres =
			container_of(res, struct cpufreq_resource, resource);
	if (sres->ticket != NULL) {
		watch_ticket_delete(sres->ticket);
		sres->ticket = NULL;
	}
}

static void resource_cpufreq_close(struct resource *res)
{
	struct cpufreq_resource *sres =
			container_of(res, struct cpufreq_resource, resource);

	if (sres->ticket != NULL)
		watch_ticket_delete(sres->ticket);
	cpufreq_close(sres->cpufreq);
	free(sres);
}

static int resource_cpufreq_read_value(struct resource *res,
		char *buf, unsigned int len)
{
	struct cpufreq_resource *sres =
			container_of(res, struct cpufreq_resource, resource);
	unsigned int value;
	int rc;

	rc = cpufreq_read_cur(sres->cpufreq, &value);
	if (rc) {
		buf[0] = '0';
		buf[1] = 0;
		return 1;
	}
	return snprintf(buf, len, "%u", value);
}

static int resource_cpufreq_write_value(struct resource *res,
		const char *val, unsigned int len)
{
	char buf[32];
	struct cpufreq_resource *sres =
			container_of(res, struct cpufreq_resource, resource);
	unsigned int value;
	int rc;

	strncpy(buf, val, sizeof(buf));
	buf[sizeof(buf) - 1] = 0;
	value = strtoul(buf, 0, 0);
	rc = cpufreq_write_max(sres->cpufreq, value);
	if (rc)
		return -1;
	return len;
}

struct resource *resource_cpufreq_open(const char *name, const char *file)
{
	struct cpufreq_resource *res;

	res = calloc(1, sizeof(*res));
	if (res == NULL)
		return NULL;

	res->cpufreq = cpufreq_open(file);
	if (res->cpufreq == NULL) {
		free(res);
		return NULL;
	}
	res->resource.write_value = resource_cpufreq_write_value;
	res->resource.enable = resource_cpufreq_enable;
	res->resource.disable = resource_cpufreq_disable;
	res->resource.read_value = resource_cpufreq_read_value;
	res->resource.close = resource_cpufreq_close;

	strncpy(res->resource.name, name, sizeof(res->resource.name));
	res->resource.name[sizeof(res->resource.name) - 1] = 0;

	return &res->resource;
}

