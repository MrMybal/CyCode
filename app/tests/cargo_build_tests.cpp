#include "cycode/CargoBuild.h"
#include "cycode/Paths.h"
#include "cycode/RustToolchain.h"
#include <algorithm>
#include <fstream>
#include <stdexcept>
namespace {
using Json = nlohmann::json;
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f) {
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, "Invalid Cargo contract accepted");
}
bool has(const std::vector<std::string> &args, const std::string &value) {
    return std::find(args.begin(), args.end(), value) != args.end();
}
} // namespace
// Part of the existing project-task test target. Does not invoke Cargo.
void cargoBuildContracts(const std::filesystem::path &fixture) {
    namespace fs = std::filesystem;
    using namespace cycode;
    for (const auto &name : std::vector<std::string>{"", "stable", "nightly-2026-10-01",
                                                     "1.93.0-x86_64-pc-windows-msvc", "my_local-rust.1"})
        validateRustToolchain(name);
    for (const auto &name :
         std::vector<std::string>{"--install", "+nightly", "../toolchain", "C:/rust", "a b", "a/b", "a\\b",
                                  "a:stream", std::string("bad\0name", 8), std::string(129, 'x')})
        rejects([&] { validateRustToolchain(name); });
    const std::vector<std::string> cargoCommand = {"cargo", "metadata", "--no-deps"};
    require(rustToolchainCommand("", "", cargoCommand) == cargoCommand, "Automatic Cargo invocation changed");
    const auto named = rustToolchainCommand("nightly-2026-10-01", "rustup-fixture", cargoCommand);
    require(named == std::vector<std::string>({"rustup-fixture", "run", "nightly-2026-10-01", "--", "cargo",
                                               "metadata", "--no-deps"}),
            "Named Cargo invocation not wrapped literally");
    require(!has(named, "--install"), "Toolchain plan requested installation");
    require(rustToolchainCommand("stable", "rustup-fixture", {"--install"}).at(3) == "--",
            "Command option escaped Rustup argument separator");
    rejects([&] { rustToolchainCommand("stable", "", cargoCommand); });
    require(rustToolchainEnvironment("").empty() && rustToolchainEnvironment("stable").at("RUSTC") == "rustc",
            "Toolchain environment not isolated from absolute compiler overrides");
    const auto root = fixture / utf8Path("cargo contracts été &");
    fs::create_directories(root / "member");
    const auto resolve = [&](const std::string &value) {
        auto path = utf8Path(value);
        if (!path.is_absolute())
            path = root / path;
        path = fs::weakly_canonical(path);
        const auto relative = path.lexically_relative(fs::canonical(root));
        if (relative.empty() || relative.is_absolute())
            throw std::runtime_error("Outside Cargo fixture");
        for (const auto &part : relative)
            if (part == "..")
                throw std::runtime_error("Outside Cargo fixture");
        return path;
    };
    Json profile = {{"configuration", "Release"},
                    {"cargoWorkspace", true},
                    {"cargoFeatures", {"member/fast", "logging"}},
                    {"cargoAllFeatures", true},
                    {"cargoNoDefaultFeatures", true},
                    {"cargoLocked", true},
                    {"offline", true}};
    auto args = cargoArguments(profile, "project.build", root / "Cargo.toml");
    require(args.front() == "build" && has(args, "--workspace") && has(args, "--release") &&
                !has(args, "--bin"),
            "Cargo workspace/default target flags lost");
    require(has(args, "--features") && has(args, "member/fast") && has(args, "--all-features") &&
                has(args, "--no-default-features") && has(args, "--locked") && has(args, "--offline"),
            "Cargo feature/lock options lost");
    profile["cargoProfile"] = "distribution";
    profile["cargoTargetKind"] = "example";
    profile["target"] = "viewer";
    args = cargoArguments(profile, "project.debug", root / "Cargo.toml");
    require(has(args, "--profile") && has(args, "distribution") && !has(args, "--release") &&
                has(args, "--example") && has(args, "viewer"),
            "Cargo custom profile/example arguments incorrect");
    args = cargoArguments(profile, "project.configure", root / "Cargo.toml");
    require(args.front() == "metadata" && has(args, "--no-deps") && !has(args, "--workspace") &&
                !has(args, "--profile") && !has(args, "--example") && has(args, "--locked") &&
                has(args, "member/fast"),
            "Cargo metadata received build-only flags or lost features");
    profile["cargoTargetKind"] = "lib";
    require(has(cargoArguments(profile, "project.test", root / "Cargo.toml"), "--lib"),
            "Cargo library tests not selected");
    rejects([&] { cargoArguments(profile, "project.launch", root / "Cargo.toml"); });
    profile["target"] = "";
    profile["cargoTargetKind"] = "bench";
    require(has(cargoArguments(profile, "project.build", root / "Cargo.toml"), "--benches"),
            "Cargo unnamed benchmarks flag incorrect");
    for (auto patch : Json::array({{{"cargoWorkspace", "yes"}},
                                   {{"cargoTargetKind", "--all-targets"}},
                                   {{"cargoFeatures", {"a", "a"}}},
                                   {{"cargoFeatures", {"--config=x"}}},
                                   {{"cargoFeatures", {"a,b"}}},
                                   {{"cargoFeatures", {"line\nbreak"}}},
                                   {{"cargoProfile", "../escape"}},
                                   {{"package", "member"}}})) {
        auto invalid = profile;
        invalid.update(patch);
        rejects([&] { validateCargoOptions(invalid); });
    }
    auto invalid = profile;
    invalid["cargoFeatures"] = Json::array();
    for (int i = 0; i < 129; ++i)
        invalid["cargoFeatures"].push_back("f" + std::to_string(i));
    rejects([&] { validateCargoOptions(invalid); });
    invalid["cargoFeatures"] = {std::string("nul\0feature", 11)};
    rejects([&] { validateCargoOptions(invalid); });
    const auto target = [&](const char *kind, const char *name) {
        return Json{{"kind", {kind}},
                    {"name", name},
                    {"src_path", pathUtf8(root / "member" / "source.rs")},
                    {"required-features", {"fast"}}};
    };
    Json metadata = {
        {"version", 1},
        {"workspace_root", pathUtf8(root)},
        {"workspace_members", {"opaque:first", "opaque:second"}},
        {"workspace_default_members", {"opaque:first"}},
        {"packages", Json::array({{{"id", "opaque:first"},
                                   {"name", "member"},
                                   {"manifest_path", pathUtf8(root / "Cargo.toml")},
                                   {"features", {{"default", {"fast"}}, {"fast", Json::array()}}},
                                   {"targets",
                                    {target("bin", "same"), target("example", "same"),
                                     target("custom-build", "script")}}},
                                  {{"id", "opaque:second"},
                                   {"name", "other"},
                                   {"manifest_path", pathUtf8(root / "member" / "Cargo.toml")},
                                   {"features", Json::object()},
                                   {"targets", {target("bin", "same")}}},
                                  {{"id", "dependency"}, {"name", "third-party"}}})}};
    const auto model = cargoWorkspaceModel(metadata, root, resolve);
    require(model.at("packages").size() == 2 && model.at("targets").size() == 3,
            "Cargo dependency/custom-build leaked into workspace catalogue");
    require(model.at("packages")[0].at("defaultMember") == true &&
                model.at("packages")[1].at("defaultMember") == false,
            "Cargo default members lost");
    require(model.at("targets")[0].at("packageId") == "opaque:first" &&
                model.at("targets")[0].at("requiredFeatures") == Json::array({"fast"}),
            "Cargo package identity/required features lost");
    const auto output = root / "artifact.fixture";
    {
        std::ofstream stream(output);
        stream << "ordinary fixture text";
    }
    Json artifact = {{"reason", "compiler-artifact"},
                     {"package_id", "opaque:first"},
                     {"target", target("example", "same")},
                     {"profile", {{"test", false}}},
                     {"executable", pathUtf8(output)}};
    Json selection = {{"cargoTargetKind", "example"}, {"target", "same"}};
    auto executable = cargoExecutable(artifact, model, selection, root, resolve);
    require(executable.at("package") == "member" && executable.at("targetKind") == "example",
            "Cargo executable identity lost");
    artifact["package_id"] = "dependency";
    require(cargoExecutable(artifact, model, selection, root, resolve).is_null(),
            "Dependency executable accepted");
    artifact["package_id"] = "opaque:first";
    artifact["profile"]["test"] = true;
    require(cargoExecutable(artifact, model, selection, root, resolve).is_null(),
            "Test harness accepted as application");
    artifact["profile"]["test"] = false;
    selection["cargoTargetKind"] = "bin";
    require(cargoExecutable(artifact, model, selection, root, resolve).is_null(),
            "Example selected as same-named binary");
    selection["cargoTargetKind"] = "example";
    artifact["executable"] = pathUtf8(root.parent_path() / "outside");
    rejects([&] { cargoExecutable(artifact, model, selection, root, resolve); });
    auto outside = metadata;
    outside["workspace_root"] = pathUtf8(root.parent_path());
    rejects([&] { cargoWorkspaceModel(outside, root, resolve); });
    auto incomplete = metadata;
    incomplete["packages"].erase(1);
    rejects([&] { cargoWorkspaceModel(incomplete, root, resolve); });
}
