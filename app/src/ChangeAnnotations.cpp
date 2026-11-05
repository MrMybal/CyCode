#include "cycode/ChangeAnnotations.h"
#include <map>
#include <set>
#include <stdexcept>

namespace cycode {
using Json = nlohmann::json;
namespace {
constexpr size_t maxAnnotations = 128, maxBytes = 512 * 1024;
std::string text(const Json &value, const char *key, size_t bound, bool required) {
    if (!value.contains(key) && !required)
        return {};
    const auto result = value.at(key).get<std::string>();
    if ((required && result.empty()) || result.size() > bound || result.find('\0') != std::string::npos)
        throw std::runtime_error("Invalid change annotation text");
    return result;
}
void bounded(const Json &value) {
    if (value.dump().size() > maxBytes)
        throw std::runtime_error("Change annotations exceed 512 KiB");
}
} // namespace
AnnotatedWorkspace normalizeChangeAnnotations(Json files, const Json &declarations) {
    if (!files.is_object() || files.size() > 100 || !declarations.is_object() ||
        declarations.size() > maxAnnotations)
        throw std::runtime_error("Invalid change annotation collection");
    bounded(declarations);
    AnnotatedWorkspace result{std::move(files), Json::array()};
    std::map<std::string, std::string> ids;
    for (const auto &[id, annotation] : declarations.items()) {
        if (id.size() > 512 || !annotation.is_object())
            throw std::runtime_error("Invalid change annotation declaration");
        const auto label = text(annotation, "label", 2048, true);
        const auto description = text(annotation, "description", 4096, false);
        const auto required = annotation.value("needsConfirmation", Json(false));
        if (!required.is_boolean())
            throw std::runtime_error("Invalid annotation confirmation flag");
        const auto issued = "annotation-" + std::to_string(ids.size() + 1);
        ids[id] = issued;
        result.annotations.push_back({{"id", issued},
                                      {"label", label},
                                      {"description", description},
                                      {"needsConfirmation", required},
                                      {"files", Json::array()}});
    }
    for (auto &[path, edits] : result.files.items()) {
        if (!edits.is_array() || edits.size() > 50000)
            throw std::runtime_error("Invalid annotated text edit list");
        for (auto &edit : edits) {
            if (!edit.is_object())
                throw std::runtime_error("Invalid annotated text edit");
            if (!edit.contains("annotationId"))
                continue;
            if (!edit.at("annotationId").is_string() ||
                edit.at("annotationId").get_ref<const std::string &>().size() > 512)
                throw std::runtime_error("Invalid text edit annotation reference");
            const auto id = edit.at("annotationId").get<std::string>();
            if (!ids.contains(id))
                throw std::runtime_error("Text edit references an unknown change annotation");
            edit["annotationId"] = ids.at(id);
        }
    }
    return result;
}
Json usedChangeAnnotations(const Json &annotations, const Json &files) {
    std::map<std::string, std::set<std::string>> used;
    for (const auto &[path, edits] : files.items())
        for (const auto &edit : edits)
            if (edit.contains("annotationId"))
                used[edit.at("annotationId").get<std::string>()].insert(path);
    Json result = Json::array();
    for (const auto &annotation : annotations) {
        const auto id = annotation.at("id").get<std::string>();
        const auto found = used.find(id);
        if (found == used.end())
            continue;
        auto item = annotation;
        item["files"] = found->second;
        result.push_back(std::move(item));
        used.erase(found);
    }
    if (!used.empty())
        throw std::runtime_error("Effective edit references an unknown change annotation");
    bounded(result);
    return result;
}
void confirmChangeAnnotations(const Json &annotations, const Json &confirmed) {
    if (!confirmed.is_array() || confirmed.size() > maxAnnotations)
        throw std::runtime_error("Invalid change annotation confirmations");
    std::set<std::string> known, required, accepted;
    for (const auto &annotation : annotations) {
        const auto id = annotation.at("id").get<std::string>();
        known.insert(id);
        if (annotation.at("needsConfirmation").get<bool>())
            required.insert(id);
    }
    for (const auto &entry : confirmed) {
        if (!entry.is_string() || entry.get_ref<const std::string &>().empty() ||
            entry.get_ref<const std::string &>().size() > 256)
            throw std::runtime_error("Invalid issued annotation confirmation ID");
        const auto id = entry.get<std::string>();
        if (!known.contains(id) || !accepted.insert(id).second)
            throw std::runtime_error("Unknown or duplicate change annotation confirmation");
    }
    for (const auto &id : required)
        if (!accepted.contains(id))
            throw std::runtime_error("Review and confirm the required change annotations before applying");
}
} // namespace cycode
