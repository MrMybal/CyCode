#pragma once
#include <deque>
#include <mutex>
#include <nlohmann/json.hpp>
namespace cycode {
class EventOrigin {
  public:
    explicit EventOrigin(std::string);
    ~EventOrigin();
    EventOrigin(const EventOrigin &) = delete;
    EventOrigin &operator=(const EventOrigin &) = delete;
    static const std::string &current();

  private:
    std::string previous_;
};
// Bounded pull journal. Never calls plugin code or acquires Engine locks.
class Events {
  public:
    Events();
    void publish(const std::string &type, nlohmann::json data) noexcept;
    nlohmann::json read(const nlohmann::json &) const;

  private:
    struct Entry {
        nlohmann::json value;
        size_t bytes;
    };
    mutable std::mutex mutex_;
    std::string epoch_;
    uint64_t sequence_ = 0, lastDropped_ = 0;
    size_t bytes_ = 0;
    std::deque<Entry> entries_;
};
} // namespace cycode
