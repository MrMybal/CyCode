#pragma once
#include "cycode/Updates.h"
#include <map>
#include <set>
namespace cycode::updates::detail {
struct ManagedFile {
    std::string digest;
    uint64_t size;
};
using ManagedFiles = std::map<std::string, ManagedFile>;
ManagedFiles managedManifest(const nlohmann::json &);
std::set<std::string> managedUnion(const ManagedFiles &, const ManagedFiles &);
} // namespace cycode::updates::detail
