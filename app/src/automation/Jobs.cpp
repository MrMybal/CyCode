#include "Internal.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cycode/Paths.h>
#ifdef _WIN32
#include <windows.h>
#endif

namespace cycode::automation {
thread_local bool withinJob = false;
namespace {
std::string stamp() {
    const auto now = std::time(nullptr);
    std::tm value{};
#ifdef _WIN32
    gmtime_s(&value, &now);
#else
    gmtime_r(&now, &value);
#endif
    std::ostringstream text;
    text << std::put_time(&value, "%Y-%m-%dT%H:%M:%SZ");
    return text.str();
}
bool safeId(const std::string& value) {
    return !value.empty() && value.size() <= 100 &&
        std::all_of(value.begin(), value.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '-' || c == '_';
        });
}
void ordinary(const std::filesystem::path& path) {
    for (auto part = path; !part.empty(); part = part.parent_path()) {
        if (std::filesystem::is_symlink(part)) throw cymcp::Error("AccessDenied", "Redirected artifact path");
#ifdef _WIN32
        const auto attributes = GetFileAttributesW(part.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            throw cymcp::Error("AccessDenied", "Redirected artifact path");
#endif
        if (part == part.parent_path()) break;
    }
}
std::filesystem::path scoped(const std::filesystem::path& path, const std::filesystem::path& root) {
    if (root.empty() || !path.is_absolute() || !root.is_absolute())
        throw cymcp::Error("AccessDenied", "An explicit absolute host import/export root is required");
    ordinary(path); ordinary(root);
    const auto relative = path.lexically_normal().lexically_relative(root.lexically_normal());
    if (relative.empty() || relative.has_root_path() || *relative.begin() == "..")
        throw cymcp::Error("AccessDenied", "Artifact path is outside the configured host root");
    return path.lexically_normal();
}
void copyArtifact(const std::filesystem::path& source, const std::filesystem::path& target, std::size_t& budget) {
    ordinary(source); ordinary(target);
    if (!std::filesystem::is_regular_file(source)) throw cymcp::Error("MissingInput", "Only regular artifact files can be copied");
    const auto size = std::filesystem::file_size(source);
    const auto modified = std::filesystem::last_write_time(source);
    if (size > budget) throw cymcp::Error("CapacityExceeded", "Artifact bundle exceeds its configured byte budget");
    std::filesystem::create_directories(target.parent_path());
    std::ifstream input(source, std::ios::binary);
    if (!input) throw cymcp::Error("FileError", "Cannot read artifact");
    AtomicFileOptions options; options.access = AtomicAccess::PrivateUser;
    options.publication = AtomicPublication::CreateExclusive;
    writeFileAtomicStream(target, [&](const AtomicByteSink& output) {
        char buffer[65536]; std::size_t copied = 0;
        while (input) {
            input.read(buffer, sizeof(buffer)); const auto count = static_cast<std::size_t>(input.gcount());
            if (count > size - copied) throw cymcp::Error("InputChanged", "Source artifact grew during copying");
            output(std::string_view(buffer, count)); copied += count;
        }
        if (!input.eof() || copied != size || std::filesystem::file_size(source) != size ||
            std::filesystem::last_write_time(source) != modified)
            throw cymcp::Error("InputChanged", "Source artifact changed during copying");
    }, options);
    budget -= static_cast<std::size_t>(size);
}
Json rewriteArtifacts(Json value, const std::filesystem::path& source, const std::filesystem::path& target) {
    if (value.is_object()) for (auto& item : value.items()) item.value() = rewriteArtifacts(item.value(), source, target);
    else if (value.is_array()) for (auto& item : value) item = rewriteArtifacts(item, source, target);
    else if (value.is_string()) {
        const auto path = utf8Path(value.get<std::string>());
        const auto relative = path.lexically_relative(source);
        if (path.is_absolute() && !relative.empty() && !relative.is_absolute() && *relative.begin() != "..") value = pathUtf8(target / relative);
    }
    return value;
}
bool canCancel(const Json& operation) {
    return operation.value("canCancel", true);
}
std::size_t ram(const Json& record) { return record.value("reservedRamBytes", std::size_t(0)); }
constexpr std::size_t pendingDiskReservation = 64ULL * 1024 * 1024;
int priority(const Json& record) {
    const std::vector<std::string> names{"Interactive", "Critical", "High", "Normal", "Background", "Low"};
    const auto found = std::find(names.begin(), names.end(), record.value("priority", "Normal"));
    return static_cast<int>(std::distance(names.begin(), found));
}
}

Jobs::Jobs(const Options& options, DomainInvoker invoke, std::function<Json(const std::string&)> describe)
    : root_(options.state), invoke_(std::move(invoke)), describe_(std::move(describe)),
      workers_(options.workers), queueCapacity_(options.queueCapacity), diskLimit_(options.jobDiskBytes),
      ramLimit_(options.jobRamBytes), importRoot_(options.importRoot), exportRoot_(options.exportRoot) {
    cymcp::private_directory(root_);
    cymcp::private_directory(root_ / "jobs");
    if (std::filesystem::exists(root_ / "state.sqlite3") && !std::filesystem::exists(root_ / "legacy-import.json"))
        throw cymcp::Error("MigrationRequired", "Legacy jobs are preserved. Run app/scripts/migrate_legacy_jobs.py --state-dir on this directory before opening it natively.");
    lease_ = std::make_unique<NativeFileLease>(root_ / "automation.lock");
    if (!lease_->acquired()) throw cymcp::Error("Busy", "This automation state directory has a live owner");
    if (std::filesystem::exists(root_ / "identities.json")) {
        const auto saved = readJson(root_ / "identities.json");
        if (saved.value("format", "") != "cycode-automation-identities/1")
            throw cymcp::Error("InvalidState", "Unknown automation identity format");
        for (const auto& row : saved.value("clients", Json::array())) clients_[row.at("clientId")] = row;
        for (const auto& row : saved.value("sessions", Json::array())) sessions_[row.at("sessionId")] = row;
    }
    // Migration imports immutable records only. Never restart a queued/running action.
    if (std::filesystem::exists(root_ / "legacy-import.json")) {
        const auto imported = readJson(root_ / "legacy-import.json", 128 * 1024 * 1024);
        if (imported.value("format", "") != "cycode-legacy-jobs/1")
            throw cymcp::Error("InvalidState", "Unknown legacy job export");
        for (const auto& row : imported.value("clients", Json::array()))
            if (safeId(row.at("clientId"))) clients_.try_emplace(row.at("clientId"), row);
        for (const auto& row : imported.value("sessions", Json::array()))
            if (safeId(row.at("sessionId"))) sessions_.try_emplace(row.at("sessionId"), row);
        for (auto row : imported.value("jobs", Json::array())) {
            const auto id = row.at("jobId").get<std::string>();
            if (!safeId(id)) throw cymcp::Error("InvalidState", "Unsafe legacy job identity");
            const auto file = root_ / "jobs" / (id + ".json");
            if (!std::filesystem::exists(file)) {
                if (!terminal(row.value("state", ""))) {
                    row["state"] = "Failed";
                    row["error"] = {{"code", "Interrupted"}, {"message", "Legacy action interrupted; never automatically replayed"}};
                }
                row["legacyImported"] = true;
                row["reservedRamBytes"] = 0;
                saveJson(file, row);
            }
        }
        persistIdentities();
    }
    for (const auto& file : std::filesystem::directory_iterator(root_ / "jobs")) {
        if (file.path().extension() != ".json") continue;
        auto entry = std::make_shared<Entry>();
        entry->record = readJson(file.path(), 64 * 1024 * 1024);
        const auto id = entry->record.at("jobId").get<std::string>();
        if (!safeId(id) || file.path().stem() != id || !entry->record.contains("clientId"))
            throw cymcp::Error("InvalidState", "Invalid durable job identity");
        if (!terminal(entry->record.value("state", ""))) {
            entry->record["state"] = "Failed";
            entry->record["error"] = {{"code", "Interrupted"}, {"message", "Previous process stopped; action was not replayed"}};
            entry->record["completedAt"] = stamp();
            persist(*entry);
        }
        jobs_.emplace(id, std::move(entry));
    }
    executor_ = std::make_unique<cymcp::Executor>(workers_, queueCapacity_);
    timer_ = std::jthread([this](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard lock(mutex_);
            if (stopping_) return;
            try { schedule(); } catch (...) { /* Preserve admitted identities; status/result access remains available. */ }
        }
    });
}
Jobs::~Jobs() { stop(); }
void Jobs::persist(const Entry& entry) {
    saveJson(root_ / "jobs" / (entry.record.at("jobId").get<std::string>() + ".json"), entry.record);
}
void Jobs::persistIdentities() {
    Json clients = Json::array(), sessions = Json::array();
    for (const auto& [_, row] : clients_) clients.push_back(row);
    for (const auto& [_, row] : sessions_) sessions.push_back(row);
    saveJson(root_ / "identities.json", {{"format", "cycode-automation-identities/1"},
        {"clients", clients}, {"sessions", sessions}});
}
void Jobs::emit(const std::string& type, const std::string& owner, Json data) {
    events_.push_back({{"sequence", ++sequence_}, {"type", type}, {"timestamp", stamp()},
                       {"clientId", owner}, {"data", std::move(data)}});
    if (events_.size() > 2000) events_.erase(events_.begin(), events_.begin() + 100);
}
std::shared_ptr<Jobs::Entry> Jobs::owned(const std::string& id, const std::string& owner) {
    const auto found = jobs_.find(id);
    if (found == jobs_.end() || found->second->record.value("clientId", "") != owner)
        throw cymcp::Error("AccessDenied", "Job is unknown or belongs to another client");
    return found->second;
}
Json Jobs::submit(const std::string& operation, Json parameters, const cymcp::CallContext& context, Json options) {
    const auto descriptor = describe_(operation);
    validateContract(parameters, descriptor.at("inputSchema"));
    context.cancellation.check();
    const std::set<std::string> priorities{"Interactive", "Critical", "High", "Normal", "Background", "Low"};
    if (!priorities.contains(options.value("priority", "Normal")))
        throw cymcp::Error("InvalidParameters", "Unknown job priority");
    for (const auto* key : {"executionTimeout", "queueTimeout"})
        if (options.contains(key) && (!options[key].is_number() || options[key].get<double>() < 0 || options[key].get<double>() > 86400))
            throw cymcp::Error("InvalidParameters", "Job timeouts must be between zero and 86400 seconds");
    std::lock_guard lock(mutex_);
    if (stopping_) throw cymcp::Error("ServiceStopping", "The automation owner is stopping");
    if (options.contains("sessionId")) {
        const auto found = sessions_.find(options.at("sessionId").get<std::string>());
        if (found == sessions_.end() || found->second.value("clientId", "") != context.principal ||
            found->second.value("closed", false) ||
            found->second.value("expiresAt", timestamp() + 1) <= timestamp())
            throw cymcp::Error("AccessDenied", "Task is unknown, closed or belongs to another client");
    }
    std::size_t unfinished = 0, stored = 0;
    for (const auto& [_, entry] : jobs_) {
        if (!terminal(entry->record.value("state", ""))) ++unfinished;
        stored += cymcp::serialized_size(entry->record);
    }
    if (unfinished >= queueCapacity_ || stored > diskLimit_ ||
        (unfinished + 1) > (diskLimit_ - stored) / pendingDiskReservation ||
        std::filesystem::space(root_).available < pendingDiskReservation * (unfinished + 1))
        throw cymcp::Error("CapacityExceeded", "Durable job storage/queue is full; export results or adjust the host quota before submitting");
    const auto requiredRam = descriptor.value("resourceRequirements", Json::object()).value("ramBytes", std::size_t(0));
    if (requiredRam > ramLimit_) throw cymcp::Error("InsufficientRAM", "Job exceeds the host RAM reservation budget");
    auto entry = std::make_shared<Entry>();
    const auto id = cymcp::random_token(16);
    entry->context = context;
    entry->context.cancellation = {};
    entry->cancellable = canCancel(descriptor);
    entry->record = {{"jobId", id}, {"operationId", operation}, {"clientId", context.principal},
        {"sessionId", options.value("sessionId", "")}, {"state", "Queued"}, {"parameters", std::move(parameters)},
        {"outputs", Json::object()}, {"error", nullptr}, {"progress", nullptr}, {"createdAt", stamp()},
        {"startedAt", nullptr}, {"completedAt", nullptr}, {"updatedAt", stamp()}, {"queuedTimestamp", timestamp()},
        {"priority", options.value("priority", "Normal")}, {"reservedRamBytes", requiredRam},
        {"exclusiveGroups", descriptor.value("exclusiveGroups", Json::array())},
        {"backendId", options.value("backendId", "")}, {"modelId", options.value("modelId", "")},
        {"canCancel", entry->cancellable}, {"durable", true}, {"visibility", "Owner"}};
    for (const auto* key : {"executionTimeout", "queueTimeout"})
        if (options.contains(key)) entry->record[key] = options[key];
    persist(*entry); // No work is admitted until the durable identity is published.
    jobs_.emplace(id, entry);
    emit("JobQueued", context.principal, {{"jobId", id}});
    schedule();
    return entry->record;
}
void Jobs::schedule() {
    for (const auto& [_, entry] : jobs_) {
        if (entry->record.value("state", "") != "Queued") continue;
        const auto timeout = entry->record.value("queueTimeout", 0.0);
        const bool expired = timeout > 0 && timestamp() - entry->record.value("queuedTimestamp", timestamp()) >= timeout;
        if (expired || entry->context.cancellation.cancelled()) {
            entry->record["state"] = "Cancelled";
            entry->record["error"] = {{"code", expired ? "QueueTimeout" : "Cancelled"}, {"message", "Job stopped before native execution"}};
            entry->record["completedAt"] = stamp(); persist(*entry); changed_.notify_all();
        }
    }
    while (!stopping_ && running_ < workers_) {
        std::shared_ptr<Entry> next;
        std::set<std::string> activeGroups;
        for (const auto& [_, entry] : jobs_) if (entry->record.value("state", "") == "Running")
            for (const auto& group : entry->record.value("exclusiveGroups", Json::array()))
                activeGroups.insert(group.get<std::string>());
        for (const auto& [_, entry] : jobs_) {
            if (entry->record.value("state", "") != "Queued") continue;
            const auto queueTimeout = entry->record.value("queueTimeout", 0.0);
            const bool expired = queueTimeout > 0 && timestamp() - entry->record.value("queuedTimestamp", timestamp()) >= queueTimeout;
            if (entry->context.cancellation.cancelled() || expired) {
                entry->record["state"] = "Cancelled";
                entry->record["error"] = {{"code", expired ? "QueueTimeout" : "Cancelled"}, {"message", "Job stopped before native execution"}};
                entry->record["completedAt"] = stamp(); persist(*entry); changed_.notify_all();
                continue;
            }
            bool blocked = false;
            for (const auto& group : entry->record.value("exclusiveGroups", Json::array()))
                blocked |= activeGroups.contains(group.get<std::string>());
            if (blocked || ram(entry->record) > ramLimit_ - reservedRam_) continue;
            if (!next || priority(entry->record) < priority(next->record) ||
                (priority(entry->record) == priority(next->record) &&
                 entry->record.at("createdAt") < next->record.at("createdAt"))) next = entry;
        }
        if (!next) return;
        next->record["state"] = "Running";
        next->record["startedAt"] = stamp();
        persist(*next);
        ++running_; reservedRam_ += ram(next->record);
        executor_->post([this, next] { execute(next); });
    }
}
void Jobs::execute(std::shared_ptr<Entry> entry) {
    Json record;
    { std::lock_guard lock(mutex_); record = entry->record; }
    Json outputs, error = nullptr;
    std::string state = "Completed";
    try {
        withinJob = true;
        auto context = entry->context;
        if (entry->cancellable && record.contains("executionTimeout")) {
            const auto seconds = record["executionTimeout"].get<double>();
            if (seconds > 0) context.cancellation.deadline = cymcp::Clock::now() +
                std::chrono::milliseconds(static_cast<long long>(seconds * 1000));
        }
        context.events = [this, entry](const cymcp::Event& event) {
            std::lock_guard lock(mutex_);
            if (event.type == "Progress") entry->record["progress"] = event.data;
            emit(event.type, entry->context.principal, {{"jobId", entry->record["jobId"]}, {"value", event.data}});
        };
        outputs = invoke_(record.at("operationId"), record.at("parameters"), context);
        withinJob = false;
    } catch (const OperationError& failure) {
        withinJob = false;
        error = failure.native;
        error["recoverable"] = true;
        state = failure.code == "Cancelled" ? "Cancelled" : "Failed";
    } catch (const cymcp::Error& failure) {
        withinJob = false;
        error = {{"code", failure.code}, {"message", failure.what()}, {"recoverable", true}};
        state = failure.code == "Cancelled" ? "Cancelled" : "Failed";
    } catch (const std::exception& failure) {
        withinJob = false;
        error = {{"code", "OperationFailed"}, {"message", failure.what()}, {"recoverable", false}};
        state = "Failed";
    }
    std::lock_guard lock(mutex_);
    entry->record["state"] = state;
    entry->record["outputs"] = outputs.is_null() ? Json::object() : std::move(outputs);
    entry->record["error"] = std::move(error);
    entry->record["completedAt"] = entry->record["updatedAt"] = stamp();
    // Persist successful mutations before exposing their result. Cancellation or
    // transport loss never causes the application to replay the operation.
    try { persist(*entry); }
    catch (const std::exception& failure) {
        entry->record["persistenceError"] = failure.what();
        entry->record["durable"] = false;
        // Keep the actual terminal output in this owner; do not replace success
        // with a generic error which could encourage a mutation retry.
    }
    --running_; reservedRam_ -= ram(entry->record);
    emit("Job" + state, entry->context.principal, {{"jobId", entry->record["jobId"]}});
    changed_.notify_all();
    schedule();
}
Json Jobs::wait(const std::string& id, const cymcp::CallContext& context) {
    std::unique_lock lock(mutex_);
    auto entry = owned(id, context.principal);
    while (!terminal(entry->record.value("state", ""))) {
        bool cancelled = context.cancellation.cancelled();
        try { context.cancellation.check(); } catch (const cymcp::Error&) { cancelled = true; }
        if (cancelled && entry->cancellable) entry->context.cancellation.cancel();
        changed_.wait_for(lock, std::chrono::milliseconds(50));
    }
    return entry->record;
}
Json Jobs::call(const std::string& method, const Json& p, const cymcp::CallContext& context) {
    if (method == "SubmitJob") return submit(p.at("operationId"), p.value("parameters", Json::object()), context, p);
    std::unique_lock lock(mutex_);
    const auto owner = context.principal;
    if (method == "GetJob") return owned(p.at("jobId"), owner)->record;
    if (method == "ListJobs") {
        Json rows = Json::array();
        for (const auto& [_, entry] : jobs_) if (entry->record.value("clientId", "") == owner) rows.push_back(entry->record);
        return rows;
    }
    if (method == "CancelJob") {
        auto entry = owned(p.at("jobId"), owner);
        if (terminal(entry->record.value("state", ""))) return entry->record;
        if (!entry->cancellable && entry->record.value("state", "") != "Queued")
            throw cymcp::Error("CannotCancel", "Native transaction has no cancellable phase; recover its durable result instead of replaying");
        entry->context.cancellation.cancel();
        if (entry->record.value("state", "") == "Queued") {
            entry->record["state"] = "Cancelled";
            entry->record["error"] = {{"code", "Cancelled"}, {"message", "Cancelled before native execution"}};
            entry->record["completedAt"] = stamp(); persist(*entry); changed_.notify_all();
        }
        return entry->record;
    }
    if (method == "CreateClient") {
        if (owner != "local-owner") throw cymcp::Error("AccessDenied", "Only the local owner can issue credentials");
        const auto id = cymcp::random_token(16), token = cymcp::random_token(32);
        clients_[id] = {{"clientId", id}, {"name", p.value("name", "local")},
            {"permissions", p.value("permissions", Json::array())}, {"tokenHash", cymcp::sha256(token)}};
        persistIdentities();
        return {{"clientId", id}, {"token", token}};
    }
    if (method == "Authenticate") {
        const auto digest = cymcp::sha256(p.at("token").get<std::string>());
        for (const auto& [_, row] : clients_)
            if (cymcp::constant_time_equal(row.value("tokenHash", ""), digest))
                return {{"clientId", row.at("clientId")}, {"permissions", row.value("permissions", Json::array())}};
        throw cymcp::Error("AuthenticationRequired", "Invalid client credential");
    }
    if (method == "CreateSession") {
        const auto id = cymcp::random_token(16);
        Json row = {{"sessionId", id}, {"clientId", owner}, {"createdAt", stamp()}, {"closed", false},
            {"temporary", p.value("temporary", true)}, {"retentionSeconds", p.value("retentionSeconds", 3600)},
            {"expiresAt", timestamp() + p.value("retentionSeconds", 3600)}};
        sessions_[id] = row; persistIdentities(); return row;
    }
    if (method == "KeepTaskAlive" || method == "FinishTask" || method == "CloseSession") {
        const auto id = p.at("sessionId").get<std::string>();
        auto found = sessions_.find(id);
        if (found == sessions_.end() || found->second.value("clientId", "") != owner)
            throw cymcp::Error("AccessDenied", "Task belongs to another client");
        if (method == "KeepTaskAlive") {
            found->second["expiresAt"] = timestamp() + found->second.value("retentionSeconds", 3600);
        } else {
            for (const auto& [_, entry] : jobs_) if (entry->record.value("sessionId", "") == id &&
                    !terminal(entry->record.value("state", ""))) {
                if (!p.value("cancelRunning", false)) throw cymcp::Error("Busy", "Wait for active jobs before finishing the task");
                if (!entry->cancellable) throw cymcp::Error("CannotCancel", "An active transaction must finish first");
                entry->context.cancellation.cancel();
            }
            found->second["closed"] = true;
            found->second["durableResultsRetained"] = true;
        }
        persistIdentities(); return found->second;
    }
    if (method == "GetEvents") {
        Json rows = Json::array();
        const auto after = p.value("after", std::uint64_t(0));
        for (const auto& row : events_) if (row.at("sequence").get<std::uint64_t>() > after && row.at("clientId") == owner) rows.push_back(row);
        return {{"events", rows}, {"nextSequence", sequence_}};
    }
    if (method == "GetRuntimeStatus") {
        std::size_t queued = 0;
        for (const auto& [_, entry] : jobs_) if (entry->record.value("state", "") == "Queued") ++queued;
        return {{"state", stopping_ ? "Stopping" : "Running"}, {"jobs", {{"running", running_}, {"queued", queued}}},
            {"resources", {{"available", {{"ramBytes", ramLimit_ - reservedRam_}, {"cpuThreads", workers_}}}}},
            {"sdk", "CyMCPSDK"}, {"sdkVersion", cymcp::version}, {"durableJobs", true}};
    }
    if (method == "ImportFile") {
        const auto sessionId = p.at("sessionId").get<std::string>();
        const auto session = sessions_.find(sessionId);
        if (session == sessions_.end() || session->second.value("clientId", "") != owner ||
            session->second.value("closed", false) || session->second.value("expiresAt", timestamp()+1) <= timestamp())
            throw cymcp::Error("AccessDenied", "Unknown, expired or closed input task");
        const auto source = scoped(utf8Path(p.at("source").get<std::string>()), importRoot_);
        std::uintmax_t retained = 0, count = 0;
        if (std::filesystem::exists(root_ / "inputs")) for (const auto& file : std::filesystem::recursive_directory_iterator(root_ / "inputs")) {
            ordinary(file.path());
            if (++count > 10000) throw cymcp::Error("CapacityExceeded", "Input retention has too many entries; export/archive it explicitly");
            if (file.is_regular_file()) retained += file.file_size();
        }
        if (retained >= diskLimit_) throw cymcp::Error("CapacityExceeded", "Input retention quota exhausted");
        const auto bundle = root_ / "inputs" / sessionId / cymcp::random_token(16);
        cymcp::private_directory(bundle);
        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> files{{source, source.filename()}};
        for (const auto& item : p.value("companions", Json::array())) {
            const auto relative = utf8Path(item.get<std::string>()).lexically_normal();
            if (relative.empty() || relative.has_root_path() || *relative.begin() == "..")
                throw cymcp::Error("InvalidParameters", "Companions must be relative to the source directory");
            files.emplace_back(scoped(source.parent_path() / relative, importRoot_), relative);
        }
        std::size_t budget = (std::min)({diskLimit_ - static_cast<std::size_t>(retained), std::size_t(64 * 1024 * 1024),
            static_cast<std::size_t>(std::filesystem::space(root_).available)});
        Json rows = Json::array();
        for (const auto& [file, relative] : files) {
            const auto target = bundle / relative;
            copyArtifact(file, target, budget);
            rows.push_back({{"path", pathUtf8(target)}, {"sizeBytes", std::filesystem::file_size(target)}, {"sha256", cymcp::sha256_file(target)}});
        }
        session->second["inputDirectory"] = pathUtf8(root_ / "inputs" / sessionId);
        persistIdentities();
        return {{"sessionId", sessionId}, {"path", pathUtf8(bundle / source.filename())},
            {"relativePath", pathUtf8((bundle / source.filename()).lexically_relative(root_))}, {"files", rows}, {"temporary", false}};
    }
    if (method == "ExportJob") {
        auto entry = owned(p.at("jobId"), owner);
        if (!terminal(entry->record.value("state", ""))) throw cymcp::Error("Busy", "Only terminal jobs can be exported");
        const auto destination = scoped(utf8Path(p.at("destination").get<std::string>()), exportRoot_);
        if (std::filesystem::exists(destination) && !std::filesystem::is_directory(destination))
            throw cymcp::Error("InvalidParameters", "Export destination must be a directory");
        const auto target = destination / ("cycode-" + entry->record.at("jobId").get<std::string>() + "-" + cymcp::random_token(16));
        cymcp::private_directory(target);
        Json outputs = entry->record.at("outputs");
        if (entry->record.contains("workspace")) {
            const auto source = utf8Path(entry->record.at("workspace").get<std::string>());
            const auto expected = root_ / "Workspaces" / owner / entry->record.at("jobId").get<std::string>();
            if (source.lexically_normal() != expected.lexically_normal()) throw cymcp::Error("AccessDenied", "Legacy artifact workspace identity mismatch");
            ordinary(source);
            std::size_t budget = diskLimit_, count = 0;
            if (std::filesystem::exists(source)) for (const auto& file : std::filesystem::recursive_directory_iterator(source)) {
                ordinary(file.path());
                if (++count > 10000) throw cymcp::Error("CapacityExceeded", "Artifact export has too many entries");
                if (file.is_regular_file()) copyArtifact(file.path(), target / "artifacts" / file.path().lexically_relative(source), budget);
            }
            outputs = rewriteArtifacts(outputs, source, target / "artifacts");
        }
        auto record = entry->record; record["outputs"] = outputs;
        saveJson(target / "job.json", record);
        return {{"jobId", entry->record.at("jobId")}, {"directory", pathUtf8(target)}, {"outputs", outputs}, {"result", "job.json"}};
    }
    if (method == "ReleaseResources") return {{"released", false}, {"note", "Live documents and durable results are retained by their native owners"}};
    throw cymcp::Error("UnknownMethod", "Unknown job control: " + method);
}
void Jobs::stop() {
    {
        std::lock_guard lock(mutex_);
        if (stopping_) return;
        stopping_ = true;
        for (const auto& [_, entry] : jobs_) if (!terminal(entry->record.value("state", ""))) {
            if (entry->cancellable) entry->context.cancellation.cancel();
            if (entry->record.value("state", "") == "Queued") {
                entry->record["state"] = "Cancelled";
                entry->record["error"] = {{"code", "Cancelled"}, {"message", "Owner stopped before execution"}};
                entry->record["completedAt"] = stamp();
                try { persist(*entry); } catch (...) {}
            }
        }
        changed_.notify_all();
    }
    if (timer_.joinable()) { timer_.request_stop(); timer_.join(); }
    executor_.reset(); // Joins the real workers; never terminates a transaction thread.
}
} // namespace cycode::automation
