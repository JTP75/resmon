#include "resmon/mqtt_publisher.hpp"

#include <mosquitto.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace resmon {

namespace {

void ensureMosquittoLibInit() {
  static std::once_flag flag;
  std::call_once(flag, [] { mosquitto_lib_init(); });
}

struct ConnectWaiter {
  std::mutex mutex;
  std::condition_variable cv;
  bool connected = false;
  int result_code = -1;
};

void onConnect(struct mosquitto*, void* userdata, int rc) {
  // userdata is cleared to nullptr once the initial connect() wait
  // completes; later reconnects call this callback too, so guard against it.
  if (userdata == nullptr) return;
  auto* waiter = static_cast<ConnectWaiter*>(userdata);
  {
    std::lock_guard<std::mutex> lock(waiter->mutex);
    waiter->connected = (rc == 0);
    waiter->result_code = rc;
  }
  waiter->cv.notify_all();
}

}  // namespace

MosquittoPublisher::MosquittoPublisher(MqttConnectConfig cfg) : cfg_(std::move(cfg)) {
  ensureMosquittoLibInit();

  mosq_ = mosquitto_new(cfg_.client_id.c_str(), true /* clean_session */, nullptr);
  if (mosq_ == nullptr) {
    throw std::runtime_error("mosquitto_new failed");
  }

  if (!cfg_.username.empty()) {
    const char* pw = cfg_.password.empty() ? nullptr : cfg_.password.c_str();
    mosquitto_username_pw_set(mosq_, cfg_.username.c_str(), pw);
  }

  if (!cfg_.will_topic.empty()) {
    mosquitto_will_set(mosq_, cfg_.will_topic.c_str(), static_cast<int>(cfg_.will_payload.size()),
                        cfg_.will_payload.data(), 1 /* qos */, true /* retain */);
  }

  if (cfg_.tls_enabled) {
    int rc = mosquitto_tls_set(mosq_, cfg_.ca_cert_path.c_str(), nullptr, nullptr, nullptr, nullptr);
    if (rc != MOSQ_ERR_SUCCESS) {
      mosquitto_destroy(mosq_);
      mosq_ = nullptr;
      throw std::runtime_error(std::string("mosquitto_tls_set failed: ") + mosquitto_strerror(rc));
    }
  }

  mosquitto_reconnect_delay_set(mosq_, 2, 30, true /* exponential backoff */);
}

MosquittoPublisher::~MosquittoPublisher() {
  if (mosq_ != nullptr) {
    mosquitto_disconnect(mosq_);
    mosquitto_loop_stop(mosq_, false);
    mosquitto_destroy(mosq_);
  }
}

void MosquittoPublisher::connect() {
  ConnectWaiter waiter;
  mosquitto_user_data_set(mosq_, &waiter);
  mosquitto_connect_callback_set(mosq_, onConnect);

  int rc = mosquitto_connect_async(mosq_, cfg_.host.c_str(), cfg_.port, cfg_.keepalive_seconds);
  if (rc != MOSQ_ERR_SUCCESS) {
    throw std::runtime_error(std::string("mosquitto_connect_async failed: ") + mosquitto_strerror(rc));
  }

  rc = mosquitto_loop_start(mosq_);
  if (rc != MOSQ_ERR_SUCCESS) {
    throw std::runtime_error(std::string("mosquitto_loop_start failed: ") + mosquitto_strerror(rc));
  }

  std::unique_lock<std::mutex> lock(waiter.mutex);
  bool ok = waiter.cv.wait_for(lock, std::chrono::seconds(cfg_.connect_timeout_seconds),
                                [&] { return waiter.connected; });
  if (!ok) {
    throw std::runtime_error("timed out connecting to MQTT broker at " + cfg_.host + ":" +
                              std::to_string(cfg_.port));
  }

  // Callback no longer needs the stack-local waiter; leave the connect
  // callback in place (harmless no-op target) but clear userdata since
  // `waiter` is about to go out of scope.
  mosquitto_user_data_set(mosq_, nullptr);
}

void MosquittoPublisher::publish(const std::string& topic, const std::string& payload, bool retain) {
  mosquitto_publish(mosq_, nullptr, topic.c_str(), static_cast<int>(payload.size()), payload.data(),
                     1 /* qos */, retain);
}

void MosquittoPublisher::publishAndFlush(const std::string& topic, const std::string& payload,
                                          bool retain) {
  publish(topic, payload, retain);
  // Give the background loop thread a moment to flush the socket before
  // the process exits (e.g. on SIGTERM).
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

}  // namespace resmon
