#include "cycode/CargoBuild.h"
#include "cycode/Paths.h"
#include "cycode/RustToolchain.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace cycode {
namespace {
using Json = nlohmann::json;
std::string text(const Json &value, const char *key, size_t maximum = 4096) {
    const auto result = value.value(key, std::string{});
    if (result.size() > maximum ||
        std::any_of(result.begin(), result.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        throw std::runtime_error(std::string("Invalid Cargo field: ") + key);
    return result;
}
bool contains(const Json &array, const std::string &item) {
    return array.is_array() && std::find(array.begin(), array.end(), item) != array.end();
}
std::string targetKind(const Json &profile) {
    return profile.value("cargoTargetKind", std::string("bin"));
}
bool runnable(const std::string &kind) {
    return kind == "bin" || kind == "example";
}
} // namespace
void validateCargoOptions(const Json &profile) {
    validateRustToolchain(text(profile,"cargoToolchain",128));
    for (auto key : {"cargoWorkspace", "cargoAllFeatures", "cargoNoDefaultFeatures", "cargoLocked"})
        if (profile.contains(key) && !profile.at(key).is_boolean())
            throw std::runtime_error(std::string("Invalid Cargo boolean: ") + key);
    const auto kind = targetKind(profile);
    if (kind != "bin" && kind != "example" && kind != "lib" && kind != "test" && kind != "bench")
        throw std::runtime_error("Unsupported Cargo target kind");
    const auto name = text(profile, "cargoProfile", 128);
    if (!name.empty() && (name.front() == '-' || name.find_first_of(" /\\.,:*?[]\t") != std::string::npos))
        throw std::runtime_error("Invalid Cargo profile name");
    const auto package = text(profile, "package"), target = text(profile, "target");
    for (const auto &field : {package, target})
        if (!field.empty() && (field.front() == '-' || field.find_first_of("*?[]") != std::string::npos))
            throw std::runtime_error("Invalid Cargo package or target");
    if (profile.value("cargoWorkspace", false) && !package.empty())
        throw std::runtime_error("Choose a Cargo package or the entire workspace");
    const auto features = profile.value("cargoFeatures", Json::array());
    if (!features.is_array() || features.size() > 128)
        throw std::runtime_error("Cargo features must be a list of at most 128 names");
    std::set<std::string> seen;
    for (const auto &feature : features) {
        if (!feature.is_string())
            throw std::runtime_error("Cargo feature must be text");
        const auto name = feature.get<std::string>();
        if (name.empty() || name.size() > 256 || name.front() == '-' ||
            std::any_of(name.begin(), name.end(),
                        [](unsigned char c) { return c <= 32 || c == 127 || c == ','; }) ||
            !seen.insert(name).second)
            throw std::runtime_error("Invalid or duplicate Cargo feature");
    }
}
std::vector<std::string> cargoArguments(const Json &profile, const std::string &operation,
                                        const std::filesystem::path &manifest) {
    validateCargoOptions(profile);
    const bool metadata = operation == "project.configure";
    if (!metadata && operation != "project.build" && operation != "project.test" &&
        operation != "project.launch" && operation != "project.debug")
        throw std::runtime_error("Unsupported Cargo operation");
    const auto kind = targetKind(profile), target = text(profile, "target");
    if ((operation == "project.launch" || operation == "project.debug") && !runnable(kind))
        throw std::runtime_error("Only Cargo binary and executable example targets can run or debug");
    std::vector<std::string> args = {metadata                      ? "metadata"
                                     : operation == "project.test" ? "test"
                                                                   : "build",
                                     "--manifest-path", pathUtf8(manifest)};
    if (metadata)
        args.insert(args.end(), {"--format-version", "1", "--no-deps"});
    else {
        args.insert(args.end(), {"--message-format=json", "--color=never"});
        const auto custom = text(profile, "cargoProfile", 128);
        if (!custom.empty())
            args.insert(args.end(), {"--profile", custom});
        else if (profile.value("configuration", std::string("Debug")) == "Release")
            args.push_back("--release");
        if (profile.value("cargoWorkspace", false))
            args.push_back("--workspace");
        const auto package = text(profile, "package");
        if (!package.empty())
            args.insert(args.end(), {"--package", package});
        if (kind == "lib")
            args.push_back("--lib");
        else if (!target.empty())
            args.insert(args.end(), {"--" + kind, target});
        else if (kind != "bin")
            args.push_back(kind == "bench" ? "--benches" : "--" + kind + "s");
    }
    if (profile.value("offline", false))
        args.push_back("--offline");
    if (profile.value("cargoLocked", false))
        args.push_back("--locked");
    if (profile.value("cargoAllFeatures", false))
        args.push_back("--all-features");
    if (profile.value("cargoNoDefaultFeatures", false))
        args.push_back("--no-default-features");
    for (const auto &feature : profile.value("cargoFeatures", Json::array()))
        args.insert(args.end(), {"--features", feature.get<std::string>()});
    return args;
}
Json cargoWorkspaceModel(const Json &metadata, const std::filesystem::path &workspace,
                         const CargoResolver &resolve) {
    if (!metadata.is_object() || metadata.value("version", 0) != 1 || !metadata.at("packages").is_array() ||
        metadata.at("packages").size() > 2000 || !metadata.at("workspace_members").is_array() ||
        metadata.at("workspace_members").size() > 2000)
        throw std::runtime_error("Invalid or excessive Cargo format-version 1 workspace");
    const auto root = resolve(text(metadata, "workspace_root", 8192));
    Json model = {{"workspaceRoot", pathUtf8(root.lexically_relative(workspace))},
                  {"packages", Json::array()},
                  {"targets", Json::array()}};
    std::set<std::string> ids;
    for (const auto &package : metadata.at("packages")) {
        const auto id = text(package, "id", 8192);
        if (!contains(metadata.at("workspace_members"), id))
            continue;
        if (id.empty() || !ids.insert(id).second)
            throw std::runtime_error("Invalid Cargo workspace package identity");
        const auto manifest = resolve(text(package, "manifest_path", 8192));
        const auto name = text(package, "name");
        const auto features = package.value("features", Json::object());
        if (name.empty() || !features.is_object() || features.size() > 2000)
            throw std::runtime_error("Invalid Cargo package features");
        Json names = Json::array();
        for (const auto &[feature, _] : features.items()) {
            if (feature.size() > 256)
                throw std::runtime_error("Cargo feature name exceeds limit");
            names.push_back(feature);
        }
        model["packages"].push_back(
            {{"id", id},
             {"name", name},
             {"manifest", pathUtf8(manifest.lexically_relative(workspace))},
             {"defaultMember", contains(metadata.value("workspace_default_members", Json::array()), id)},
             {"features", names}});
        const auto &targets = package.at("targets");
        if (!targets.is_array() || targets.size() > 2000)
            throw std::runtime_error("Invalid Cargo package targets");
        for (const auto &target : targets) {
            const auto &kinds = target.at("kind");
            if (!kinds.is_array() || kinds.size() > 32)
                throw std::runtime_error("Invalid Cargo target kinds");
            for (const auto &kind : kinds)
                if (!kind.is_string())
                    throw std::runtime_error("Invalid Cargo target kind");
            if (contains(kinds, "custom-build"))
                continue;
            if (model["targets"].size() >= 2000)
                throw std::runtime_error("Cargo target limit exceeded");
            const auto required = target.value("required-features", Json::array());
            if (!required.is_array() || required.size() > 2000)
                throw std::runtime_error("Invalid Cargo required features");
            for (const auto &feature : required)
                if (!feature.is_string() || feature.get_ref<const std::string &>().size() > 256)
                    throw std::runtime_error("Invalid Cargo required feature");
            model["targets"].push_back(
                {{"name", text(target, "name")},
                 {"package", name},
                 {"packageId", id},
                 {"kind", kinds},
                 {"path", pathUtf8(resolve(text(target, "src_path", 8192)).lexically_relative(workspace))},
                 {"requiredFeatures", required}});
        }
    }
    if (ids.size() != metadata.at("workspace_members").size())
        throw std::runtime_error("Cargo workspace members are incomplete");
    return model;
}
Json cargoExecutable(const Json &message, const Json &model, const Json &profile,
                     const std::filesystem::path &workspace, const CargoResolver &resolve) {
    if (!message.is_object() || message.value("reason", "") != "compiler-artifact" ||
        !message.value("executable", Json()).is_string() ||
        message.value("profile", Json::object()).value("test", false))
        return nullptr;
    const auto &target = message.at("target");
    const auto kind = contains(target.at("kind"), "bin")       ? "bin"
                      : contains(target.at("kind"), "example") ? "example"
                                                               : "";
    if (!runnable(kind) || targetKind(profile) != kind)
        return nullptr;
    const auto packageId = text(message, "package_id", 8192), name = text(target, "name");
    if (!text(profile, "target").empty() && name != text(profile, "target"))
        return nullptr;
    for (const auto &known : model.at("targets")) {
        if (known.at("packageId") != packageId || known.at("name") != name ||
            !contains(known.at("kind"), kind))
            continue;
        const auto file = resolve(text(message, "executable", 8192));
        if (!std::filesystem::is_regular_file(file))
            throw std::runtime_error("Cargo executable artifact is missing");
        return {{"name", name},
                {"package", known.at("package")},
                {"packageId", packageId},
                {"targetKind", kind},
                {"path", pathUtf8(file.lexically_relative(workspace))},
                {"kind", "native"},
                {"mainClass", ""}};
    }
    return nullptr;
}
} // namespace cycode
