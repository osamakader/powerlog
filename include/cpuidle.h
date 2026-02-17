#ifndef CPUIDLE_H
#define CPUIDLE_H

#include <stdbool.h>

#define MAX_CPUS     256
#define MAX_CSTATES  16

typedef struct {
	char name[MAX_CPUS][MAX_CSTATES][32];
	unsigned long time[MAX_CPUS][MAX_CSTATES];
	unsigned long usage[MAX_CPUS][MAX_CSTATES];
	int num_states[MAX_CPUS];
	int num_cpus;
	bool available;
} cpuidle_data_t;

void cpuidle_collect(cpuidle_data_t *data);
void cpuidle_log(const cpuidle_data_t *data);

#endif /* CPUIDLE_H */
