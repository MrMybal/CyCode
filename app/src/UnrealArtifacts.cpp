#include "cycode/Process.h"
#include "cycode/Recovery.h"
#include "cycode/UnrealLaunch.h"
#include "cycode/UnrealProject.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <regex>
#include <set>
namespace cycode {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
bool beneath(const fs::path &path, const fs::path &root) {
    auto p = pathUtf8(path.lexically_normal()), r = pathUtf8(root.lexically_normal());
#ifdef _WIN32
    std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return std::tolower(c); });
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c) { return std::tolower(c); });
#endif
    if (!r.ends_with('/'))
        r += '/';
    return p == r.substr(0, r.size() - 1) || p.starts_with(r);
}
struct Inspection {
    Json result = {{"supported", true},
                   {"complete", true},
                   {"ready", false},
                   {"receiptRequired", false},
                   {"receipt", Json::object()},
                   {"engineManifest", Json::object()},
                   {"products", Json::array()},
                   {"manifests", Json::array()},
                   {"modules", Json::array()},
                   {"issues", Json::array()},
                   {"scope", "projectEditorOutputs"}};
    fs::path engine, project;
    fs::path reading;
    std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    size_t bytes = 0, metadata = 0, entries = 0;
    std::set<std::string> manifests;
    void tick() const {
        if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error("Artifact inspection exceeded its cooperative five-second budget");
    }
    void issue(const std::string &code, const fs::path &path, const std::string &detail) {
        if (result.at("issues").size() >= 128) {
            result["issues"].back() = {
                {"code", "inspectionLimit"},
                {"path", ""},
                {"detail", "128-issue limit reached; remaining outputs were not inspected"}};
            throw std::runtime_error("Artifact issue limit exceeded");
        }
        auto bounded = [](const std::string &input, size_t limit) {
            auto value = Json::parse(Json(input).dump(-1, ' ', false, Json::error_handler_t::replace))
                             .get<std::string>();
            if (value.size() > limit) {
                size_t cut = limit;
                while (cut > 0 && (static_cast<unsigned char>(value[cut]) & 0xc0) == 0x80)
                    --cut;
                value.resize(cut);
            }
            return value;
        };
        result["issues"].push_back(
            {{"code", code}, {"path", bounded(pathUtf8(path), 8192)}, {"detail", bounded(detail, 16384)}});
    }
    void entry() {
        tick();
        if (++entries > 4096)
            throw std::runtime_error("Artifact entry limit exceeded");
    }
    fs::path checked(const fs::path &input) const {
        tick();
        const auto path = fs::absolute(input).lexically_normal();
        if (pathUtf8(path).size() > 4096 || (!beneath(path, engine) && !beneath(path, project)))
            throw std::runtime_error("Artifact path is outside the selected engine/project");
        const auto canonical = fs::weakly_canonical(path);
        if (!beneath(canonical, engine) && !beneath(canonical, project))
            throw std::runtime_error("Artifact path redirects outside the selected engine/project");
        return canonical;
    }
    Json read(const fs::path &input) {
        reading = input;
        tick();
        const auto path = checked(input);
        if (++metadata > 256)
            throw std::runtime_error("Artifact metadata file limit exceeded");
        const auto size = fs::file_size(path);
        if (size > 4 * 1024 * 1024 || (bytes += static_cast<size_t>(size)) > 16 * 1024 * 1024)
            throw std::runtime_error("Artifact metadata byte limit exceeded");
        const auto modified = fs::last_write_time(path);
        std::ifstream file(path, std::ios::binary);
        std::string text(static_cast<size_t>(size) + 1, '\0');
        file.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (file.bad() || file.gcount() != static_cast<std::streamsize>(size) ||
            fs::file_size(path) != size || fs::last_write_time(path) != modified)
            throw std::runtime_error("Artifact metadata changed during inspection");
        text.resize(static_cast<size_t>(size));
        return Json::parse(text);
    }
    std::string text(const Json &object, const char *key) const {
        auto value = object.at(key).get<std::string>();
        if (value.empty() || value.size() > 4096 ||
            std::any_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; }))
            throw std::runtime_error("Invalid artifact metadata text");
        return value;
    }
    fs::path expand(const std::string &value, const fs::path &relative) const {
        std::string expanded = value;
        if (expanded.starts_with("$(EngineDir)/"))
            expanded = pathUtf8(engine / "Engine") + expanded.substr(12);
        else if (expanded.starts_with("$(ProjectDir)/"))
            expanded = pathUtf8(project) + expanded.substr(13);
        if (expanded.find('$') != std::string::npos)
            throw std::runtime_error("Unknown Unreal receipt path variable");
        const auto p = utf8Path(expanded);
        return checked(p.is_absolute() ? p : relative / p);
    }
    bool present(const fs::path &path) const {
        tick();
        std::error_code ec;
        return fs::is_regular_file(path, ec) && fs::file_size(path, ec) > 0 && !ec;
    }
    void manifest(const fs::path &input, const std::string &expectedId, bool required) {
        const auto path = checked(input);
        if (!manifests.insert(pathUtf8(path)).second)
            return;
        if (!fs::is_regular_file(path)) {
            if (required)
                issue("missingManifest", path, "Project module manifest is missing");
            return;
        }
        entry();
        auto data = read(path);
        const auto id = text(data, "BuildId");
        const auto &modules = data.at("Modules");
        if (!modules.is_object() || modules.size() > 4096)
            throw std::runtime_error("Invalid or oversized project module map");
        result["manifests"].push_back({{"path", pathUtf8(path)},
                                       {"buildId", id},
                                       {"moduleCount", modules.size()},
                                       {"matchesEngine", id == expectedId}});
        if (id != expectedId)
            issue("buildIdMismatch", path, "Project manifest BuildId differs from the selected engine");
        for (auto it = modules.begin(); it != modules.end(); ++it) {
            entry();
            if (it.key().empty() || it.key().size() > 256)
                throw std::runtime_error("Invalid Unreal module name");
            const auto name = it.value().get<std::string>();
            if (name.empty() || name.size() > 512 || name.find_first_of("/\\:") != std::string::npos ||
                name == "." || name == ".." ||
                std::any_of(name.begin(), name.end(), [](unsigned char c) { return c < 32 || c == 127; }))
                throw std::runtime_error("Module manifest must contain plain binary filenames");
            const auto file = checked(path.parent_path() / utf8Path(name));
            const bool exists = present(file);
            result["modules"].push_back({{"name", it.key()},
                                         {"path", pathUtf8(file)},
                                         {"manifest", pathUtf8(path)},
                                         {"present", exists}});
            if (!exists)
                issue("missingModule", file, "A manifest-listed project module binary is missing or empty");
        }
    }
};
} // namespace
UnrealProject::Json UnrealProject::artifacts(const Json &params) {
    std::unique_lock lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock())
        throw std::runtime_error("Wait for the current Unreal operation before inspecting outputs");
    return artifactsLocked(inspectLocked(params));
}
UnrealProject::Json UnrealProject::artifactsLocked(const Json &model) {
    Inspection scan;
    try {
        if (!model.value("available", false))
            throw std::runtime_error("Open an Unreal project before inspecting outputs");
        const auto profile = model.at("profile");
        scan.result["profile"] = profile;
        const auto platform = profile.at("platform").get<std::string>(),
                   configuration = profile.at("configuration").get<std::string>(),
                   target = profile.at("target").get<std::string>();
        const int major = model.at("engineVersion").value("MajorVersion", 0);
        if ((major != 4 && major != 5) ||
            !std::set<std::string>{"Win64", "Mac", "Linux", "LinuxArm64"}.contains(platform) ||
            (configuration != "Development" && configuration != "DebugGame") ||
            (!target.empty() && !std::regex_match(target, std::regex("[A-Za-z_][A-Za-z0-9_]*Editor")))) {
            scan.result["supported"] = false;
            scan.issue("unsupported", {},
                       "Inspection supports UE4/UE5 desktop Editor outputs in Development/DebugGame");
        } else {
            const auto project = resolve_(profile.at("project"));
            scan.project = fs::canonical(project.parent_path());
            scan.engine = fs::canonical(utf8Path(profile.at("engineRoot")));
            const auto base = major == 4 ? "UE4Editor" : "UnrealEditor";
            const auto binaryDirectory =
                scan.engine / "Engine/Binaries" / (platform == "LinuxArm64" ? "Linux" : platform);
            const auto editor =
                scan.checked(unrealEditorExecutable(scan.engine, major, platform, configuration));
            if (!scan.present(editor))
                scan.issue("missingEditor", editor,
                           "The selected editor/configuration executable is missing or empty");
            const auto engineManifest = scan.checked(binaryDirectory / (std::string(base) + ".modules"));
            std::string engineId;
            scan.result["engineManifest"] = {{"path", pathUtf8(engineManifest)},
                                             {"present", fs::is_regular_file(engineManifest)}};
            if (fs::is_regular_file(engineManifest)) {
                const auto metadata = scan.read(engineManifest);
                engineId = scan.text(metadata, "BuildId");
                if (!metadata.at("Modules").is_object() || metadata.at("Modules").size() > 8192)
                    throw std::runtime_error("Invalid engine module map");
                scan.result["engineManifest"]["buildId"] = engineId;
            } else
                scan.issue("missingEngineManifest", engineManifest,
                           "The selected engine's shared module manifest is missing");
            bool required = !model.value("declaredModules", Json::array()).empty();
            for (const auto &module : model.value("modules", Json::array()))
                required |= beneath(resolve_(module.at("path")), scan.project / "Source");
            scan.result["receiptRequired"] = required;
            const auto projectBinaries = scan.project / "Binaries" / platform;
            const auto receipt = scan.checked(
                projectBinaries /
                (target + (configuration == "Development" ? "" : "-" + platform + "-" + configuration) +
                 ".target"));
            scan.result["receipt"] = {{"path", target.empty() ? "" : pathUtf8(receipt)},
                                      {"present", !target.empty() && fs::is_regular_file(receipt)}};
            if (!target.empty() && fs::is_regular_file(receipt)) {
                const auto data = scan.read(receipt);
                const auto kind = scan.text(data, "TargetType");
                scan.result["receipt"]["targetType"] = kind;
                if (kind != "Editor" || scan.text(data, "TargetName") != target ||
                    scan.text(data, "Platform") != platform ||
                    scan.text(data, "Configuration") != configuration)
                    scan.issue("receiptContextMismatch", receipt,
                               "Receipt type/target/platform/configuration does not match the selected "
                               "Editor context");
                if (scan.expand(scan.text(data, "Project"), receipt.parent_path()) != fs::canonical(project))
                    scan.issue("receiptProjectMismatch", receipt, "Receipt belongs to a different project");
                const auto id = scan.text(data.at("Version"), "BuildId");
                scan.result["receipt"]["buildId"] = id;
                if (id != engineId)
                    scan.issue("receiptBuildIdMismatch", receipt,
                               "Receipt BuildId differs from the selected engine manifest");
                if (data.contains("Launch") &&
                    scan.expand(scan.text(data, "Launch"), receipt.parent_path()) != editor)
                    scan.issue("receiptLaunchMismatch", receipt,
                               "Receipt launch executable differs from the selected editor configuration");
                const auto &products = data.at("BuildProducts");
                if (!products.is_array() || products.size() > 8192)
                    throw std::runtime_error("Invalid or oversized receipt build product list");
                for (const auto &product : products) {
                    scan.tick();
                    const auto raw = scan.text(product, "Path"), type = scan.text(product, "Type");
                    // Engine products and unrecorded dependencies are deliberately outside this
                    // project-output assessment.
                    if (raw.starts_with("$(EngineDir)/"))
                        continue;
                    const auto path = scan.expand(raw, receipt.parent_path());
                    if (!beneath(path, scan.project))
                        continue;
                    if (path.extension() == ".modules")
                        scan.manifest(path, engineId, true);
                    else if (type == "DynamicLibrary" || type == "Executable") {
                        scan.entry();
                        const bool exists = scan.present(path);
                        scan.result["products"].push_back(
                            {{"path", pathUtf8(path)}, {"type", type}, {"present", exists}});
                        if (!exists)
                            scan.issue("missingProduct", path,
                                       "A receipt-listed executable/library is missing or empty");
                    }
                }
            } else if (required)
                scan.issue("missingReceipt", receipt,
                           "The project Editor build receipt is missing; compile the selected target first");
            const auto moduleName =
                std::string(base) +
                (configuration == "Development" ? "" : "-" + platform + "-" + configuration) + ".modules";
            scan.manifest(projectBinaries / moduleName, engineId, required);
        }
    } catch (const std::exception &e) {
        scan.result["complete"] = false;
        if (scan.result.at("issues").size() < 128)
            scan.issue("inspectionError", scan.reading, e.what());
    }
    scan.result["ready"] = scan.result.at("supported").get<bool>() &&
                           scan.result.at("complete").get<bool>() && scan.result.at("issues").empty();
    scan.result["revision"] = RecoveryStore::fingerprint(scan.result.dump());
    return scan.result;
}
} // namespace cycode
