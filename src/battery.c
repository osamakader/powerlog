#include "battery.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

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
		if (strncmp(ent->d_name, "BAT", 3) == 0 ||
		    strstr(ent->d_name, "battery") != NULL) {
			snprintf(names[count], 32, "%.31s", ent->d_name);
			count++;
		}
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
	}

	data->available = (data->num_batteries > 0);
}

void battery_log(const battery_data_t *data)
{
	int i;

	if (!data->available)
		return;

	printf("[Battery]\n");
	for (i = 0; i < data->num_batteries; i++) {
		if (data->capacity[i] >= 0)
			printf("  %s: %d%% (%s)\n", data->name[i], data->capacity[i],
			       data->status[i][0] ? data->status[i] : "?");
		else
			printf("  %s: N/A (%s)\n", data->name[i],
			       data->status[i][0] ? data->status[i] : "?");
	}
}
