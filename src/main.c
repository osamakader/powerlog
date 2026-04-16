/*
 * Power Consumption Logger
 * Logs CPU frequency, C-states, thermal state, battery level, and regulator status.
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <strings.h>

#include <stdbool.h>
#include "common.h"
#include "alerts.h"
#include "cpufreq.h"
#include "cpuidle.h"
#include "thermal.h"
#include "battery.h"
#include "regulator.h"

#define DEFAULT_INTERVAL_MS 5000u
#define TIMESTAMP_BUF_SIZE 48 /* YYYY-mm-dd HH:MM:SS.mmm + NUL */

static volatile sig_atomic_t running = 1;

/* Bare number = seconds; suffix `s` = seconds, `ms` = milliseconds */
static int parse_time_ms(const char *arg, unsigned *out_ms)
{
	char *end;
	double v = strtod(arg, &end);

	if (end == arg)
		return -1;
	if (*end == '\0') {
		if (v < 0)
			return -1;
		*out_ms = (unsigned)(v * 1000.0 + 0.5);
		return 0;
	}
	if (strcasecmp(end, "ms") == 0) {
		if (v < 0)
			return -1;
		*out_ms = (unsigned)(v + 0.5);
		return 0;
	}
	if (strcasecmp(end, "s") == 0) {
		if (v < 0)
			return -1;
		*out_ms = (unsigned)(v * 1000.0 + 0.5);
		return 0;
	}
	return -1;
}

static void sleep_ms(unsigned ms)
{
	struct timespec req, rem;

	req.tv_sec = ms / 1000u;
	req.tv_nsec = (long)(ms % 1000u) * 1000000L;
	while (nanosleep(&req, &rem) == -1) {
		if (errno != EINTR)
			break;
		req = rem;
	}
}

static uint64_t monotonic_ms_since(const struct timespec *t0)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)(now.tv_sec - t0->tv_sec) * 1000ULL
		+ (uint64_t)(now.tv_nsec - t0->tv_nsec) / 1000000ULL;
}

static void sig_handler(int sig)
{
	(void)sig;
	running = 0;
}

/* Wall clock with millisecond suffix (for time-series alignment) */
static void format_ts_wall(char *buf, size_t len, bool json_fmt)
{
	struct timespec rtc;
	struct tm *tm;

	clock_gettime(CLOCK_REALTIME, &rtc);
	tm = localtime(&rtc.tv_sec);
	if (json_fmt)
		strftime(buf, len, "%Y-%m-%dT%H:%M:%S", tm);
	else
		strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm);
	if (strlen(buf) + 5 < len)
		snprintf(buf + strlen(buf), len - strlen(buf), ".%03ld",
			 (long)(rtc.tv_nsec / 1000000L));
}

static void log_all(FILE *out, bool json,
		    cpufreq_data_t *cf, const cpufreq_data_t *cf_prev,
		    cpuidle_data_t *ci,
		    thermal_data_t *th, const thermal_data_t *th_prev,
		    battery_data_t *bat, regulator_data_t *reg,
		    bool with_cpuidle, const alert_config_t *alerts, alert_edge_state_t *edge,
		    unsigned interval_ms, bool print_all, unsigned sample_no)
{
	char ts[TIMESTAMP_BUF_SIZE];
	char ts_human[TIMESTAMP_BUF_SIZE];

	cpufreq_collect(cf);
	thermal_collect(th);
	battery_collect(bat);
	if (json || print_all) {
		if (with_cpuidle)
			cpuidle_collect(ci);
		regulator_collect(reg);
	}

	format_ts_wall(ts_human, sizeof(ts_human), false);
	format_ts_wall(ts, sizeof(ts), true);
	if (alerts->thermal_max_c >= 0 || alerts->battery_low_pct >= 0)
		alerts_notify_edges(stderr, alerts, edge, th, bat, ts_human);

	if (json) {
		fprintf(out, "{\n  \"timestamp\": \"%s\",\n  ", ts);
		cpufreq_json(out, cf, cf_prev);
		if (with_cpuidle) {
			fprintf(out, ",\n  ");
			cpuidle_json(out, ci);
		}
		fprintf(out, ",\n  ");
		thermal_json(out, th, th_prev, interval_ms);
		fprintf(out, ",\n  ");
		battery_json(out, bat);
		fprintf(out, ",\n  ");
		regulator_json(out, reg);
		fprintf(out, ",\n  ");
		alerts_json(out, alerts, th, bat);
		fprintf(out, "\n}\n");
	} else {
		if (sample_no == 1u) {
			char buf[TIMESTAMP_BUF_SIZE];

			format_ts_wall(buf, sizeof(buf), false);
			log_text_timestamp(out, buf);
		} else {
			log_text_sample_index(out, sample_no);
		}
		cpufreq_log(out, cf, cf_prev, print_all);
		if (print_all && with_cpuidle)
			cpuidle_log(out, ci);
		thermal_log(out, th, th_prev, interval_ms, print_all);
		battery_log(out, bat);
		if (print_all)
			regulator_log(out, reg);
	}
	fflush(out);
}

static void usage(const char *prog)
{
	printf("Usage: %s [OPTIONS]\n", prog);
	printf("Power Consumption Logger - logs CPU freq, C-states, thermal, battery, regulators\n\n");
	printf("Options:\n");
	printf("  -i, --interval TIME  Sample interval: e.g. 5, 5s, 100ms (default: 5s)\n");
	printf("  -d, --duration TIME  Stop after TIME (e.g. 10s); omit to run until Ctrl+C\n");
	printf("  -o, --output FILE    Write log to file (default: stdout)\n");
	printf("  -j, --json           Output in JSON format (pretty-printed)\n");
	printf("  -a, --all            Text: print every CPU, thermal zone, regulators, etc.\n");
	printf("                       (default: summary + hottest thermal + battery only)\n");
	printf("  -T, --alert-thermal C  Alert when any zone reaches C °C (stderr + JSON alerts)\n");
	printf("  -B, --alert-battery PCT Alert when battery ≤ PCT%% while discharging\n");
	printf("  -h, --help           Show this help\n");
}

int main(int argc, char *argv[])
{
	unsigned interval_ms = DEFAULT_INTERVAL_MS;
	unsigned duration_ms = 0;
	FILE *out = stdout;
	char *out_path = NULL;
	bool json_mode = false;
	bool print_all = false;
	bool with_cpuidle = false;
	alert_config_t alert_cfg;
	alert_edge_state_t alert_edge;
	int opt;
	static struct option long_opts[] = {
		{ "interval", required_argument, 0, 'i' },
		{ "duration", required_argument, 0, 'd' },
		{ "output",   required_argument, 0, 'o' },
		{ "json",     no_argument,       0, 'j' },
		{ "help",     no_argument,       0, 'h' },
		{ "with-cpuidle", no_argument, 0, 'w' },
		{ "alert-thermal", required_argument, 0, 'T' },
		{ "alert-battery", required_argument, 0, 'B' },
		{ "all", no_argument, 0, 'a' },
		{ 0, 0, 0, 0 }
	};

	alert_config_init(&alert_cfg);
	alert_edge_init(&alert_edge);

	while ((opt = getopt_long(argc, argv, "i:d:o:jhwaT:B:", long_opts, NULL)) != -1) {
		switch (opt) {
		case 'i':
			if (parse_time_ms(optarg, &interval_ms) != 0 || interval_ms < 1u) {
				fprintf(stderr, "%s: invalid interval: %s (use e.g. 5, 5s, 100ms)\n",
					argv[0], optarg);
				return 1;
			}
			break;
		case 'd':
			if (parse_time_ms(optarg, &duration_ms) != 0 || duration_ms < 1u) {
				fprintf(stderr, "%s: invalid duration: %s (use e.g. 10s, 500ms)\n",
					argv[0], optarg);
				return 1;
			}
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
		case 'a':
			print_all = true;
			break;
		case 'T':
			alert_cfg.thermal_max_c = atoi(optarg);
			break;
		case 'B':
			alert_cfg.battery_low_pct = atoi(optarg);
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
	cpufreq_data_t cf_prev = {0};
	cpuidle_data_t ci = {0};
	thermal_data_t th = {0};
	thermal_data_t th_prev = {0};
	battery_data_t bat = {0};
	regulator_data_t reg = {0};
	struct timespec t0;
	unsigned sample_no = 0;

	clock_gettime(CLOCK_MONOTONIC, &t0);

	while (running) {
		sample_no++;
		log_all(out, json_mode, &cf, &cf_prev, &ci, &th, &th_prev, &bat, &reg,
			with_cpuidle, &alert_cfg, &alert_edge, interval_ms, print_all,
			sample_no);
		cf_prev = cf;
		th_prev = th;

		if (duration_ms > 0 && monotonic_ms_since(&t0) >= (uint64_t)duration_ms)
			break;

		if (!running)
			break;

		if (duration_ms > 0) {
			uint64_t elapsed = monotonic_ms_since(&t0);
			uint64_t remaining;

			if (elapsed >= (uint64_t)duration_ms)
				break;
			remaining = (uint64_t)duration_ms - elapsed;
			sleep_ms(interval_ms < remaining ? interval_ms : (unsigned)remaining);
		} else {
			sleep_ms(interval_ms);
		}
	}

	if (out_path && out)
		fclose(out);

	return 0;
}
