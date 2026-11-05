#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
namespace cycode {
// Git config key\nvalue\0 and ls-files --stage -z, not human status text.
nlohmann::json parseGitSubmodules(const std::string &configuration, const std::string &index);
nlohmann::json gitSubmoduleAddModel(const nlohmann::json &parameters);
nlohmann::json gitSubmoduleSettingsModel(const nlohmann::json &parameters);
std::string gitSubmoduleSettingsRemainder(const std::string &configuration, const std::string &key);
nlohmann::json gitSubmoduleDeinitModel(const nlohmann::json &entry, const std::string &index,
                                       const std::string &flags);
std::vector<std::string> gitSubmoduleArguments(const std::string &action, const std::string &path,
                                               bool fetch = false);
} // namespace cycode
