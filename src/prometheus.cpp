#include "resmon/prometheus.hpp"

#include <sstream>

namespace resmon {

namespace {

std::string trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

}  // namespace

PrometheusMetrics parsePrometheusText(const std::string& text) {
  PrometheusMetrics metrics;
  std::istringstream stream(text);
  std::string line;

  while (std::getline(stream, line)) {
    std::string trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') continue;

    size_t last_space = trimmed.find_last_of(" \t");
    if (last_space == std::string::npos) continue;

    std::string name_part = trim(trimmed.substr(0, last_space));
    std::string value_part = trim(trimmed.substr(last_space + 1));
    if (name_part.empty() || value_part.empty()) continue;

    size_t brace = name_part.find('{');
    std::string name = brace == std::string::npos ? name_part : name_part.substr(0, brace);
    if (name.empty()) continue;

    try {
      size_t pos = 0;
      double value = std::stod(value_part, &pos);
      if (pos != value_part.size()) continue;  // trailing garbage
      metrics[name] = value;
    } catch (const std::exception&) {
      continue;  // malformed value; skip this line
    }
  }

  return metrics;
}

double promGet(const PrometheusMetrics& metrics, const std::string& key, double default_value) {
  auto it = metrics.find(key);
  return it == metrics.end() ? default_value : it->second;
}

}  // namespace resmon
