#include "cycode/TaskOutputText.h"
#include <nlohmann/json.hpp>
namespace cycode {
TaskOutputText::TaskOutputText(ProcessOutputObserver sink, bool cargo)
    : sink_(std::move(sink)), cargo_(cargo) {}
ProcessOutputObserver TaskOutputText::observer() {
    return sink_ ? ProcessOutputObserver([this](std::string_view bytes) { append(bytes); })
                 : ProcessOutputObserver{};
}
void TaskOutputText::line() noexcept {
    try {
        const auto value = nlohmann::json::parse(pending_, nullptr, false);
        if (value.is_object() && value.contains("reason") && value.at("reason").is_string()) {
            const auto reason = value.at("reason").get<std::string>();
            if (reason == "compiler-message" && value.contains("message") &&
                value.at("message").is_object()) {
                const auto &message = value.at("message");
                const auto rendered = message.find("rendered"), text = message.find("message");
                if (rendered != message.end() && rendered->is_string())
                    sink_(rendered->get_ref<const std::string &>());
                else if (text != message.end() && text->is_string())
                    sink_(text->get_ref<const std::string &>());
                sink_("\n");
                return;
            }
            if (reason == "compiler-artifact" || reason == "build-script-executed" ||
                reason == "build-finished")
                return;
        }
        sink_(pending_);
        sink_("\n");
    } catch (...) {
        try {
            sink_("[CyCode: Cargo live message unavailable; inspect the final task result]\n");
        } catch (...) {
        }
    }
}
void TaskOutputText::append(std::string_view bytes) noexcept {
    if (!sink_)
        return;
    try {
        if (!cargo_) {
            sink_(bytes);
            return;
        }
        for (char c : bytes) {
            if (c == '\n') {
                if (!skipping_)
                    line();
                pending_.clear();
                skipping_ = false;
            } else if (!skipping_) {
                if (pending_.size() == 64 * 1024) {
                    pending_.clear();
                    skipping_ = true;
                    sink_("[CyCode: oversized Cargo protocol line omitted from live output]\n");
                } else
                    pending_ += c;
            }
        }
    } catch (...) {
        pending_.clear();
        skipping_ = true;
        try {
            sink_("[CyCode: live output framing failed; inspect the final task result]\n");
        } catch (...) {
        }
    }
}
void TaskOutputText::flush() noexcept {
    if (sink_ && cargo_ && !skipping_ && !pending_.empty())
        line();
    pending_.clear();
    skipping_ = false;
}
} // namespace cycode
