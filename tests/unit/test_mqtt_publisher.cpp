#include "resmon/mqtt_publisher.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace resmon {
namespace {

TEST(MqttPublisherConstruct, PlainConfigConstructsWithoutThrowing) {
  MqttConnectConfig cfg;
  cfg.host = "localhost";
  cfg.client_id = "resmon-test-plain";
  EXPECT_NO_THROW({ MosquittoPublisher pub(cfg); });
}

TEST(MqttPublisherConstruct, TlsWithMissingCaCertThrows) {
  MqttConnectConfig cfg;
  cfg.host = "localhost";
  cfg.client_id = "resmon-test-tls-missing";
  cfg.tls_enabled = true;
  cfg.ca_cert_path = "/nonexistent/path/ca.pem";
  EXPECT_THROW({ MosquittoPublisher pub(cfg); }, std::runtime_error);
}

TEST(MqttPublisherConstruct, TlsInsecureWithReadableCaCertConstructsWithoutThrowing) {
  // mosquitto_tls_set only checks that the file is readable, not that it's
  // a valid PEM -- any existing file works here without needing a real cert.
  MqttConnectConfig cfg;
  cfg.host = "localhost";
  cfg.client_id = "resmon-test-tls-insecure";
  cfg.tls_enabled = true;
  cfg.tls_insecure = true;
  cfg.ca_cert_path = "/etc/hostname";
  EXPECT_NO_THROW({ MosquittoPublisher pub(cfg); });
}

TEST(MqttPublisherConstruct, TlsInsecureStillRequiresReadableCaCert) {
  // tls_insecure only skips hostname/SAN verification -- chain validation
  // against ca_cert_path still applies, so a missing file still throws.
  MqttConnectConfig cfg;
  cfg.host = "localhost";
  cfg.client_id = "resmon-test-tls-insecure-missing-ca";
  cfg.tls_enabled = true;
  cfg.tls_insecure = true;
  cfg.ca_cert_path = "/nonexistent/path/ca.pem";
  EXPECT_THROW({ MosquittoPublisher pub(cfg); }, std::runtime_error);
}

TEST(MqttPublisherConnect, TimesOutWhenBrokerUnreachable) {
  MqttConnectConfig cfg;
  cfg.host = "127.0.0.1";
  cfg.port = 1;  // nothing listens here; connect should never succeed
  cfg.client_id = "resmon-test-timeout";
  cfg.connect_timeout_seconds = 1;
  MosquittoPublisher pub(cfg);
  EXPECT_THROW(pub.connect(), std::runtime_error);
}

}  // namespace
}  // namespace resmon
