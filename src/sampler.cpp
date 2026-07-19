#include "resmon/sampler.hpp"

#include <chrono>

namespace resmon {

Sampler::Sampler(std::vector<std::unique_ptr<ICollector>> collectors, std::string hostname,
                  int schema_version)
    : collectors_(std::move(collectors)),
      hostname_(std::move(hostname)),
      schema_version_(schema_version),
      start_time_(std::chrono::steady_clock::now()) {}

nlohmann::json Sampler::sampleOnce() {
  nlohmann::json j;
  j["schema"] = schema_version_;
  j["host"] = hostname_;
  j["ts"] = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
  j["uptime_s"] = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start_time_)
                       .count();

  for (auto& collector : collectors_) {
    try {
      j[collector->name()] = collector->collect();
    } catch (const std::exception&) {
      j[collector->name()] = nullptr;
    }
  }

  return j;
}

}  // namespace resmon
