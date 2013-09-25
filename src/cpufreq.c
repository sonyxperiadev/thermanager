#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>

#include "cpufreq.h"

struct cpufreq {
	char dir[PATH_MAX];
	int max_freq_fd;
	int cur_freq_fd;
};

static int cpufreq_open_file(const char *dir, const char *file, int mode)
{
	char fname[PATH_MAX];
	snprintf(fname, sizeof(fname), "%s/%s", dir, file);
	return open(fname, mode);
}

static int cpufreq_open_max(struct cpufreq *cf)
{
	int rc;

	if (cf->max_freq_fd == -1) {
		rc = cpufreq_open_file(cf->dir, "scaling_max_freq", O_RDWR);
		if (rc == -1)
			return -1;
		cf->max_freq_fd = rc;
	}

	return 0;
}

static int cpufreq_open_cur(struct cpufreq *cf)
{
	int rc;

	if (cf->cur_freq_fd == -1) {
		rc = cpufreq_open_file(cf->dir, "scaling_cur_freq", O_RDONLY);
		if (rc == -1)
			return -1;
		cf->cur_freq_fd = rc;
	}
	return 0;
}

struct cpufreq *cpufreq_open(const char *dir)
{
	struct cpufreq *cf;

	cf = calloc(1, sizeof(*cf));
	if (cf == NULL)
		return NULL;

	strncpy(cf->dir, dir, sizeof(cf->dir));
	cf->dir[sizeof(cf->dir) - 1] = 0;

	cf->max_freq_fd = -1;
	cf->cur_freq_fd = -1;

	return cf;
}

void cpufreq_close(struct cpufreq *cf)
{
	if (cf->max_freq_fd != -1)
		close(cf->max_freq_fd);
	if (cf->cur_freq_fd != -1)
		close(cf->cur_freq_fd);
	free(cf);
}

int cpufreq_read_max(struct cpufreq *cf, unsigned int *value)
{
	char buf[32];
	int rc;

	if (value == NULL)
		return -1;
	rc = cpufreq_open_max(cf);
	if (rc)
		return -1;

	rc = read(cf->max_freq_fd, buf, sizeof(buf));
	lseek(cf->max_freq_fd, 0, SEEK_SET);
	if (rc <= 0) {
		close(cf->max_freq_fd);
		cf->max_freq_fd = -1;
		return -1;
	}

	*value = strtoul(buf, 0, 0);
	return 0;
}

int cpufreq_write_max(struct cpufreq *cf, unsigned int value)
{
	char buf[32];
	int rc;

	rc = cpufreq_open_max(cf);
	if (rc)
		return -1;
	rc = snprintf(buf, sizeof(buf), "%u", value);
	rc = write(cf->max_freq_fd, buf, rc);
	lseek(cf->max_freq_fd, 0, SEEK_SET);
	if (rc <= 0) {
		close(cf->max_freq_fd);
		cf->max_freq_fd = -1;
		return -1;
	}

	return -(rc <= 0);
}

int cpufreq_read_cur(struct cpufreq *cf, unsigned int *value)
{
	char buf[32];
	int rc;

	if (value == NULL)
		return -1;

	rc = cpufreq_open_cur(cf);
	if (rc)
		return -1;

	rc = read(cf->cur_freq_fd, buf, sizeof(buf));
	lseek(cf->cur_freq_fd, 0, SEEK_SET);
	if (rc <= 0) {
		close(cf->cur_freq_fd);
		cf->cur_freq_fd = -1;
		return -1;
	}

	*value = strtol(buf, 0, 0);
	return 0;
}
