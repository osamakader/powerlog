#ifndef ALERTS_H
#define ALERTS_H

#include <stdbool.h>
#include <stdio.h>

#include "battery.h"
#include "thermal.h"

typedef struct {
	int thermal_max_c;   /* °C; -1 = disabled */
	int battery_low_pct; /* 0–100; -1 = disabled */
} alert_config_t;

typedef struct {
	bool thermal_hot;
	bool battery_low;
} alert_edge_state_t;

void alert_config_init(alert_config_t *cfg);
void alert_edge_init(alert_edge_state_t *st);

void alerts_notify_edges(FILE *err, const alert_config_t *cfg, alert_edge_state_t *edge,
			 const thermal_data_t *th, const battery_data_t *bat, const char *ts);

void alerts_json(FILE *out, const alert_config_t *cfg, const thermal_data_t *th,
		 const battery_data_t *bat);

#endif /* ALERTS_H */
