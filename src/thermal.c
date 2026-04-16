#include "thermal.h"
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Returns 0 and fills key; -1 → other_sensors */
static int thermal_map_key(const char *t, char *key, size_t keylen)
{
	if (!t || !t[0])
		return -1;

	if (strcmp(t, "x86_pkg_temp") == 0) {
		snprintf(key, keylen, "cpu_package");
		return 0;
	}
	if (strncmp(t, "iwlwifi", 7) == 0) {
		snprintf(key, keylen, "wifi");
		return 0;
	}
	if (strcasecmp(t, "SKIN") == 0) {
		snprintf(key, keylen, "skin");
		return 0;
	}
	if (strcmp(t, "TCPU") == 0) {
		snprintf(key, keylen, "cpu_die");
		return 0;
	}
	if (strcmp(t, "TCPU_PCI") == 0) {
		snprintf(key, keylen, "cpu_pci");
		return 0;
	}
	if (strcmp(t, "INT3400 Thermal") == 0) {
		snprintf(key, keylen, "acpi_thermal");
		return 0;
	}
	return -1;
}

void thermal_zone_label(const char *sysfs_type, char *out, size_t outlen)
{
	char key[64];

	if (!out || outlen == 0)
		return;
	if (thermal_map_key(sysfs_type, key, sizeof(key)) == 0) {
		snprintf(out, outlen, "%s", key);
		return;
	}
	snprintf(out, outlen, "%s", sysfs_type && sysfs_type[0] ? sysfs_type : "zone");
}

typedef struct {
	char key[32];
	char sysfs[64];
	int temp_mc;
	int zone_idx;
} thermal_mapped_t;

typedef struct {
	char id[64];
	int temp_mc;
	int zone_idx;
} thermal_other_t;

static void print_temp_c_line(FILE *out, float temp_c)
{
	fprintf(out, "%.1f °C", temp_c);
}

static void print_temp_delta(FILE *out, int zone_idx, const thermal_data_t *curr,
			     const thermal_data_t *prev)
{
	if (!prev || zone_idx < 0 || zone_idx >= prev->num_zones)
		return;
	if (curr->temp_mc[zone_idx] < 0 || prev->temp_mc[zone_idx] < 0)
		return;
	fprintf(out, " (%+.1f °C)",
		(curr->temp_mc[zone_idx] - prev->temp_mc[zone_idx]) / 1000.0);
}

static int key_sort_rank(const char *key)
{
	if (strcmp(key, "cpu_package") == 0)
		return 0;
	if (strcmp(key, "cpu_die") == 0)
		return 1;
	if (strcmp(key, "cpu_pci") == 0)
		return 2;
	if (strcmp(key, "wifi") == 0)
		return 3;
	if (strcmp(key, "skin") == 0)
		return 4;
	if (strcmp(key, "acpi_thermal") == 0)
		return 5;
	return 100;
}

static int cmp_mapped(const void *a, const void *b)
{
	const thermal_mapped_t *pa = a, *pb = b;
	int ra = key_sort_rank(pa->key);
	int rb = key_sort_rank(pb->key);

	if (ra != rb)
		return ra - rb;
	return strcmp(pa->key, pb->key);
}

static int count_key(const thermal_mapped_t *m, int nm, const char *key)
{
	int c = 0, i;

	for (i = 0; i < nm; i++) {
		if (strcmp(m[i].key, key) == 0)
			c++;
	}
	return c;
}

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

/* Hottest zone across all thermal_zone* entries */
bool thermal_hottest_zone(const thermal_data_t *data, char *label, size_t lablen, float *temp_c,
			  int *zone_idx)
{
	int max_mc = -1;
	int hi = -1;
	int i;

	for (i = 0; i < data->num_zones; i++) {
		if (data->temp_mc[i] < 0)
			continue;
		if (max_mc < 0 || data->temp_mc[i] > max_mc) {
			max_mc = data->temp_mc[i];
			hi = i;
		}
	}
	if (hi < 0)
		return false;
	if (zone_idx)
		*zone_idx = hi;
	if (temp_c)
		*temp_c = max_mc / 1000.0f;
	thermal_zone_label(data->type[hi][0] ? data->type[hi] : "zone", label, lablen);
	return true;
}

void thermal_log(FILE *out, const thermal_data_t *data, const thermal_data_t *prev,
		 unsigned interval_ms, bool full)
{
	thermal_mapped_t mapped[MAX_THERMAL_ZONES];
	thermal_other_t other[MAX_THERMAL_ZONES];
	int nm = 0, no = 0;
	int i;
	char hot_label[64];
	int zhot;
	float tc;

	if (!data->available)
		return;

	if (!full) {
		log_text_section(out, "Thermal");
		if (thermal_hottest_zone(data, hot_label, sizeof(hot_label), &tc, &zhot)) {
			fprintf(out, "  Hottest: %s %.1f °C", hot_label, tc);
			if (prev && prev->num_zones > zhot && interval_ms > 0 &&
			    data->temp_mc[zhot] >= 0 && prev->temp_mc[zhot] >= 0) {
				double dcdt = (double)(data->temp_mc[zhot] - prev->temp_mc[zhot]) /
					(double)interval_ms;

				fprintf(out, ", dT/dt %+.3f °C/s", dcdt);
			}
			fprintf(out, "\n");
		} else {
			fprintf(out, "  Hottest: N/A\n");
		}
		return;
	}

	for (i = 0; i < data->num_zones; i++) {
		const char *t = data->type[i][0] ? data->type[i] : "zone";
		char key[32];

		if (thermal_map_key(t, key, sizeof(key)) == 0) {
			snprintf(mapped[nm].key, sizeof(mapped[nm].key), "%s", key);
			snprintf(mapped[nm].sysfs, sizeof(mapped[nm].sysfs), "%s", t);
			mapped[nm].temp_mc = data->temp_mc[i];
			mapped[nm].zone_idx = i;
			nm++;
		} else {
			snprintf(other[no].id, sizeof(other[no].id), "%s", t);
			other[no].temp_mc = data->temp_mc[i];
			other[no].zone_idx = i;
			no++;
		}
	}

	qsort(mapped, (size_t)nm, sizeof(mapped[0]), cmp_mapped);

	log_text_section(out, "Thermal");
	if (thermal_hottest_zone(data, hot_label, sizeof(hot_label), &tc, &zhot)) {
		fprintf(out, "  Hottest: %s %.1f °C", hot_label, tc);
		if (prev && prev->num_zones > zhot && interval_ms > 0 &&
		    data->temp_mc[zhot] >= 0 && prev->temp_mc[zhot] >= 0) {
			double dcdt = (double)(data->temp_mc[zhot] - prev->temp_mc[zhot]) /
				(double)interval_ms;

			fprintf(out, ", dT/dt %+.3f °C/s", dcdt);
		}
		fprintf(out, "\n");
	}
	for (i = 0; i < nm; i++) {
		int nk = count_key(mapped, nm, mapped[i].key);

		if (mapped[i].temp_mc >= 0) {
			if (nk > 1) {
				fprintf(out, "  %s (%s): ", mapped[i].key, mapped[i].sysfs);
				print_temp_c_line(out, mapped[i].temp_mc / 1000.0f);
				print_temp_delta(out, mapped[i].zone_idx, data, prev);
				fprintf(out, "\n");
			} else {
				fprintf(out, "  %s: ", mapped[i].key);
				print_temp_c_line(out, mapped[i].temp_mc / 1000.0f);
				print_temp_delta(out, mapped[i].zone_idx, data, prev);
				fprintf(out, "\n");
			}
		} else {
			fprintf(out, "  %s: N/A\n", mapped[i].key);
		}
	}

	if (no > 0) {
		fprintf(out, "  Other sensors:\n");
		for (i = 0; i < no; i++) {
			if (other[i].temp_mc >= 0) {
				fprintf(out, "    %s: ", other[i].id);
				print_temp_c_line(out, other[i].temp_mc / 1000.0f);
				print_temp_delta(out, other[i].zone_idx, data, prev);
				fprintf(out, "\n");
			} else {
				fprintf(out, "    %s: N/A\n", other[i].id);
			}
		}
	}
}

void thermal_json(FILE *out, const thermal_data_t *data, const thermal_data_t *prev,
		  unsigned interval_ms)
{
	thermal_mapped_t mapped[MAX_THERMAL_ZONES];
	thermal_other_t other[MAX_THERMAL_ZONES];
	int nm = 0, no = 0;
	int i, j, first, subfirst;
	const char *keys[] = {
		"cpu_package", "cpu_die", "cpu_pci", "wifi", "skin", "acpi_thermal"
	};
	size_t nkeys = sizeof(keys) / sizeof(keys[0]);
	char hot_label[64];
	int zhot;
	float tc;
	int have_hot;

	if (!data->available) {
		fprintf(out, "\"thermal\": {\"mapped\": {}, \"other_sensors\": []}");
		return;
	}

	have_hot = thermal_hottest_zone(data, hot_label, sizeof(hot_label), &tc, &zhot);

	for (i = 0; i < data->num_zones; i++) {
		const char *t = data->type[i][0] ? data->type[i] : "zone";
		char key[32];

		if (thermal_map_key(t, key, sizeof(key)) == 0) {
			snprintf(mapped[nm].key, sizeof(mapped[nm].key), "%s", key);
			snprintf(mapped[nm].sysfs, sizeof(mapped[nm].sysfs), "%s", t);
			mapped[nm].temp_mc = data->temp_mc[i];
			mapped[nm].zone_idx = i;
			nm++;
		} else {
			snprintf(other[no].id, sizeof(other[no].id), "%s", t);
			other[no].temp_mc = data->temp_mc[i];
			other[no].zone_idx = i;
			no++;
		}
	}

	fprintf(out, "\"thermal\": {\n    \"summary\": ");
	if (have_hot) {
		fprintf(out, "{\"hottest_label\": \"");
		json_escape_fprintf(out, hot_label);
		fprintf(out, "\", \"hottest_c\": %.2f", tc);
		if (prev && prev->num_zones > zhot && interval_ms > 0 &&
		    data->temp_mc[zhot] >= 0 && prev->temp_mc[zhot] >= 0) {
			double dcdt = (double)(data->temp_mc[zhot] - prev->temp_mc[zhot]) /
				(double)interval_ms;

			fprintf(out, ", \"hottest_delta_c_per_s\": %.6f", dcdt);
		}
		fprintf(out, "}");
	} else {
		fprintf(out, "null");
	}
	fprintf(out, ",\n    \"mapped\": {\n");
	first = 1;
	for (j = 0; j < (int)nkeys; j++) {
		const char *k = keys[j];
		int cnt = count_key(mapped, nm, k);

		if (cnt == 0)
			continue;
		if (!first)
			fprintf(out, ",\n");
		first = 0;
		fprintf(out, "      \"%s\": ", k);
		if (cnt == 1) {
			for (i = 0; i < nm; i++) {
				if (strcmp(mapped[i].key, k) != 0)
					continue;
				fprintf(out, "%.2f", mapped[i].temp_mc >= 0 ?
					mapped[i].temp_mc / 1000.0 : -1.0);
				break;
			}
		} else {
			fprintf(out, "{");
			subfirst = 1;
			for (i = 0; i < nm; i++) {
				if (strcmp(mapped[i].key, k) != 0)
					continue;
				if (!subfirst)
					fprintf(out, ", ");
				subfirst = 0;
				fprintf(out, "\"");
				json_escape_fprintf(out, mapped[i].sysfs);
				fprintf(out, "\": %.2f", mapped[i].temp_mc >= 0 ?
					mapped[i].temp_mc / 1000.0 : -1.0);
			}
			fprintf(out, "}");
		}
	}
	fprintf(out, "\n    },\n    \"other_sensors\": [\n");
	for (i = 0; i < no; i++) {
		if (i > 0)
			fprintf(out, ",\n");
		fprintf(out, "      {\"id\": \"");
		json_escape_fprintf(out, other[i].id);
		fprintf(out, "\", \"temp_c\": %.2f", other[i].temp_mc >= 0 ?
			other[i].temp_mc / 1000.0 : -1.0);
		if (prev && prev->num_zones > other[i].zone_idx &&
		    other[i].temp_mc >= 0 && prev->temp_mc[other[i].zone_idx] >= 0) {
			fprintf(out, ", \"delta_c\": %.2f",
				(other[i].temp_mc - prev->temp_mc[other[i].zone_idx]) / 1000.0);
		}
		fprintf(out, "}");
	}
	fprintf(out, "\n    ]");
	if (prev && prev->num_zones == data->num_zones) {
		fprintf(out, ",\n    \"zone_delta_c\": [\n");
		for (i = 0; i < data->num_zones; i++) {
			if (i > 0)
				fprintf(out, ",\n");
			if (data->temp_mc[i] >= 0 && prev->temp_mc[i] >= 0)
				fprintf(out, "      %.2f",
					(data->temp_mc[i] - prev->temp_mc[i]) / 1000.0);
			else
				fprintf(out, "      null");
		}
		fprintf(out, "\n    ]");
	}
	fprintf(out, "\n  }");
}
