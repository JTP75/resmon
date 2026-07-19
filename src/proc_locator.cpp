#include "resmon/proc_locator.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

#include "resmon/fsutil.hpp"

namespace resmon {

namespace fs = std::filesystem;

namespace {

bool isAllDigits(const std::string& s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
}

std::optional<uint64_t> readRssBytes(const fs::path& status_path) {
  auto contents = fsutil::readFile(status_path);
  if (!contents) return std::nullopt;
  std::istringstream stream(*contents);
  std::string line;
  while (std::getline(stream, line)) {
    if (line.rfind("VmRSS:", 0) == 0) {
      std::istringstream ls(line.substr(6));
      uint64_t kb = 0;
      if (ls >> kb) return kb * 1024;
      return std::nullopt;
    }
  }
  return std::nullopt;
}

}  // namespace

std::optional<LlamaProcess> findLlamaProcess(const std::string& proc_root,
                                              const std::string& binary_name_hint) {
  fs::path proc_dir = fsutil::join(proc_root, {"proc"});

  std::error_code ec;
  if (!fs::exists(proc_dir, ec) || ec) return std::nullopt;

  for (const auto& entry : fs::directory_iterator(proc_dir, ec)) {
    if (ec) break;
    if (!entry.is_directory()) continue;

    std::string dirname = entry.path().filename().string();
    if (!isAllDigits(dirname)) continue;

    auto cmdline = fsutil::readFile(entry.path() / "cmdline");
    if (!cmdline || cmdline->empty()) continue;

    // cmdline is NUL-separated argv; substring match is enough to find our target.
    if (cmdline->find(binary_name_hint) == std::string::npos) continue;

    LlamaProcess proc;
    proc.pid = std::stol(dirname);
    proc.rss_bytes = readRssBytes(entry.path() / "status").value_or(0);
    return proc;
  }

  return std::nullopt;
}

}  // namespace resmon
