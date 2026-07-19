#include "resmon/command_runner.hpp"

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>

extern char** environ;

namespace resmon {

CommandResult SubprocessCommandRunner::run(const std::vector<std::string>& argv) const {
  CommandResult result;
  if (argv.empty()) {
    result.error = "empty argv";
    return result;
  }

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    result.error = std::string("pipe() failed: ") + std::strerror(errno);
    return result;
  }

  std::vector<char*> c_argv;
  c_argv.reserve(argv.size() + 1);
  for (const auto& a : argv) c_argv.push_back(const_cast<char*>(a.c_str()));
  c_argv.push_back(nullptr);

  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
  posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
  posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);

  pid_t pid = 0;
  int rc = posix_spawnp(&pid, c_argv[0], &actions, nullptr, c_argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  close(pipe_fds[1]);

  if (rc != 0) {
    close(pipe_fds[0]);
    result.error = std::string("posix_spawnp failed: ") + std::strerror(rc);
    return result;
  }

  std::array<char, 4096> buf{};
  ssize_t n;
  while ((n = read(pipe_fds[0], buf.data(), buf.size())) > 0) {
    result.stdout_text.append(buf.data(), static_cast<size_t>(n));
  }
  close(pipe_fds[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
    result.ok = true;
  } else {
    result.error = "command exited with non-zero status";
  }
  return result;
}

}  // namespace resmon
