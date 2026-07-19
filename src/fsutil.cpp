#include "resmon/fsutil.hpp"

#include <fstream>
#include <sstream>

namespace resmon::fsutil {

std::filesystem::path join(const std::string& root, std::initializer_list<std::string_view> parts) {
  std::filesystem::path p(root);
  for (auto part : parts) p /= part;
  return p;
}

std::optional<std::string> readFile(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return std::nullopt;
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::optional<long long> readInt(const std::filesystem::path& path) {
  auto contents = readFile(path);
  if (!contents) return std::nullopt;
  try {
    size_t pos = 0;
    long long value = std::stoll(*contents, &pos);
    return value;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace resmon::fsutil
