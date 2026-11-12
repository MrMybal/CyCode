#pragma once
#include <stdexcept>
#include <string>
#include <vector>
namespace cycode {
// Global options for every Git process started on an opened folder. That folder is
// untrusted: its .git/config can name a core.fsmonitor program, which Git runs
// whenever it refreshes the index (status, diff, stash, checkout, worktree add...).
// Keep this the one list shared by all call sites; per-file copies drifted apart.
inline const std::vector<std::string> &gitSafetyOptions() {
    static const std::vector<std::string> options{"-c", "core.fsmonitor=false"};
    return options;
}
// Inserts gitSafetyOptions() right after argv[0], which must be "git".
inline void hardenGitArguments(std::vector<std::string> &arguments) {
    if (arguments.empty() || arguments.front() != "git")
        throw std::logic_error("Git safety options require a git argv");
    arguments.insert(arguments.begin() + 1, gitSafetyOptions().begin(), gitSafetyOptions().end());
}
} // namespace cycode
