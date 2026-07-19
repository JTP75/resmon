#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "resmon/collectors/collector.hpp"

namespace resmon {

// Reads Intel RAPL package energy and derives average power (W) from the
// delta between consecutive samples. The first sample has no prior
// reading to diff against, so it returns nullopt -- this is expected, not
// an error. Handles the energy counter wrapping at max_energy_range_uj.
class RaplReader {
 public:
  explicit RaplReader(std::string sysfs_root);
  std::optional<double> samplePowerWatts();

 private:
  std::string sysfs_root_;
  std::optional<long long> prev_energy_uj_;
  std::chrono::steady_clock::time_point prev_time_;
  long long max_energy_range_uj_ = 0;
  bool max_range_loaded_ = false;
};

// Reads /proc/stat cumulative jiffy counters and derives instantaneous
// utilization from the delta between consecutive samples. Like RaplReader,
// the first sample returns nullopt.
class CpuLoadReader {
 public:
  struct Sample {
    double overall_pct = 0.0;
    std::vector<double> per_core_pct;
  };

  struct Jiffies {
    unsigned long long total = 0;
    unsigned long long idle = 0;
  };

  explicit CpuLoadReader(std::string proc_root);
  std::optional<Sample> sample();

 private:
  std::string proc_root_;
  std::optional<Jiffies> prev_overall_;
  std::vector<Jiffies> prev_per_core_;
};

struct CpuTempReading {
  std::optional<double> package_c;
  std::vector<double> core_c;
};

// Finds the coretemp hwmon chip by name (hwmon numbering is not stable
// across boots) and reads its package + per-core temperatures.
CpuTempReading readCpuTemps(const std::string& sysfs_root);

// Average of "cpu MHz" fields in /proc/cpuinfo, or nullopt if unreadable.
std::optional<double> readCpuFreqMhz(const std::string& proc_root);

class CpuCollector : public ICollector {
 public:
  CpuCollector(std::string sysfs_root, std::string proc_root);
  std::string name() const override;
  nlohmann::json collect() override;

 private:
  std::string sysfs_root_;
  std::string proc_root_;
  RaplReader rapl_;
  CpuLoadReader load_;
};

}  // namespace resmon
