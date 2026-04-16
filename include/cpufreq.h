#ifndef CPUFREQ_H
#define CPUFREQ_H

#include <stdbool.h>
#include <stdio.h>

#define MAX_CPUS 256

typedef struct {
	int freq_khz[MAX_CPUS];
	char governor[MAX_CPUS][32];
	char energy_pref[MAX_CPUS][32];  /* energy_performance_preference (Intel P-state) */
	int num_cpus;
	bool available;
} cpufreq_data_t;

void cpufreq_collect(cpufreq_data_t *data);
/* avg/min/max/var MHz² from current freqs; min_mhz/max_mhz -1 if none valid */
void cpufreq_summary(const cpufreq_data_t *data, double *avg_mhz, int *min_mhz, int *max_mhz,
		     double *var_mhz2);
void cpufreq_log(FILE *out, const cpufreq_data_t *data, const cpufreq_data_t *prev, bool full);
void cpufreq_json(FILE *out, const cpufreq_data_t *data, const cpufreq_data_t *prev);

#endif /* CPUFREQ_H */
