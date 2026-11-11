#pragma once
#include <atomic>
#include <filesystem>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
// Bounded working buffers, independent of the total database byte count.
std::string digestFileStreaming(const std::filesystem::path &, const std::atomic<bool> * = nullptr);
void visitCompilationDatabase(const std::filesystem::path &,
                              const std::function<void(const nlohmann::json &)> &,
                              const std::atomic<bool> * = nullptr);
} // namespace cycode
