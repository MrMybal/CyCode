#pragma once
#include "cycode/Error.h"
#include <functional>
namespace cycode {
inline constexpr const char *coreProtocol="cycode-core/1";
inline constexpr size_t maximumNativeRequestBytes=8*1024*1024;
using NativeDispatch=std::function<nlohmann::json(const std::string &,const nlohmann::json &)>;
nlohmann::json nativeErrorResponse(const std::string &code,const std::string &message,
    const nlohmann::json &id=nullptr,const nlohmann::json &details=nlohmann::json::object());
// Validates the shared envelope and translates typed owner failures. It owns
// neither documents nor engines, jobs, identities or permission policies.
nlohmann::json respondNative(const nlohmann::json &,const NativeDispatch &) noexcept;
} // namespace cycode
