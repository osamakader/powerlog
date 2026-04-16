#ifndef THERMAL_H
#define THERMAL_H

#include <stdbool.h>
#include <stdio.h>

#define MAX_THERMAL_ZONES 32

typedef struct {
	char type[MAX_THERMAL_ZONES][64];
	int temp_mc[MAX_THERMAL_ZONES];  /* millidegrees Celsius */
	int num_zones;
	bool available;
} thermal_data_t;

void thermal_collect(thermal_data_t *data);
void thermal_log(FILE *out, const thermal_data_t *data, const thermal_data_t *prev,
		   unsigned interval_ms, bool full);
void thermal_json(FILE *out, const thermal_data_t *data, const thermal_data_t *prev,
		  unsigned interval_ms);

/* Display label for sysfs type (mapped names or raw); for alerts / UI */
void thermal_zone_label(const char *sysfs_type, char *out, size_t outlen);

#endif /* THERMAL_H */
