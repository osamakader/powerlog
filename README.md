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

# Custom interval (e.g. every 2 seconds)
./powerlog -i 2

# Log to file
./powerlog -o /var/log/powerlog.txt

# Combined
./powerlog -i 10 -o /var/log/powerlog.txt
```

## Options

| Option | Description |
|--------|-------------|
| `-i, --interval SEC` | Polling interval in seconds (default: 5) |
| `-o, --output FILE` | Write log to file instead of stdout |
| `-h, --help` | Show help |

## Logged Data

- **CPU Frequency**: Current frequency (MHz) and governor per CPU
- **C-States**: Idle state names, residency time (µs), and usage counts
- **Thermal**: Temperature per thermal zone (°C)
- **Battery**: Capacity (%) and status (Charging/Discharging/Full)
- **Regulators**: State (enabled/disabled) and voltage (µV) when available

## Install

```bash
sudo make install
```

## License

MIT
