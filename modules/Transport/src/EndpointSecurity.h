#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace cycode::detail {
std::string nativeEndpointSecret();
bool nativeEndpointSecretMatches(const std::string &candidate,const std::string &expected) noexcept;
// Publish a credential descriptor atomically, private from its initial creation.
// The host must select a trusted directory; this does not secure a shared parent.
void publishNativeDescriptor(const std::filesystem::path &,const std::string &,
                             const std::optional<std::string> &baseline);
}
