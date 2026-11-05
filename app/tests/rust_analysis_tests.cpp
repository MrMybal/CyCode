#include "cycode/Engine.h"
#include "cycode/Paths.h"
#include "cycode/RustAnalysis.h"
#include "cycode/RustToolchain.h"
#include <fstream>
#include <stdexcept>
namespace {
using Json = nlohmann::json;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "Invalid Rust analysis configuration accepted");
}
} // namespace
// Joins the existing lifecycle target. No Cargo, rust-analyzer or fixture executable.
void rustAnalysisContracts(const std::filesystem::path &fixture) {
    using namespace cycode;
    namespace fs = std::filesystem;
    const auto root = fixture / utf8Path("rust analysis été");
    fs::create_directories(root / "member");
    {
        std::ofstream out(root / "member/Cargo.toml");
        out << "[package]\nname=\"member\"\nversion=\"0.1.0\"\n";
    }
    const auto profile = cargoRustAnalysisProfile({{"directory", "member"},
                                                   {"cargoFeatures", {"member/gpu", "tracing"}},
                                                   {"cargoNoDefaultFeatures", true},
                                                   {"cargoLocked", true},
                                                   {"offline", true}},
                                                  root);
    require(profile.at("manifest") == "member/Cargo.toml" && profile.at("features").size() == 2,
            "Project-to-analysis selection lost");
    Json original = {{"rust-analyzer",
                      {{"cargo", {{"sysroot", "keep-sysroot"}, {"extraArgs", {"--frozen", "--quiet"}}}},
                       {"completion", {{"limit", 55}}}}},
                     {"$initializationOptions",
                      {{"cargo", {{"sysroot", "initial-sysroot"}}}, {"diagnostics", {{"enable", false}}}}},
                     {"$workspaceReadyNotification", "experimental/serverStatus"}};
    const auto baseline = original;
    auto resolved = rustAnalysisSettings(original, profile, root);
    for (const char *key : {"rust-analyzer", "$initializationOptions"}) {
        const auto &options = resolved.at(key);
        require(options.at("cargo").at("features") == profile.at("features"),
                "Initialization/configuration feature values diverged");
        require(options.at("cargo").at("noDefaultFeatures") == true, "Default features lost");
        require(options.at("linkedProjects") ==
                    Json::array({pathUtf8(fs::canonical(root / "member/Cargo.toml"))}),
                "Linked Cargo manifest incorrect");
        require(options.at("cargo").at("buildScripts").at("enable") == false &&
                    options.at("procMacro").at("enable") == false && options.at("checkOnSave") == false,
                "Minimal custom settings enabled project rules");
        require(options.at("check").at("features").is_null() &&
                    options.at("check").at("noDefaultFeatures").is_null(),
                "Check features do not inherit Cargo features");
    }
    require(original == baseline, "Typed overlay modified base settings");
    auto named = profile;
    named["toolchain"] = "nightly-2026-10-01";
    const auto namedSettings = rustAnalysisSettings(original, named, root);
    for (const auto *key : {"rust-analyzer", "$initializationOptions"}) {
        require(namedSettings.at(key).at("cargo").at("sysroot") == "discover",
                "Private/default sysroot retained for named toolchain");
        require(namedSettings.at(key).at("cargo").at("extraEnv").at("RUSTUP_TOOLCHAIN") ==
                        named.at("toolchain") &&
                    namedSettings.at(key).at("cargo").at("extraEnv").at("RUSTC") == "rustc",
                "Analyzer cargo/rustc toolchain selection diverged");
    }
    require(rustAnalysisArguments(profile, {}, {"bundled-analyzer"}, "") ==
                std::vector<std::string>{"bundled-analyzer"},
            "Automatic analyzer default changed");
    require(
        rustAnalysisArguments(named, {}, {"bundled-analyzer"}, "rustup-fixture") ==
            std::vector<std::string>({"rustup-fixture", "run", "nightly-2026-10-01", "--", "rust-analyzer"}),
        "Named toolchain did not select its analyzer component");
    require(rustAnalysisArguments(named, {"custom-server & é", "--stdio"}, {}, "rustup-fixture") ==
                std::vector<std::string>(
                    {"rustup-fixture", "run", "nightly-2026-10-01", "--", "custom-server & é", "--stdio"}),
            "Explicit analyzer argv lost or shell-expanded");
    rejects([&] { rustAnalysisArguments(named, {}, {}, ""); });
    auto automatic = named;
    automatic["toolchain"] = "";
    require(normalizeRustAnalysisProfile(automatic) == profile,
            "Empty toolchain did not restore legacy analysis shape");
    auto invalidToolchain = named;
    invalidToolchain["toolchain"] = "--install";
    rejects([&] { normalizeRustAnalysisProfile(invalidToolchain); });
    require(resolved.at("rust-analyzer").at("cargo").at("extraArgs") ==
                Json::array({"--quiet", "--offline", "--locked"}),
            "Resolution flags duplicated or trusted extras lost");
    require(resolved.at("rust-analyzer").at("cargo").at("sysroot") == "keep-sysroot" &&
                resolved.at("$initializationOptions").at("cargo").at("sysroot") == "initial-sysroot",
            "Independent server settings lost");
    auto all = profile;
    all["allFeatures"] = true;
    all["offline"] = false;
    all["locked"] = false;
    resolved = rustAnalysisSettings(original, all, root);
    require(resolved.at("rust-analyzer").at("cargo").at("features") == "all" &&
                resolved.at("rust-analyzer").at("cargo").at("extraArgs") == Json::array({"--quiet"}),
            "All-features or resolution disable incorrect");
    const Json trusted = {{"cargo", {{"buildScripts", {{"enable", true}}}}},
                          {"procMacro", {{"enable", true}}},
                          {"checkOnSave", true}};
    resolved = rustAnalysisSettings(trusted, profile, root);
    require(resolved.at("$initializationOptions").at("cargo").at("buildScripts").at("enable") == true &&
                resolved.at("rust-analyzer").at("procMacro").at("enable") == true,
            "Explicit trusted rule settings overridden");
    require(resolved.at("cargo").at("features") == profile.at("features"),
            "Flat Cargo settings remained stale");
    for (const auto &invalid :
         Json::array({{{"manifest", "../Cargo.toml"}},
                      {{"manifest", "/outside/Cargo.toml"}},
                      {{"manifest", "member/rust-project.json"}},
                      {{"manifest", "member\\Cargo.toml"}},
                      {{"manifest", "C:/project/Cargo.toml"}},
                      {{"manifest", "member/Cargo.toml"}, {"command", "cargo"}},
                      {{"manifest", "member/Cargo.toml"}, {"features", {"duplicate", "duplicate"}}},
                      {{"manifest", "member/Cargo.toml"}, {"locked", "yes"}}}))
        rejects([&] { normalizeRustAnalysisProfile(invalid); });
    auto missing = profile;
    missing["manifest"] = "absent/Cargo.toml";
    rejects([&] { rustAnalysisSettings(original, missing, root); });
    rejects([&] { rustAnalysisSettings({{"rust-analyzer", 42}}, profile, root); });
    rejects([&] { rustAnalysisSettings({{"cargo", {{"extraArgs", {"bad", 1}}}}}, profile, root); });
    rejects([&] { rustAnalysisSettings({{"large", std::string(50000, 'x')}}, profile, root); });
    std::error_code ec;
    fs::create_directory_symlink(fixture, root / "outside", ec);
    if (!ec) {
        auto escape = profile;
        escape["manifest"] = "outside/Cargo.toml";
        rejects([&] { rustAnalysisSettings(original, escape, root); });
    }
    const auto storage = fixture / "rust-preferences";
    {
        Engine engine(root, storage);
        engine.configureLanguage("", false, false);
        const auto inspected =
            engine.dispatch("project.inspect", {{"system", "cargo"},
                                                {"directory", "member"},
                                                {"cargoFeatures", {"member/gpu", "tracing"}},
                                                {"cargoNoDefaultFeatures", true},
                                                {"cargoLocked", true},
                                                {"offline", true}});
        require(inspected.at("rustAnalysis") == profile,
                "Inspect and native Cargo analysis mapping diverged");
        require(!engine.languageStatus().at("servers").at("rust").at("enabled").get<bool>(),
                "Read-only inspection enabled Rust");
        engine.dispatch("language.configure", {{"language", "rust"},
                                               {"enabled", false},
                                               {"cargoProject", profile},
                                               {"save", true},
                                               {"expectedPreferencesRevision", 0}});
        auto status = engine.languageStatus().at("servers").at("rust");
        require(!status.at("enabled").get<bool>() && status.at("configuration").at("arguments").empty() &&
                    status.at("configuration").at("settings").empty(),
                "Typed profile enabled Rust or serialized resolved launch data");
        require(status.at("configuration").at("cargoProject") == profile, "Raw Cargo analysis profile lost");
        const auto before = status.at("configuration");
        rejects([&] {
            engine.dispatch("language.configure", {{"language", "rust"},
                                                   {"enabled", false},
                                                   {"cargoProject", all},
                                                   {"save", true},
                                                   {"expectedPreferencesRevision", 0}});
        });
        require(engine.languageStatus().at("servers").at("rust").at("configuration") == before,
                "Stale save replaced running configuration");
        rejects([&] {
            engine.dispatch("language.configure",
                            {{"language", "python"}, {"enabled", false}, {"cargoProject", profile}});
        });
        engine.dispatch("language.configure",
                        {{"language", "rust"},
                         {"enabled", false},
                         {"arguments", {"not-run-rust-server"}},
                         {"settings", {{"rust-analyzer", {{"completion", {{"limit", 20}}}}}}}});
        require(engine.languageStatus().at("servers").at("rust").at("configuration").contains("cargoProject"),
                "Editing raw settings silently removed Cargo profile");
        engine.dispatch("language.configure",
                        {{"language", "rust"}, {"enabled", false}, {"cargoProject", nullptr}});
        status = engine.languageStatus().at("servers").at("rust");
        require(!status.at("configuration").contains("cargoProject") &&
                    status.at("settings") == status.at("configuration").at("settings"),
                "Clearing override left derived Rust options");
        require(engine.dispatch("language.preferences").at("profiles").at("rust").at("cargoProject") ==
                    profile,
                "Session-only clearing modified saved preference");
        fs::rename(root / "member/Cargo.toml", root / "member/manifest.fixture");
        engine.dispatch("language.configure",
                        {{"language", "rust"}, {"enabled", false}, {"cargoProject", profile}});
        require(!engine.languageStatus().at("servers").at("rust").at("enabled").get<bool>(),
                "Missing manifest prevented disabling Rust");
        rejects([&] {
            engine.dispatch("language.configure", {{"language", "rust"},
                                                   {"enabled", true},
                                                   {"save", true},
                                                   {"cargoProject", profile},
                                                   {"expectedPreferencesRevision", 1}});
        });
        require(engine.dispatch("language.preferences").at("revision") == 1,
                "Failed path preflight saved a preference");
        fs::rename(root / "member/manifest.fixture", root / "member/Cargo.toml");
    }
    Engine reopened(root, storage);
    reopened.configureLanguage("", false, false);
    require(!reopened.languageStatus().at("servers").at("rust").at("enabled").get<bool>(),
            "Reading saved Cargo preference activated Rust");
    reopened.dispatch("language.configure", {{"language", "rust"}, {"enabled", false}});
    require(reopened.languageStatus().at("servers").at("rust").at("configuration").at("cargoProject") ==
                profile,
            "Reopening lost saved Cargo analysis selection");
    Engine namedEngine(root, fixture / "named-rust-preferences");
    namedEngine.configureLanguage("", false, false);
    namedEngine.dispatch("language.configure", {{"language", "rust"},
                                                {"enabled", false},
                                                {"cargoProject", named},
                                                {"save", true},
                                                {"expectedPreferencesRevision", 0}});
    const auto namedSaved = namedEngine.dispatch("language.preferences").at("profiles").at("rust");
    require(namedSaved.at("cargoProject").at("toolchain") == named.at("toolchain") &&
                namedSaved.at("arguments").empty(),
            "Named toolchain saved a resolved wrapper or lost its symbolic selection");
    require(!namedEngine.languageStatus().at("servers").at("rust").at("enabled").get<bool>(),
            "Saving a disabled named toolchain enabled Rust");
}
