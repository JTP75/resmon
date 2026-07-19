#pragma once

#include <functional>

#include "resmon/command_runner.hpp"

namespace resmon::testsupport {

// Delegates to a caller-provided handler so tests can branch on argv (e.g.
// distinguish nvidia-smi's --query-gpu vs --query-compute-apps calls)
// without needing a real nvidia-smi/driver present.
class FakeCommandRunner : public ICommandRunner {
 public:
  using Handler = std::function<CommandResult(const std::vector<std::string>&)>;

  explicit FakeCommandRunner(Handler handler) : handler_(std::move(handler)) {}

  CommandResult run(const std::vector<std::string>& argv) const override { return handler_(argv); }

 private:
  Handler handler_;
};

}  // namespace resmon::testsupport
