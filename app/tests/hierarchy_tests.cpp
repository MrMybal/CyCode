#include "cycode/Commands.h"
#include "cycode/Hierarchy.h"
#include <stdexcept>
namespace {
using Json = nlohmann::json;
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    try {
        fn();
    } catch (const std::exception &) {
        return;
    }
    throw std::runtime_error("Malformed hierarchy data accepted");
}
} // namespace
void hierarchyContracts() {
    using namespace cycode;
    const Json range = {{"start", {{"line", 1}, {"character", 2}}}, {"end", {{"line", 3}, {"character", 8}}}};
    const Json selection = {{"start", {{"line", 2}, {"character", 0}}},
                            {"end", {{"line", 2}, {"character", 4}}}};
    Json item = {{"name", "méthode"},
                 {"detail", "void méthode()"},
                 {"uri", "file:///workspace/main.cpp"},
                 {"kind", 12},
                 {"range", range},
                 {"selectionRange", selection},
                 {"data", {{"serverItem", 42}}}};
    const auto original = item;
    validateHierarchyItem(item);
    check(item == original, "Opaque hierarchy data was modified during validation");
    check(hierarchyMethod("call") == "textDocument/prepareCallHierarchy", "Call hierarchy preparation");
    check(hierarchyMethod("call", "incoming") == "callHierarchy/incomingCalls", "Incoming call routing");
    check(hierarchyMethod("call", "outgoing") == "callHierarchy/outgoingCalls", "Outgoing call routing");
    check(hierarchyMethod("type") == "textDocument/prepareTypeHierarchy", "Type hierarchy preparation");
    check(hierarchyMethod("type", "supertypes") == "typeHierarchy/supertypes", "Base type routing");
    check(hierarchyMethod("type", "subtypes") == "typeHierarchy/subtypes", "Derived type routing");
    rejects([] { hierarchyMethod("call", "subtypes"); });
    rejects([] { hierarchyMethod("type", "incoming"); });
    rejects([] { hierarchyMethod("unknown"); });
    item["selectionRange"]["start"]["line"] = 0;
    rejects([&] { validateHierarchyItem(item); });
    item = original;
    item["kind"] = 27;
    rejects([&] { validateHierarchyItem(item); });
    item = original;
    item["data"] = std::string(64 * 1024, 'x');
    rejects([&] { validateHierarchyItem(item); });
    auto malformed = range;
    malformed["start"]["character"] = -1;
    rejects([&] { hierarchyRange(malformed); });
    malformed = range;
    malformed["start"]["line"] = 1.5;
    rejects([&] { hierarchyRange(malformed); });
    malformed = range;
    malformed["end"]["line"] = uint64_t(1) << 40;
    rejects([&] { hierarchyRange(malformed); });
    malformed = range;
    malformed["end"]["line"] = 0;
    rejects([&] { hierarchyRange(malformed); });
    Keymap keys;
    keys.load("rider");
    check(keys.press("ctrl+alt+h", 0, false, true).command == "navigate.callHierarchy",
          "Rider call hierarchy shortcut");
    check(keys.press("ctrl+h", 1, false, true).command == "navigate.typeHierarchy",
          "Rider type hierarchy shortcut");
    keys.load("visualstudio");
    check(keys.press("ctrl+alt+k", 2, false, true).command == "navigate.callHierarchy",
          "Visual Studio call hierarchy shortcut");
    keys.load("vscode");
    check(keys.press("shift+alt+h", 3, false, true).command == "navigate.callHierarchy",
          "VS Code call hierarchy shortcut");
}
