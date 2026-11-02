#pragma once
#include "cycode/Paths.h"
#include <nlohmann/json.hpp>
#include <cyui/Core.h>
#include <cyui/modules/CodeEditor.h>
#include <cyui/modules/Commands.h>
#include <cyui/modules/Diagnostics.h>
#include <cyui/modules/Diff.h>
#include <cyui/modules/DocumentViews.h>
#include <cyui/modules/Inspector.h>
#include <map>

namespace cycode {
cyui::ItemId uiId(const std::string &);
void cancelPropertyDrafts(cyui::InspectorState &);
struct ProblemLocation {
    std::string path;
    int line = 1, column = 1;
    bool utf16 = false;
};
// The application owns navigation permissions and coordinate conversion.
struct ProblemModel {
    std::vector<cyui::Diagnostic> rows;
    std::map<cyui::ItemId, ProblemLocation> locations;
    void add(const std::string &key, cyui::LogLevel, const std::string &source,
             const std::string &message, ProblemLocation);
    const std::vector<cyui::Diagnostic> &matching(const cyui::TextFilter &) const;
  private:
    uint64_t revision_ = 0;
    mutable uint64_t filteredRevision_ = ~uint64_t(0);
    mutable std::string filteredQuery_, filteredLocale_;
    mutable std::vector<cyui::Diagnostic> filteredRows_;
};
class OutputLog {
  public:
    void synchronize(std::string &);
    cyui::LogBuffer buffer;
    cyui::LogState state;
  private:
    std::string seen_;
};
struct InformationDocument {
    cyui::Document document;
    cyui::DocumentState state;
    cyui::CodeDocument code;
    cyui::CodeViewState codeState;
};
} // namespace cycode
