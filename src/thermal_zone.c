#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>

#include "log.h"
#include "watch.h"
#include "thermal_zone.h"

struct thermal_trip {
	int temp;
	int temp_fd;
	int type_fd;
	struct watch_ticket *ticket;
};

struct thermal_zone {
	int temp_fd;
	int mode_fd;

	int force_enabled;
	struct thermal_trip trips[2];
};

static int thermal_trip_init(struct thermal_trip *trip,
		const char *dir, const char *type)
{
	char fname[PATH_MAX];
	int i;

	trip->ticket = NULL;
	trip->temp = INT_MIN;

	for (i = 0; i < 8; ++i) {
		snprintf(fname, sizeof(fname), "%s/trip_point_%d_type", dir, i);
		trip->type_fd = open(fname, O_RDONLY);
		if (trip->type_fd == -1)
			return -1;
		read(trip->type_fd, fname, sizeof(fname));
		if (!strncmp(fname, type, strlen(type)))
			break;
		close(trip->type_fd);
		trip->type_fd = -1;
	}

	if (i == 8)
		return -1;

	snprintf(fname, sizeof(fname), "%s/trip_point_%d_temp", dir, i);
	trip->temp_fd = open(fname, O_RDWR);
	if (trip->temp_fd == -1) {
		close(trip->type_fd);
		return -1;
	}

	return 0;
}

static void thermal_trip_fini(struct thermal_trip *trip)
{
	if (trip->ticket != NULL)
		watch_ticket_delete(trip->ticket);
	if (trip->temp_fd != -1)
		close(trip->temp_fd);
	if (trip->type_fd != -1)
		close(trip->type_fd);

	trip->ticket = NULL;
	trip->temp_fd = -1;
	trip->type_fd = -1;
}

static int thermal_trip_set(struct thermal_trip *trip, int temp)
{
	char buf[13];
	int rc;

	if (temp == INT_MAX || temp == INT_MIN) // disable ?
		return 0;

	if (temp == trip->temp)
		return 0;

	if (trip->temp_fd == -1)
		return -1;

	rc = snprintf(buf, sizeof(buf), "%d", temp);
	write(trip->temp_fd, buf, rc);
	lseek(trip->temp_fd, 0, SEEK_SET);
	trip->temp = temp;
	return 0;
}

static void thermal_trip_cb(void *data, struct watch_ticket *ticket)
{
	struct thermal_trip *trip = (struct thermal_trip *)data;
	if (trip->type_fd != -1) {
		char buf[PATH_MAX];
		read(trip->type_fd, buf, sizeof(buf));
		lseek(trip->type_fd, 0, SEEK_SET);
	}

	watch_ticket_clear(ticket);
}

static void thermal_trip_enable(struct thermal_trip *trip)
{
	if (trip->ticket == NULL) {
		if (trip->type_fd != -1)
			trip->ticket = watch_manager_add_fd(trip->type_fd);
		else
			trip->ticket = watch_manager_add_timeout(5000);
		watch_ticket_callback(trip->ticket, thermal_trip_cb, trip);
	}
}

static void thermal_trip_disable(struct thermal_trip *trip)
{
	if (trip->ticket != NULL) {
		watch_ticket_delete(trip->ticket);
		trip->ticket = NULL;
	}
}

struct thermal_zone *thermal_zone_open(const char *dir)
{
	struct thermal_zone *tz;
	char fname[PATH_MAX];

	tz = calloc(1, sizeof(*tz));
	if (tz == NULL)
		return NULL;

	snprintf(fname, sizeof(fname), "%s/temp", dir);
	tz->temp_fd = open(fname, O_RDONLY);
	if (tz->temp_fd == -1) {
		free(tz);
		return NULL;
	}

	snprintf(fname, sizeof(fname), "%s/mode", dir);
	tz->mode_fd = open(fname, O_RDWR);
	/* failure ok */

	thermal_trip_init(&tz->trips[0], dir, "configurable_low");
	thermal_trip_init(&tz->trips[1], dir, "configurable_hi");

	return tz;
}

void thermal_zone_close(struct thermal_zone *tz)
{
	thermal_trip_fini(&tz->trips[0]);
	thermal_trip_fini(&tz->trips[1]);
	thermal_zone_disable(tz);

	if (tz->mode_fd != -1)
		close(tz->mode_fd);
	if (tz->temp_fd != -1)
		close(tz->temp_fd);

	free(tz);
}

void thermal_zone_enable(struct thermal_zone *tz)
{
	char buf[10];
	int rc;

	rc = read(tz->mode_fd, buf, sizeof(buf));
	if (rc <= 0)
		return;

	lseek(tz->mode_fd, 0, SEEK_SET);
	if (rc < 7 || strncmp(buf, "enabled", 7)) {
		write(tz->mode_fd, "enabled", 7);
		lseek(tz->mode_fd, 0, SEEK_SET);
		tz->force_enabled = 1;
	}
	thermal_trip_enable(&tz->trips[0]);
	thermal_trip_enable(&tz->trips[1]);
}

void thermal_zone_disable(struct thermal_zone *tz)
{
	thermal_trip_disable(&tz->trips[1]);
	thermal_trip_disable(&tz->trips[0]);

	if (!tz->force_enabled)
		return;

	write(tz->mode_fd, "disabled", 8);
	lseek(tz->mode_fd, 0, SEEK_SET);
}

int thermal_zone_read(struct thermal_zone *tz, char *buf, unsigned int blen)
{
	int rc;
	rc = read(tz->temp_fd, buf, blen);

	lseek(tz->temp_fd, 0, SEEK_SET);

	return rc;
}

int thermal_zone_set_trip(struct thermal_zone *tz, int lower, int upper)
{
	if (thermal_trip_set(&tz->trips[0], lower))
		return -1;

	if (thermal_trip_set(&tz->trips[1], upper))
		return -1;

	return 0;
}
