# resq — resmon MQTT subscriber CLI

Lightweight CLI for querying the latest retained `resmon` MQTT snapshot. Designed to be AI-agent-friendly: each subcommand returns a focused view, `--json` emits machine-readable output, and dotted paths let you pull a single value.

## Requirements

- **Python 3.8+** (stdlib only — no external deps)
- **`mosquitto_sub`** from the `mosquitto-clients` package

## Quick start

```bash
# 1. Copy and fill in the env template
cp resq.env.example ~/.resq.env
$EDITOR ~/.resq.env

# 2a. Point resq at it directly...
./resq --env-file ~/.resq.env                  # high-level overview
./resq --env-file ~/.resq.env tokens           # token rates

# ...or 2b. source it into your shell instead
source ~/.resq.env
./resq
```

`--env-file` only fills in variables that aren't already set in the shell, so exported env vars still win — and CLI flags win over both.

## Usage

```
resq [--from-file PATH] [--env-file PATH] [--json] [SUBCOMMAND] [ARGS]
```

**Global flags**

| Flag | Description |
|---|---|
| `--from-file PATH` | Read snapshot from a JSON file instead of MQTT (offline testing) |
| `--env-file PATH` | Load `RESMON_*` variables from this file (shell env still takes precedence) |
| `--json` | Emit machine-readable JSON output |
| `-h, --help` | Show help |

**Subcommands**

| Command | Description |
|---|---|
| `overview` | High-level summary (default when no subcommand given) |
| `cpu` | CPU load, frequency, temperature, power |
| `mem` | System memory and swap |
| `gpu` | GPU temperature, utilization, VRAM, power (one block per card) |
| `llama` | llama-server health, throughput summary, active slots |
| `tokens` | Token rates (`predicted_tps`, `prompt_tps`) + lifetime totals |
| `slots` | Per-slot details (ctx size, prompt tokens, decoded/remaining) |
| `raw [PATH]` | Full snapshot, or value at a dotted path (e.g. `gpu.0.temp_c`) |
| `hosts` | List publishing hosts and online/offline status |

**Examples**

```bash
resq                              # overview (default)
resq tokens                       # token rates
resq raw gpu.0.temp_c             # single value: 46.0
resq --json overview              # JSON overview for downstream tools
resq --json raw mem.total_bytes   # just the total memory in bytes
resq hosts                        # which machines are publishing
```

## Environment variables

All connection settings are encapsulated in env vars (CLI flags override). The same `RESMON_*` names used by the publisher work here — one env file for both sides.

| Variable | Required | Default | Description |
|---|---|---|---|
| `RESMON_MQTT_HOST` | yes | — | Broker hostname or IP |
| `RESMON_MQTT_PORT` | no | `8883` | Broker port |
| `RESMON_CA_CERT` | no | — | Path to CA certificate for TLS |
| `RESMON_MQTT_TLS_INSECURE` | no | `false` | Skip hostname/SAN check (truthy: `1`, `true`, `yes`, `on`) |
| `RESMON_MQTT_USERNAME` | no | — | Broker username |
| `RESMON_MQTT_PASSWORD` | no | — | Broker password |
| `RESMON_TOPIC_PREFIX` | no | `resmon` | MQTT topic prefix |

## How it works

The publisher writes `state` messages **retained** to `resmon/<host>/state`. `resq` runs `mosquitto_sub -C 1` against that topic, which returns the latest retained snapshot immediately (no waiting for the next interval). A 5-second timeout (`-W 5`) prevents hanging if the broker is unreachable.

Staleness is computed from the `ts` field in the snapshot — when the publisher goes offline, retained data becomes stale and `overview` flags it as `[STALE]`.

## Offline testing

Use `--from-file` with any JSON snapshot (e.g. one captured from `mosquitto_sub -t 'resmon/#'`):

```bash
./resq --from-file snapshot.json overview
./resq --from-file snapshot.json raw gpu.0.temp_c
```
