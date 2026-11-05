#pragma once
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace cycode::updates {
enum class DownloadPhase { preparing, receiving, verifying, staging, ready, cancelled, failed };
struct DownloadStatus {
    DownloadStatus(uint64_t total, std::string tag) : totalBytes(total), version(std::move(tag)) {}
    const uint64_t totalBytes;
    const std::string version;
    std::atomic<DownloadPhase> phase{DownloadPhase::preparing};
    std::atomic<uint64_t> resumed{0}, networkBytes{0}, requiredBytes{0}, availableBytes{0};
    std::atomic<bool> spaceKnown{false}, fullFallback{false};
};
// Main-thread observations only. Retained prefixes never enter the rate counter.
struct DownloadMeter {
    double sampleTime = 0, lastDataTime = 0, bytesPerSecond = 0;
    uint64_t sampleBytes = 0, observedBytes = 0;
    bool active = false;
    constexpr void observe(double now, uint64_t bytes, bool receiving) noexcept {
        if (!receiving || !(now >= 0 && now <= 1e12)) {
            *this = {};
            return;
        }
        if (!active || now < sampleTime || bytes < observedBytes) {
            *this = {};
            active = true;
            sampleTime = lastDataTime = now;
            sampleBytes = observedBytes = bytes;
            return;
        }
        if (bytes > observedBytes)
            lastDataTime = now;
        observedBytes = bytes;
        const auto elapsed = now - sampleTime;
        if (elapsed >= 0.5) {
            const double rate = double(bytes - sampleBytes) / elapsed;
            bytesPerSecond = bytesPerSecond > 0 ? 0.5 * bytesPerSecond + 0.5 * rate : rate;
            sampleBytes = bytes;
            sampleTime = now;
        }
        if (now - lastDataTime > 3)
            bytesPerSecond = 0;
    }
    constexpr std::optional<uint64_t> remaining(uint64_t received, uint64_t total) const noexcept {
        if (!active || !(bytesPerSecond > 0 && bytesPerSecond <= 1e12) || received > total)
            return std::nullopt;
        const auto seconds = double(total - received) / bytesPerSecond;
        if (!(seconds >= 0 && seconds <= 86400))
            return std::nullopt;
        const auto rounded = static_cast<uint64_t>(seconds);
        return rounded + (double(rounded) < seconds ? 1 : 0);
    }
};
} // namespace cycode::updates
