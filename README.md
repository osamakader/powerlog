# powerlog

Power Consumption Logger for Linux. Logs CPU frequency, C-states, thermal state, battery level, and regulator status by reading from sysfs.

## Requirements

- Linux with sysfs
- Root access may be needed for some sysfs paths (e.g. regulator)

## Build

```bash
make
```

## Usage

```bash
# Log to stdout every 5 seconds (default)
./powerlog

# Custom interval (bare number = seconds; suffix `s` or `ms`)
./powerlog -i 2
./powerlog -i 100ms

# Time-series: sample every 100ms for 10 seconds, then exit
./powerlog --interval 100ms --duration 10s

Timestamps include milliseconds so fast intervals stay ordered in logs and JSON.

# Log to file
./powerlog -o /var/log/powerlog.txt

# Combined
./powerlog -i 10 -o /var/log/powerlog.txt

# Alerts (stderr on threshold crossing; JSON includes an `alerts` array when active)
./powerlog -T 80 -B 15
./powerlog -j -o log.json -T 85
```

## Options

| Option | Description |
|--------|-------------|
| `-i, --interval TIME` | Sample period: `5` or `5s` = seconds, `100ms` = milliseconds (default: 5s) |
| `-d, --duration TIME` | Stop after TIME (same format as interval); omit to run until interrupted |
| `-o, --output FILE` | Write log to file instead of stdout |
| `-j, --json` | Output in JSON format (pretty-printed) |
| `-w, --with-cpuidle` | Include C-state (cpuidle) data |
| `-T, --alert-thermal C` | Alert when any thermal zone reaches C °C |
| `-B, --alert-battery PCT` | Alert when battery is at or below PCT % while discharging |
| `-h, --help` | Show help |
| `-a, --all` | print all |
| `-b, --brief` | print brief table |

## Alerts

- **Thermal** (`-T`): Compares the hottest zone to the threshold. Messages go to **stderr** when the condition starts or clears (edge-triggered). With `-j`, each snapshot includes an `alerts` array listing current thermal/battery violations.
- **Battery** (`-B`): Only evaluated while the pack reports **Discharging** (or Unknown). Cleared when capacity rises above the threshold or status changes.

## Logged Data

- **CPU Frequency**: Current frequency (MHz) and energy performance preference per CPU
- **C-States** (with `-w`): Idle state names, residency time (µs), and usage counts
- **Thermal**: Known sysfs types are renamed (e.g. `x86_pkg_temp` → `cpu_package`, `iwlwifi_*` → `wifi`). OEM sensors such as `SEN1`… are listed under **Other sensors**. Text lines append **(Δ °C)** vs the previous sample when available. JSON adds `zone_delta_c` (per thermal zone index), optional `delta_c` on `other_sensors` entries, and `delta_mhz` on each CPU in `cpufreq`.
- **Battery**: `BAT*` power supplies only (e.g. `BAT0`); HID accessories without capacity are omitted. Text/JSON only list entries with a readable capacity.
- **Regulators**: State (enabled/disabled) and voltage (µV) when available. The kernel `regulator-dummy` placeholder is omitted.

## Install

```bash
sudo make install
```

## License

MIT
