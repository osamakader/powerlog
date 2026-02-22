#include "cpuidle.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cpuidle_collect(cpuidle_data_t *data)
{
	char path[SYSFS_PATH_MAX];
	char buf[READ_BUF_SIZE];
	int cpu, state;

	data->num_cpus = 0;
	data->available = false;

	for (cpu = 0; cpu < MAX_CPUS; cpu++) {
		data->num_states[cpu] = 0;

		for (state = 0; state < MAX_CSTATES; state++) {
			snprintf(path, sizeof(path),
				 "/sys/devices/system/cpu/cpu%d/cpuidle/state%d/name", cpu, state);
			if (!path_exists(path))
				break;

			if (read_sysfs(path, data->name[cpu][state], 32) < 0)
				data->name[cpu][state][0] = '\0';

			snprintf(path, sizeof(path),
				 "/sys/devices/system/cpu/cpu%d/cpuidle/state%d/time", cpu, state);
			if (read_sysfs(path, buf, sizeof(buf)) > 0)
				data->time[cpu][state] = strtoul(buf, NULL, 10);
			else
				data->time[cpu][state] = 0;

			snprintf(path, sizeof(path),
				 "/sys/devices/system/cpu/cpu%d/cpuidle/state%d/usage", cpu, state);
			if (read_sysfs(path, buf, sizeof(buf)) > 0)
				data->usage[cpu][state] = strtoul(buf, NULL, 10);
			else
				data->usage[cpu][state] = 0;
		}

		data->num_states[cpu] = state;
		if (state > 0)
			data->num_cpus = cpu + 1;
	}

	data->available = (data->num_cpus > 0);
}

void cpuidle_log(FILE *out, const cpuidle_data_t *data)
{
	int cpu, state;

	if (!data->available)
		return;

	fprintf(out, "[C-States]\n");
	for (cpu = 0; cpu < data->num_cpus; cpu++) {
		fprintf(out, "  cpu%d:\n", cpu);
		for (state = 0; state < data->num_states[cpu]; state++) {
			fprintf(out, "    %s: time=%lu us, usage=%lu\n",
				data->name[cpu][state][0] ? data->name[cpu][state] : "?",
				data->time[cpu][state], data->usage[cpu][state]);
		}
	}
}

void cpuidle_json(FILE *out, const cpuidle_data_t *data)
{
	int cpu, state;

	fprintf(out, "\"cpuidle\": [\n    ");
	for (cpu = 0; cpu < data->num_cpus; cpu++) {
		if (cpu > 0)
			fprintf(out, ",\n    ");
		fprintf(out, "{\"cpu\": %d, \"states\": [\n      ", cpu);
		for (state = 0; state < data->num_states[cpu]; state++) {
			if (state > 0)
				fprintf(out, ",\n      ");
			fprintf(out, "{\"name\": \"");
			json_escape_fprintf(out, data->name[cpu][state][0] ?
					   data->name[cpu][state] : "?");
			fprintf(out, "\", \"time_us\": %lu, \"usage\": %lu}",
				data->time[cpu][state], data->usage[cpu][state]);
		}
		fprintf(out, "\n    ]}");
	}
	fprintf(out, "\n  ]");
}
