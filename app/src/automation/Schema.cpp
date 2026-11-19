#include "Internal.h"
#include <set>

namespace cycode::automation {
namespace {
// CyMCPSDK deliberately rejects these three JSON Schema keywords. Keep the
// canonical contracts and validate the extensions in the host, never silently
// erase them from the actual authorization/validation path.
const std::set<std::string> extensions{"pattern", "dependentRequired", "contains"};
void validate(const Json& value, const Json& schema, unsigned depth) {
    if (depth > 64) throw cymcp::Error("InvalidParameters", "Contract nesting exceeds limit");
    if (schema.is_boolean()) {
        cymcp::validate(value, schema);
        return;
    }
    Json base = schema;
    for (const auto* key : {"properties", "additionalProperties", "items", "allOf", "anyOf", "oneOf",
                            "not", "if", "then", "else", "pattern", "dependentRequired", "contains"})
        base.erase(key);
    cymcp::validate(value, base);
    auto fail = [](const char* reason) { throw cymcp::Error("InvalidParameters", reason); };
    if (value.is_string() && schema.contains("pattern")) {
        // Patterns are trusted, versioned contract constants, never caller regexes.
        const auto pattern = schema.at("pattern").get<std::string>();
        thread_local std::map<std::string, std::regex> compiled;
        const auto [found, inserted] = compiled.try_emplace(pattern, pattern);
        (void)inserted;
        if (!std::regex_search(value.get_ref<const std::string&>(), found->second))
            fail("String does not match the canonical contract pattern");
    }
    auto matches = [&](const Json& sub) {
        try { validate(value, sub, depth + 1); return true; }
        catch (const cymcp::Error& e) { if (e.code != "InvalidParameters") throw; return false; }
    };
    if (value.is_object()) {
        const auto properties = schema.value("properties", Json::object());
        for (const auto& [name, member] : value.items()) {
            if (properties.contains(name)) validate(member, properties[name], depth + 1);
            else if (schema.contains("additionalProperties"))
                validate(member, schema["additionalProperties"], depth + 1);
        }
        for (const auto& [name, required] : schema.value("dependentRequired", Json::object()).items())
            if (value.contains(name)) for (const auto& dependency : required)
                if (!value.contains(dependency.get<std::string>())) fail("Dependent property is missing");
    }
    if (value.is_array()) {
        for (const auto& member : value)
            if (schema.contains("items")) validate(member, schema["items"], depth + 1);
        if (schema.contains("contains")) {
            bool found = false;
            for (const auto& member : value) {
                try { validate(member, schema["contains"], depth + 1); found = true; break; }
                catch (const cymcp::Error& e) { if (e.code != "InvalidParameters") throw; }
            }
            if (!found) fail("Array has no member matching the canonical contains constraint");
        }
    }
    if (schema.contains("allOf")) for (const auto& sub : schema["allOf"])
        validate(value, sub, depth + 1);
    for (const auto* key : {"anyOf", "oneOf"}) if (schema.contains(key)) {
        std::size_t count = 0;
        for (const auto& sub : schema[key]) if (matches(sub)) ++count;
        if (!count || (std::string(key) == "oneOf" && count != 1)) fail("Canonical alternative constraint failed");
    }
    if (schema.contains("not") && matches(schema["not"])) fail("Canonical negative constraint failed");
    if (schema.contains("if")) {
        const auto* branch = matches(schema["if"]) ? "then" : "else";
        if (schema.contains(branch)) validate(value, schema[branch], depth + 1);
    }
}
}
void validateContract(const Json& value, const Json& schema) { validate(value, schema, 0); }
Json sdkSchema(const Json& schema) {
    if (schema.is_boolean()) return schema;
    Json result = schema;
    // The SDK profile is a structural projection. Conditional validation remains
    // in validateContract so a removed regex inside `not` cannot reject all values.
    for (const auto* key : {"pattern", "dependentRequired", "contains", "allOf", "anyOf", "oneOf", "not", "if", "then", "else"})
        result.erase(key);
    if (result.contains("properties"))
        for (auto& item : result["properties"].items()) item.value() = sdkSchema(item.value());
    for (const auto* key : {"items", "additionalProperties"})
        if (result.contains(key)) result[key] = sdkSchema(result[key]);
    cymcp::check_schema(result);
    return result;
}
Json readJson(const std::filesystem::path& path, std::size_t maximum) {
    if (std::filesystem::is_symlink(path) || std::filesystem::file_size(path) > maximum)
        throw cymcp::Error("InvalidState", "Missing, redirected or oversized automation data");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw cymcp::Error("FileError", "Cannot read automation data");
    return cymcp::parse_json(std::string(std::istreambuf_iterator<char>(input), {}), maximum);
}
void saveJson(const std::filesystem::path& path, const Json& value) {
    std::filesystem::create_directories(path.parent_path());
    AtomicFileOptions options;
    options.access = AtomicAccess::PrivateUser;
    options.publication = AtomicPublication::Replace;
    writeFileAtomic(path, value.dump(), options);
}
double timestamp() {
    return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
}
bool terminal(const std::string& state) { return state == "Completed" || state == "Failed" || state == "Cancelled"; }
} // namespace cycode::automation
