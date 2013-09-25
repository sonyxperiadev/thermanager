#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include "list.h"

struct resource {
	char name[256];

	int (* prepare)(struct resource *);
	void (* set_edges)(struct resource *, int upper, int lower);
	void (* enable)(struct resource *);
	void (* disable)(struct resource *);
	void (* close)(struct resource *);

	int (* read_value)(struct resource *, char *, unsigned int len);
	int (* write_value)(struct resource *, const char *, unsigned int len);

	struct list_node list_node;
};

struct resource *resource_manager_find(const char *name);
void resource_manager_add(struct resource *res);
void resource_manager_remove(struct resource *res);
void resource_manager_prepare(void);

struct resource *resource_tz_open(const char *name, const char *file);
struct resource *resource_sysfs_open(const char *name, const char *file);
struct resource *resource_union_open(const char *name,
		int count, const char **children_names);
struct resource *resource_alias_open(const char *name, const char *aliased);
struct resource *resource_halt_open(const char *name, int delay);
struct resource *resource_echo_open(const char *name);
struct resource *resource_deadband_open(const char *name,
		const char *resource, unsigned int deadband);
struct resource *resource_msmadc_open(const char *name, const char *file);
struct resource *resource_intent_open(const char *name, const char *intent);
struct resource *resource_cpufreq_open(const char *name, const char *file);

void resource_close(struct resource *res);
void resource_set_edges(struct resource *, int lower, int upper);
int resource_prepare(struct resource *res);
void resource_enable(struct resource *res);
void resource_disable(struct resource *res);
int resource_read_value(struct resource *res, char *buf, unsigned int len);
int resource_write_value(struct resource *res,
		const char *val, unsigned int len);

int resource_read_int(struct resource *res);
int resource_write_int(struct resource *res, int value);

#endif
