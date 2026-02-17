#ifndef CPUFREQ_H
#define CPUFREQ_H

#include <stdbool.h>

#define MAX_CPUS 256

typedef struct {
	int freq_khz[MAX_CPUS];
	char governor[MAX_CPUS][32];
	char energy_pref[MAX_CPUS][32];  /* energy_performance_preference (Intel P-state) */
	int num_cpus;
	bool available;
} cpufreq_data_t;

void cpufreq_collect(cpufreq_data_t *data);
void cpufreq_log(const cpufreq_data_t *data);

#endif /* CPUFREQ_H */
