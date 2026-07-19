#include "resmon/config.hpp"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

namespace resmon {
namespace {

// Builds an argv-like array from string args (arg0 = program name is added
// automatically). The backing strings must outlive the returned pointers,
// so this returns both.
struct Argv {
  std::vector<std::string> storage;
  std::vector<char*> ptrs;

  int argc() const { return static_cast<int>(ptrs.size()); }
  char** argv() { return ptrs.data(); }
};

Argv makeArgv(std::vector<std::string> args) {
  Argv result;
  result.storage.push_back("resmon");
  for (auto& a : args) result.storage.push_back(std::move(a));
  for (auto& s : result.storage) result.ptrs.push_back(s.data());
  return result;
}

EnvLookup fakeEnv(std::map<std::string, std::string> values) {
  return [values = std::move(values)](std::string_view name) -> std::optional<std::string> {
    auto it = values.find(std::string(name));
    if (it == values.end()) return std::nullopt;
    return it->second;
  };
}

EnvLookup emptyEnv() { return fakeEnv({}); }

// A config that satisfies all validation without relying on defaults that
// might change; every test starts from these required flags and overrides.
std::vector<std::string> baseArgs() {
  return {"--hostname", "test-host", "--mqtt-scheme", "mqtt"};
}

TEST(Config, DefaultsApplyWhenNothingProvided) {
  auto argv = makeArgv(baseArgs());
  Config cfg = parseConfig(argv.argc(), argv.argv(), emptyEnv());

  EXPECT_EQ(cfg.llama_url, "http://localhost:11434");
  EXPECT_EQ(cfg.mqtt_host, "localhost");
  EXPECT_EQ(cfg.topic_prefix, "resmon");
  EXPECT_EQ(cfg.interval_seconds, 10);
  EXPECT_EQ(cfg.hostname, "test-host");
  EXPECT_EQ(cfg.client_id, "resmon-test-host");
  EXPECT_EQ(cfg.stateTopic(), "resmon/test-host/state");
  EXPECT_EQ(cfg.statusTopic(), "resmon/test-host/status");
}

TEST(Config, CliOverridesDefault) {
  auto args = baseArgs();
  args.insert(args.end(), {"--interval", "42", "--topic-prefix", "custom"});
  auto argv = makeArgv(args);
  Config cfg = parseConfig(argv.argc(), argv.argv(), emptyEnv());

  EXPECT_EQ(cfg.interval_seconds, 42);
  EXPECT_EQ(cfg.topic_prefix, "custom");
}

TEST(Config, EnvOverridesDefaultWhenNoCli) {
  auto argv = makeArgv(baseArgs());
  auto env = fakeEnv({{"RESMON_INTERVAL", "77"}, {"RESMON_TOPIC_PREFIX", "envprefix"}});
  Config cfg = parseConfig(argv.argc(), argv.argv(), env);

  EXPECT_EQ(cfg.interval_seconds, 77);
  EXPECT_EQ(cfg.topic_prefix, "envprefix");
}

TEST(Config, CliTakesPrecedenceOverEnv) {
  auto args = baseArgs();
  args.insert(args.end(), {"--interval", "5"});
  auto argv = makeArgv(args);
  auto env = fakeEnv({{"RESMON_INTERVAL", "77"}});
  Config cfg = parseConfig(argv.argc(), argv.argv(), env);

  EXPECT_EQ(cfg.interval_seconds, 5);
}

TEST(Config, MqttSchemeDefaultsPortPlain) {
  auto argv = makeArgv({"--hostname", "h", "--mqtt-scheme", "mqtt"});
  Config cfg = parseConfig(argv.argc(), argv.argv(), emptyEnv());
  EXPECT_EQ(cfg.resolvedMqttPort(), 1883);
  EXPECT_FALSE(cfg.tlsEnabled());
}

TEST(Config, MqttSchemeDefaultsPortTls) {
  auto argv = makeArgv(
      {"--hostname", "h", "--mqtt-scheme", "mqtts", "--ca-cert", "/tmp/ca.pem"});
  Config cfg = parseConfig(argv.argc(), argv.argv(), emptyEnv());
  EXPECT_EQ(cfg.resolvedMqttPort(), 8883);
  EXPECT_TRUE(cfg.tlsEnabled());
}

TEST(Config, ExplicitPortOverridesSchemeDefault) {
  auto argv = makeArgv(
      {"--hostname", "h", "--mqtt-scheme", "mqtts", "--ca-cert", "/tmp/ca.pem", "--mqtt-port", "9999"});
  Config cfg = parseConfig(argv.argc(), argv.argv(), emptyEnv());
  EXPECT_EQ(cfg.resolvedMqttPort(), 9999);
}

TEST(Config, MqttsWithoutCaCertThrows) {
  auto argv = makeArgv({"--hostname", "h", "--mqtt-scheme", "mqtts"});
  EXPECT_THROW(parseConfig(argv.argc(), argv.argv(), emptyEnv()), ConfigError);
}

TEST(Config, InvalidSchemeThrows) {
  auto argv = makeArgv({"--hostname", "h", "--mqtt-scheme", "ftp"});
  EXPECT_THROW(parseConfig(argv.argc(), argv.argv(), emptyEnv()), ConfigError);
}

TEST(Config, NonPositiveIntervalThrows) {
  auto args = baseArgs();
  args.insert(args.end(), {"--interval", "0"});
  auto argv = makeArgv(args);
  EXPECT_THROW(parseConfig(argv.argc(), argv.argv(), emptyEnv()), ConfigError);
}

TEST(Config, InvalidIntervalTextThrows) {
  auto args = baseArgs();
  args.insert(args.end(), {"--interval", "not-a-number"});
  auto argv = makeArgv(args);
  EXPECT_THROW(parseConfig(argv.argc(), argv.argv(), emptyEnv()), ConfigError);
}

TEST(Config, HelpRequestedThrows) {
  auto argv = makeArgv({"--help"});
  EXPECT_THROW(parseConfig(argv.argc(), argv.argv(), emptyEnv()), HelpRequested);
}

TEST(Config, PasswordPreferredFromEnv) {
  auto argv = makeArgv(baseArgs());
  auto env = fakeEnv({{"RESMON_MQTT_PASSWORD", "secret"}});
  Config cfg = parseConfig(argv.argc(), argv.argv(), env);
  EXPECT_EQ(cfg.password, "secret");
}

TEST(Config, CustomClientIdOverridesDerived) {
  auto args = baseArgs();
  args.insert(args.end(), {"--client-id", "my-client"});
  auto argv = makeArgv(args);
  Config cfg = parseConfig(argv.argc(), argv.argv(), emptyEnv());
  EXPECT_EQ(cfg.client_id, "my-client");
}

TEST(Config, TlsInsecureDefaultsFalse) {
  auto argv = makeArgv(baseArgs());
  Config cfg = parseConfig(argv.argc(), argv.argv(), emptyEnv());
  EXPECT_FALSE(cfg.tls_insecure);
}

TEST(Config, TlsInsecureFlagSetsTrue) {
  auto args = baseArgs();
  args.insert(args.end(), {"--tls-insecure"});
  auto argv = makeArgv(args);
  Config cfg = parseConfig(argv.argc(), argv.argv(), emptyEnv());
  EXPECT_TRUE(cfg.tls_insecure);
}

TEST(Config, TlsInsecureFromEnvIsTruthy) {
  auto argv = makeArgv(baseArgs());
  auto env = fakeEnv({{"RESMON_MQTT_TLS_INSECURE", "true"}});
  Config cfg = parseConfig(argv.argc(), argv.argv(), env);
  EXPECT_TRUE(cfg.tls_insecure);
}

TEST(Config, TlsInsecureFromEnvRejectsNonTruthyValue) {
  auto argv = makeArgv(baseArgs());
  auto env = fakeEnv({{"RESMON_MQTT_TLS_INSECURE", "nope"}});
  Config cfg = parseConfig(argv.argc(), argv.argv(), env);
  EXPECT_FALSE(cfg.tls_insecure);
}

}  // namespace
}  // namespace resmon
