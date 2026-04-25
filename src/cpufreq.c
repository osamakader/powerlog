#include "cpufreq.h"
#include "common.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

/* Population variance of per-CPU MHz (integer MHz from kHz/1000) */
static void cpufreq_stats_internal(const cpufreq_data_t *data, double *avg_mhz, int *min_mhz,
				   int *max_mhz, double *var_mhz2)
{
	int n = 0;
	double sum_mhz = 0.0;
	int minf = INT_MAX;
	int maxf = -1;
	int i;

	for (i = 0; i < data->num_cpus; i++) {
		if (data->freq_khz[i] < 0)
			continue;
		int mhz = data->freq_khz[i] / 1000;

		n++;
		sum_mhz += (double)data->freq_khz[i] / 1000.0;
		if (mhz < minf)
			minf = mhz;
		if (mhz > maxf)
			maxf = mhz;
	}
	if (n == 0) {
		*avg_mhz = 0.0;
		*min_mhz = -1;
		*max_mhz = -1;
		*var_mhz2 = 0.0;
		return;
	}
	*avg_mhz = sum_mhz / (double)n;
	*min_mhz = minf;
	*max_mhz = maxf;
	{
		double mean = *avg_mhz;
		double acc = 0.0;

		for (i = 0; i < data->num_cpus; i++) {
			if (data->freq_khz[i] < 0)
				continue;
			double mhz = (double)data->freq_khz[i] / 1000.0;
			double d = mhz - mean;

			acc += d * d;
		}
		*var_mhz2 = acc / (double)n;
	}
}

void cpufreq_summary(const cpufreq_data_t *data, double *avg_mhz, int *min_mhz, int *max_mhz,
		     double *var_mhz2)
{
	cpufreq_stats_internal(data, avg_mhz, min_mhz, max_mhz, var_mhz2);
}

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

void cpufreq_log(FILE *out, const cpufreq_data_t *data, const cpufreq_data_t *prev, bool full)
{
	int i;

	if (!data->available)
		return;

	log_text_section(out, "CPU frequency");
	{
		double avg, var;
		int min_mhz, max_mhz;

		cpufreq_stats_internal(data, &avg, &min_mhz, &max_mhz, &var);
		if (min_mhz >= 0)
			fprintf(out, "  Summary: avg %.0f MHz, min %d MHz, max %d MHz, variance %.0f MHz²\n",
				avg, min_mhz, max_mhz, var);
	}
	if (!full)
		return;
	for (i = 0; i < data->num_cpus; i++) {
		if (data->freq_khz[i] >= 0) {
			fprintf(out, "  cpu%d: %d MHz", i, data->freq_khz[i] / 1000);
			if (data->energy_pref[i][0])
				fprintf(out, " [%s]", data->energy_pref[i]);
			if (prev && prev->num_cpus > i && prev->freq_khz[i] >= 0) {
				int dmhz = (data->freq_khz[i] - prev->freq_khz[i]) / 1000;

				fprintf(out, " (%+d MHz)", dmhz);
			}
			fprintf(out, "\n");
		} else {
			fprintf(out, "  cpu%d: N/A\n", i);
		}
	}
}

void cpufreq_json(FILE *out, const cpufreq_data_t *data, const cpufreq_data_t *prev)
{
	int i;
	double avg, var;
	int min_mhz, max_mhz;

	cpufreq_stats_internal(data, &avg, &min_mhz, &max_mhz, &var);

	fprintf(out, "\"cpufreq\": {\n    \"summary\": ");
	if (data->available && min_mhz >= 0) {
		fprintf(out, "{\"avg_mhz\": %.2f, \"min_mhz\": %d, \"max_mhz\": %d, \"variance_mhz2\": %.2f}",
			avg, min_mhz, max_mhz, var);
	} else {
		fprintf(out, "null");
	}
	fprintf(out, ",\n    \"cpus\": [\n    ");
	for (i = 0; i < data->num_cpus; i++) {
		if (i > 0)
			fprintf(out, ",\n    ");
		fprintf(out, "{\"cpu\": %d, \"freq_mhz\": %d", i,
			data->freq_khz[i] >= 0 ? data->freq_khz[i] / 1000 : -1);
		if (data->energy_pref[i][0]) {
			fprintf(out, ", \"energy_pref\": ");
			json_fprintf_string(out, data->energy_pref[i]);
		}
		if (prev && prev->num_cpus > i && data->freq_khz[i] >= 0 && prev->freq_khz[i] >= 0) {
			fprintf(out, ", \"delta_mhz\": %d",
				(data->freq_khz[i] - prev->freq_khz[i]) / 1000);
		}
		fprintf(out, "}");
	}
	fprintf(out, "\n    ]\n  }");
}
