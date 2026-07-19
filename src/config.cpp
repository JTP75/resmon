#include "resmon/config.hpp"

#include <getopt.h>
#include <unistd.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "resmon/constants.hpp"

namespace resmon {

namespace {

namespace c = constants;

std::string resolveLocalHostname() {
  std::array<char, 256> buf{};
  if (::gethostname(buf.data(), buf.size()) == 0) {
    buf[buf.size() - 1] = '\0';
    return std::string(buf.data());
  }
  return "unknown-host";
}

// Resolves a single setting with precedence: CLI value > env var > default.
std::string resolveStr(const std::optional<std::string>& cli_value,
                        const EnvLookup& env, std::string_view env_name,
                        std::string_view default_value) {
  if (cli_value.has_value()) return *cli_value;
  if (auto v = env(env_name)) return *v;
  return std::string(default_value);
}

int parseIntOrThrow(std::string_view name, const std::string& text) {
  try {
    size_t pos = 0;
    int value = std::stoi(text, &pos);
    if (pos != text.size()) throw std::invalid_argument("trailing chars");
    return value;
  } catch (const std::exception&) {
    throw ConfigError("invalid integer for " + std::string(name) + ": '" + text + "'");
  }
}

}  // namespace

std::optional<std::string> systemEnvLookup(std::string_view name) {
  const char* value = std::getenv(std::string(name).c_str());
  if (value == nullptr) return std::nullopt;
  return std::string(value);
}

int Config::resolvedMqttPort() const {
  if (mqtt_port > 0) return mqtt_port;
  return tlsEnabled() ? c::kDefaultMqttPortTls : c::kDefaultMqttPortPlain;
}

bool Config::tlsEnabled() const { return mqtt_scheme == c::kMqttSchemeMqtts; }

std::string Config::stateTopic() const {
  return topic_prefix + "/" + hostname + "/" + std::string(c::kStateTopicSuffix);
}

std::string Config::statusTopic() const {
  return topic_prefix + "/" + hostname + "/" + std::string(c::kStatusTopicSuffix);
}

std::string usageText() {
  std::ostringstream os;
  os << "resmon - llama-server & system monitor, publishes to MQTT\n\n"
     << "Usage: resmon [options]\n\n"
     << "  --llama-url URL        llama-server base URL (default: " << c::kDefaultLlamaUrl << ")\n"
     << "  --mqtt-scheme mqtt|mqtts   (default: " << c::kDefaultMqttScheme << ")\n"
     << "  --mqtt-host HOST       broker host (default: " << c::kDefaultMqttHost << ")\n"
     << "  --mqtt-port PORT       broker port (default: 1883 plain / 8883 tls)\n"
     << "  --ca-cert PATH         CA cert file, required for mqtts\n"
     << "  --username USER        optional broker username\n"
     << "  --password PASS        optional broker password (prefer " << c::kEnvPassword << " env var)\n"
     << "  --client-id ID         MQTT client id (default: derived from hostname)\n"
     << "  --topic-prefix PREFIX  topic namespace (default: " << c::kDefaultTopicPrefix << ")\n"
     << "  --hostname NAME        override reported hostname (default: system hostname)\n"
     << "  --interval SECONDS     sampling interval (default: " << c::kDefaultIntervalSeconds << ")\n"
     << "  --sysfs-root PATH      sysfs root, for testing (default: /)\n"
     << "  --proc-root PATH       procfs root, for testing (default: /)\n"
     << "  --help                 show this text\n";
  return os.str();
}

Config parseConfig(int argc, char** argv, const EnvLookup& env) {
  std::optional<std::string> cli_llama_url, cli_mqtt_scheme, cli_mqtt_host, cli_mqtt_port,
      cli_ca_cert, cli_username, cli_password, cli_client_id, cli_topic_prefix, cli_hostname,
      cli_interval, cli_sysfs_root, cli_proc_root;

  enum LongOpt {
    kOptLlamaUrl = 1000,
    kOptMqttScheme,
    kOptMqttHost,
    kOptMqttPort,
    kOptCaCert,
    kOptUsername,
    kOptPassword,
    kOptClientId,
    kOptTopicPrefix,
    kOptHostname,
    kOptInterval,
    kOptSysfsRoot,
    kOptProcRoot,
    kOptHelp,
  };

  static const struct option kLongOptions[] = {
      {"llama-url", required_argument, nullptr, kOptLlamaUrl},
      {"mqtt-scheme", required_argument, nullptr, kOptMqttScheme},
      {"mqtt-host", required_argument, nullptr, kOptMqttHost},
      {"mqtt-port", required_argument, nullptr, kOptMqttPort},
      {"ca-cert", required_argument, nullptr, kOptCaCert},
      {"username", required_argument, nullptr, kOptUsername},
      {"password", required_argument, nullptr, kOptPassword},
      {"client-id", required_argument, nullptr, kOptClientId},
      {"topic-prefix", required_argument, nullptr, kOptTopicPrefix},
      {"hostname", required_argument, nullptr, kOptHostname},
      {"interval", required_argument, nullptr, kOptInterval},
      {"sysfs-root", required_argument, nullptr, kOptSysfsRoot},
      {"proc-root", required_argument, nullptr, kOptProcRoot},
      {"help", no_argument, nullptr, kOptHelp},
      {nullptr, 0, nullptr, 0},
  };

  // getopt_long is process-global state; reset so repeated calls in the
  // same process (unit tests) don't see stale state from a prior parse.
  optind = 1;
  opterr = 0;

  int opt;
  while ((opt = getopt_long(argc, argv, "", kLongOptions, nullptr)) != -1) {
    switch (opt) {
      case kOptLlamaUrl: cli_llama_url = optarg; break;
      case kOptMqttScheme: cli_mqtt_scheme = optarg; break;
      case kOptMqttHost: cli_mqtt_host = optarg; break;
      case kOptMqttPort: cli_mqtt_port = optarg; break;
      case kOptCaCert: cli_ca_cert = optarg; break;
      case kOptUsername: cli_username = optarg; break;
      case kOptPassword: cli_password = optarg; break;
      case kOptClientId: cli_client_id = optarg; break;
      case kOptTopicPrefix: cli_topic_prefix = optarg; break;
      case kOptHostname: cli_hostname = optarg; break;
      case kOptInterval: cli_interval = optarg; break;
      case kOptSysfsRoot: cli_sysfs_root = optarg; break;
      case kOptProcRoot: cli_proc_root = optarg; break;
      case kOptHelp: throw HelpRequested();
      case '?':
      default:
        throw ConfigError("unrecognized option: " +
                           std::string(optind > 0 && optind <= argc ? argv[optind - 1] : "?"));
    }
  }

  Config cfg;
  cfg.llama_url = resolveStr(cli_llama_url, env, c::kEnvLlamaUrl, c::kDefaultLlamaUrl);
  cfg.mqtt_scheme = resolveStr(cli_mqtt_scheme, env, c::kEnvMqttScheme, c::kDefaultMqttScheme);
  cfg.mqtt_host = resolveStr(cli_mqtt_host, env, c::kEnvMqttHost, c::kDefaultMqttHost);
  cfg.ca_cert = resolveStr(cli_ca_cert, env, c::kEnvCaCert, "");
  cfg.username = resolveStr(cli_username, env, c::kEnvUsername, "");
  cfg.password = resolveStr(cli_password, env, c::kEnvPassword, "");
  cfg.topic_prefix = resolveStr(cli_topic_prefix, env, c::kEnvTopicPrefix, c::kDefaultTopicPrefix);
  cfg.sysfs_root = resolveStr(cli_sysfs_root, env, c::kEnvSysfsRoot, c::kDefaultSysfsRoot);
  cfg.proc_root = resolveStr(cli_proc_root, env, c::kEnvProcRoot, c::kDefaultProcRoot);

  cfg.hostname = resolveStr(cli_hostname, env, c::kEnvHostname, "");
  if (cfg.hostname.empty()) cfg.hostname = resolveLocalHostname();

  cfg.client_id = resolveStr(cli_client_id, env, c::kEnvClientId, "");
  if (cfg.client_id.empty()) cfg.client_id = std::string(c::kClientIdPrefix) + cfg.hostname;

  std::string port_text = resolveStr(cli_mqtt_port, env, c::kEnvMqttPort, "");
  cfg.mqtt_port = port_text.empty() ? -1 : parseIntOrThrow("--mqtt-port", port_text);

  std::string interval_text =
      resolveStr(cli_interval, env, c::kEnvInterval, std::to_string(c::kDefaultIntervalSeconds));
  cfg.interval_seconds = parseIntOrThrow("--interval", interval_text);

  // --- validation ---
  if (cfg.mqtt_scheme != c::kMqttSchemeMqtt && cfg.mqtt_scheme != c::kMqttSchemeMqtts) {
    throw ConfigError("--mqtt-scheme must be 'mqtt' or 'mqtts', got: '" + cfg.mqtt_scheme + "'");
  }
  if (cfg.tlsEnabled() && cfg.ca_cert.empty()) {
    throw ConfigError("--ca-cert is required when --mqtt-scheme is 'mqtts'");
  }
  if (cfg.mqtt_port > 0 && (cfg.mqtt_port > 65535)) {
    throw ConfigError("--mqtt-port out of range: " + std::to_string(cfg.mqtt_port));
  }
  if (cfg.interval_seconds <= 0) {
    throw ConfigError("--interval must be a positive integer, got: " +
                       std::to_string(cfg.interval_seconds));
  }
  if (cfg.llama_url.empty()) {
    throw ConfigError("--llama-url must not be empty");
  }
  if (cfg.mqtt_host.empty()) {
    throw ConfigError("--mqtt-host must not be empty");
  }

  return cfg;
}

}  // namespace resmon
