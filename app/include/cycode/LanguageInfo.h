#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
// Bounded, display-only LSP information. Markdown/HTML is never executed.
nlohmann::json hoverInformation(const nlohmann::json &reply,const std::string &document);
nlohmann::json signatureInformation(const nlohmann::json &reply);
}
