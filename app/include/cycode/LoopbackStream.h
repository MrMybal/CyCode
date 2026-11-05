#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
namespace cycode {
// Nonblocking, owned IPv4 loopback connection. No DNS, listener or child process.
class LoopbackStream {
  public:
    LoopbackStream();
    ~LoopbackStream();
    bool connect(std::uint16_t port);
    // Positive: bytes transferred; zero: would block; negative: closed/error.
    std::ptrdiff_t read(char *, std::size_t);
    std::ptrdiff_t write(const char *, std::size_t);
    void shutdown();
    LoopbackStream(const LoopbackStream &) = delete;
    LoopbackStream &operator=(const LoopbackStream &) = delete;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cycode
