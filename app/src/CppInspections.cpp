#include "cycode/BuildLimits.h"
#include "cycode/Engine.h"
#include "cycode/Error.h"
#include "cycode/FileTime.h"
#include "cycode/ProcessText.h"
#include "cycode/StreamingData.h"
#include "cycode/TextFile.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

namespace cycode {
namespace {
std::string inspectionChecks(const std::string &profile) {
    if (profile == "recommended")
        return "-*,clang-analyzer-*,bugprone-*,performance-*,portability-*";
    if (profile == "security")
        return "-*,clang-analyzer-*,cert-*,bugprone-*";
    if (profile == "modernize")
        return "-*,modernize-*";
    if (profile == "performance")
        return "-*,performance-*";
    if (profile == "portability")
        return "-*,portability-*";
    throw std::runtime_error("Unknown C++ inspection profile");
}
} // namespace

Json Engine::inspectCpp(const Json &params, const std::atomic<bool> *cancellation) {
    const auto timeout = buildTimeout(params, 180);
    if (timeout > std::chrono::seconds(900))
        throw NativeError(ErrorCode::InvalidParams, "C++ inspection timeout must be 1..900 seconds");
    const auto profile = params.value("profile", std::string("recommended"));
    const auto checks = inspectionChecks(profile);
    const auto path = resolve(params.at("path"));
    const auto key = pathUtf8(path.lexically_relative(root_));
    if (!LanguageService::supportsCpp(key))
        throw NativeError(ErrorCode::InvalidParams, "Select an actual C/C++ translation unit");
    const auto database = resolve(params.at("compilationDatabase"));
    if (database.filename() != "compile_commands.json")
        throw std::runtime_error("Select a real compile_commands.json inside the workspace");
    const auto databaseDigest = digestFileStreaming(database, cancellation);
    // Never run a header using invented fallback flags. Each call inspects one
    // explicitly selected, actually compiled translation unit and its headers.
    size_t matching = 0;
    visitCompilationDatabase(
        database,
        [&](const Json &unit) {
            checkBuildCancellation(cancellation);
            if (!unit.is_object() || !unit.contains("file") || !unit.contains("directory"))
                throw std::runtime_error("Malformed inspection compilation command");
            const auto directory = utf8Path(unit.at("directory"));
            if (directory.is_relative())
                throw std::runtime_error("Inspection compile directory must be absolute");
            std::error_code error;
            const auto source =
                std::filesystem::weakly_canonical(directory / utf8Path(unit.at("file")), error);
            if (!error && source == path) {
                // LLVM prefers arguments when present. A file/directory pair alone
                // is not a real command and must never trigger guessed fallback flags.
                if (unit.contains("arguments")) {
                    const auto &args = unit.at("arguments");
                    if (!args.is_array() || args.empty() || !args[0].is_string() ||
                        args[0].get_ref<const std::string &>().empty() ||
                        std::any_of(args.begin(), args.end(),
                                    [](const Json &arg) { return !arg.is_string(); }))
                        throw NativeError(ErrorCode::InvalidParams,
                                          "Inspection compilation arguments are missing or invalid");
                } else if (!unit.contains("command") || !unit.at("command").is_string() ||
                           unit.at("command").get_ref<const std::string &>().empty())
                    throw NativeError(ErrorCode::InvalidParams,
                                      "Inspection needs a real compiler command, not fallback flags");
                ++matching;
            }
        },
        cancellation);
    if (digestFileStreaming(database, cancellation) != databaseDigest)
        throw NativeError(ErrorCode::RevisionConflict, "Inspection compilation database changed during scan");
    if (matching != 1)
        throw std::runtime_error("Select exactly one compilation context for this translation unit");
    uint64_t revision = 0, identity = 0, generation = 0, workspaceRevision = 0;
    std::string sourceText, sourceFingerprint;
    bool pagedSource = false;
    {
        std::lock_guard lock(mutex_);
        for (const auto &[name, document] : documents_)
            if (document.dirty || !document.diskConflict.empty())
                throw std::runtime_error(
                    "Save or reconcile live documents before disk-based C++ inspections");
        for (const auto &[name, document] : pagedDocuments_)
            if (document.knownDirty || document.busy)
                throw NativeError(ErrorCode::RevisionConflict,
                                  "Save or reconcile paged documents before disk-based C++ inspections");
        const auto paged = pagedDocuments_.find(key);
        if (paged != pagedDocuments_.end()) {
            pagedSource = true;
            revision = paged->second.revision;
            identity = paged->second.identity;
            if (revision != params.at("expectedRevision"))
                throw NativeError(ErrorCode::RevisionConflict,
                                  "Read the current paged revision before inspection");
            sourceFingerprint = digestFileStreaming(path, cancellation);
            if (!paged->second.text->matchesDisk())
                throw NativeError(ErrorCode::DiskConflict, "Paged source changed outside CyCode");
        } else {
            auto &document = openLocked(key);
            if (document.revision != params.at("expectedRevision"))
                throw NativeError(ErrorCode::RevisionConflict,
                                  "Read the current document revision before inspection");
            revision = document.revision;
            identity = document.identity;
            sourceText = document.disk;
        }
        generation = documentGenerationLocked();
        workspaceRevision = monitor_.snapshot().revision;
    }
    std::string executable = params.value("executable", std::string{});
    if (executable.empty()) {
        std::vector<std::filesystem::path> hints;
        const auto clangd = toolPath("clangd");
        if (!clangd.empty())
            hints.push_back(utf8Path(clangd).parent_path());
        executable = pathUtf8(findExecutableOnPath("clang-tidy", hints));
    }
    if (executable.empty() || !std::filesystem::is_regular_file(utf8Path(executable)))
        throw NativeError(ErrorCode::BackendUnavailable,
                          "Install clang-tidy or select its executable explicitly");
    checkBuildCancellation(cancellation);
    const std::vector<std::string> arguments = {executable,
                                                pathUtf8(path),
                                                "-p=" + pathUtf8(database.parent_path()),
                                                "--quiet",
                                                "--use-color=false",
                                                "--checks=" + checks,
                                                "--warnings-as-errors="};
    // No -fix, plugins, shell command, generated compiler flags or source writes.
    const auto process = runProcess(arguments, root_, timeout, 1024 * 1024, cancellation, {},
                                    processOutputObserver(cancellation));
    const auto output = processOutputText(process.output, true);
    Json diagnostics = Json::array();
    size_t external = 0;
    bool diagnosticsTruncated = false;
    static const std::regex diagnostic(
        R"re(^(.*):([0-9]+):([0-9]+): (warning|error|note|fatal error): (.*)$)re");
    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::smatch match;
        if (!std::regex_match(line, match, diagnostic))
            continue;
        std::string relative;
        try {
            relative = pathUtf8(resolve(match[1].str()).lexically_relative(root_));
        } catch (...) {
            ++external;
            continue;
        }
        try {
            const auto row = std::stoull(match[2].str()), column = std::stoull(match[3].str());
            if (!row || !column || row > 10000000 || column > 10000000)
                continue;
            auto message = match[5].str();
            std::string code;
            const auto bracket = message.rfind(" [");
            if (bracket != std::string::npos && message.back() == ']') {
                code = message.substr(bracket + 2, message.size() - bracket - 3);
                message.resize(bracket);
            }
            if (diagnostics.size() == 2000) {
                diagnosticsTruncated = true;
                continue;
            }
            diagnostics.push_back({{"path", relative},
                                   {"line", row},
                                   {"byteColumn", column},
                                   {"severity", match[4].str()},
                                   {"message", message},
                                   {"code", code}});
        } catch (const std::exception &) { /* invalid external coordinates */
        }
    }
    bool stale = false;
    {
        std::lock_guard lock(mutex_);
        if (pagedSource) {
            const auto found = pagedDocuments_.find(key);
            stale = found == pagedDocuments_.end() || found->second.identity != identity ||
                    found->second.revision != revision || found->second.busy;
        } else {
            const auto found = documents_.find(key);
            stale = found == documents_.end() || found->second.identity != identity ||
                    found->second.revision != revision;
        }
        stale = stale || documentGenerationLocked() != generation ||
                monitor_.snapshot().revision != workspaceRevision;
    }
    try {
        stale = stale ||
                (pagedSource ? digestFileStreaming(path) != sourceFingerprint
                             : readTextFileBytes(path) != sourceText) ||
                digestFileStreaming(database) != databaseDigest;
    } catch (...) {
        stale = true;
    }
    const bool truncated = process.truncated || diagnosticsTruncated;
    return {{"path", key},
            {"revision", revision},
            {"documentId", identity},
            {"profile", profile},
            {"compilationDatabase", pathUtf8(database.lexically_relative(root_))},
            {"executable", executable},
            {"status", stale               ? "stale"
                       : process.cancelled ? "cancelled"
                       : process.timedOut  ? "timed-out"
                       : truncated         ? "truncated"
                       : process.exitCode  ? "failed"
                                           : "complete"},
            {"stale", stale},
            {"exitCode", process.exitCode},
            {"cancelled", process.cancelled},
            {"timedOut", process.timedOut},
            {"truncated", truncated},
            {"output", output},
            {"diagnostics", diagnostics},
            {"externalDiagnostics", external},
            {"sourceWritten", false},
            {"positionEncoding", "utf8-byte-columns"}};
}
} // namespace cycode
