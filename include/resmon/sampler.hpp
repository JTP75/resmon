#pragma once

#include <chrono>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "resmon/collectors/collector.hpp"

namespace resmon {

// Orchestrates collectors into a single JSON document per sample. Each
// collector is isolated in a try/catch so one failing collector (e.g.
// llama-server down) never prevents the rest of the sample from publishing.
class Sampler {
 public:
  Sampler(std::vector<std::unique_ptr<ICollector>> collectors, std::string hostname,
          int schema_version);

  nlohmann::json sampleOnce();

 private:
  std::vector<std::unique_ptr<ICollector>> collectors_;
  std::string hostname_;
  int schema_version_;
  std::chrono::steady_clock::time_point start_time_;
};

}  // namespace resmon
