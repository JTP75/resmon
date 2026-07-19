#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "resmon/collectors/cpu_collector.hpp"
#include "resmon/collectors/gpu_collector.hpp"
#include "resmon/collectors/llama_collector.hpp"
#include "resmon/collectors/mem_collector.hpp"
#include "resmon/command_runner.hpp"
#include "resmon/config.hpp"
#include "resmon/constants.hpp"
#include "resmon/http_client.hpp"
#include "resmon/mqtt_publisher.hpp"
#include "resmon/sampler.hpp"

namespace {

std::atomic<bool> g_running{true};

void handleShutdownSignal(int) { g_running.store(false); }

// Sleeps in short increments so SIGTERM/SIGINT is honored promptly instead
// of waiting out the full sampling interval.
void interruptibleSleep(int total_seconds) {
  const auto tick = std::chrono::milliseconds(100);
  auto remaining = std::chrono::milliseconds(std::chrono::seconds(total_seconds));
  while (remaining > std::chrono::milliseconds(0) && g_running.load()) {
    auto step = std::min(tick, remaining);
    std::this_thread::sleep_for(step);
    remaining -= step;
  }
}

}  // namespace

int main(int argc, char** argv) {
  namespace c = resmon::constants;
  resmon::Config cfg;
  try {
    cfg = resmon::parseConfig(argc, argv);
  } catch (const resmon::HelpRequested&) {
    std::cout << resmon::usageText();
    return 0;
  } catch (const resmon::ConfigError& e) {
    std::cerr << "error: " << e.what() << "\n\n" << resmon::usageText();
    return 1;
  }

  auto http = std::make_shared<resmon::CurlHttpClient>(c::kDefaultHttpTimeoutMs);
  auto command_runner = std::make_shared<resmon::SubprocessCommandRunner>();
  auto gpu_client = std::make_shared<resmon::NvidiaSmiClient>(command_runner);

  std::vector<std::unique_ptr<resmon::ICollector>> collectors;
  collectors.push_back(std::make_unique<resmon::CpuCollector>(cfg.sysfs_root, cfg.proc_root));
  collectors.push_back(std::make_unique<resmon::MemCollector>(cfg.proc_root));
  collectors.push_back(std::make_unique<resmon::GpuCollector>(command_runner));
  collectors.push_back(std::make_unique<resmon::LlamaCollector>(http, cfg.llama_url, cfg.proc_root,
                                                                  gpu_client));

  resmon::Sampler sampler(std::move(collectors), cfg.hostname, c::kSchemaVersion);

  resmon::MqttConnectConfig mqtt_cfg;
  mqtt_cfg.host = cfg.mqtt_host;
  mqtt_cfg.port = cfg.resolvedMqttPort();
  mqtt_cfg.client_id = cfg.client_id;
  mqtt_cfg.username = cfg.username;
  mqtt_cfg.password = cfg.password;
  mqtt_cfg.tls_enabled = cfg.tlsEnabled();
  mqtt_cfg.ca_cert_path = cfg.ca_cert;
  mqtt_cfg.tls_insecure = cfg.tls_insecure;
  mqtt_cfg.will_topic = cfg.statusTopic();
  mqtt_cfg.will_payload = std::string(c::kStatusOffline);
  mqtt_cfg.keepalive_seconds = c::kMqttKeepaliveSeconds;
  mqtt_cfg.connect_timeout_seconds = c::kMqttConnectTimeoutSeconds;

  std::unique_ptr<resmon::MosquittoPublisher> publisher_ptr;
  try {
    publisher_ptr = std::make_unique<resmon::MosquittoPublisher>(mqtt_cfg);
    publisher_ptr->connect();
  } catch (const std::exception& e) {
    std::cerr << "error: failed to connect to MQTT broker: " << e.what() << "\n";
    return 1;
  }
  resmon::MosquittoPublisher& publisher = *publisher_ptr;

  publisher.publish(cfg.statusTopic(), std::string(c::kStatusOnline), true);
  std::cout << "resmon connected: " << mqtt_cfg.host << ":" << mqtt_cfg.port
            << " state=" << cfg.stateTopic() << " status=" << cfg.statusTopic()
            << " interval=" << cfg.interval_seconds << "s\n";

  std::signal(SIGINT, handleShutdownSignal);
  std::signal(SIGTERM, handleShutdownSignal);

  while (g_running.load()) {
    nlohmann::json sample = sampler.sampleOnce();
    publisher.publish(cfg.stateTopic(), sample.dump(), true);
    interruptibleSleep(cfg.interval_seconds);
  }

  publisher.publishAndFlush(cfg.statusTopic(), std::string(c::kStatusOffline), true);
  return 0;
}
