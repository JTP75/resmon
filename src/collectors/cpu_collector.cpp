#include "resmon/collectors/cpu_collector.hpp"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <vector>

#include "resmon/constants.hpp"
#include "resmon/fsutil.hpp"

namespace resmon {

namespace fs = std::filesystem;
namespace c = constants;

// --- RaplReader ---

RaplReader::RaplReader(std::string sysfs_root) : sysfs_root_(std::move(sysfs_root)) {}

std::optional<double> RaplReader::samplePowerWatts() {
  fs::path pkg_dir = fsutil::join(sysfs_root_, {c::kRaplPackageDir});
  auto energy = fsutil::readInt(pkg_dir / std::string(c::kRaplEnergyFile));
  if (!energy) return std::nullopt;

  if (!max_range_loaded_) {
    auto max_range = fsutil::readInt(pkg_dir / std::string(c::kRaplMaxEnergyFile));
    max_energy_range_uj_ = max_range.value_or(0);
    max_range_loaded_ = true;
  }

  auto now = std::chrono::steady_clock::now();
  if (!prev_energy_uj_) {
    prev_energy_uj_ = *energy;
    prev_time_ = now;
    return std::nullopt;
  }

  long long delta_energy = *energy - *prev_energy_uj_;
  if (delta_energy < 0 && max_energy_range_uj_ > 0) {
    delta_energy += max_energy_range_uj_;
  }

  double delta_seconds = std::chrono::duration<double>(now - prev_time_).count();

  prev_energy_uj_ = *energy;
  prev_time_ = now;

  if (delta_energy < 0 || delta_seconds <= 0.0) return std::nullopt;

  return (static_cast<double>(delta_energy) / 1e6) / delta_seconds;
}

// --- CpuLoadReader ---

CpuLoadReader::CpuLoadReader(std::string proc_root) : proc_root_(std::move(proc_root)) {}

namespace {

bool parseStatLine(const std::string& line, CpuLoadReader::Jiffies* out) {
  std::istringstream ls(line);
  std::string label;
  ls >> label;
  if (label.rfind("cpu", 0) != 0) return false;

  unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0,
                      steal = 0;
  if (!(ls >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
    // Some kernels report fewer fields; missing ones default to 0 above,
    // but require at least the first four (user/nice/system/idle).
    if (ls.fail() && !ls.eof()) return false;
  }

  out->idle = idle + iowait;
  out->total = user + nice + system + idle + iowait + irq + softirq + steal;
  return true;
}

}  // namespace

std::optional<CpuLoadReader::Sample> CpuLoadReader::sample() {
  auto contents = fsutil::readFile(fsutil::join(proc_root_, {c::kCpuStatPath}));
  if (!contents) return std::nullopt;

  Jiffies overall;
  std::vector<Jiffies> per_core;
  std::istringstream stream(*contents);
  std::string line;
  bool have_overall = false;

  while (std::getline(stream, line)) {
    if (line.rfind("cpu ", 0) == 0) {
      if (parseStatLine(line, &overall)) have_overall = true;
    } else if (line.rfind("cpu", 0) == 0 && line.size() > 3 && std::isdigit((unsigned char)line[3])) {
      Jiffies core;
      if (parseStatLine(line, &core)) per_core.push_back(core);
    } else if (line.rfind("cpu", 0) == 0) {
      continue;
    } else {
      break;  // /proc/stat lists cpu lines first; stop once we're past them
    }
  }

  if (!have_overall) return std::nullopt;

  auto pctFromDelta = [](const Jiffies& prev, const Jiffies& cur) -> double {
    if (cur.total <= prev.total) return 0.0;
    unsigned long long delta_total = cur.total - prev.total;
    unsigned long long delta_idle = cur.idle >= prev.idle ? cur.idle - prev.idle : 0;
    if (delta_idle > delta_total) delta_idle = delta_total;
    return 100.0 * (1.0 - static_cast<double>(delta_idle) / static_cast<double>(delta_total));
  };

  if (!prev_overall_) {
    prev_overall_ = overall;
    prev_per_core_ = per_core;
    return std::nullopt;
  }

  Sample result;
  result.overall_pct = pctFromDelta(*prev_overall_, overall);
  for (size_t i = 0; i < per_core.size() && i < prev_per_core_.size(); ++i) {
    result.per_core_pct.push_back(pctFromDelta(prev_per_core_[i], per_core[i]));
  }

  prev_overall_ = overall;
  prev_per_core_ = per_core;
  return result;
}

// --- CPU temperature (coretemp hwmon) ---

namespace {

std::string trimStr(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::optional<fs::path> findCoretempDir(const std::string& sysfs_root) {
  fs::path hwmon_dir = fsutil::join(sysfs_root, {c::kHwmonClassDir});
  std::error_code ec;
  if (!fs::exists(hwmon_dir, ec) || ec) return std::nullopt;

  for (const auto& entry : fs::directory_iterator(hwmon_dir, ec)) {
    if (ec) break;
    auto name = fsutil::readFile(entry.path() / "name");
    if (name && trimStr(*name) == c::kCoretempChipName) return entry.path();
  }
  return std::nullopt;
}

}  // namespace

CpuTempReading readCpuTemps(const std::string& sysfs_root) {
  CpuTempReading result;
  auto chip_dir = findCoretempDir(sysfs_root);
  if (!chip_dir) return result;

  std::vector<std::pair<int, double>> indexed_cores;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(*chip_dir, ec)) {
    if (ec) break;
    std::string fname = entry.path().filename().string();
    if (fname.rfind("temp", 0) != 0 || fname.find("_label") == std::string::npos) continue;

    auto label_text = fsutil::readFile(entry.path());
    if (!label_text) continue;
    std::string label = trimStr(*label_text);

    std::string input_fname = fname.substr(0, fname.find("_label")) + "_input";
    auto input_text = fsutil::readFile(entry.path().parent_path() / input_fname);
    if (!input_text) continue;

    double milli_c;
    try {
      milli_c = std::stod(trimStr(*input_text));
    } catch (const std::exception&) {
      continue;
    }
    double celsius = milli_c / 1000.0;

    if (label.rfind("Package", 0) == 0) {
      result.package_c = celsius;
    } else if (label.rfind("Core", 0) == 0) {
      // Extract the numeric suffix from "tempN_label" to keep cores ordered.
      std::string digits;
      for (char ch : fname) {
        if (std::isdigit((unsigned char)ch)) digits += ch;
        else if (!digits.empty()) break;
      }
      int idx = digits.empty() ? 0 : std::stoi(digits);
      indexed_cores.emplace_back(idx, celsius);
    }
  }

  std::sort(indexed_cores.begin(), indexed_cores.end());
  for (auto& [idx, temp] : indexed_cores) result.core_c.push_back(temp);

  return result;
}

std::optional<double> readCpuFreqMhz(const std::string& proc_root) {
  auto contents = fsutil::readFile(fsutil::join(proc_root, {c::kCpuInfoPath}));
  if (!contents) return std::nullopt;

  std::istringstream stream(*contents);
  std::string line;
  double sum = 0.0;
  int count = 0;
  while (std::getline(stream, line)) {
    if (line.rfind("cpu MHz", 0) != 0) continue;
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    try {
      sum += std::stod(trimStr(line.substr(colon + 1)));
      ++count;
    } catch (const std::exception&) {
      continue;
    }
  }
  if (count == 0) return std::nullopt;
  return sum / count;
}

// --- CpuCollector ---

CpuCollector::CpuCollector(std::string sysfs_root, std::string proc_root)
    : sysfs_root_(sysfs_root), proc_root_(proc_root), rapl_(sysfs_root), load_(proc_root) {}

std::string CpuCollector::name() const { return "cpu"; }

nlohmann::json CpuCollector::collect() {
  nlohmann::json j;

  auto load_sample = load_.sample();
  if (load_sample) {
    j["load_pct"] = load_sample->overall_pct;
    j["per_core_pct"] = load_sample->per_core_pct;
  } else {
    j["load_pct"] = nullptr;
    j["per_core_pct"] = nlohmann::json::array();
  }

  auto freq = readCpuFreqMhz(proc_root_);
  j["freq_mhz"] = freq ? nlohmann::json(*freq) : nlohmann::json(nullptr);

  auto temps = readCpuTemps(sysfs_root_);
  nlohmann::json temp_json;
  temp_json["package"] = temps.package_c ? nlohmann::json(*temps.package_c) : nlohmann::json(nullptr);
  temp_json["cores"] = temps.core_c;
  j["temp_c"] = temp_json;

  auto power = rapl_.samplePowerWatts();
  j["power_w"] = power ? nlohmann::json(*power) : nlohmann::json(nullptr);

  return j;
}

}  // namespace resmon
