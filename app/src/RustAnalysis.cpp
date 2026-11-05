#include "cycode/RustAnalysis.h"
#include "cycode/CargoBuild.h"
#include "cycode/Paths.h"
#include "cycode/RustToolchain.h"
#include <algorithm>
#include <stdexcept>
namespace cycode {
namespace {
using Json = nlohmann::json;
std::filesystem::path manifestPath(const Json &profile, const std::filesystem::path &workspace) {
    const auto root = std::filesystem::canonical(workspace);
    const auto manifest = std::filesystem::weakly_canonical(root / utf8Path(profile.at("manifest")));
    const auto relative = manifest.lexically_relative(root);
    if (relative.empty() || relative.is_absolute())
        throw std::runtime_error("Rust analysis manifest is outside workspace");
    for (const auto &part : relative)
        if (part == "..")
            throw std::runtime_error("Rust analysis manifest is outside workspace");
    if (!std::filesystem::is_regular_file(manifest))
        throw std::runtime_error("Rust analysis Cargo.toml is missing");
    return manifest;
}
void object(Json &parent, const char *key) {
    if (!parent.contains(key))
        parent[key] = Json::object();
    if (!parent.at(key).is_object())
        throw std::runtime_error(std::string("Rust analysis settings require an object: ") + key);
}
void apply(Json &options, const Json &profile, const std::string &manifest) {
    if (!options.is_object())
        throw std::runtime_error("Rust analysis options must be an object");
    object(options, "cargo");
    auto &cargo = options["cargo"];
    cargo["features"] = profile.at("allFeatures").get<bool>() ? Json("all") : profile.at("features");
    cargo["noDefaultFeatures"] = profile.at("noDefaultFeatures");
    // The typed profile owns resolution flags; other trusted extra args survive.
    const auto previous = cargo.value("extraArgs", Json::array());
    if (!previous.is_array() || previous.size() > 128)
        throw std::runtime_error("Invalid Rust cargo.extraArgs");
    Json args = Json::array();
    for (const auto &arg : previous) {
        if (!arg.is_string() || arg.get_ref<const std::string &>().size() > 8192 ||
            arg.get_ref<const std::string &>().find('\0') != std::string::npos)
            throw std::runtime_error("Invalid Rust cargo.extraArgs item");
        if (arg != "--offline" && arg != "--locked" && arg != "--frozen")
            args.push_back(arg);
    }
    if (profile.at("offline").get<bool>())
        args.push_back("--offline");
    if (profile.at("locked").get<bool>())
        args.push_back("--locked");
    if (args.size() > 128)
        throw std::runtime_error("Rust cargo.extraArgs limit exceeded");
    cargo["extraArgs"] = std::move(args);
    const auto toolchain = profile.value("toolchain", std::string{});
    if (!toolchain.empty()) {
        // Do not retain CyCode's private fallback sysroot for another toolchain.
        cargo["sysroot"] = "discover";
        object(cargo, "extraEnv");
        cargo["extraEnv"].update(rustToolchainEnvironment(toolchain));
        object(options, "check");
        object(options["check"], "extraEnv");
        for (const auto *key : {"RUSTUP_TOOLCHAIN", "CARGO", "RUSTC"})
            options["check"]["extraEnv"].erase(key);
    }
    options["linkedProjects"] = Json::array({manifest});
    // Never implicitly opt a custom minimal launch into executing project rules.
    object(cargo, "buildScripts");
    if (!cargo["buildScripts"].contains("enable"))
        cargo["buildScripts"]["enable"] = false;
    object(options, "procMacro");
    if (!options["procMacro"].contains("enable"))
        options["procMacro"]["enable"] = false;
    if (!options.contains("checkOnSave"))
        options["checkOnSave"] = false;
    // Explicit per-check overrides would otherwise diverge from the selected features.
    object(options, "check");
    options["check"]["features"] = nullptr;
    options["check"]["noDefaultFeatures"] = nullptr;
}
} // namespace
Json normalizeRustAnalysisProfile(const Json &value) {
    if (!value.is_object() || value.dump().size() > 49152)
        throw std::runtime_error("Invalid Rust analysis profile");
    for (const auto &[key, _] : value.items())
        if (key != "manifest" && key != "features" && key != "allFeatures" && key != "noDefaultFeatures" &&
            key != "offline" && key != "locked" && key != "toolchain")
            throw std::runtime_error("Unknown Rust analysis profile field");
    const auto name = value.at("manifest").get<std::string>();
    if (name.empty() || name.size() > 8192 ||
        std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
        throw std::runtime_error("Invalid Rust analysis manifest");
    const auto path = utf8Path(name);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory() ||
        path.filename() != "Cargo.toml" || name.find('\\') != std::string::npos ||
        name.find(':') != std::string::npos)
        throw std::runtime_error("Rust analysis requires a workspace-relative Cargo.toml path");
    for (const auto &part : path)
        if (part == "..")
            throw std::runtime_error("Rust analysis manifest escapes workspace");
    Json result = {{"manifest", pathUtf8(path.lexically_normal())},
                   {"features", value.value("features", Json::array())},
                   {"allFeatures", value.value("allFeatures", Json(false))},
                   {"noDefaultFeatures", value.value("noDefaultFeatures", Json(false))},
                   {"offline", value.value("offline", Json(false))},
                   {"locked", value.value("locked", Json(false))}};
    validateCargoOptions({{"cargoFeatures", result.at("features")},
                          {"cargoAllFeatures", result.at("allFeatures")},
                          {"cargoNoDefaultFeatures", result.at("noDefaultFeatures")},
                          {"cargoLocked", result.at("locked")}});
    if (!result.at("offline").is_boolean())
        throw std::runtime_error("Invalid Rust offline flag");
    const auto toolchain = value.value("toolchain", std::string{});
    validateRustToolchain(toolchain);
    if (!toolchain.empty())
        result["toolchain"] = toolchain;
    return result;
}
Json cargoRustAnalysisProfile(const Json &project, const std::filesystem::path &workspace) {
    validateCargoOptions(project);
    auto result = normalizeRustAnalysisProfile(
        {{"manifest", pathUtf8(utf8Path(project.value("directory", std::string("."))) / "Cargo.toml")},
         {"features", project.value("cargoFeatures", Json::array())},
         {"allFeatures", project.value("cargoAllFeatures", false)},
         {"noDefaultFeatures", project.value("cargoNoDefaultFeatures", false)},
         {"offline", project.value("offline", false)},
         {"locked", project.value("cargoLocked", false)},
         {"toolchain", project.value("cargoToolchain", std::string{})}});
    (void)manifestPath(result, workspace);
    return result;
}
Json rustAnalysisSettings(const Json &settings, const Json &value, const std::filesystem::path &workspace) {
    if (!settings.is_object() || settings.dump().size() > 65536)
        throw std::runtime_error("Invalid Rust analysis settings");
    const auto profile = normalizeRustAnalysisProfile(value);
    const auto manifest = pathUtf8(manifestPath(profile, workspace));
    auto result = settings;
    Json base = settings;
    for (const auto *key : {"$initializationOptions", "$workspaceReadyNotification"})
        base.erase(key);
    Json configuration = settings.value("rust-analyzer", base);
    Json initialization = settings.value("$initializationOptions", configuration);
    apply(configuration, profile, manifest);
    apply(initialization, profile, manifest);
    // Keep supported flat Rust settings consistent when a user supplied that form.
    if (!settings.contains("rust-analyzer")) {
        result["cargo"] = configuration.at("cargo");
        result["linkedProjects"] = configuration.at("linkedProjects");
        result["check"] = configuration.at("check");
        result["procMacro"] = configuration.at("procMacro");
        result["checkOnSave"] = configuration.at("checkOnSave");
    }
    result["rust-analyzer"] = std::move(configuration);
    result["$initializationOptions"] = std::move(initialization);
    if (result.dump().size() > 65536)
        throw std::runtime_error("Resolved Rust analysis settings exceed 64 KiB");
    return result;
}
std::vector<std::string> rustAnalysisArguments(const Json &value, const std::vector<std::string> &requested,
                                               const std::vector<std::string> &defaults,
                                               const std::string &rustup) {
    const auto profile = normalizeRustAnalysisProfile(value);
    const auto toolchain = profile.value("toolchain", std::string{});
    if (toolchain.empty())
        return requested.empty() ? defaults : requested;
    // Ask Rustup for its analyzer component in the selected toolchain context.
    // Explicit user argv can still select another compatible server binary.
    return rustToolchainCommand(toolchain, rustup,
                                requested.empty() ? std::vector<std::string>{"rust-analyzer"} : requested);
}
} // namespace cycode
