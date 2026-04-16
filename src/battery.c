#include "battery.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <math.h>

/* µW from power_now, or voltage_now (µV) * current_now (µA) / 10^6 */
static long read_supply_power_uw(const char *name)
{
	char path[SYSFS_PATH_MAX];
	char buf[READ_BUF_SIZE];
	long long uv, ua;
	long p;

	snprintf(path, sizeof(path), "/sys/class/power_supply/%s/power_now", name);
	if (read_sysfs(path, buf, sizeof(buf)) > 0) {
		p = atol(buf);
		if (p != 0)
			return p;
	}
	snprintf(path, sizeof(path), "/sys/class/power_supply/%s/voltage_now", name);
	if (read_sysfs(path, buf, sizeof(buf)) <= 0)
		return -1;
	uv = atol(buf);
	if (uv <= 0)
		return -1;
	snprintf(path, sizeof(path), "/sys/class/power_supply/%s/current_now", name);
	if (read_sysfs(path, buf, sizeof(buf)) <= 0)
		return -1;
	ua = atol(buf);
	if (ua == 0)
		return -1;
	return (long)((uv * ua) / 1000000LL);
}

static bool status_discharging(const char *status)
{
	if (!status || !status[0])
		return true;
	return strcasecmp(status, "Discharging") == 0 || strcasecmp(status, "Unknown") == 0;
}

static int find_batteries(char names[][32], int max)
{
	DIR *dir;
	struct dirent *ent;
	int count = 0;

	dir = opendir("/sys/class/power_supply");
	if (!dir)
		return 0;

	while ((ent = readdir(dir)) != NULL && count < max) {
		if (ent->d_name[0] == '.')
			continue;
		/* BAT0/BAT1/… only; avoids hidpp_battery_*, etc. with no useful capacity */
		if (strncmp(ent->d_name, "BAT", 3) != 0)
			continue;
		snprintf(names[count], 32, "%.31s", ent->d_name);
		count++;
	}
	closedir(dir);
	return count;
}

void battery_collect(battery_data_t *data)
{
	char path[SYSFS_PATH_MAX];
	char buf[READ_BUF_SIZE];
	int i;

	data->num_batteries = find_batteries(data->name, MAX_BATTERIES);
	data->available = false;

	for (i = 0; i < data->num_batteries; i++) {
		snprintf(path, sizeof(path),
			 "/sys/class/power_supply/%s/capacity", data->name[i]);
		if (read_sysfs(path, buf, sizeof(buf)) > 0)
			data->capacity[i] = atoi(buf);
		else
			data->capacity[i] = -1;

		snprintf(path, sizeof(path),
			 "/sys/class/power_supply/%s/status", data->name[i]);
		if (read_sysfs(path, data->status[i], sizeof(data->status[i])) < 0)
			data->status[i][0] = '\0';

		data->power_uw[i] = read_supply_power_uw(data->name[i]);
	}

	data->available = (data->num_batteries > 0);
}

void battery_log(FILE *out, const battery_data_t *data)
{
	int i;
	int n;

	if (!data->available)
		return;

	n = 0;
	for (i = 0; i < data->num_batteries; i++) {
		if (data->capacity[i] < 0)
			continue;
		if (n == 0)
			log_text_section(out, "Battery");
		fprintf(out, "  %s: %d%% (%s)", data->name[i], data->capacity[i],
			data->status[i][0] ? data->status[i] : "?");
		if (data->power_uw[i] != -1 && status_discharging(data->status[i])) {
			double w = fabs((double)data->power_uw[i]) / 1e6;
			fprintf(out, ", discharge %.2f W", w);
		}
		fprintf(out, "\n");
		n++;
	}
}

void battery_json(FILE *out, const battery_data_t *data)
{
	int i;
	int n;

	fprintf(out, "\"battery\": [\n    ");
	n = 0;
	for (i = 0; i < data->num_batteries; i++) {
		if (data->capacity[i] < 0)
			continue;
		if (n > 0)
			fprintf(out, ",\n    ");
		fprintf(out, "{\"name\": \"");
		json_escape_fprintf(out, data->name[i]);
		fprintf(out, "\", \"capacity\": %d, \"status\": \"",
			data->capacity[i]);
		json_escape_fprintf(out, data->status[i][0] ? data->status[i] : "?");
		fprintf(out, "\"");
		if (data->power_uw[i] != -1 && status_discharging(data->status[i]))
			fprintf(out, ", \"discharge_w\": %.4f",
				fabs((double)data->power_uw[i]) / 1e6);
		fprintf(out, "}");
		n++;
	}
	fprintf(out, "\n  ]");
}
