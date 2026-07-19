#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "resmon/collectors/collector.hpp"

namespace resmon {

struct MemReading {
  std::optional<uint64_t> total_bytes;
  std::optional<uint64_t> used_bytes;
  std::optional<uint64_t> available_bytes;
  std::optional<uint64_t> swap_total_bytes;
  std::optional<uint64_t> swap_used_bytes;
};

// Parses /proc/meminfo. used_bytes = total - available (MemAvailable
// already accounts for reclaimable cache/buffers, unlike total - free).
MemReading readMemInfo(const std::string& proc_root);

class MemCollector : public ICollector {
 public:
  explicit MemCollector(std::string proc_root);
  std::string name() const override;
  nlohmann::json collect() override;

 private:
  std::string proc_root_;
};

}  // namespace resmon
