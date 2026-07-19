#pragma once

#include <cstdint>
#include <string_view>

// Central home for literals that would otherwise be scattered across the
// codebase: default config values, sysfs/proc paths, HTTP endpoint paths,
// MQTT topic templates, env var names, and the JSON schema version.
namespace resmon::constants {

// --- JSON schema ---
inline constexpr int kSchemaVersion = 1;

// --- llama-server ---
inline constexpr std::string_view kDefaultLlamaUrl = "http://localhost:11434";
inline constexpr std::string_view kLlamaHealthPath = "/health";
inline constexpr std::string_view kLlamaSlotsPath = "/slots";
inline constexpr std::string_view kLlamaMetricsPath = "/metrics";
inline constexpr std::string_view kLlamaBinaryNameHint = "llama-server";

// --- MQTT ---
inline constexpr std::string_view kMqttSchemeMqtt = "mqtt";
inline constexpr std::string_view kMqttSchemeMqtts = "mqtts";
inline constexpr std::string_view kDefaultMqttScheme = kMqttSchemeMqtts;
inline constexpr std::string_view kDefaultMqttHost = "localhost";
inline constexpr int kDefaultMqttPortPlain = 1883;
inline constexpr int kDefaultMqttPortTls = 8883;
inline constexpr std::string_view kDefaultTopicPrefix = "resmon";
inline constexpr std::string_view kStateTopicSuffix = "state";
inline constexpr std::string_view kStatusTopicSuffix = "status";
inline constexpr std::string_view kStatusOnline = "online";
inline constexpr std::string_view kStatusOffline = "offline";
inline constexpr std::string_view kClientIdPrefix = "resmon-";
inline constexpr int kMqttKeepaliveSeconds = 30;
inline constexpr int kMqttConnectTimeoutSeconds = 10;
// Values of RESMON_MQTT_TLS_INSECURE (case-insensitive) treated as "true".
inline constexpr std::string_view kTruthyEnvValues[] = {"1", "true", "yes", "on"};

// --- Sampling ---
inline constexpr int kDefaultIntervalSeconds = 10;
inline constexpr int kDefaultHttpTimeoutMs = 3000;

// --- Filesystem roots (overridable for hermetic tests) ---
inline constexpr std::string_view kDefaultSysfsRoot = "/";
inline constexpr std::string_view kDefaultProcRoot = "/";

// --- CPU / hwmon ---
inline constexpr std::string_view kHwmonClassDir = "sys/class/hwmon";
inline constexpr std::string_view kCoretempChipName = "coretemp";
inline constexpr std::string_view kCpuStatPath = "proc/stat";
inline constexpr std::string_view kCpuInfoPath = "proc/cpuinfo";

// --- RAPL ---
inline constexpr std::string_view kRaplPackageDir = "sys/class/powercap/intel-rapl:0";
inline constexpr std::string_view kRaplEnergyFile = "energy_uj";
inline constexpr std::string_view kRaplMaxEnergyFile = "max_energy_range_uj";
inline constexpr std::string_view kRaplNameFile = "name";

// --- Memory ---
inline constexpr std::string_view kMeminfoPath = "proc/meminfo";

// --- GPU (via nvidia-smi subprocess; see gpu_collector for rationale) ---
inline constexpr std::string_view kNvidiaSmiBinary = "nvidia-smi";
inline constexpr std::string_view kNvidiaSmiGpuQueryFields =
    "index,name,temperature.gpu,power.draw,power.limit,utilization.gpu,utilization.memory,"
    "memory.used,memory.total,clocks.sm,fan.speed";
inline constexpr std::string_view kNvidiaSmiComputeAppsQueryFields = "pid,used_memory";
inline constexpr std::string_view kNvidiaSmiCsvFormat = "csv,noheader,nounits";
inline constexpr std::string_view kNvidiaSmiNotAvailableMarkers[] = {"[N/A]", "[Not Supported]",
                                                                      "N/A"};

// --- Env var names (mirror CLI flags; CLI takes precedence) ---
inline constexpr std::string_view kEnvLlamaUrl = "RESMON_LLAMA_URL";
inline constexpr std::string_view kEnvMqttScheme = "RESMON_MQTT_SCHEME";
inline constexpr std::string_view kEnvMqttHost = "RESMON_MQTT_HOST";
inline constexpr std::string_view kEnvMqttPort = "RESMON_MQTT_PORT";
inline constexpr std::string_view kEnvCaCert = "RESMON_CA_CERT";
inline constexpr std::string_view kEnvUsername = "RESMON_MQTT_USERNAME";
inline constexpr std::string_view kEnvPassword = "RESMON_MQTT_PASSWORD";
inline constexpr std::string_view kEnvClientId = "RESMON_MQTT_CLIENT_ID";
inline constexpr std::string_view kEnvTopicPrefix = "RESMON_TOPIC_PREFIX";
inline constexpr std::string_view kEnvHostname = "RESMON_HOSTNAME";
inline constexpr std::string_view kEnvInterval = "RESMON_INTERVAL";
inline constexpr std::string_view kEnvSysfsRoot = "RESMON_SYSFS_ROOT";
inline constexpr std::string_view kEnvProcRoot = "RESMON_PROC_ROOT";
inline constexpr std::string_view kEnvTlsInsecure = "RESMON_MQTT_TLS_INSECURE";

}  // namespace resmon::constants
