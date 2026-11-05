#include "cycode/DocumentReference.h"
#include "cycode/TextFile.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <random>
#include <stdexcept>
namespace cycode {
void validateUntitledName(const std::string &name) {
    validateEditorText(name);
    if (name.empty() || name.size() > 128 || name == "." || name == ".." ||
        name.find_first_of("/\\:<>\"|?*#%") != std::string::npos || name.back() == '.' ||
        name.back() == ' ' ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        throw std::runtime_error("Expected a short document name without folders or reserved characters");
}
bool isUntitledDocument(const std::string &reference) {
    if (!reference.starts_with("untitled://") || reference.size() > 256)
        return false;
    const auto slash = reference.find('/', 11);
    if (slash == std::string::npos || slash == 11 || slash - 11 > 100)
        return false;
    if (!std::all_of(reference.begin() + 11, reference.begin() + slash, [](unsigned char c) {
            return (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9') || c == '-';
        }))
        return false;
    try {
        validateUntitledName(reference.substr(slash + 1));
        return true;
    } catch (const std::exception &) {
        return false;
    }
}
std::string untitledDocumentName(const std::string &reference) {
    if (!isUntitledDocument(reference))
        throw std::runtime_error("Invalid untitled document reference");
    return reference.substr(reference.find('/', 11) + 1);
}
std::string newUntitledReference(const std::string &name) {
    validateUntitledName(name);
    static std::atomic<uint64_t> sequence{0};
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    return "untitled://" + std::to_string(stamp) + "-" + std::to_string(std::random_device{}()) + "-" +
           std::to_string(++sequence) + "/" + name;
}
} // namespace cycode
