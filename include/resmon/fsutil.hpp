#pragma once

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>

namespace resmon::fsutil {

// Joins root with path components regardless of whether root has a
// trailing slash -- used so sysfs/proc paths work identically against the
// real "/" root and a test fixture directory root.
std::filesystem::path join(const std::string& root, std::initializer_list<std::string_view> parts);

// Reads a whole file into a string. Returns nullopt if it can't be opened
// (missing, permission denied) -- callers treat that as "field unavailable",
// not a hard error.
std::optional<std::string> readFile(const std::filesystem::path& path);

// Reads a file and parses it as an integer type. Returns nullopt if the
// file is missing or its contents aren't a valid integer.
std::optional<long long> readInt(const std::filesystem::path& path);

}  // namespace resmon::fsutil
