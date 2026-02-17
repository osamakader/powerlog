#ifndef THERMAL_H
#define THERMAL_H

#include <stdbool.h>

#define MAX_THERMAL_ZONES 32

typedef struct {
	char type[MAX_THERMAL_ZONES][64];
	int temp_mc[MAX_THERMAL_ZONES];  /* millidegrees Celsius */
	int num_zones;
	bool available;
} thermal_data_t;

void thermal_collect(thermal_data_t *data);
void thermal_log(const thermal_data_t *data);

#endif /* THERMAL_H */
