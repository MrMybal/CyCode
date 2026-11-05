#pragma once
#include "cycode/Process.h"
namespace cycode {
// Display-only framing; the process result and its protocol bytes stay unchanged.
class TaskOutputText {
  public:
    TaskOutputText(ProcessOutputObserver sink, bool cargo = false);
    ProcessOutputObserver observer();
    void flush() noexcept;

  private:
    void append(std::string_view) noexcept;
    void line() noexcept;
    ProcessOutputObserver sink_;
    std::string pending_;
    bool cargo_ = false, skipping_ = false;
};
} // namespace cycode
