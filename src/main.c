/*
 * Power Consumption Logger
 * Logs CPU frequency, C-states, thermal state, battery level, and regulator status.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include <stdbool.h>
#include "common.h"
#include "cpufreq.h"
#include "cpuidle.h"
#include "thermal.h"
#include "battery.h"
#include "regulator.h"

#define DEFAULT_INTERVAL_SEC 5
#define TIMESTAMP_BUF_SIZE 64

static volatile sig_atomic_t running = 1;

static void sig_handler(int sig)
{
	(void)sig;
	running = 0;
}

static void print_timestamp(FILE *out)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char buf[TIMESTAMP_BUF_SIZE];

	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
	fprintf(out, "\n=== %s ===\n", buf);
}

static void log_all(FILE *out, bool json,
		    cpufreq_data_t *cf, cpuidle_data_t *ci,
		    thermal_data_t *th, battery_data_t *bat, regulator_data_t *reg,
		    bool with_cpuidle)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char ts[TIMESTAMP_BUF_SIZE];

	cpufreq_collect(cf);
	cpuidle_collect(ci);
	thermal_collect(th);
	battery_collect(bat);
	regulator_collect(reg);

	if (json) {
		strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", tm);
		fprintf(out, "{\n  \"timestamp\": \"%s\",\n  ", ts);
		cpufreq_json(out, cf);
		fprintf(out, ",\n  ");
		if (with_cpuidle)
			cpuidle_json(out, ci);
		fprintf(out, ",\n  ");
		thermal_json(out, th);
		fprintf(out, ",\n  ");
		battery_json(out, bat);
		fprintf(out, ",\n  ");
		regulator_json(out, reg);
		fprintf(out, "\n}\n");
	} else {
		print_timestamp(out);
		cpufreq_log(out, cf);
		if (with_cpuidle)
			cpuidle_log(out, ci);
		thermal_log(out, th);
		battery_log(out, bat);
		regulator_log(out, reg);
	}
	fflush(out);
}

static void usage(const char *prog)
{
	printf("Usage: %s [OPTIONS]\n", prog);
	printf("Power Consumption Logger - logs CPU freq, C-states, thermal, battery, regulators\n\n");
	printf("Options:\n");
	printf("  -i, --interval SEC   Polling interval in seconds (default: %d)\n", DEFAULT_INTERVAL_SEC);
	printf("  -o, --output FILE    Write log to file (default: stdout)\n");
	printf("  -j, --json           Output in JSON format (pretty-printed)\n");
	printf("  -h, --help           Show this help\n");
}

int main(int argc, char *argv[])
{
	int interval = DEFAULT_INTERVAL_SEC;
	FILE *out = stdout;
	char *out_path = NULL;
	bool json_mode = false;
	bool with_cpuidle = false;
	int opt;
	static struct option long_opts[] = {
		{ "interval", required_argument, 0, 'i' },
		{ "output",   required_argument, 0, 'o' },
		{ "json",     no_argument,       0, 'j' },
		{ "help",     no_argument,       0, 'h' },
		{ "with-cpuidle", no_argument,       0, 'w' },
		{ 0, 0, 0, 0 }
	};

	while ((opt = getopt_long(argc, argv, "i:o:jhw", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'i':
			interval = atoi(optarg);
			if (interval < 1)
				interval = 1;
			break;
		case 'o':
			out_path = optarg;
			break;
		case 'j':
			json_mode = true;
			break;
		case 'h':
			usage(argv[0]);
			return 0;
		case 'w':
			with_cpuidle = true;
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}

	if (out_path) {
		out = fopen(out_path, "a");
		if (!out) {
			perror(out_path);
			return 1;
		}
	}

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

	cpufreq_data_t cf = {0};
	cpuidle_data_t ci = {0};
	thermal_data_t th = {0};
	battery_data_t bat = {0};
	regulator_data_t reg = {0};

	while (running) {
		log_all(out, json_mode, &cf, &ci, &th, &bat, &reg, with_cpuidle);
		if (running)
			sleep(interval);
	}

	if (out_path && out)
		fclose(out);

	return 0;
}
