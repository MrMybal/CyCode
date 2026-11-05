#pragma once
#include <filesystem>
#include <memory>
namespace cycode {
enum class NativeFileLeaseMode { exclusive, shared };
// Nonblocking OS lock. Keep the lock file in place to retain one inode/identity.
class NativeFileLease {
  public:
    explicit NativeFileLease(const std::filesystem::path &,
                             NativeFileLeaseMode = NativeFileLeaseMode::exclusive);
    ~NativeFileLease();
    bool acquired() const;
    NativeFileLease(const NativeFileLease &) = delete;
    NativeFileLease &operator=(const NativeFileLease &) = delete;

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace cycode
