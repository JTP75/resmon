#pragma once

#include <memory>
#include <string>

#include "resmon/collectors/collector.hpp"
#include "resmon/collectors/gpu_collector.hpp"
#include "resmon/http_client.hpp"

namespace resmon {

// Aggregates llama-server health/throughput/slots (via HTTP) with process
// RSS (via /proc) and GPU VRAM attribution (via NvidiaSmiClient). If
// llama-server is unreachable, `up` is false and the rest of the fields
// are null -- the other collectors' data still gets published.
class LlamaCollector : public ICollector {
 public:
  LlamaCollector(std::shared_ptr<IHttpClient> http, std::string base_url, std::string proc_root,
                 std::shared_ptr<NvidiaSmiClient> gpu_client);
  std::string name() const override;
  nlohmann::json collect() override;

 private:
  std::shared_ptr<IHttpClient> http_;
  std::string base_url_;
  std::string proc_root_;
  std::shared_ptr<NvidiaSmiClient> gpu_client_;
};

}  // namespace resmon
