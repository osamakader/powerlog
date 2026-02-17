#ifndef REGULATOR_H
#define REGULATOR_H

#include <stdbool.h>

#define MAX_REGULATORS 64

typedef struct {
	char name[MAX_REGULATORS][64];    /* human-readable from sysfs */
	char state[MAX_REGULATORS][32];   /* enabled, disabled */
	int microvolts[MAX_REGULATORS];   /* -1 if N/A */
	char sysfs_name[MAX_REGULATORS][32];  /* regulator.N for path */
	int num_regulators;
	bool available;
} regulator_data_t;

void regulator_collect(regulator_data_t *data);
void regulator_log(const regulator_data_t *data);

#endif /* REGULATOR_H */
