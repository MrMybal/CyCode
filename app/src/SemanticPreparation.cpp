#include "cycode/AtomicFile.h"
#include "cycode/Engine.h"
#include "cycode/Error.h"
#include "cycode/StreamingData.h"
#include "cycode/TextEdits.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cwchar>
#include <fstream>
#include <monocypher.h>
#include <random>
#include <set>
#include <string_view>

namespace cycode {
namespace {
std::filesystem::path compilerDatabase(const LanguageService &language, const std::filesystem::path &root) {
    auto directory = language.compilationDatabase("cpp");
    const auto config = language.configuration("cpp");
    if (config.contains("arguments")) {
        const auto &args = config.at("arguments");
        for (size_t i = 0; i < args.size(); ++i) {
            const auto arg = args[i].get<std::string>();
            const auto prefix = arg.starts_with("--compile-commands-dir=")  ? 23
                                : arg.starts_with("-compile-commands-dir=") ? 22
                                                                            : 0;
            if (prefix)
                directory = utf8Path(arg.substr(prefix));
            else if ((arg == "--compile-commands-dir" || arg == "-compile-commands-dir") &&
                     i + 1 < args.size())
                directory = utf8Path(args[++i].get<std::string>());
        }
    }
    if (directory.empty())
        directory = root;
    if (directory.is_relative())
        directory = root / directory;
    return std::filesystem::weakly_canonical(directory / "compile_commands.json");
}
Json sourcePosition(const std::string &text, size_t at) {
    const auto begin = text.rfind('\n', at ? at - 1 : 0);
    const auto start = begin == std::string::npos ? 0 : begin + 1;
    return {{"line", std::count(text.begin(), text.begin() + at, '\n')},
            {"character", LanguageClient::utf16Length(text.substr(start, at - start))}};
}
bool identifierByte(unsigned char c) { return c >= 128 || std::isalnum(c) || c == '_'; }

constexpr size_t analysisCacheLimit = 32 * 1024 * 1024;
std::string analysisBytes(const std::filesystem::path &file, size_t maximum) {
    if (std::filesystem::is_symlink(file) || !std::filesystem::is_regular_file(file))
        throw NativeError(ErrorCode::FileSystemError, "Analysis state requires a regular, unlinked file");
    std::ifstream input(file, std::ios::binary);
    if (!input)
        throw NativeError(ErrorCode::FileSystemError, "Cannot read analysis state");
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 || size > std::streamoff(maximum))
        throw NativeError(ErrorCode::CapacityExceeded, "Analysis state exceeds its byte bound");
    input.seekg(0);
    std::string bytes(static_cast<size_t>(size), '\0');
    input.read(bytes.data(), size);
    if ((!input && size) || input.peek() != std::char_traits<char>::eof())
        throw NativeError(ErrorCode::RevisionConflict, "Analysis state changed during read");
    return bytes;
}
std::string analysisDigest(const std::string &bytes) {
    std::array<uint8_t, 32> hash{};
    crypto_blake2b(hash.data(), hash.size(), reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size());
    std::string result;
    for (auto value : hash) {
        result += "0123456789abcdef"[value >> 4];
        result += "0123456789abcdef"[value & 15];
    }
    return result;
}
std::string analysisCheckpointId() {
    std::random_device source;
    std::string result = "analysis-";
    for (size_t i = 0; i < 32; ++i)
        result += "0123456789abcdef"[source() & 15];
    return result;
}
bool analysisCheckpointId(const std::string &id) {
    return id.size() == 41 && id.starts_with("analysis-") &&
           std::all_of(id.begin() + 9, id.end(),
                       [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
std::filesystem::path analysisDirectory(const std::filesystem::path &root, bool create) {
    auto directory = root;
    for (const auto *name : {".cycode", "analysis-progress"}) {
        directory /= name;
        std::error_code error;
        const auto status = std::filesystem::symlink_status(directory, error);
        if (error && error != std::errc::no_such_file_or_directory)
            throw NativeError(ErrorCode::FileSystemError, "Cannot inspect analysis storage");
        const auto canonical = std::filesystem::weakly_canonical(directory);
#ifdef _WIN32
        const bool same = _wcsicmp(canonical.c_str(), directory.c_str()) == 0;
#else
        const bool same = canonical == directory;
#endif
        if (!same || std::filesystem::is_symlink(status) ||
            (std::filesystem::exists(status) && !std::filesystem::is_directory(status)))
            throw NativeError(ErrorCode::FileSystemError,
                              "Analysis storage cannot traverse redirected paths");
        if (create)
            std::filesystem::create_directory(directory);
    }
    return directory;
}
Json analysisSavedBytes(const std::string &bytes) {
    return Json::parse(bytes, [](int depth, Json::parse_event_t, Json &) {
        if (depth > 12)
            throw NativeError(ErrorCode::InvalidParams, "Analysis checkpoint nesting exceeds its bound");
        return true;
    });
}

Json analysisSaved(const std::filesystem::path &file) {
    return analysisSavedBytes(analysisBytes(file, analysisCacheLimit));
}

std::filesystem::path analysisParts(const std::filesystem::path &manifest, bool create) {
    const auto directory = manifest.parent_path() / (pathUtf8(manifest.stem()) + ".parts");
    const auto canonical = std::filesystem::weakly_canonical(directory);
#ifdef _WIN32
    const bool same = _wcsicmp(canonical.c_str(), directory.c_str()) == 0;
#else
    const bool same = canonical == directory;
#endif
    if (!same || std::filesystem::is_symlink(directory) ||
        (std::filesystem::exists(directory) && !std::filesystem::is_directory(directory)))
        throw NativeError(ErrorCode::FileSystemError, "Redirected analysis pages refused");
    if (create)
        std::filesystem::create_directory(directory);
    return directory;
}
bool analysisHash(const std::string &hash) {
    return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
void visitAnalysisArray(const Json &manifest, const std::filesystem::path &file, const std::string &kind,
                        const std::function<void(const Json &)> &visit) {
    if (manifest.at("format") == 1) {
        const auto &values = manifest.at(kind);
        if (!values.is_array())
            throw NativeError(ErrorCode::InvalidParams, "Invalid legacy analysis array");
        for (const auto &value : values)
            visit(value);
        return;
    }
    if (manifest.at("format") != 2)
        throw NativeError(ErrorCode::InvalidParams, "Unsupported analysis format");
    const auto count = manifest.at(kind == "paths" ? "total" : "observed").get<size_t>();
    const auto &pages = manifest.at(kind == "paths" ? "pathPages" : "recordPages");
    if (!pages.is_array() || pages.size() != (count / 256 + (count % 256 != 0)))
        throw NativeError(ErrorCode::InvalidParams, "Invalid analysis page count");
    const auto directory = analysisParts(file, false);
    size_t seen = 0;
    for (const auto &item : pages) {
        const auto hash = item.get<std::string>();
        if (!analysisHash(hash))
            throw NativeError(ErrorCode::InvalidParams, "Invalid analysis page identity");
        const auto bytes = analysisBytes(directory / (kind + "-" + hash + ".json"), 16 * 1024 * 1024);
        if (analysisDigest(bytes) != hash)
            throw NativeError(ErrorCode::RevisionConflict, "Analysis page bytes changed");
        const auto values = Json::parse(bytes, [](int depth, Json::parse_event_t, Json &) {
            if (depth > 12)
                throw NativeError(ErrorCode::InvalidParams, "Analysis page nesting exceeds its bound");
            return true;
        });
        const auto expected = std::min<size_t>(256, count - seen);
        if (!values.is_array() || values.size() != expected)
            throw NativeError(ErrorCode::InvalidParams, "Invalid analysis page extent");
        for (const auto &value : values) {
            visit(value);
            ++seen;
        }
    }
}

} // namespace

std::string Engine::analysisCompilerContext() const {
    auto context = language_.configuration("cpp").dump();
    const auto file = compilerDatabase(language_, root_);
    context += pathUtf8(file);
    std::error_code error;
    const auto stamp = std::filesystem::last_write_time(file, error);
    if (error)
        context += "missing";
    else {
        context += std::to_string(stamp.time_since_epoch().count());
        context += ':';
        context += std::to_string(std::filesystem::file_size(file, error));
    }
    return context;
}

Json Engine::analysisOperation(const std::string &method, const Json &params) {
    if (method == "analysis.prepare")
        return prepareAnalysis(params);
    if (method == "analysis.checkpoints") {
        Json entries = Json::array();
        size_t invalid = 0, examined = 0;
        bool truncated = false;
        const auto directory = analysisDirectory(root_, false);
        if (std::filesystem::is_directory(directory))
            for (const auto &entry : std::filesystem::directory_iterator(directory)) {
                if (++examined > 1024) {
                    truncated = true;
                    break;
                }
                const auto id = pathUtf8(entry.path().stem());
                if (entry.path().extension() != ".json" || !analysisCheckpointId(id))
                    continue;
                try {
                    const auto value = analysisSaved(entry.path());
                    const auto format = value.at("format").get<int>();
                    if ((format != 1 && format != 2) || value.at("workspace") != pathUtf8(root_) ||
                        value.at("checkpoint") != id || value.at("scope") != "compilation-database")
                        throw NativeError(ErrorCode::InvalidParams, "Invalid analysis checkpoint");
                    const auto total =
                        format == 1 ? value.at("paths").size() : value.at("total").get<size_t>();
                    const auto observed =
                        format == 1 ? value.at("records").size() : value.at("observed").get<size_t>();
                    if (observed > total)
                        throw NativeError(ErrorCode::InvalidParams, "Invalid analysis counts");
                    entries.push_back({{"checkpoint", id},
                                       {"total", total},
                                       {"observed", observed},
                                       {"scope", value.at("scope")},
                                       {"proofReused", false}});
                    if (entries.size() == 128) {
                        truncated = true;
                        break;
                    }
                } catch (const std::exception &) {
                    ++invalid;
                }
            }
        return {{"checkpoints", entries}, {"invalid", invalid}, {"truncated", truncated}};
    }
    std::lock_guard lock(analysisStateMutex_);
    if (method == "analysis.cancel") {
        const auto id = params.value("operationId", std::string{});
        const bool matched = analysisCancellation_ && analysisState_.at("state") == "running" &&
                             (id.empty() || id == analysisOperationId_);
        if (matched)
            analysisCancellation_->store(true);
        return {{"cancelled", matched}};
    }
    if (method == "analysis.results") {
        if (params.at("session") != analysisSession_.id)
            throw NativeError(ErrorCode::RevisionConflict, "Analysis session was replaced; prepare again");
        const auto offset = params.value("offset", size_t{0}), limit = params.value("limit", size_t{128});
        if (limit < 1 || limit > 256 || offset > analysisSession_.records.size())
            throw NativeError(ErrorCode::InvalidParams, "Invalid analysis result page");
        Json rows = Json::array();
        const auto end = std::min(offset + limit, analysisSession_.records.size());
        for (size_t i = offset; i < end; ++i)
            rows.push_back(analysisSession_.records.at(i));
        return {{"session", analysisSession_.id},
                {"documents", std::move(rows)},
                {"offset", offset},
                {"nextOffset", end},
                {"available", analysisSession_.records.size()},
                {"total", analysisSession_.paths.size()},
                {"hasMore", end < analysisSession_.records.size()}};
    }
    const auto id = params.value("operationId", std::string{});
    if (!id.empty() && id != analysisOperationId_)
        return {{"state", "idle"}, {"phase", "idle"}, {"completed", 0},
                {"total", 0},      {"progress", 0},   {"error", ""}};
    auto result = analysisState_;
    if (result.at("state") == "running" && result.contains("currentPath") && result.at("currentPath") != "")
        result["currentFileMilliseconds"] =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - analysisFileStarted_)
                .count();
    return result;
}

Json Engine::prepareAnalysis(const Json &params, const std::string &symbol, const std::string &source,
                             uint64_t *issuedGeneration, Json *renameEdits) {
    std::unique_lock operation(analysisMutex_, std::try_to_lock);
    if (!operation.owns_lock())
        throw NativeError(ErrorCode::BackendUnavailable,
                          "Another semantic preparation is active; observe analysis.status and retry");
    const bool rename = renameEdits != nullptr;
    const auto cancellation = std::make_shared<std::atomic<bool>>(false);
    AnalysisSession work;
    Json collected = Json::object();
    std::map<std::string, StoredBlob> baselines;
    std::shared_ptr<SpoolStore> renameStore;
    std::string temporary;
    uint64_t temporaryId = 0, temporaryRevision = 0;
    {
        std::lock_guard lock(analysisStateMutex_);
        analysisOperationId_ = params.value("operationId", std::string{});
        analysisCancellation_ = cancellation;
        analysisState_ = {{"state", "running"}, {"phase", "discovering"}, {"completed", 0},
                          {"total", 0},         {"progress", 0},          {"error", ""}};
    }
    auto checked = [&] {
        if (cancellation->load())
            throw NativeError(ErrorCode::Cancelled, "Semantic preparation cancelled");
    };
    auto cleanup = [&] {
        if (temporary.empty())
            return;
        std::lock_guard lock(mutex_);
        const auto found = documents_.find(temporary);
        if (analysisTemporaryDocuments_.erase(temporary) && found != documents_.end() &&
            found->second.identity == temporaryId && found->second.revision == temporaryRevision &&
            !found->second.dirty) {
            language_.close(temporary);
            documents_.erase(found);
        }
        temporary.clear();
    };
    auto publish = [&](const char *phase) {
        work.summary["observed"] = work.records.size();
        work.summary["remaining"] = work.paths.size() - work.records.size();
        std::lock_guard lock(analysisStateMutex_);
        analysisState_["phase"] = phase;
        if (std::string_view(phase) != "analysing")
            analysisState_["currentPath"] = "";
        analysisState_["completed"] = work.records.size();
        analysisState_["total"] = work.paths.size();
        analysisState_["progress"] =
            work.paths.empty() ? 0 : std::min<size_t>(99, 100 * work.records.size() / work.paths.size());
        analysisState_["session"] = work.id;
        analysisState_["coverage"] = work.summary;
        analysisState_["resumable"] = !rename && work.discoveryComplete;
        if (!work.checkpoint.empty()) {
            analysisState_["checkpoint"] = work.checkpoint;
            analysisState_["checkpointRevision"] = work.checkpointRevision;
            analysisState_["historicalObserved"] = work.historicalObserved;
            analysisState_["currentPassObserved"] = work.records.size() - work.historicalObserved;
            analysisState_["proofReused"] = false;
        }
    };
    // A checkpoint is native compiler evidence, not a second job scheduler.
    // The official SDK owns each call and cancellation. Current compiler evidence
    // expires with this guarded pass; optional durable metadata retains traversal
    // only and is explicitly downgraded on restoration.
    auto checkpoint = [&] {
        if (!work.checkpoint.empty() && work.discoveryComplete) {
            const auto directory = analysisDirectory(root_, true),
                       file = directory / (work.checkpoint + ".json");
            if (work.compilerDigest != digestFileStreaming(compilerDatabase(language_, root_)))
                throw NativeError(ErrorCode::RevisionConflict,
                                  "Compiler database bytes changed; preserve the previous checkpoint");
            const bool exists = std::filesystem::exists(file);
            if ((work.checkpointRevision.empty() && exists) ||
                (!work.checkpointRevision.empty() &&
                 (!exists ||
                  analysisDigest(analysisBytes(file, analysisCacheLimit)) != work.checkpointRevision)))
                throw NativeError(ErrorCode::RevisionConflict,
                                  "Saved analysis progress changed; preserve it and inspect checkpoints");
            const auto parts = analysisParts(file, true);
            auto writePages = [&](const std::string &kind, size_t count,
                                  const std::function<Json(size_t)> &at, Json &pages, size_t start) {
                for (size_t index = start; index * 256 < count; ++index) {
                    if (work.records.size() == work.historicalObserved)
                        checked();
                    Json page = Json::array();
                    for (size_t i = index * 256; i < std::min(count, (index + 1) * 256); ++i)
                        page.push_back(at(i));
                    const auto text = page.dump();
                    if (text.size() > 16 * 1024 * 1024)
                        throw NativeError(ErrorCode::CapacityExceeded,
                                          "An analysis page exceeds its 16 MiB working budget");
                    const auto hash = analysisDigest(text);
                    const auto target = parts / (kind + "-" + hash + ".json");
                    if (std::filesystem::exists(target)) {
                        if (analysisDigest(analysisBytes(target, 16 * 1024 * 1024)) != hash)
                            throw NativeError(ErrorCode::RevisionConflict, "Stored analysis page changed");
                    } else
                        writeMetadataFileAtomic(target, text);
                    if (index < pages.size())
                        pages[index] = hash;
                    else
                        pages.push_back(hash);
                }
            };
            writePages(
                "paths", work.paths.size(), [&](size_t i) { return Json(work.paths.at(i)); }, work.pathPages,
                work.pathPages.size());
            writePages(
                "records", work.records.size(), [&](size_t i) { return work.records.at(i); },
                work.recordPages, work.savedRecords / 256);
            const Json value = {{"format", 2},
                                {"workspace", pathUtf8(root_)},
                                {"checkpoint", work.checkpoint},
                                {"scope", work.scope},
                                {"compilerContext", work.compilerContext},
                                {"compilerDigest", work.compilerDigest},
                                {"total", work.paths.size()},
                                {"observed", work.records.size()},
                                {"pathPages", work.pathPages},
                                {"recordPages", work.recordPages},
                                {"summary", work.summary}};
            const auto bytes = value.dump();
            if (bytes.size() > analysisCacheLimit)
                throw NativeError(ErrorCode::CapacityExceeded,
                                  "Analysis manifest exceeds its working budget");
            writeMetadataFileAtomic(file, bytes);
            work.savedRecords = work.records.size();
            work.checkpointRevision = analysisDigest(bytes);
            std::lock_guard lock(analysisStateMutex_);
            analysisState_["checkpointRevision"] = work.checkpointRevision;
        }
        std::lock_guard lock(analysisStateMutex_);
        analysisSession_ = work;
    };
    auto guard = [&] {
        checked();
        if (documentGenerationLocked() != work.generation)
            throw NativeError(ErrorCode::RevisionConflict,
                              "A document changed during semantic preparation; read and prepare again");
        if (monitor_.snapshot().revision != work.workspaceRevision)
            throw NativeError(ErrorCode::RevisionConflict,
                              "Workspace observation changed during semantic preparation; prepare again");
        for (const auto &[backend, epoch] : work.epochs)
            if (language_.backendEpoch(backend) != epoch)
                throw NativeError(ErrorCode::RevisionConflict,
                                  "Language configuration changed during semantic preparation");
    };
    auto request = [&](const std::string &method, const Json &data) {
        for (;;) {
            {
                std::lock_guard lock(mutex_);
                guard();
            }
            try {
                return language_.request(method, data, cancellation).get();
            } catch (const NativeError &error) {
                if (error.code() != ErrorCode::DeadlineExceeded)
                    throw;
            }
        }
    };
    try {
        const auto continuation = params.value("continuation", std::string{});
        const auto restore = params.value("checkpoint", std::string{});
        if (!restore.empty()) {
            if (rename || !analysisCheckpointId(restore) || params.contains("paths") ||
                params.contains("scope") || params.contains("pathPrefix") || !continuation.empty() ||
                params.contains("persist"))
                throw NativeError(ErrorCode::InvalidParams,
                                  "Restore only an issued checkpoint and optional batchSize");
            const auto directory = analysisDirectory(root_, false), file = directory / (restore + ".json");
            if (std::filesystem::is_symlink(file) || !std::filesystem::is_regular_file(file))
                throw NativeError(ErrorCode::InvalidParams, "The issued analysis checkpoint does not exist");
            {
                std::lock_guard lock(analysisStateMutex_);
                if (analysisSession_.checkpoint == restore)
                    work.checkpointLease = analysisSession_.checkpointLease;
            }
            if (!work.checkpointLease)
                work.checkpointLease = std::make_shared<NativeFileLease>(directory / (restore + ".lock"));
            if (!work.checkpointLease->acquired())
                throw NativeError(ErrorCode::BackendUnavailable,
                                  "Analysis checkpoint is in use by another Core");
            const auto bytes = analysisBytes(file, analysisCacheLimit);
            const auto value = analysisSavedBytes(bytes);
            const auto format = value.at("format").get<int>();
            if ((format != 1 && format != 2) || value.at("workspace") != pathUtf8(root_) ||
                value.at("checkpoint") != restore || value.at("scope") != "compilation-database")
                throw NativeError(ErrorCode::InvalidParams, "Invalid saved analysis inventory");
            work.compilerContext = value.at("compilerContext");
            work.compilerDigest = value.at("compilerDigest");
            if (work.compilerContext != analysisCompilerContext() ||
                work.compilerDigest !=
                    digestFileStreaming(compilerDatabase(language_, root_), cancellation.get()))
                throw NativeError(
                    ErrorCode::RevisionConflict,
                    "Compilation context changed since the checkpoint; start a fresh inventory");
            std::set<std::string> unique;
            visitAnalysisArray(value, file, "paths", [&](const Json &entry) {
                checked();
                const auto name = entry.get<std::string>();
                const auto relative = utf8Path(name);
                if (name.empty() || name.size() > 32768 || relative.has_root_path() ||
                    name.find_first_of("\\:\r\n") != std::string::npos ||
                    name.find('\0') != std::string::npos ||
                    std::any_of(relative.begin(), relative.end(),
                                [](const auto &part) { return part == ".." || part == "."; }) ||
                    !LanguageService::supportsCpp(name) || !unique.insert(name).second)
                    throw NativeError(ErrorCode::InvalidParams, "Invalid path in analysis checkpoint");
                work.paths.push_back(name);
            });
            size_t recordIndex = 0;
            visitAnalysisArray(value, file, "records", [&](const Json &saved) {
                checked();
                auto row = saved;
                const auto i = recordIndex++;
                if (i >= work.paths.size())
                    throw NativeError(ErrorCode::InvalidParams, "Too many analysis observations");
                if (row.at("path") != work.paths.at(i) || !row.at("revision").is_number_unsigned() ||
                    !row.at("errors").is_number_unsigned() || !row.at("diagnostics").is_string())
                    throw NativeError(ErrorCode::InvalidParams, "Invalid historical analysis record");
                static const std::set<std::string> fields = {"path",
                                                             "revision",
                                                             "diagnostics",
                                                             "errors",
                                                             "evidence",
                                                             "elapsedMilliseconds",
                                                             "previousDiagnostics",
                                                             "error"};
                if (row.dump().size() > 8192)
                    throw NativeError(ErrorCode::InvalidParams,
                                      "Historical analysis record exceeds its bound");
                for (const auto &[key, field] : row.items())
                    if (!fields.contains(key))
                        throw NativeError(ErrorCode::InvalidParams, "Unknown historical analysis field");
                if (row.contains("error") && (!row.at("error").is_string() ||
                                              row.at("error").get_ref<const std::string &>().size() > 2048))
                    throw NativeError(ErrorCode::InvalidParams, "Invalid historical analysis error");
                if (row.at("revision") == 0 && (!row.contains("error") || row.at("error") == ""))
                    throw NativeError(ErrorCode::InvalidParams,
                                      "An unopened historical source needs its failure");
                if (row.contains("elapsedMilliseconds") &&
                    (!row.at("elapsedMilliseconds").is_number() || row.at("elapsedMilliseconds") < 0))
                    throw NativeError(ErrorCode::InvalidParams, "Invalid historical analysis timing");
                const auto previous = row.at("diagnostics").get<std::string>();
                if (previous != "clean" && previous != "errors" && previous != "unknown")
                    throw NativeError(ErrorCode::InvalidParams, "Invalid historical diagnostic state");
                row["previousDiagnostics"] = previous;
                row["diagnostics"] = "unknown";
                row["errors"] = 0;
                row["evidence"] = "historical-checkpoint";
                work.records.push_back(std::move(row));
            });
            if (format == 2)
                work.pathPages = value.at("pathPages");
            // Historical diagnostics were explicitly downgraded: publish new
            // immutable record pages once; never modify the prior generation.
            work.id = "analysis-" + std::to_string(nextAnalysisSession_++);
            work.scope = "compilation-database";
            work.historicalObserved = work.records.size();
            work.discoveryComplete = true;
            const auto previousSummary = value.at("summary");
            work.summary = Json::object();
            for (const auto *key : {"commands", "translationUnits", "outOfScope"}) {
                if (!previousSummary.at(key).is_number_unsigned())
                    throw NativeError(ErrorCode::InvalidParams, "Invalid checkpoint inventory counts");
                work.summary[key] = previousSummary.at(key);
            }
            work.summary["diagnosticClean"] = 0;
            work.summary["diagnosticErrors"] = 0;
            work.summary["diagnosticUnknown"] = work.records.size();
            {
                std::lock_guard lock(mutex_);
                work.generation = documentGenerationLocked();
                work.workspaceRevision = monitor_.snapshot().revision;
            }
            work.epochs["cpp"] = language_.backendEpoch("cpp");
            work.checkpoint = restore;
            work.checkpointRevision = analysisDigest(bytes);
        } else if (!continuation.empty()) {
            if (rename || params.contains("paths") || params.contains("scope") ||
                params.contains("pathPrefix"))
                throw NativeError(ErrorCode::InvalidParams,
                                  "Continue with the issued session ID and an optional batchSize only");
            std::lock_guard lock(analysisStateMutex_);
            if (continuation != analysisSession_.id)
                throw NativeError(ErrorCode::RevisionConflict,
                                  "Analysis continuation expired or was replaced");
            work = analysisSession_;
            if (!work.discoveryComplete || work.scope == "workspace-spelling-candidates")
                throw NativeError(
                    ErrorCode::InvalidParams,
                    "This observation is not resumable; start a new preparation or global rename");
        } else {
            work.id = "analysis-" + std::to_string(nextAnalysisSession_++);
            work.scope = rename ? "workspace-spelling-candidates"
                                : params.value("scope", std::string("explicit-files"));
            work.summary = {{"diagnosticClean", 0}, {"diagnosticErrors", 0}, {"diagnosticUnknown", 0},
                            {"commands", 0},        {"translationUnits", 0}, {"outOfScope", 0}};
            {
                std::lock_guard lock(mutex_);
                work.generation = documentGenerationLocked();
                work.workspaceRevision = monitor_.snapshot().revision;
            }
            if (issuedGeneration && work.generation != *issuedGeneration)
                throw NativeError(ErrorCode::RevisionConflict,
                                  "Workspace changed before semantic preparation");
            std::set<std::string> paths;
            if (!source.empty())
                paths.insert(source);
            work.compilerContext = analysisCompilerContext();
            if (work.scope == "compilation-database") {
                if (params.contains("paths"))
                    throw NativeError(ErrorCode::InvalidParams,
                                      "Compilation-database scope does not accept paths");
                const auto file = compilerDatabase(language_, root_);
                const auto prefix = params.value("pathPrefix", std::string{});
                const auto folder = prefix.empty() ? root_ : resolve(prefix);
                if (!std::filesystem::is_directory(folder))
                    throw NativeError(ErrorCode::InvalidParams,
                                      "pathPrefix must be an existing workspace directory");
                size_t commands = 0, outside = 0;
                std::set<std::string> allUnits;
                // Consume one command object at a time. Do not retain 37,000
                // expanded compiler argument lists merely to enumerate sources.
                visitCompilationDatabase(
                    file,
                    [&](const Json &value) {
                        checked();
                        if (!value.is_object() || !value.value("file", Json()).is_string() ||
                            !value.value("directory", Json()).is_string())
                            throw NativeError(ErrorCode::InvalidParams, "Invalid compilation command entry");
                        ++commands;
                        auto path = utf8Path(value.at("file").get<std::string>());
                        if (path.is_relative())
                            path = utf8Path(value.at("directory").get<std::string>()) / path;
                        path = std::filesystem::weakly_canonical(path);
                        if (!containsPath(root_, path) || !LanguageService::supportsCpp(pathUtf8(path))) {
                            ++outside;
                            return;
                        }
                        const auto key = pathUtf8(resolve(pathUtf8(path)).lexically_relative(root_));
                        allUnits.insert(key);
                        if (containsPath(folder, path))
                            paths.insert(key);
                    },
                    cancellation.get());
                work.summary["commands"] = commands;
                work.summary["translationUnits"] = allUnits.size();
                work.summary["outOfScope"] = allUnits.size() - paths.size() + outside;
                work.epochs["cpp"] = language_.backendEpoch("cpp");
            } else if (!rename) {
                if (work.scope != "explicit-files" || !params.contains("paths") ||
                    !params.at("paths").is_array() || params.at("paths").empty())
                    throw NativeError(
                        ErrorCode::InvalidParams,
                        "Analysis requires explicit source paths or compilation-database scope");
                for (const auto &entry : params.at("paths")) {
                    const auto path = pathUtf8(resolve(entry.get<std::string>()).lexically_relative(root_));
                    if (!language_.accepts(path))
                        throw NativeError(ErrorCode::UnsupportedOperation,
                                          "No configured language backend for " + path);
                    paths.insert(path);
                }
            } else {
                std::string pending;
                bool invalidPath = false;
                const std::vector<std::string> args = {"rg",
                                                       "--files-with-matches",
                                                       "--null",
                                                       "--hidden",
                                                       "--no-ignore",
                                                       "--fixed-strings",
                                                       "--word-regexp",
                                                       "-g",
                                                       "*.{c,cc,cpp,cxx,h,hh,hpp,hxx,inl,ipp,tpp,ixx,cppm}",
                                                       "-g",
                                                       "!.git",
                                                       "-g",
                                                       "!/runtime",
                                                       "-g",
                                                       "!/data",
                                                       "-g",
                                                       "!.cycode",
                                                       "--",
                                                       symbol,
                                                       "."};
                const auto result =
                    runProcessStreamed(args, root_, *cancellation, [&](std::string_view chunk) {
                        pending.append(chunk);
                        for (size_t at; (at = pending.find('\0')) != std::string::npos;) {
                            auto value = pending.substr(0, at);
                            pending.erase(0, at + 1);
                            std::replace(value.begin(), value.end(), '\\', '/');
                            if (value.starts_with("./"))
                                value.erase(0, 2);
                            try {
                                const auto path = pathUtf8(resolve(value).lexically_relative(root_));
                                if (LanguageService::supportsCpp(path))
                                    paths.insert(path);
                            } catch (const std::exception &) {
                                invalidPath = true;
                                cancellation->store(true);
                            }
                        }
                        if (pending.size() > 32768) {
                            invalidPath = true;
                            cancellation->store(true);
                        }
                    });
                if (invalidPath)
                    throw NativeError(ErrorCode::FileSystemError,
                                      "A rename candidate could not be safely resolved");
                checked();
                if (result.exitCode > 1 || !pending.empty())
                    throw NativeError(ErrorCode::OperationFailed,
                                      "Rename discovery failed: " + result.output);
                std::lock_guard lock(mutex_);
                guard();
                for (const auto &[path, doc] : documents_)
                    if (LanguageService::supportsCpp(path) && doc.text.find(symbol) != std::string::npos)
                        paths.insert(path);
            }
            work.discoveryComplete = true;
            work.paths.assign(paths.begin(), paths.end());
            if (rename) {
                const auto first = std::find(work.paths.begin(), work.paths.end(), source);
                if (first != work.paths.end())
                    std::rotate(work.paths.begin(), first, first + 1);
            }
            for (const auto &path : work.paths)
                work.epochs.try_emplace(language_.backend(path),
                                        language_.backendEpoch(language_.backend(path)));
        }
        if (params.value("persist", false)) {
            if (rename || work.scope != "compilation-database")
                throw NativeError(ErrorCode::InvalidParams,
                                  "Persistence requires compilation-database scope");
            if (work.checkpoint.empty()) {
                const auto directory = analysisDirectory(root_, true);
                work.checkpoint = analysisCheckpointId();
                work.checkpointLease =
                    std::make_shared<NativeFileLease>(directory / (work.checkpoint + ".lock"));
                if (!work.checkpointLease->acquired())
                    throw NativeError(ErrorCode::BackendUnavailable, "Cannot acquire analysis checkpoint");
                work.compilerDigest =
                    digestFileStreaming(compilerDatabase(language_, root_), cancellation.get());
            }
        }
        if (analysisCompilerContext() != work.compilerContext)
            throw NativeError(ErrorCode::RevisionConflict,
                              "Compiler database or configuration changed; prepare again");
        {
            std::lock_guard lock(mutex_);
            guard();
        }
        publish("analysing");
        checkpoint();
        std::set<std::string> usrs;
        const auto batch =
            params.value("batchSize", rename                                 ? work.paths.size()
                                      : work.scope == "compilation-database" ? size_t{64}
                                                                             : work.paths.size());
        if (!rename && params.contains("batchSize") && batch > 4096)
            throw NativeError(ErrorCode::InvalidParams, "batchSize must be 0..4096");
        const auto end = std::min(work.paths.size(), work.records.size() + batch);
        for (size_t index = work.records.size(); index < end; ++index) {
            checked();
            const auto &path = work.paths[index];
            const auto started = std::chrono::steady_clock::now();
            {
                std::lock_guard lock(analysisStateMutex_);
                analysisState_["currentPath"] = path;
                analysisFileStarted_ = started;
            }
            uint64_t revision = 0, identity = 0;
            std::string uri, text;
            try {
                {
                    std::lock_guard lock(mutex_);
                    guard();
                    const bool owned = !documents_.contains(path);
                    const auto revisionBeforeOpen = nextRevision_;
                    auto &doc = openLocked(path, false);
                    if (owned)
                        analysisRevisionSlots_ += nextRevision_ - revisionBeforeOpen;
                    work.generation = documentGenerationLocked();
                    revision = doc.revision;
                    identity = doc.identity;
                    text = doc.text;
                    uri = LanguageClient::uri(resolve(path));
                    if (owned) {
                        analysisTemporaryDocuments_.insert(path);
                        temporary = path;
                        temporaryId = identity;
                        temporaryRevision = revision;
                    }
                    // Reopening only the LSP draft is deliberate for explicit C++
                    // analysis: clangd suppresses identical-text diagnostic updates.
                    // Native buffers, identities, revisions and UI ownership stay intact.
                    language_.sync(path, doc.text, revision, identity, true, false,
                                   LanguageService::supportsCpp(path));
                }
                const Json document = {{"textDocument", {{"uri", uri}}}};
                (void)request("textDocument/documentSymbol", document);
                if (rename && path == source) {
                    const auto info =
                        request("textDocument/symbolInfo",
                                {{"textDocument", {{"uri", uri}}}, {"position", params.at("position")}});
                    const auto rows = info.is_array() ? info : Json::array({info});
                    for (const auto &row : rows)
                        if (row.is_object() && row.value("usr", Json()).is_string() &&
                            !row.at("usr").get<std::string>().empty())
                            usrs.insert(row.at("usr").get<std::string>());
                    if (usrs.size() != 1)
                        throw NativeError(ErrorCode::UnsupportedOperation,
                                          "Compiler did not identify one unambiguous C++ symbol; no partial "
                                          "preview issued");
                }
                // A compiler AST response and version-bound diagnostics are different
                // evidence. Unknown/errorful diagnostics never become a clean proof.
                std::string diagnostic = "unknown";
                size_t errorCount = 0;
                for (size_t attempt = 0; attempt < 100; ++attempt) {
                    checked();
                    const auto status = language_.status();
                    const auto backend = language_.backend(path);
                    const auto server =
                        status.contains("servers") ? status.at("servers").at(backend) : status;
                    const auto publications = server.value("diagnosticRevisions", Json::object());
                    if (publications.contains(path) &&
                        publications.at(path).value("revision", uint64_t{0}) == revision &&
                        publications.at(path).value("id", uint64_t{0}) == identity) {
                        const auto diagnostics = server.value("diagnostics", Json::object());
                        const auto publishedUri = publications.at(path).value("uri", uri);
                        if (diagnostics.contains(publishedUri)) {
                            for (const auto &item : diagnostics.at(publishedUri))
                                if (item.value("severity", 0) == 1)
                                    ++errorCount;
                            diagnostic = errorCount ? "errors" : "clean";
                            break;
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                if (rename) {
                    bool matched = false;
                    for (size_t at = 0; (at = text.find(symbol, at)) != std::string::npos;
                         at += symbol.size()) {
                        if ((at && identifierByte(text[at - 1])) ||
                            (at + symbol.size() < text.size() && identifierByte(text[at + symbol.size()])))
                            continue;
                        const auto position = sourcePosition(text, at);
                        const auto info = request("textDocument/symbolInfo",
                                                  {{"textDocument", {{"uri", uri}}}, {"position", position}});
                        const auto rows = info.is_array() ? info : Json::array({info});
                        for (const auto &row : rows)
                            if (row.is_object() && row.value("usr", Json()).is_string() &&
                                usrs.contains(row.at("usr").get<std::string>()))
                                matched = true;
                        if (!matched)
                            continue;
                        // Ask the real compiler at a confirmed occurrence in THIS
                        // current AST. Keep only this file's compiler-issued edits;
                        // other files are verified during their own rolling visit.
                        const auto edits =
                            request("textDocument/rename", {{"textDocument", {{"uri", uri}}},
                                                            {"position", position},
                                                            {"newName", params.at("newName")}});
                        Json own = Json::array();
                        if (edits.is_object() && edits.contains("changes"))
                            for (const auto &[target, list] : edits.at("changes").items())
                                if (std::filesystem::weakly_canonical(LanguageClient::fromUri(target)) ==
                                    resolve(path))
                                    own = list;
                        if (edits.is_object() && edits.contains("documentChanges"))
                            for (const auto &change : edits.at("documentChanges")) {
                                if (!change.contains("textDocument"))
                                    throw NativeError(ErrorCode::UnsupportedOperation,
                                                      "Rename requires unsupported file operations");
                                if (std::filesystem::weakly_canonical(LanguageClient::fromUri(
                                        change.at("textDocument").at("uri"))) == resolve(path)) {
                                    if (!own.empty())
                                        throw NativeError(ErrorCode::OperationFailed,
                                                          "Duplicate compiler rename document");
                                    own = change.at("edits");
                                }
                            }
                        if (own.empty())
                            throw NativeError(ErrorCode::OperationFailed,
                                              "Compiler omitted the confirmed current rename file; no "
                                              "partial preview issued");
                        for (const auto &edit : own)
                            if (edit.contains("annotationId"))
                                throw NativeError(ErrorCode::UnsupportedOperation,
                                                  "Annotated C++ rename requires adapter support");
                        (void)applyTextEdits(text, own, symbol);
                        collected[uri] = own;
                        if (!renameStore)
                            renameStore = std::make_shared<SpoolStore>(root_);
                        baselines[path] = renameStore->put(text);
                        break;
                    }
                }
                {
                    std::lock_guard lock(mutex_);
                    guard();
                    const auto &doc = documents_.at(path);
                    if (doc.identity != identity || doc.revision != revision)
                        throw NativeError(ErrorCode::RevisionConflict, "An analysis document was replaced");
                }
                work.records.push_back(
                    {{"path", path},
                     {"revision", revision},
                     {"diagnostics", diagnostic},
                     {"errors", errorCount},
                     {"evidence", "current-process"},
                     {"elapsedMilliseconds",
                      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                          .count()}});
                const auto count = diagnostic == "clean"    ? "diagnosticClean"
                                   : diagnostic == "errors" ? "diagnosticErrors"
                                                            : "diagnosticUnknown";
                work.summary[count] = work.summary.value(count, size_t{0}) + 1;
                cleanup();
                publish("analysing");
            } catch (const std::exception &error) {
                if (rename)
                    throw;
                const auto *native = dynamic_cast<const NativeError *>(&error);
                if (native && (native->code() == ErrorCode::Cancelled ||
                               native->code() == ErrorCode::RevisionConflict ||
                               native->code() == ErrorCode::BackendUnavailable ||
                               native->code() == ErrorCode::OperationFailed))
                    throw;
                cleanup();
                const auto message = std::string(error.what()).substr(0, 2048);
                work.records.push_back(
                    {{"path", path},
                     {"revision", revision},
                     {"diagnostics", "unknown"},
                     {"errors", 0},
                     {"evidence", "current-process"},
                     {"error", message},
                     {"elapsedMilliseconds",
                      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                          .count()}});
                work.summary["diagnosticUnknown"] = work.summary.value("diagnosticUnknown", size_t{0}) + 1;
                publish("analysing");
            }
        }
        if (analysisCompilerContext() != work.compilerContext)
            throw NativeError(ErrorCode::RevisionConflict, "Compiler database changed during preparation");
        {
            std::lock_guard lock(mutex_);
            guard();
            if (rename) {
                if (work.summary.value("diagnosticErrors", size_t{0}) ||
                    work.summary.value("diagnosticUnknown", size_t{0}))
                    throw NativeError(
                        ErrorCode::BackendUnavailable,
                        "Some rename candidates lack clean version-bound compiler diagnostics; inspect "
                        "analysis.results. No partial preview issued",
                        {{"partialPreviewIssued", false}, {"session", work.id}, {"coverage", work.summary}});
                // Reopen only affected native buffers; validate the exact text
                // used by the per-file compiler, then let the normal transaction
                // validate all disk baselines before ANY apply.
                for (const auto &[path, before] : baselines) {
                    auto &doc = openLocked(path);
                    work.generation = documentGenerationLocked();
                    if (doc.text != before->load())
                        throw NativeError(ErrorCode::RevisionConflict,
                                          "A prepared rename file changed before preview");
                    if (!doc.diskConflict.empty())
                        throw NativeError(ErrorCode::DiskConflict,
                                          "A prepared rename file conflicts with disk");
                }
                *renameEdits = {{"changes", std::move(collected)}};
            }
            if (issuedGeneration)
                *issuedGeneration = work.generation;
        }
        const bool complete = work.records.size() == work.paths.size();
        publish(complete ? "completed" : "pending");
        checkpoint();
        std::lock_guard lock(analysisStateMutex_);
        analysisState_["state"] = complete ? "completed" : "pending";
        if (complete)
            analysisState_["progress"] = 100;
        auto result = analysisState_;
        result["documents"] = Json::array();
        for (size_t i = 0; i < std::min<size_t>(128, work.records.size()); ++i)
            result["documents"].push_back(work.records.at(i));
        result["scope"] = work.scope;
        result["wholeProjectIndexCompleteness"] = "unknown";
        result["continuation"] = complete ? "" : work.id;
        result["nextOffset"] = result.at("documents").size();
        return result;
    } catch (const std::exception &error) {
        cleanup();
        if (!work.id.empty()) {
            publish("failed");
            try {
                checkpoint();
            } catch (const std::exception &storageError) {
                std::lock_guard lock(analysisStateMutex_);
                analysisState_["checkpointError"] = storageError.what();
                analysisSession_ = work;
            }
        }
        std::lock_guard lock(analysisStateMutex_);
        const auto *native = dynamic_cast<const NativeError *>(&error);
        analysisState_["state"] = native && native->code() == ErrorCode::Cancelled ? "cancelled" : "failed";
        analysisState_["phase"] = analysisState_["state"];
        analysisState_["error"] = error.what();
        throw;
    }
}
} // namespace cycode
