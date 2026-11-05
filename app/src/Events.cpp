#include "cycode/Events.h"
#include <chrono>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <vector>
namespace cycode {
namespace {
using Json = nlohmann::json;
thread_local std::string origin;
std::string epoch() {
    std::random_device random;
    std::ostringstream value;
    for (int i = 0; i < 4; ++i)
        value << std::hex << std::setfill('0') << std::setw(8) << uint32_t(random());
    return value.str();
}
constexpr uint64_t maxSequence = 9007199254740991ULL;
uint64_t cursorValue(const Json &value) {
    if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<int64_t>() < 0) ||
        value.get<uint64_t>() > maxSequence)
        throw std::runtime_error("Invalid event sequence");
    return value.get<uint64_t>();
}
} // namespace
EventOrigin::EventOrigin(std::string value) : previous_(std::move(origin)) {
    origin = std::move(value);
}
EventOrigin::~EventOrigin() {
    origin = std::move(previous_);
}
const std::string &EventOrigin::current() {
    return origin;
}
Events::Events() : epoch_(epoch()) {}
void Events::publish(const std::string &type, Json data) noexcept {
    std::lock_guard lock(mutex_);
    try {
        if (sequence_ == maxSequence) {
            auto next = epoch();
            epoch_ = std::move(next);
            sequence_ = lastDropped_ = 0;
            entries_.clear();
            bytes_ = 0;
        }
        const auto sequence = ++sequence_;
        Json value = {{"sequence", sequence},
                      {"origin", EventOrigin::current()},
                      {"type", type},
                      {"timestampMs", std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count()},
                      {"data", std::move(data)}};
        const auto size = value.dump().size();
        if (size > 65536) {
            lastDropped_ = sequence;
            return;
        }
        entries_.push_back({std::move(value), size});
        bytes_ += size;
        while (entries_.size() > 512 || bytes_ > 4 * 1024 * 1024) {
            bytes_ -= entries_.front().bytes;
            entries_.pop_front();
        }
    } catch (...) {
        lastDropped_ = sequence_;
    }
}
Json Events::read(const Json &parameters) const {
    auto after = cursorValue(parameters.value("afterSequence", Json(0)));
    const auto limitValue = parameters.value("limit", Json(128));
    if (!limitValue.is_number_integer() || limitValue < 1 || limitValue > 512)
        throw std::runtime_error("Event limit must be 1 through 512");
    const auto limit = limitValue.get<size_t>();
    const auto requestedEpoch = parameters.value("epoch", std::string{});
    if (after && requestedEpoch.empty())
        throw std::runtime_error("Resuming events requires the issued epoch");
    const auto types = parameters.value("types", Json::array());
    if (!types.is_array() || types.size() > 16)
        throw std::runtime_error("Invalid event type filters");
    std::vector<std::string> filters;
    for (const auto &value : types) {
        const auto type = value.get<std::string>();
        if (type.empty() || type.size() > 64 || type.find('\0') != std::string::npos ||
            (type.find('*') != std::string::npos && type.find('*') != type.size() - 1))
            throw std::runtime_error("Invalid event filter");
        filters.push_back(type);
    }
    const bool tail = parameters.value("tail", false);
    const auto exclude = parameters.value("excludeOrigin", std::string{});
    std::lock_guard lock(mutex_);
    if (!requestedEpoch.empty() && requestedEpoch != epoch_)
        throw std::runtime_error("Event epoch changed; refresh the workspace snapshot");
    if (after > sequence_)
        throw std::runtime_error("Event cursor is ahead of this workspace");
    const auto oldest =
        entries_.empty() ? sequence_ + 1 : entries_.front().value.at("sequence").get<uint64_t>();
    Json result = Json::array();
    uint64_t next = after;
    if (tail)
        next = sequence_;
    else {
        for (const auto &entry : entries_) {
            const auto sequence = entry.value.at("sequence").get<uint64_t>();
            if (sequence <= after)
                continue;
            next = sequence;
            const auto &type = entry.value.at("type").get_ref<const std::string &>();
            bool matches = filters.empty();
            for (const auto &filter : filters)
                matches |= filter.back() == '*' ? type.starts_with(filter.substr(0, filter.size() - 1))
                                                : type == filter;
            if (!exclude.empty() && entry.value.at("origin") == exclude)
                matches = false;
            if (matches)
                result.push_back(entry.value);
            if (result.size() == limit)
                break;
        }
        if (result.size() < limit)
            next = sequence_;
    }
    return {{"epoch", epoch_},
            {"oldestSequence", oldest},
            {"latestSequence", sequence_},
            {"nextSequence", next},
            {"gap", !tail && (after < oldest - 1 || after < lastDropped_)},
            {"hasMore", next < sequence_},
            {"events", std::move(result)}};
}
} // namespace cycode
