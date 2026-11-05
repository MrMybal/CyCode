#pragma once
#include "cycode/LanguageClient.h"
namespace cycode {
struct ServerLaunch {
    std::vector<std::string> argv;
    Json settings = Json::object();
    Json environment = Json::object();
};
const std::vector<std::string> &languageBackends();
std::string languageBackend(const std::string &path);
std::string languageId(const std::string &path);
std::string languageServerName(const std::string &backend);
bool languageUsesLegacyLexer(const std::string &path);
ServerLaunch defaultServerLaunch(const std::filesystem::path &workspace, const std::string &backend);
// Data-only ABI2 contribution; routing and process ownership stay in the host.
void validateLanguageServer(const Json &definition);
std::string contributedLanguageId(const Json &definition, const std::string &path);
} // namespace cycode
