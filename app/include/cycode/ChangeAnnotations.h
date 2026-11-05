#pragma once
#include <nlohmann/json.hpp>

namespace cycode {
struct AnnotatedWorkspace {
    nlohmann::json files, annotations;
};
// Validate passive LSP labels and replace server IDs with plan-local issued IDs.
AnnotatedWorkspace normalizeChangeAnnotations(nlohmann::json files, const nlohmann::json &declarations);
// Keep only annotations attached to effective edits, with their actual file list.
nlohmann::json usedChangeAnnotations(const nlohmann::json &annotations, const nlohmann::json &effectiveFiles);
void confirmChangeAnnotations(const nlohmann::json &annotations, const nlohmann::json &confirmed);
} // namespace cycode
