#pragma once
#include <nlohmann/json.hpp>
#include <string>
namespace cycode {
void validateProjectTemplate(const nlohmann::json &definition);
nlohmann::json expandProjectTemplate(const nlohmann::json &definition, const std::string &name,
                                     const nlohmann::json &options);
} // namespace cycode
