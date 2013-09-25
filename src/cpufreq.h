#ifndef _CPUFREQ_H_
#define _CPUFREQ_H_

struct cpufreq;

struct cpufreq *cpufreq_open(const char *dir);
void cpufreq_close(struct cpufreq *cf);

int cpufreq_read_cur(struct cpufreq *cf, unsigned int *value);
int cpufreq_read_max(struct cpufreq *cf, unsigned int *value);
int cpufreq_write_max(struct cpufreq *cf, unsigned int value);

#endif
