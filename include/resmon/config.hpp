#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace resmon {

// Thrown for any invalid/missing configuration (bad flag, bad value,
// mqtts without a CA cert, non-positive interval, etc).
class ConfigError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Thrown when --help is requested; caller (main) should print usage() and exit 0.
class HelpRequested : public std::exception {};

// Looks up an environment-style variable by name. Production code uses
// systemEnvLookup (real getenv); tests inject a fake map for hermetic,
// order-independent precedence testing.
using EnvLookup = std::function<std::optional<std::string>(std::string_view)>;

std::optional<std::string> systemEnvLookup(std::string_view name);

struct Config {
  std::string llama_url;
  std::string mqtt_scheme;
  std::string mqtt_host;
  int mqtt_port = -1;  // -1 sentinel: not explicitly set, resolved from scheme
  std::string ca_cert;
  // Skips verifying the broker cert's hostname/SAN against mqtt_host (still
  // validates the cert chains to ca_cert). Needed when connecting by IP or
  // to a cert issued for a different name -- mirrors mosquitto_pub/sub's
  // --insecure flag.
  bool tls_insecure = false;
  std::string username;
  std::string password;
  std::string client_id;
  std::string topic_prefix;
  std::string hostname;
  int interval_seconds = 0;
  std::string sysfs_root;
  std::string proc_root;

  // Effective MQTT port: mqtt_port if explicitly set, else the scheme's default.
  int resolvedMqttPort() const;

  std::string stateTopic() const;
  std::string statusTopic() const;
  bool tlsEnabled() const;
};

// Parses configuration from CLI args (highest precedence), then env vars
// (via `env`), then built-in defaults. Throws ConfigError on invalid input,
// HelpRequested if --help was passed.
Config parseConfig(int argc, char** argv, const EnvLookup& env = systemEnvLookup);

// Usage/help text, printed by main() on HelpRequested or ConfigError.
std::string usageText();

}  // namespace resmon
