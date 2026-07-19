#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "resmon/collectors/collector.hpp"
#include "resmon/command_runner.hpp"

namespace resmon {

struct GpuSample {
  int index = -1;
  std::string name;
  std::optional<double> temp_c;
  std::optional<double> power_w;
  std::optional<double> power_limit_w;
  std::optional<double> util_gpu_pct;
  std::optional<double> util_mem_pct;
  std::optional<uint64_t> vram_used_bytes;
  std::optional<uint64_t> vram_total_bytes;
  std::optional<double> sm_clock_mhz;
  std::optional<double> fan_pct;
};

// Parses `nvidia-smi --query-gpu=<constants::kNvidiaSmiGpuQueryFields>
// --format=<constants::kNvidiaSmiCsvFormat>` output. Fields nvidia-smi
// reports as unsupported ("[N/A]" etc) become nullopt rather than a parse
// failure -- a card lacking a fan sensor shouldn't drop the whole sample.
std::vector<GpuSample> parseNvidiaSmiGpuCsv(const std::string& csv_text);

// Parses `nvidia-smi --query-compute-apps=pid,used_memory
// --format=<constants::kNvidiaSmiCsvFormat>` into pid -> VRAM bytes.
std::map<long, uint64_t> parseNvidiaSmiComputeAppsCsv(const std::string& csv_text);

// Wraps nvidia-smi invocations behind ICommandRunner so it's usable by both
// GpuCollector (aggregate per-GPU stats) and LlamaCollector (per-process
// VRAM attribution) without either depending on the other.
class NvidiaSmiClient {
 public:
  explicit NvidiaSmiClient(std::shared_ptr<ICommandRunner> runner);
  std::vector<GpuSample> queryGpus() const;
  std::optional<uint64_t> vramUsedBytesForPid(long pid) const;

 private:
  std::shared_ptr<ICommandRunner> runner_;
};

class GpuCollector : public ICollector {
 public:
  explicit GpuCollector(std::shared_ptr<ICommandRunner> runner);
  std::string name() const override;
  nlohmann::json collect() override;

 private:
  NvidiaSmiClient client_;
};

}  // namespace resmon
