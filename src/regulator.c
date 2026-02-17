#include "regulator.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static int find_regulators(char names[][32], int max)
{
	DIR *dir;
	struct dirent *ent;
	int count = 0;

	dir = opendir("/sys/class/regulator");
	if (!dir)
		return 0;

	while ((ent = readdir(dir)) != NULL && count < max) {
		if (ent->d_name[0] == '.')
			continue;
		if (strncmp(ent->d_name, "regulator.", 10) == 0) {
			snprintf(names[count], 32, "%.31s", ent->d_name);
			count++;
		}
	}
	closedir(dir);
	return count;
}

void regulator_collect(regulator_data_t *data)
{
	char path[SYSFS_PATH_MAX];
	char buf[READ_BUF_SIZE];
	int i;

	data->num_regulators = find_regulators(data->sysfs_name, MAX_REGULATORS);
	data->available = false;

	for (i = 0; i < data->num_regulators; i++) {
		snprintf(path, sizeof(path),
			 "/sys/class/regulator/%s/name", data->sysfs_name[i]);
		if (read_sysfs(path, buf, sizeof(buf)) > 0) {
			snprintf(data->name[i], 64, "%.63s", buf);
		} else {
			snprintf(data->name[i], 64, "%.63s", data->sysfs_name[i]);
		}

		snprintf(path, sizeof(path),
			 "/sys/class/regulator/%s/state", data->sysfs_name[i]);
		if (read_sysfs(path, data->state[i], sizeof(data->state[i])) < 0)
			data->state[i][0] = '\0';

		snprintf(path, sizeof(path),
			 "/sys/class/regulator/%s/microvolts", data->sysfs_name[i]);
		if (read_sysfs(path, buf, sizeof(buf)) > 0)
			data->microvolts[i] = atoi(buf);
		else
			data->microvolts[i] = -1;
	}

	data->available = (data->num_regulators > 0);
}

void regulator_log(const regulator_data_t *data)
{
	int i;

	if (!data->available)
		return;

	printf("[Regulators]\n");
	for (i = 0; i < data->num_regulators; i++) {
		printf("  %s: %s", data->name[i],
		       data->state[i][0] ? data->state[i] : "?");
		if (data->microvolts[i] >= 0)
			printf(" (%d µV)", data->microvolts[i]);
		printf("\n");
	}
}
