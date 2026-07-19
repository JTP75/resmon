#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace resmon {

// Common interface for all metric collectors. collect() is non-const:
// several collectors (cpu power/load) hold delta state between calls.
// Implementations must never throw for expected failure modes (missing
// sysfs file, unreachable server) -- they report those as null/absent
// fields so one failing collector never takes down the whole sample.
class ICollector {
 public:
  virtual ~ICollector() = default;
  virtual std::string name() const = 0;
  virtual nlohmann::json collect() = 0;
};

}  // namespace resmon
