#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>

#define MAX_BATTERIES 8

typedef struct {
	char name[MAX_BATTERIES][32];
	int capacity[MAX_BATTERIES];
	char status[MAX_BATTERIES][32];  /* Charging, Discharging, Full, Unknown */
	int num_batteries;
	bool available;
} battery_data_t;

void battery_collect(battery_data_t *data);
void battery_log(const battery_data_t *data);

#endif /* BATTERY_H */
