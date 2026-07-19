# resmon

Monitors llama-server diagnostics (health, throughput, slots, process
memory/VRAM) and host performance (CPU load/temp/power, system memory, GPU
temp/power/utilization/VRAM/clocks/fan) and publishes one JSON snapshot per
interval to an MQTT broker over TLS.

## Prerequisites

```
sudo apt-get install libmosquitto-dev libcurl4-openssl-dev nlohmann-json3-dev
```

GoogleTest is fetched automatically by CMake at configure time. GPU stats
are read via the `nvidia-smi` CLI (ships with the NVIDIA driver) rather than
linking NVML directly, so no CUDA dev packages are required.

## Build

```
cmake -B build
cmake --build build -j$(nproc)
```

Produces `build/resmon`.

## Test

```
ctest --test-dir build --output-on-failure
```

All tests are hermetic: unit tests exercise parsers and delta-state logic
directly (with temp-directory fixtures for stateful readers like RAPL
power), and the integration suite drives the full collect -> merge ->
publish pipeline through fakes for HTTP, subprocess, and MQTT -- no real
llama-server, GPU, or broker is required to run the suite.

## Configuration

Every setting is a CLI flag with an equivalent `RESMON_*` environment
variable fallback (CLI takes precedence). Run `resmon --help` for the
authoritative list, or see `include/resmon/constants.hpp` for the exact
flag/env-var/default mapping. Key settings:

| Flag | Env var | Default | Notes |
|---|---|---|---|
| `--llama-url` | `RESMON_LLAMA_URL` | `http://localhost:11434` | |
| `--mqtt-scheme` | `RESMON_MQTT_SCHEME` | `mqtts` | `mqtt` or `mqtts` |
| `--mqtt-host` | `RESMON_MQTT_HOST` | `localhost` | |
| `--mqtt-port` | `RESMON_MQTT_PORT` | 1883 (mqtt) / 8883 (mqtts) | |
| `--ca-cert` | `RESMON_CA_CERT` | (none) | required when scheme is `mqtts` |
| `--tls-insecure` | `RESMON_MQTT_TLS_INSECURE` | `false` | skip broker cert hostname/SAN check (chain validation against `--ca-cert` still applies); needed when connecting by IP or to a cert issued for a different name -- same as `mosquitto_pub`/`sub`'s `--insecure` |
| `--username` / `--password` | `RESMON_MQTT_USERNAME` / `RESMON_MQTT_PASSWORD` | (none) | optional, in addition to TLS |
| `--interval` | `RESMON_INTERVAL` | `10` | seconds |
| `--topic-prefix` | `RESMON_TOPIC_PREFIX` | `resmon` | |

State is published retained to `<prefix>/<hostname>/state`; availability
(`online`/`offline`, retained, with a broker-side Last Will for the
`offline` case on an unclean disconnect) to `<prefix>/<hostname>/status`.

## Deploy

### 1. RAPL permissions (CPU power)

CPU package power comes from Intel RAPL, which is root-only by default. A
udev rule makes it group-readable so resmon can run unprivileged:

```
sudo cp systemd/99-resmon-rapl.rules /etc/udev/rules.d/
sudo udevadm control --reload
sudo udevadm trigger -s powercap -c add
```

Without this, `cpu.power_w` is simply reported as `null` -- every other
field is unaffected.

### 2. Secrets/config file

```
sudo mkdir -p /etc/resmon
sudo cp systemd/resmon.env.example /etc/resmon/resmon.env
sudo chmod 600 /etc/resmon/resmon.env
sudo $EDITOR /etc/resmon/resmon.env   # set RESMON_MQTT_HOST, RESMON_CA_CERT, etc.
```

### 3. systemd service

```
sudo cp systemd/resmon.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now resmon
```

## Verify

```
systemctl status resmon
journalctl -u resmon -f
mosquitto_sub -h <broker> -p 8883 --cafile /etc/resmon/ca.pem \
  -t 'resmon/#' -v
```

You should see a retained `status` message (`online`), followed by a
`state` message every interval. Stopping the service
(`sudo systemctl stop resmon`) should publish a retained `offline` status
before exiting; killing it uncleanly should still deliver `offline` via the
broker's Last Will.
