#pragma once

#include <map>
#include <string>

namespace resmon {

using PrometheusMetrics = std::map<std::string, double>;

// Parses Prometheus text-exposition format into name->value. HELP/TYPE
// comment lines and blank lines are skipped; malformed data lines are
// skipped rather than throwing, since one bad metric shouldn't fail the
// whole scrape. Label sets (metric{k="v"} value) are stripped, keeping the
// bare metric name as the key.
PrometheusMetrics parsePrometheusText(const std::string& text);

// Looks up `key`, returning `default_value` if absent.
double promGet(const PrometheusMetrics& metrics, const std::string& key, double default_value = 0.0);

}  // namespace resmon
