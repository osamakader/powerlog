#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdio.h>

#define MAX_BATTERIES 8

typedef struct {
	char name[MAX_BATTERIES][32];
	int capacity[MAX_BATTERIES];
	char status[MAX_BATTERIES][32];  /* Charging, Discharging, Full, Unknown */
	long power_uw[MAX_BATTERIES];    /* sysfs power_now or V*I; µW; -1 = unknown */
	int num_batteries;
	bool available;
} battery_data_t;

void battery_collect(battery_data_t *data);
void battery_log(FILE *out, const battery_data_t *data);
void battery_json(FILE *out, const battery_data_t *data);

#endif /* BATTERY_H */
