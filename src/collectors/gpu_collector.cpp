#include "resmon/collectors/gpu_collector.hpp"

#include <sstream>
#include <vector>

#include "resmon/constants.hpp"

namespace resmon {

namespace c = constants;

namespace {

std::string trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

bool isNotAvailable(const std::string& token) {
  for (auto marker : c::kNvidiaSmiNotAvailableMarkers) {
    if (token == marker) return true;
  }
  return false;
}

std::optional<double> parseOptDouble(const std::string& raw) {
  std::string token = trim(raw);
  if (token.empty() || isNotAvailable(token)) return std::nullopt;
  try {
    size_t pos = 0;
    double v = std::stod(token, &pos);
    return v;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<uint64_t> parseOptMibToBytes(const std::string& raw) {
  auto mib = parseOptDouble(raw);
  if (!mib) return std::nullopt;
  return static_cast<uint64_t>(*mib * 1024.0 * 1024.0);
}

std::vector<std::string> splitCsvLine(const std::string& line) {
  std::vector<std::string> fields;
  std::istringstream ls(line);
  std::string field;
  while (std::getline(ls, field, ',')) fields.push_back(trim(field));
  return fields;
}

}  // namespace

std::vector<GpuSample> parseNvidiaSmiGpuCsv(const std::string& csv_text) {
  std::vector<GpuSample> result;
  std::istringstream stream(csv_text);
  std::string line;

  while (std::getline(stream, line)) {
    if (trim(line).empty()) continue;
    auto f = splitCsvLine(line);
    if (f.size() != 11) continue;  // must match kNvidiaSmiGpuQueryFields field count

    GpuSample sample;
    try {
      sample.index = std::stoi(f[0]);
    } catch (const std::exception&) {
      continue;
    }
    sample.name = f[1];
    sample.temp_c = parseOptDouble(f[2]);
    sample.power_w = parseOptDouble(f[3]);
    sample.power_limit_w = parseOptDouble(f[4]);
    sample.util_gpu_pct = parseOptDouble(f[5]);
    sample.util_mem_pct = parseOptDouble(f[6]);
    sample.vram_used_bytes = parseOptMibToBytes(f[7]);
    sample.vram_total_bytes = parseOptMibToBytes(f[8]);
    sample.sm_clock_mhz = parseOptDouble(f[9]);
    sample.fan_pct = parseOptDouble(f[10]);

    result.push_back(std::move(sample));
  }

  return result;
}

std::map<long, uint64_t> parseNvidiaSmiComputeAppsCsv(const std::string& csv_text) {
  std::map<long, uint64_t> result;
  std::istringstream stream(csv_text);
  std::string line;

  while (std::getline(stream, line)) {
    if (trim(line).empty()) continue;
    auto f = splitCsvLine(line);
    if (f.size() != 2) continue;

    try {
      long pid = std::stol(f[0]);
      auto bytes = parseOptMibToBytes(f[1]);
      if (bytes) result[pid] = *bytes;
    } catch (const std::exception&) {
      continue;
    }
  }

  return result;
}

NvidiaSmiClient::NvidiaSmiClient(std::shared_ptr<ICommandRunner> runner) : runner_(std::move(runner)) {}

std::vector<GpuSample> NvidiaSmiClient::queryGpus() const {
  std::vector<std::string> argv = {
      std::string(c::kNvidiaSmiBinary),
      "--query-gpu=" + std::string(c::kNvidiaSmiGpuQueryFields),
      "--format=" + std::string(c::kNvidiaSmiCsvFormat),
  };
  auto result = runner_->run(argv);
  if (!result.ok) return {};
  return parseNvidiaSmiGpuCsv(result.stdout_text);
}

std::optional<uint64_t> NvidiaSmiClient::vramUsedBytesForPid(long pid) const {
  std::vector<std::string> argv = {
      std::string(c::kNvidiaSmiBinary),
      "--query-compute-apps=" + std::string(c::kNvidiaSmiComputeAppsQueryFields),
      "--format=" + std::string(c::kNvidiaSmiCsvFormat),
  };
  auto result = runner_->run(argv);
  if (!result.ok) return std::nullopt;
  auto apps = parseNvidiaSmiComputeAppsCsv(result.stdout_text);
  auto it = apps.find(pid);
  if (it == apps.end()) return std::nullopt;
  return it->second;
}

GpuCollector::GpuCollector(std::shared_ptr<ICommandRunner> runner) : client_(std::move(runner)) {}

std::string GpuCollector::name() const { return "gpu"; }

nlohmann::json GpuCollector::collect() {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& g : client_.queryGpus()) {
    nlohmann::json j;
    j["index"] = g.index;
    j["name"] = g.name;
    j["temp_c"] = g.temp_c ? nlohmann::json(*g.temp_c) : nlohmann::json(nullptr);
    j["power_w"] = g.power_w ? nlohmann::json(*g.power_w) : nlohmann::json(nullptr);
    j["power_limit_w"] = g.power_limit_w ? nlohmann::json(*g.power_limit_w) : nlohmann::json(nullptr);
    j["util_gpu_pct"] = g.util_gpu_pct ? nlohmann::json(*g.util_gpu_pct) : nlohmann::json(nullptr);
    j["util_mem_pct"] = g.util_mem_pct ? nlohmann::json(*g.util_mem_pct) : nlohmann::json(nullptr);
    j["vram_used_bytes"] = g.vram_used_bytes ? nlohmann::json(*g.vram_used_bytes) : nlohmann::json(nullptr);
    j["vram_total_bytes"] =
        g.vram_total_bytes ? nlohmann::json(*g.vram_total_bytes) : nlohmann::json(nullptr);
    j["sm_clock_mhz"] = g.sm_clock_mhz ? nlohmann::json(*g.sm_clock_mhz) : nlohmann::json(nullptr);
    j["fan_pct"] = g.fan_pct ? nlohmann::json(*g.fan_pct) : nlohmann::json(nullptr);
    arr.push_back(std::move(j));
  }
  return arr;
}

}  // namespace resmon
