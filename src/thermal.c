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
} thermal_mapped_t;

typedef struct {
	char id[64];
	int temp_mc;
} thermal_other_t;

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

void thermal_log(FILE *out, const thermal_data_t *data)
{
	thermal_mapped_t mapped[MAX_THERMAL_ZONES];
	thermal_other_t other[MAX_THERMAL_ZONES];
	int nm = 0, no = 0;
	int i;

	if (!data->available)
		return;

	for (i = 0; i < data->num_zones; i++) {
		const char *t = data->type[i][0] ? data->type[i] : "zone";
		char key[32];

		if (thermal_map_key(t, key, sizeof(key)) == 0) {
			snprintf(mapped[nm].key, sizeof(mapped[nm].key), "%s", key);
			snprintf(mapped[nm].sysfs, sizeof(mapped[nm].sysfs), "%s", t);
			mapped[nm].temp_mc = data->temp_mc[i];
			nm++;
		} else {
			snprintf(other[no].id, sizeof(other[no].id), "%s", t);
			other[no].temp_mc = data->temp_mc[i];
			no++;
		}
	}

	qsort(mapped, (size_t)nm, sizeof(mapped[0]), cmp_mapped);

	log_text_section(out, "Thermal");
	for (i = 0; i < nm; i++) {
		int nk = count_key(mapped, nm, mapped[i].key);

		if (mapped[i].temp_mc >= 0) {
			if (nk > 1)
				fprintf(out, "  %s (%s): %d.%d °C\n", mapped[i].key, mapped[i].sysfs,
					mapped[i].temp_mc / 1000,
					(mapped[i].temp_mc % 1000) / 100);
			else
				fprintf(out, "  %s: %d.%d °C\n", mapped[i].key,
					mapped[i].temp_mc / 1000,
					(mapped[i].temp_mc % 1000) / 100);
		} else {
			fprintf(out, "  %s: N/A\n", mapped[i].key);
		}
	}

	if (no > 0) {
		fprintf(out, "  Other sensors:\n");
		for (i = 0; i < no; i++) {
			if (other[i].temp_mc >= 0)
				fprintf(out, "    %s: %d.%d °C\n", other[i].id,
					other[i].temp_mc / 1000,
					(other[i].temp_mc % 1000) / 100);
			else
				fprintf(out, "    %s: N/A\n", other[i].id);
		}
	}
}

void thermal_json(FILE *out, const thermal_data_t *data)
{
	thermal_mapped_t mapped[MAX_THERMAL_ZONES];
	thermal_other_t other[MAX_THERMAL_ZONES];
	int nm = 0, no = 0;
	int i, j, first, subfirst;
	const char *keys[] = {
		"cpu_package", "cpu_die", "cpu_pci", "wifi", "skin", "acpi_thermal"
	};
	size_t nkeys = sizeof(keys) / sizeof(keys[0]);

	if (!data->available) {
		fprintf(out, "\"thermal\": {\"mapped\": {}, \"other_sensors\": []}");
		return;
	}

	for (i = 0; i < data->num_zones; i++) {
		const char *t = data->type[i][0] ? data->type[i] : "zone";
		char key[32];

		if (thermal_map_key(t, key, sizeof(key)) == 0) {
			snprintf(mapped[nm].key, sizeof(mapped[nm].key), "%s", key);
			snprintf(mapped[nm].sysfs, sizeof(mapped[nm].sysfs), "%s", t);
			mapped[nm].temp_mc = data->temp_mc[i];
			nm++;
		} else {
			snprintf(other[no].id, sizeof(other[no].id), "%s", t);
			other[no].temp_mc = data->temp_mc[i];
			no++;
		}
	}

	fprintf(out, "\"thermal\": {\n    \"mapped\": {\n");
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
		fprintf(out, "\", \"temp_c\": %.2f}",
			other[i].temp_mc >= 0 ? other[i].temp_mc / 1000.0 : -1.0);
	}
	fprintf(out, "\n    ]\n  }");
}
