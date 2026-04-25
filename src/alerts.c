#include "alerts.h"
#include "common.h"
#include "thermal.h"
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

/* stderr: bold red for new violations, bold yellow for clear; plain *** if not a tty */
static void alert_banner(FILE *err, bool is_new_violation)
{
	int fd = fileno(err);

	if (!is_new_violation) {
		if (isatty(fd))
			fprintf(err, "\033[1;33m*** alert cleared ***\033[0m ");
		else
			fprintf(err, "*** alert cleared *** ");
		return;
	}
	if (isatty(fd))
		fprintf(err, "\033[1;31m*** ALERT ***\033[0m ");
	else
		fprintf(err, "*** ALERT *** ");
}

void alert_config_init(alert_config_t *cfg)
{
	cfg->thermal_max_c = -1;
	cfg->battery_low_pct = -1;
}

void alert_edge_init(alert_edge_state_t *st)
{
	st->thermal_hot = false;
	st->battery_low = false;
}

static void thermal_worst(const thermal_data_t *th, int *max_mc, char *zone, size_t zone_len)
{
	int i;
	char label[64];

	*max_mc = -1;
	if (zone_len > 0)
		zone[0] = '\0';

	for (i = 0; i < th->num_zones; i++) {
		if (th->temp_mc[i] < 0)
			continue;
		if (*max_mc < 0 || th->temp_mc[i] > *max_mc) {
			*max_mc = th->temp_mc[i];
			thermal_zone_label(th->type[i][0] ? th->type[i] : "zone", label, sizeof(label));
			snprintf(zone, zone_len, "%s", label);
		}
	}
}

static bool battery_discharging(const char *status)
{
	if (!status || !status[0])
		return true;
	return strcasecmp(status, "Discharging") == 0 || strcasecmp(status, "Unknown") == 0;
}

static bool thermal_over(const alert_config_t *cfg, const thermal_data_t *th)
{
	int max_mc;
	char z[64];

	if (cfg->thermal_max_c < 0 || !th->available)
		return false;

	thermal_worst(th, &max_mc, z, sizeof(z));
	if (max_mc < 0)
		return false;
	return (max_mc / 1000) >= cfg->thermal_max_c;
}

static bool battery_below(const alert_config_t *cfg, const battery_data_t *bat)
{
	int i;

	if (cfg->battery_low_pct < 0 || !bat->available)
		return false;

	for (i = 0; i < bat->num_batteries; i++) {
		if (bat->capacity[i] < 0)
			continue;
		if (!battery_discharging(bat->status[i]))
			continue;
		if (bat->capacity[i] <= cfg->battery_low_pct)
			return true;
	}
	return false;
}

void alerts_notify_edges(FILE *err, const alert_config_t *cfg, alert_edge_state_t *edge,
			 const thermal_data_t *th, const battery_data_t *bat, const char *ts)
{
	bool now_thermal = thermal_over(cfg, th);
	bool now_battery = battery_below(cfg, bat);
	int max_mc;
	char zone[64];

	if (cfg->thermal_max_c >= 0) {
		if (now_thermal && !edge->thermal_hot) {
			thermal_worst(th, &max_mc, zone, sizeof(zone));
			alert_banner(err, true);
			fprintf(err, "%s thermal: %s at %d.%d°C (threshold %d°C)\n", ts, zone,
				max_mc / 1000, (max_mc % 1000) / 100, cfg->thermal_max_c);
			edge->thermal_hot = true;
		} else if (!now_thermal && edge->thermal_hot) {
			alert_banner(err, false);
			fprintf(err, "%s thermal: back below %d°C\n", ts, cfg->thermal_max_c);
			edge->thermal_hot = false;
		}
	}

	if (cfg->battery_low_pct >= 0) {
		if (now_battery && !edge->battery_low) {
			alert_banner(err, true);
			fprintf(err, "%s battery: at or below %d%% while discharging\n", ts,
				cfg->battery_low_pct);
			edge->battery_low = true;
		} else if (!now_battery && edge->battery_low) {
			alert_banner(err, false);
			fprintf(err, "%s battery: recovered above %d%%\n", ts,
				cfg->battery_low_pct);
			edge->battery_low = false;
		}
	}
}

void alerts_json(FILE *out, const alert_config_t *cfg, const thermal_data_t *th,
		 const battery_data_t *bat)
{
	int max_mc;
	char zone[64];
	int i;
	int n = 0;

	fprintf(out, "\"alerts\": [\n");

	if (cfg->thermal_max_c >= 0 && th->available && thermal_over(cfg, th)) {
		thermal_worst(th, &max_mc, zone, sizeof(zone));
		fprintf(out, "    {\"type\": \"thermal\", \"zone\": ");
		json_fprintf_string(out, zone);
		fprintf(out, ", \"temp_c\": %.2f, \"threshold_c\": %d}",
			max_mc / 1000.0, cfg->thermal_max_c);
		n++;
	}

	if (cfg->battery_low_pct >= 0 && bat->available && battery_below(cfg, bat)) {
		if (n)
			fprintf(out, ",\n");
		fprintf(out, "    {\"type\": \"battery\", \"threshold_pct\": %d, \"batteries\": [\n",
			cfg->battery_low_pct);
		{
			int first = 1;

			for (i = 0; i < bat->num_batteries; i++) {
				if (bat->capacity[i] < 0)
					continue;
				if (!battery_discharging(bat->status[i]))
					continue;
				if (bat->capacity[i] > cfg->battery_low_pct)
					continue;
				if (!first)
					fprintf(out, ",\n");
				first = 0;
				fprintf(out, "      {\"name\": ");
				json_fprintf_string(out, bat->name[i]);
				fprintf(out, ", \"capacity\": %d, \"status\": ",
					bat->capacity[i]);
				json_fprintf_string(out, bat->status[i][0] ? bat->status[i] : "?");
				fprintf(out, "}");
			}
		}
		fprintf(out, "\n    ]}");
		n++;
	}

	if (n == 0)
		fprintf(out, "\n");
	fprintf(out, "  ]");
}
