#include "RecoveryDiagnostic.h"
#include "cycode/NativeTrace.h"
#include "cycode/Paths.h"
#include "cycode/Recovery.h"
#include "cycode/TextFile.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace cycode {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr const char *document = "notes/AgentProbe.md";
constexpr const char *session = "session-00000000000000000000000000000001";
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
void ordinary(const fs::path &path) {
    require(fs::absolute(path).lexically_normal() == fs::weakly_canonical(path),
            "Diagnostic fixture cannot contain redirected paths");
}
std::string fixtureText(const fs::path &root, const char *name) {
    const auto file = root / name;
    ordinary(file);
    require(fs::is_regular_file(file) && fs::file_size(file) <= 4096,
            "Diagnostic fixture must contain a regular file of at most 4 KiB");
    const auto text = readTextFileBytes(file);
    validateEditorText(text);
    return text;
}
bool control(std::string_view expected) {
    char line[32]{};
    if (!std::cin.getline(line, sizeof(line))) {
        if (std::cin.eof() && std::cin.gcount() == 0) return false;
        throw std::runtime_error("Invalid/oversized diagnostic control line");
    }
    require(std::string_view(line) == expected, "Unexpected diagnostic control line");
    return true;
}
void phase(const char *mode, const char *name, const Json &status) {
    std::cout << Json({{"protocol", "cycode-recovery-diagnostic/1"}, {"mode", mode},
                       {"phase", name}, {"status", status}}).dump() << '\n' << std::flush;
}
Json fixtureIndex(const std::string &text, unsigned generation) {
    const auto suffix = std::to_string(generation);
    // Synthetic metadata, matching the real store's schema. No mutable editor model.
    Json record = {{"id", "buffer-1"}, {"path", document}, {"checkpoint", "buffer-1-" + suffix},
        {"revision", generation + 1}, {"updatedAt", uint64_t(1)}, {"bytes", text.size()},
        {"baselineBytes", size_t(0)}, {"textFile", "text-buffer-1-" + suffix + ".utf8"},
        {"baselineFile", "base-buffer-1-1.utf8"}, {"textHash", RecoveryStore::fingerprint(text)},
        {"baselineHash", RecoveryStore::fingerprint("")}, {"bom", false}, {"crlf", false},
        {"crOnly", false}, {"existed", true}};
    return {{"schemaVersion", 1}, {"session", session}, {"createdAt", uint64_t(1)},
            {"buffers", Json::array({record})}};
}
} // namespace

int runOwnedRecoveryDiagnostic(const fs::path &input, bool filesOnly) {
    const auto root = fs::canonical(input);
    ordinary(root);
    const auto marker = Json::parse(fixtureText(root, ".cycode-diagnostic.json"));
    require(marker == Json({{"schemaVersion", 1}, {"scope", "owned-recovery-antivirus-diagnostic"},
                            {"synthetic", true}}), "Explicit owned synthetic fixture marker required");
    ordinary(root / ".cycode");
    require(!fs::exists(root / ".cycode"), "Diagnostic refuses an existing Core cache");
    require(fixtureText(root, document).empty(), "Diagnostic source baseline must be a real empty file");
    const auto initial = fixtureText(root, "diagnostic-initial.txt"),
               refreshed = fixtureText(root, "diagnostic-refreshed.txt");
    require(initial != refreshed && normalizeEditorText(initial) == initial &&
                normalizeEditorText(refreshed) == refreshed, "Distinct normalized diagnostic texts required");
    if (!filesOnly) {
        // The actual production class, without Engine, workers, endpoint or SDK.
        RecoveryStore store(root);
        store.checkpoint({document, initial, "", 2, false, false, false, true});
        phase("recovery-only", "initial-checkpoint", store.status());
        if (!control("refresh")) return 0;
        store.checkpoint({document, refreshed, "", 3, false, false, false, true});
        phase("recovery-only", "refreshed-checkpoint", store.status());
        control("quit");
        return 0; // Dirty snapshots remain available as evidence, as in the real store.
    }
    // Same text/base/index publication order using the production atomic writer.
    // No RecoveryStore instance, lease, Engine, worker or service is constructed.
    const auto cache = root / ".cycode", recovery = cache / "recovery", folder = recovery / session;
    for (const auto &path : {cache, recovery, folder}) {
        ordinary(path);
        require(fs::create_directory(path), "Diagnostic cache directory already exists");
        ordinary(path);
    }
    const auto text1 = folder / "text-buffer-1-1.utf8", text2 = folder / "text-buffer-1-2.utf8",
               base = folder / "base-buffer-1-1.utf8", index = folder / "session.json";
    const auto first = fixtureIndex(initial, 1).dump() + "\n";
    writeTextFileAtomic(text1, initial, std::nullopt);
    writeTextFileAtomic(base, "", std::nullopt);
    writeTextFileAtomic(index, first, std::nullopt);
    phase("files-only", "initial-checkpoint", {{"session", session}, {"documents", 1}});
    if (!control("refresh")) return 0;
    writeTextFileAtomic(text2, refreshed, std::nullopt);
    {
        NativeTraceScope trace(NativeTraceEvent::RecoveryIndexCommit);
        writeTextFileAtomic(index, fixtureIndex(refreshed, 2).dump() + "\n", first);
    }
    pauseNativeRecoveryCleanupForDiagnosis();
    {
        ordinary(text1);
        NativeTraceScope trace(NativeTraceEvent::RecoveryRemoveText);
        std::error_code error;
        const bool removed = fs::remove(text1, error);
        trace.finish(error ? NativeTraceResult::Failure : removed ? NativeTraceResult::Success : NativeTraceResult::Missing,
                     static_cast<uint64_t>(error.value()));
        require(removed && !error, "Diagnostic old text removal failed");
    }
    phase("files-only", "refreshed-checkpoint", {{"session", session}, {"documents", 1}});
    control("quit");
    return 0;
}

int runOwnedFileOperationDiagnostic(const fs::path &input, std::string_view operation) {
    require(operation == "write" || operation == "index" || operation == "remove" || operation == "refresh",
            "Diagnostic operation must be write, index, remove or refresh");
    const auto root = fs::canonical(input);
    ordinary(root);
    const auto marker = Json::parse(fixtureText(root, ".cycode-diagnostic.json"));
    require(marker == Json({{"schemaVersion", 1}, {"scope", "owned-recovery-operation-diagnostic"},
                            {"synthetic", true}}), "Owned operation fixture marker required");
    require(fixtureText(root, document).empty(), "Diagnostic source must be a real empty file");
    const auto initial = fixtureText(root, "diagnostic-initial.txt"),
               refreshed = fixtureText(root, "diagnostic-refreshed.txt");
    require(initial != refreshed && normalizeEditorText(initial) == initial &&
                normalizeEditorText(refreshed) == refreshed, "Distinct normalized diagnostic texts required");
    const auto folder = root / ".cycode" / "recovery" / session;
    for (const auto &path : {root / ".cycode", root / ".cycode" / "recovery", folder}) {
        ordinary(path);
        require(fs::is_directory(path), "Diagnostic fixture directory must be prepared by its owner");
    }
    const auto text1 = folder / "text-buffer-1-1.utf8", text2 = folder / "text-buffer-1-2.utf8",
               base = folder / "base-buffer-1-1.utf8", index = folder / "session.json";
    for (const auto &path : {text1, text2, base, index}) ordinary(path);
    require(fixtureText(root, ".cycode/recovery/session-00000000000000000000000000000001/text-buffer-1-1.utf8") == initial &&
                fixtureText(root, ".cycode/recovery/session-00000000000000000000000000000001/base-buffer-1-1.utf8").empty(),
            "Unexpected initial diagnostic payloads");
    const bool preseedText2 = operation == "index" || operation == "remove";
    if (preseedText2)
        require(fixtureText(root, ".cycode/recovery/session-00000000000000000000000000000001/text-buffer-1-2.utf8") == refreshed,
                "Refreshed payload must already be prepared");
    else
        require(!fs::exists(text2), "New diagnostic payload already exists");
    const auto expectedIndex = fixtureIndex(operation == "remove" ? refreshed : initial,
                                          operation == "remove" ? 2 : 1).dump() + "\n";
    require(fixtureText(root, ".cycode/recovery/session-00000000000000000000000000000001/session.json") == expectedIndex,
            "Diagnostic index differs from the exact synthetic baseline");
    size_t count = 0;
    for (const auto &entry : fs::directory_iterator(folder)) {
        const auto name = entry.path().filename();
        require(++count <= 4 && entry.is_regular_file() && !entry.is_symlink() &&
                    (name == text1.filename() || name == base.filename() || name == index.filename() ||
                     (preseedText2 && name == text2.filename())), "Unexpected diagnostic cache entry");
    }
    require(count == (preseedText2 ? 4 : 3), "Incomplete diagnostic fixture");
    const Json status = {{"operation", std::string(operation)}, {"session", session}, {"documents", 1}};
    phase("file-operation", "ready", status);
    if (!control("execute")) return 0;
    if (operation == "write" || operation == "refresh")
        writeTextFileAtomic(text2, refreshed, std::nullopt);
    if (operation == "index" || operation == "refresh") {
        NativeTraceScope trace(NativeTraceEvent::RecoveryIndexCommit);
        writeTextFileAtomic(index, fixtureIndex(refreshed, 2).dump() + "\n", expectedIndex);
    }
    if (operation == "remove" || operation == "refresh") {
        ordinary(text1);
        require(readTextFileBytes(text1) == initial, "Old diagnostic text changed");
        NativeTraceScope trace(NativeTraceEvent::RecoveryRemoveText);
        std::error_code error;
        const bool removed = fs::remove(text1, error);
        trace.finish(error ? NativeTraceResult::Failure : removed ? NativeTraceResult::Success : NativeTraceResult::Missing,
                     static_cast<uint64_t>(error.value()));
        require(removed && !error, "Owned diagnostic text removal failed");
    }
    phase("file-operation", "completed", status);
    control("quit");
    return 0;
}
} // namespace cycode
