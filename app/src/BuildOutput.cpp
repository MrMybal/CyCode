#include "cycode/ProcessText.h"
#include "cycode/TaskOutput.h"
#include <algorithm>

namespace cycode {
std::string TaskOutput::begin(const std::string &method, const std::string &operationId,
                              const Json &context) {
    static std::atomic<uint64_t> sequence{0};
    const auto stream = "task-" +
                        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                        std::to_string(++sequence);
    auto nextStream = stream, nextId = operationId, nextMethod = method;
    auto nextContext = context;
    std::lock_guard lock(mutex_);
    stream_.swap(nextStream);
    operationId_.swap(nextId);
    method_.swap(nextMethod);
    context_.swap(nextContext);
    chunks_.clear();
    retained_ = 0;
    bytes_ = dropped_ = 0;
    revision_ = 1;
    running_ = true;
    cancelling_ = incomplete_ = false;
    return stream;
}
void TaskOutput::context(const std::string &stream, const Json &value) {
    auto next = value;
    std::lock_guard lock(mutex_);
    if (stream != stream_ || !running_)
        return;
    context_.swap(next);
    ++revision_;
}
void TaskOutput::cancelling() {
    std::lock_guard lock(mutex_);
    if (running_ && !cancelling_) {
        cancelling_ = true;
        ++revision_;
    }
}
void TaskOutput::finish() {
    std::lock_guard lock(mutex_);
    if (running_) {
        running_ = false;
        cancelling_ = false;
        ++revision_;
    }
}
ProcessOutputObserver TaskOutput::observer(const std::string &stream) {
    return [this, stream](std::string_view bytes) { append(stream, bytes); };
}
void TaskOutput::append(const std::string &stream, std::string_view bytes) noexcept {
    std::lock_guard lock(mutex_);
    if (stream != stream_ || !running_ || bytes.empty())
        return;
    try {
        bytes_ += bytes.size();
        ++revision_;
        while (!bytes.empty()) {
            if (chunks_.empty() || chunks_.back().size() == 8192)
                chunks_.emplace_back();
            auto &chunk = chunks_.back();
            const auto count = std::min(bytes.size(), size_t(8192) - chunk.size());
            chunk.append(bytes.data(), count);
            retained_ += count;
            bytes.remove_prefix(count);
            while (retained_ > 256 * 1024) {
                auto &first = chunks_.front();
                const auto removed = std::min(first.size(), retained_ - 256 * 1024);
                first.erase(0, removed);
                retained_ -= removed;
                dropped_ += removed;
                if (first.empty())
                    chunks_.pop_front();
            }
        }
    } catch (...) {
        // A journal allocation failure does not stop or release a live build.
        incomplete_ = true;
        chunks_.clear();
        retained_ = 0;
        dropped_ = bytes_;
    }
}
TaskOutput::Json TaskOutput::read(const Json &parameters) const {
    if (!parameters.is_object())
        throw std::runtime_error("Task output parameters must be an object");
    for (auto it = parameters.begin(); it != parameters.end(); ++it)
        if (it.key() != "streamId" && it.key() != "revision")
            throw std::runtime_error("Unknown task output parameter");
    const bool conditional = parameters.contains("streamId");
    if (conditional != parameters.contains("revision"))
        throw std::runtime_error("Task output streamId and revision must be supplied together");
    std::string stream;
    uint64_t revision = 0;
    if (conditional) {
        stream = parameters.at("streamId").get<std::string>();
        const auto &value = parameters.at("revision");
        if (stream.empty() || stream.size() > 128 ||
            !std::all_of(stream.begin(), stream.end(),
                         [](unsigned char c) {
                             return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                                    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
                         }) ||
            (!value.is_number_unsigned() && (!value.is_number_integer() || value.get<int64_t>() < 0)))
            throw std::runtime_error("Invalid task output observation");
        revision = value.get<uint64_t>();
    }
    Json result;
    std::string raw;
    {
        std::lock_guard lock(mutex_);
        const bool changed = !conditional || stream != stream_ || revision != revision_;
        result = {{"running", running_},         {"cancelling", running_ && cancelling_},
                  {"operationId", operationId_}, {"method", method_},
                  {"streamId", stream_},         {"revision", revision_},
                  {"changed", changed},          {"byteCount", bytes_},
                  {"droppedBytes", dropped_},    {"incomplete", incomplete_},
                  {"encoding", "utf8-repaired"}, {"output", ""}};
        if (!context_.empty())
            result["context"] = context_;
        if (changed) {
            raw.reserve(retained_);
            for (const auto &chunk : chunks_)
                raw += chunk;
        }
    }
    // Every response replaces the previous tail. No incremental UTF-8 boundary or
    // terminal-control state is carried between polls; final process output stays raw.
    for (auto &c : raw)
        if ((static_cast<unsigned char>(c) < 32 && c != '\r' && c != '\n' && c != '\t') || c == 127)
            c = '?';
    result["output"] = processOutputText(raw, true);
    return result;
}
} // namespace cycode
