#include "resmon/collectors/mem_collector.hpp"

#include <map>
#include <sstream>

#include "resmon/constants.hpp"
#include "resmon/fsutil.hpp"

namespace resmon {

namespace c = constants;

namespace {

std::map<std::string, uint64_t> parseMeminfoKb(const std::string& text) {
  std::map<std::string, uint64_t> values;
  std::istringstream stream(text);
  std::string line;
  while (std::getline(stream, line)) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string key = line.substr(0, colon);
    std::istringstream rest(line.substr(colon + 1));
    uint64_t kb = 0;
    if (rest >> kb) values[key] = kb;
  }
  return values;
}

}  // namespace

MemReading readMemInfo(const std::string& proc_root) {
  MemReading reading;
  auto contents = fsutil::readFile(fsutil::join(proc_root, {c::kMeminfoPath}));
  if (!contents) return reading;

  auto values = parseMeminfoKb(*contents);
  auto get = [&](const char* key) -> std::optional<uint64_t> {
    auto it = values.find(key);
    if (it == values.end()) return std::nullopt;
    return it->second * 1024ULL;
  };

  reading.total_bytes = get("MemTotal");
  reading.available_bytes = get("MemAvailable");
  if (reading.total_bytes && reading.available_bytes) {
    reading.used_bytes = *reading.total_bytes - std::min(*reading.total_bytes, *reading.available_bytes);
  }
  reading.swap_total_bytes = get("SwapTotal");
  auto swap_free = get("SwapFree");
  if (reading.swap_total_bytes && swap_free) {
    reading.swap_used_bytes = *reading.swap_total_bytes - std::min(*reading.swap_total_bytes, *swap_free);
  }

  return reading;
}

MemCollector::MemCollector(std::string proc_root) : proc_root_(std::move(proc_root)) {}

std::string MemCollector::name() const { return "mem"; }

nlohmann::json MemCollector::collect() {
  auto r = readMemInfo(proc_root_);
  nlohmann::json j;
  j["total_bytes"] = r.total_bytes ? nlohmann::json(*r.total_bytes) : nlohmann::json(nullptr);
  j["used_bytes"] = r.used_bytes ? nlohmann::json(*r.used_bytes) : nlohmann::json(nullptr);
  j["available_bytes"] = r.available_bytes ? nlohmann::json(*r.available_bytes) : nlohmann::json(nullptr);
  j["swap_total_bytes"] = r.swap_total_bytes ? nlohmann::json(*r.swap_total_bytes) : nlohmann::json(nullptr);
  j["swap_used_bytes"] = r.swap_used_bytes ? nlohmann::json(*r.swap_used_bytes) : nlohmann::json(nullptr);
  return j;
}

}  // namespace resmon
