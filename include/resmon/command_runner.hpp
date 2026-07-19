#pragma once

#include <string>
#include <vector>

namespace resmon {

struct CommandResult {
  bool ok = false;
  std::string stdout_text;
  std::string error;
};

// Seam over subprocess execution so GPU collection is testable without a
// real nvidia-smi/driver present.
class ICommandRunner {
 public:
  virtual ~ICommandRunner() = default;
  virtual CommandResult run(const std::vector<std::string>& argv) const = 0;
};

// Runs argv directly via fork/exec (no shell), avoiding any shell-injection
// risk from interpolated arguments.
class SubprocessCommandRunner : public ICommandRunner {
 public:
  CommandResult run(const std::vector<std::string>& argv) const override;
};

}  // namespace resmon
