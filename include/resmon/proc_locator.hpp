#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace resmon {

struct LlamaProcess {
  long pid = -1;
  uint64_t rss_bytes = 0;
};

// Scans proc_root/proc/<pid>/cmdline for a process whose argv[0] contains
// `binary_name_hint`, returning its pid and current RSS. Returns nullopt if
// no matching process is found or it can't be read (process exited
// mid-scan, permission denied, etc).
std::optional<LlamaProcess> findLlamaProcess(const std::string& proc_root,
                                              const std::string& binary_name_hint);

}  // namespace resmon
