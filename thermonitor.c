#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

struct sensor {
	const char *file;
	int try_anyway;
	int (* parse_fn)(char *inout, unsigned int len);
	u64 time;
	int fd;
	int dlen;
	char buffer[256];
};

int gpubusy_parse(char *inout, unsigned int len)
{
	long v = strtol(inout, 0, 0);
	return snprintf(inout, len, "%ld", v);
}

int msmadc_parse(char *inout, unsigned int len)
{
	char *in = inout + 7;
	char *p = strchr(in, ' ');
	if (p) {
		*p = 0;
		memmove(inout, in, (p - in));
		return p - in;
	} else {
		memmove(inout, in, len - 7);
		return len - 7;
	}
}

static struct sensor sensors[] = {
	{ "/sys/class/thermal/thermal_zone0/temp", 1 },
	{ "/sys/class/thermal/thermal_zone1/temp", 1 },
	{ "/sys/class/thermal/thermal_zone2/temp", 1 },
	{ "/sys/class/thermal/thermal_zone3/temp", 1 },
	{ "/sys/class/thermal/thermal_zone4/temp", 1 },
	{ "/sys/class/thermal/thermal_zone5/temp", 1 },
	{ "/sys/class/thermal/thermal_zone6/temp", 1 },
	{ "/sys/class/thermal/thermal_zone7/temp", 1 },
	{ "/sys/class/thermal/thermal_zone8/temp", 1 },
	{ "/sys/class/thermal/thermal_zone9/temp", 1 },
	{ "/sys/class/thermal/thermal_zone10/temp", 1 },
	{ "/sys/class/thermal/thermal_zone11/temp", 1 },
	{ "/sys/class/thermal/thermal_zone12/temp", 1 },
	//{ "/sys/module/pm8921_charger/parameters/usb_max_current" },
	//{ "/sys/module/pm8921_charger/parameters/thermal_mitigation" },
	//{ "/sys/devices/platform/wcnss_wlan.0/thermal_mitigation" },
	//{ "/sys/class/kgsl/kgsl-3d0/max_gpuclk" },
	{ "/sys/class/kgsl/kgsl-3d0/gpubusy", 0, gpubusy_parse },
	{ "/sys/class/leds/lm3533-lcd-bl/brightness" },
	{ "/sys/class/leds/lm3533-lcd-bl-0/brightness" },
	{ "/sys/class/leds/lm3533-lcd-bl-1/brightness" },
	{ "/sys/class/backlight/lcd-backlight/brightness" },
	{ "/sys/bus/platform/devices/pm8xxx-adc/pba_therm", 0, msmadc_parse },
	{ "/sys/bus/platform/devices/pm8xxx-adc/bl_therm", 0, msmadc_parse },
	{ "/sys/bus/platform/devices/pm8xxx-adc/apq_therm", 0, msmadc_parse },
	{ "/sys/bus/platform/devices/pm8xxx-adc/batt_therm", 0, msmadc_parse },
	{ "/sys/bus/platform/devices/pm8xxx-adc/pmic_therm", 0, msmadc_parse },
	{ "/sys/bus/platform/devices/pm8xxx-adc/chg_temp", 0, msmadc_parse },
	{ "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq", 1 },
	{ "/sys/devices/system/cpu/cpu1/cpufreq/cpuinfo_cur_freq", 1 },
	{ "/sys/devices/system/cpu/cpu2/cpufreq/cpuinfo_cur_freq", 1 },
	{ "/sys/devices/system/cpu/cpu3/cpufreq/cpuinfo_cur_freq", 1 },
};

u64 time_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (u64)tv.tv_sec*1000 + tv.tv_usec/1000;
}

struct sensor_data {
	u8 sensor;
	u8 strlen;
	char result[256];
};

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

struct packet {
	u64 time;
	u32 length;
	char data[sizeof(struct sensor_data) * ARRAY_SIZE(sensors)];
};

int monitor(const char *file)
{
	char buf[256];
	FILE *fp;
	int i;

	fp = fopen(file, "r+b");
	if (fp != NULL) { /* smart append */
		u64 flength;
		u32 length;
		u64 time;
		u64 fpos;

		fseek(fp, 0, SEEK_END);
		flength = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		fpos = 0;
		while (fpos < flength) {
			fseek(fp, fpos, SEEK_SET);
			fread(&time, 1, sizeof(time), fp);
			fread(&length, 1, sizeof(length), fp);
			if (ferror(fp) || feof(fp))
				break;
			fpos += 8 + 4 + length;
		}
		fseek(fp, fpos, SEEK_SET);
	} else {
		fp = fopen(file, "wb");
		if (fp == NULL)
			return -1;
	}

	for (i = 0; i < (int)ARRAY_SIZE(sensors); ++i) {
		if (!strncmp(sensors[i].file, "/sys/class/thermal", 18)) {
			char f[PATH_MAX];
			int rc;
			rc = snprintf(f, sizeof(f), "%s", sensors[i].file);
			rc = snprintf(f + (rc - 4), sizeof(f) - (rc - 4), "mode");
			rc = open(f, O_RDWR);
			if (rc >= 0) {
				write(rc, "enabled", 7);
				close(rc);
			}
		}
		sensors[i].fd = open(sensors[i].file, O_RDONLY);
		if (sensors[i].fd == -1 && !sensors[i].try_anyway)
			perror(sensors[i].file);
	}

	for (;;) {
		u64 time = time_ms();
		u32 length = 0;
		for (i = 0; i < (int)ARRAY_SIZE(sensors); ++i) {
			int rc = 0;

			if (!sensors[i].try_anyway) {
				if (sensors[i].fd == -1)
					continue;
			} else if (sensors[i].fd == -1) {
				sensors[i].fd = open(sensors[i].file, O_RDONLY);
			}

			if (sensors[i].fd != -1) {
				rc = read(sensors[i].fd, buf, sizeof(buf) - 1);
				if (rc <= 0) {
					close(sensors[i].fd);
					sensors[i].fd = -1;
				} else if (sensors[i].parse_fn) {
					rc = sensors[i].parse_fn(buf, rc);
				}
			}

			if (sensors[i].fd != -1) {
				lseek(sensors[i].fd, 0, SEEK_SET);
			} else {
				buf[0] = '0';
				rc = 1;
			}
			buf[rc] = 0;
			if (!strcmp(buf, sensors[i].buffer))
				continue;
			memcpy(sensors[i].buffer, buf, rc + 1);
			sensors[i].dlen = rc;
			sensors[i].time = time;
			length += 2 + rc;
		}
		fwrite(&time, 1, sizeof(time), fp);
		fwrite(&length, 1, sizeof(length), fp);
		for (i = 0; i < (int)ARRAY_SIZE(sensors); ++i) {
			u8 dlen = sensors[i].dlen;
			u8 idx = i;
			if (sensors[i].time != time)
				continue;
			fwrite(&idx, 1, sizeof(idx), fp);
			fwrite(&dlen, 1, sizeof(dlen), fp);
			fwrite(sensors[i].buffer, 1, sensors[i].dlen, fp);
		}
		fflush(fp);
		poll(NULL, 0, 10000);
	}

	fclose(fp);

	return 0;
}

int parse(const char *file)
{
	struct sensor_data *p;
	struct packet pkt;
	char buf[256];
	FILE *fp;
	int i;

	fp = fopen(file, "rb");
	if (fp == NULL)
		return -1;

	while (!feof(fp)) {
		int last = -1;
		u32 length;
		u64 time;

		fread(&time, 1, sizeof(time), fp);
		fread(&length, 1, sizeof(length), fp);
		if (ferror(fp) || feof(fp))
			break;
		fread(pkt.data, 1, length, fp);
		p = (struct sensor_data *)pkt.data;
		printf("%llu; ", time);
		while ((char *)p - pkt.data < (int)length) {
			for (i = last + 1; i < (int)p->sensor; ++i) {
				if (sensors[i].buffer[0] == 0)
					continue;
				printf("%s; ", sensors[i].buffer);
			}
			memcpy(buf, p->result, p->strlen);
			buf[p->strlen] = 0;
			if (p->strlen > 0 && buf[p->strlen - 1] == '\n')
				buf[p->strlen - 1] = 0;
			memcpy(sensors[p->sensor].buffer, buf, p->strlen + 1);
			printf("%s; ", buf);
			last = p->sensor;
			p = (struct sensor_data *)((char *)p + p->strlen + 2);
		}
		for (i = last + 1; i < (int)ARRAY_SIZE(sensors); ++i) {
			if (sensors[i].buffer[0] == 0)
				continue;
			printf("%s; ", sensors[i].buffer);
		}
		printf("\n");
	}
	printf("time; ");
	for (i = 0; i < (int)ARRAY_SIZE(sensors); ++i) {
		if (sensors[i].buffer[0] == 0)
			continue;
		printf("%s; ", sensors[i].file);
	}
	printf("\n");

	fclose(fp);
	return 0;
}

int daemonize(const char *file)
{
	switch (fork()) {
	case -1: return -1;
	case  0: return monitor(file);
	default: return 0;
	}
}

int main(int argc, char **argv)
{
	if (argc == 3 && !strcmp(argv[1], "monitor")) {
		return monitor(argv[2]);
	} else if (argc == 3 && !strcmp(argv[1], "daemon")) {
		return daemonize(argv[2]);
	} else if (argc == 3 && !strcmp(argv[1], "parse")) {
		return parse(argv[2]);
	}
	fprintf(stderr, "Usage: %s <daemon|monitor|parse> <file>\n", argv[0]);
	return -1;
}
