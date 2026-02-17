#include "cpufreq.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cpufreq_collect(cpufreq_data_t *data)
{
	char path[SYSFS_PATH_MAX];
	char buf[READ_BUF_SIZE];
	int i;

	data->num_cpus = 0;
	data->available = false;

	for (i = 0; i < MAX_CPUS; i++) {
		snprintf(path, sizeof(path),
			 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
		if (!path_exists(path))
			break;

		if (read_sysfs(path, buf, sizeof(buf)) > 0) {
			data->freq_khz[i] = atoi(buf);
		} else {
			data->freq_khz[i] = -1;
		}

		snprintf(path, sizeof(path),
			 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		if (read_sysfs(path, data->governor[i], sizeof(data->governor[i])) > 0) {
			/* governor read ok */
		} else {
			data->governor[i][0] = '\0';
		}

		snprintf(path, sizeof(path),
			 "/sys/devices/system/cpu/cpu%d/cpufreq/energy_performance_preference", i);
		if (read_sysfs(path, data->energy_pref[i], sizeof(data->energy_pref[i])) > 0) {
			/* energy preference read ok (Intel P-state power mode) */
		} else {
			data->energy_pref[i][0] = '\0';
		}
	}

	data->num_cpus = i;
	data->available = (data->num_cpus > 0);
}

void cpufreq_log(const cpufreq_data_t *data)
{
	int i;

	if (!data->available)
		return;

	printf("[CPU Frequency]\n");
	for (i = 0; i < data->num_cpus; i++) {
		if (data->freq_khz[i] >= 0) {
			printf("  cpu%d: %d MHz", i, data->freq_khz[i] / 1000);
			if (data->energy_pref[i][0])
				printf(" [%s]", data->energy_pref[i]);
			printf("\n");
		} else {
			printf("  cpu%d: N/A\n", i);
		}
	}
}
