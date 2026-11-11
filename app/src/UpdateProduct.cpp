#include "cycode/Updates.h"
#include "cycode/UpdateProductConfig.h"
#include "cycode/update/detail/UpdateFiles.h"

namespace cycode::updates {
const char *const repository = CYCODE_UPDATE_REPOSITORY;
const ProductIdentity &productIdentity() noexcept {
    static constexpr std::string_view legacyRequired[]{
        CYCODE_UPDATE_APPLICATION, CYCODE_UPDATE_HELPER, "cycode_cli.exe", "app/CyTool.json",
        "app/locales/ui.json", "LICENSE", "COPYRIGHT"};
    static constexpr std::string_view consumerRequired[]{
        CYCODE_UPDATE_APPLICATION, CYCODE_UPDATE_HELPER, "LICENSE", "COPYRIGHT"};
    static constexpr ProductIdentity identity{
        CYCODE_UPDATE_PRODUCT, CYCODE_UPDATE_REPOSITORY, CYCODE_UPDATE_VERSION,
        CYCODE_UPDATE_APPLICATION, CYCODE_UPDATE_HELPER, CYCODE_UPDATE_MARKER,
        CYCODE_UPDATE_MARKER_TEXT, CYCODE_UPDATE_TOKEN_ENV,
        CYCODE_UPDATE_LEGACY ? std::span<const std::string_view>(legacyRequired)
                             : std::span<const std::string_view>(consumerRequired),
        CYCODE_UPDATE_LEGACY};
    return identity;
}
bool packageArchiveName(std::string_view name, std::string_view release, std::string_view target,
                        bool installed) {
    return packageArchiveNameFor(productIdentity().product, name, release, target, installed);
}
bool detail::matchesProduct(const nlohmann::json &metadata) {
    const auto &identity = productIdentity();
    if (!metadata.is_object()) return false;
    if (!metadata.contains("product") && !metadata.contains("repository"))
        return identity.acceptsLegacyMetadata;
    return metadata.contains("product") && metadata.contains("repository") &&
           metadata.at("product") == identity.product && metadata.at("repository") == identity.repository;
}
void detail::requireProduct(const nlohmann::json &metadata) {
    if (!matchesProduct(metadata))
        throw std::runtime_error("Update metadata belongs to another product or repository");
}
nlohmann::json detail::productMetadata(nlohmann::json metadata) {
    metadata["product"] = productIdentity().product;
    metadata["repository"] = productIdentity().repository;
    return metadata;
}
} // namespace cycode::updates
