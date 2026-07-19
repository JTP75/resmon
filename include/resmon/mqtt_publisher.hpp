#pragma once

#include <string>

struct mosquitto;

namespace resmon {

// Seam over MQTT publishing so the sampler is testable without a broker.
class IMqttSink {
 public:
  virtual ~IMqttSink() = default;
  virtual void publish(const std::string& topic, const std::string& payload, bool retain) = 0;
};

struct MqttConnectConfig {
  std::string host;
  int port = 1883;
  std::string client_id;
  std::string username;  // empty = no auth
  std::string password;
  bool tls_enabled = false;
  std::string ca_cert_path;  // required if tls_enabled
  // Skips verifying the broker cert's hostname/SAN against `host` (chain
  // validation against ca_cert_path still applies). Mirrors mosquitto_pub/
  // sub's --insecure; needed when connecting by IP or to a cert issued for
  // a different name.
  bool tls_insecure = false;
  std::string will_topic;
  std::string will_payload;
  int keepalive_seconds = 30;
  int connect_timeout_seconds = 10;
};

// Wraps libmosquitto: TLS (CA-only, one-sided), optional username/password,
// a Last Will & Testament for availability, and a background network
// thread (mosquitto_loop_start) that auto-reconnects on connection loss.
class MosquittoPublisher : public IMqttSink {
 public:
  explicit MosquittoPublisher(MqttConnectConfig cfg);
  ~MosquittoPublisher() override;

  MosquittoPublisher(const MosquittoPublisher&) = delete;
  MosquittoPublisher& operator=(const MosquittoPublisher&) = delete;

  // Connects (async) and starts the background loop, blocking up to
  // connect_timeout_seconds for the initial connection. Throws
  // std::runtime_error if it doesn't connect in time.
  void connect();

  void publish(const std::string& topic, const std::string& payload, bool retain) override;

  // Publishes `payload` to `topic` synchronously-ish and blocks briefly so
  // it has a chance to leave the socket before the process exits -- used
  // to send an explicit "offline" status on graceful shutdown.
  void publishAndFlush(const std::string& topic, const std::string& payload, bool retain);

 private:
  MqttConnectConfig cfg_;
  ::mosquitto* mosq_ = nullptr;
};

}  // namespace resmon
