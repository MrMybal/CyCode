#pragma once
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cycode {
// Construct from the owner's validated UTF-8 buffer with LF line endings.
// One pass records line lengths/surrogate splits; queries never rescan the file.
class SymbolTextBounds {
  public:
    explicit SymbolTextBounds(const std::string &);
    bool accepts(const nlohmann::json &range) const;

  private:
    std::vector<uint32_t> lineUnits_;
    std::vector<uint64_t> surrogateSplits_;
};
using PendingSymbolResolver =
    std::function<nlohmann::json(const std::string &uri, const nlohmann::json &item)>;
using SymbolResolver = std::function<nlohmann::json(const std::string &uri, const nlohmann::json &selection,
                                                    const nlohmann::json &extent)>;
// Snapshot-local nonzero node IDs preserve the server's hierarchy in a flat,
// bounded compatibility list. The owner validates source access and revisions.
nlohmann::json collectSymbols(const nlohmann::json &response, const std::string &documentUri, size_t limit,
                              bool typesOnly, const SymbolResolver &, const PendingSymbolResolver & = {});
uint64_t enclosingSymbol(const nlohmann::json &symbols, const std::string &path,
                         const nlohmann::json &position);
const char *symbolKindName(int kind);
} // namespace cycode
