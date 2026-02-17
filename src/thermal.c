#include "thermal.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void thermal_collect(thermal_data_t *data)
{
	char path[SYSFS_PATH_MAX];
	char buf[READ_BUF_SIZE];
	int i;

	data->num_zones = 0;
	data->available = false;

	for (i = 0; i < MAX_THERMAL_ZONES; i++) {
		snprintf(path, sizeof(path),
			 "/sys/class/thermal/thermal_zone%d/type", i);
		if (!path_exists(path))
			break;

		if (read_sysfs(path, data->type[i], sizeof(data->type[i])) < 0)
			data->type[i][0] = '\0';

		snprintf(path, sizeof(path),
			 "/sys/class/thermal/thermal_zone%d/temp", i);
		if (read_sysfs(path, buf, sizeof(buf)) > 0)
			data->temp_mc[i] = atoi(buf);
		else
			data->temp_mc[i] = -1;
	}

	data->num_zones = i;
	data->available = (data->num_zones > 0);
}

void thermal_log(const thermal_data_t *data)
{
	int i;

	if (!data->available)
		return;

	printf("[Thermal]\n");
	for (i = 0; i < data->num_zones; i++) {
		if (data->temp_mc[i] >= 0)
			printf("  %s: %d.%d °C\n", data->type[i][0] ? data->type[i] : "zone",
			       data->temp_mc[i] / 1000, (data->temp_mc[i] % 1000) / 100);
		else
			printf("  %s: N/A\n", data->type[i][0] ? data->type[i] : "zone");
	}
}
