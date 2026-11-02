#include "cycode/LanguageClient.h"
#include "cycode/Tools.h"
#include <array>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace cycode {
struct LanguageClient::Child {
#ifdef _WIN32
    HANDLE input = nullptr, output = nullptr, errors = nullptr, process = nullptr, job = nullptr;
#else
    int input = -1, output = -1, errors = -1;
    pid_t process = -1;
#endif
};
LanguageClient::LanguageClient() = default;
LanguageClient::~LanguageClient() {
    stop();
}
std::string LanguageClient::uri(const std::filesystem::path &path) {
    auto raw = pathUtf8(std::filesystem::absolute(path));
    std::ostringstream result;
    result << "file://";
    if (!raw.starts_with('/'))
        result << '/';
    for (unsigned char c : raw) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
            c == ':' || c == '-' || c == '_' || c == '.' || c == '~')
            result << c;
        else
            result << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
                   << static_cast<int>(c);
    }
    return result.str();
}
size_t LanguageClient::utf16Length(const std::string &text) {
    size_t units = 0;
    for (unsigned char c : text) {
        if ((c & 0xc0) == 0x80)
            continue;
        units += c >= 0xf0 ? 2 : 1;
    }
    return units;
}
std::filesystem::path LanguageClient::fromUri(const std::string &uri) {
    if (!uri.starts_with("file:///"))
        throw std::runtime_error("Only local file URIs are supported");
    std::string path;
    for (size_t i = 7; i < uri.size(); ++i) {
        if (uri[i] != '%') {
            if (uri[i] == '?' || uri[i] == '#')
                throw std::runtime_error("Invalid file URI");
            path += uri[i];
            continue;
        }
        auto hex = [](char c) -> int {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;
            if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;
            return -1;
        };
        if (i + 2 >= uri.size() || hex(uri[i + 1]) < 0 || hex(uri[i + 2]) < 0)
            throw std::runtime_error("Invalid URI escape");
        char c = static_cast<char>(hex(uri[i + 1]) * 16 + hex(uri[i + 2]));
        if (!c)
            throw std::runtime_error("Invalid URI character");
        path += c;
        i += 2;
    }
#ifdef _WIN32
    if (path.size() >= 3 && path[0] == '/' && path[2] == ':')
        path.erase(0, 1);
#endif
    return utf8Path(path);
}
bool LanguageClient::start(const std::filesystem::path &root, const std::string &executable,
                           const std::filesystem::path &compilationDatabase) {
    waitForWorkspace_ = false;
    Json settings = Json::object();
    if (!compilationDatabase.empty()) settings["compilationDatabasePath"] = pathUtf8(compilationDatabase);
    return startImpl(root, {executable, "--background-index", "--log=error", "--header-insertion=never", "--completion-style=detailed"}, settings, false);
}
bool LanguageClient::startServer(const std::filesystem::path &root, const std::vector<std::string> &argv, Json configuration, bool waitForWorkspace) {
    waitForWorkspace_ = waitForWorkspace;
    return startImpl(root, argv, std::move(configuration), false);
}
bool LanguageClient::startRaw(const std::filesystem::path &root, const std::string &executable) { waitForWorkspace_ = false; return startImpl(root, {executable}, Json::object(), true); }
bool LanguageClient::startRaw(const std::filesystem::path &root, const std::vector<std::string> &argv, Json environment) { waitForWorkspace_ = false; return startImpl(root,argv,Json::object(),true,environment); }
std::deque<Json> LanguageClient::receiveRaw() {
    std::lock_guard lock(mutex_); std::deque<Json> result; result.swap(incoming_); incomingBytes_ = 0; return result;
}
bool LanguageClient::startImpl(const std::filesystem::path &root, const std::vector<std::string> &argv,
                              Json configuration, bool raw,const Json &overrides) {
    stop();
    workspaceReady_ = false; workspaceLoadToken_ = nullptr;
    if (argv.empty() || argv.front().empty()) { error_ = "Language server executable is missing"; return false; }
    configuration_ = std::move(configuration);
    workspaceFolders_ = Json::array({{{"uri",uri(root)},{"name",pathUtf8(root.filename())}}});
    raw_ = raw; incomingBytes_ = 0;
    child_ = std::make_unique<Child>();
    initialized_ = false;
    diagnostics_ = Json::object();
    ++diagnosticGeneration_;
    {
        std::lock_guard lock(mutex_);
        error_.clear();
        incoming_.clear();
        outgoing_.clear();
        outgoingBytes_ = 0;
    }
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE childIn = nullptr, childOut = nullptr, childErr = nullptr;
    if (!CreatePipe(&childIn, &child_->input, &security, 0) ||
        !CreatePipe(&child_->output, &childOut, &security, 0) ||
        !CreatePipe(&child_->errors, &childErr, &security, 0)) {
        error_ = "Cannot create language server pipes";
        stop();
        return false;
    }
    SetHandleInformation(child_->input, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(child_->output, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(child_->errors, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = childIn;
    si.hStdOutput = childOut;
    si.hStdError = childErr;
    PROCESS_INFORMATION process{};
    std::wstring command;
    for (const auto &arg : argv) {
        if (!command.empty()) command += L' ';
        command += L'"'; size_t slashes = 0;
        // Windows CRT argv quoting, including quotes and trailing backslashes.
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, arg.data(), int(arg.size()), nullptr, 0);
        std::wstring wide(size, 0); MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, arg.data(), int(arg.size()), wide.data(), size);
        for (wchar_t c : wide) {
            if (c == L'\\') { ++slashes; continue; }
            command.append(c == L'"' ? slashes*2+1 : slashes, L'\\'); slashes = 0; command += c;
        }
        command.append(slashes*2,L'\\'); command += L'"';
    }
    child_->job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit{};
    limit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (child_->job)
        SetInformationJobObject(child_->job, JobObjectExtendedLimitInformation, &limit, sizeof(limit));
    // Windows accepts duplicate Path/PATH entries; MSBuild/.NET reject them.
    auto insensitive = [](const std::wstring &a,const std::wstring &b) { return _wcsicmp(a.c_str(),b.c_str()) < 0; };
    std::map<std::wstring,std::wstring,decltype(insensitive)> variables(insensitive);
    if (auto inherited = GetEnvironmentStringsW()) {
        for (auto entry = inherited; *entry; entry += wcslen(entry)+1) {
            std::wstring value(entry); const auto separator = value.find(L'=',1);
            if (separator != std::wstring::npos) variables[value.substr(0,separator)] = value;
        }
        FreeEnvironmentStringsW(inherited);
    }
    auto wideText = [](const std::string &text) {
        const int count = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);
        std::wstring value(count,0); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),value.data(),count); return value;
    };
    for (auto i = overrides.begin(); i != overrides.end(); ++i) {
        const auto key = wideText(i.key());
        if (i->is_null()) variables.erase(key);
        else variables[key] = key+L"="+wideText(i->get<std::string>());
    }
    std::vector<wchar_t> environment;
    for (const auto &[key,value] : variables) { environment.insert(environment.end(),value.begin(),value.end()); environment.push_back(0); }
    environment.push_back(0);
    bool created =
        CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                       environment.data(), root.c_str(), &si, &process) != FALSE;
    CloseHandle(childIn);
    CloseHandle(childOut);
    CloseHandle(childErr);
    if (!created) {
        error_ = "Cannot start language server: " + argv.front();
        stop();
        return false;
    }
    child_->process = process.hProcess;
    if (child_->job)
        AssignProcessToJobObject(child_->job, process.hProcess);
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
#else
    int in[2], out[2], err[2];
    if (pipe(in) || pipe(out) || pipe(err)) {
        error_ = "Cannot create language server pipes";
        stop();
        return false;
    }
    auto pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        dup2(in[0], 0);
        dup2(out[1], 1);
        dup2(err[1], 2);
        close(in[0]);
        close(in[1]);
        close(out[0]);
        close(out[1]);
        close(err[0]);
        close(err[1]);
        if (chdir(root.c_str()))
            _exit(126);
        for (auto i = overrides.begin(); i != overrides.end(); ++i) {
            if (i->is_null()) unsetenv(i.key().c_str());
            else setenv(i.key().c_str(),i->get_ref<const std::string &>().c_str(),1);
        }
        std::vector<char *> arguments;
        for (const auto &arg : argv) arguments.push_back(const_cast<char *>(arg.c_str()));
        arguments.push_back(nullptr);
        execvp(arguments.front(), arguments.data());
        _exit(127);
    }
    close(in[0]);
    close(out[1]);
    close(err[1]);
    child_->input = in[1];
    child_->output = out[0];
    child_->errors = err[0];
    child_->process = pid;
    if (pid < 0) {
        error_ = "Cannot start language server";
        stop();
        return false;
    }
    setpgid(pid, pid);
    fcntl(child_->output, F_SETFL, O_NONBLOCK);
    fcntl(child_->errors, F_SETFL, O_NONBLOCK);
#endif
    running_ = true;
    reader_ = std::thread([this] { readLoop(); });
    writer_ = std::thread([this] { writeLoop(); });
    if (raw) return true;
    Json initialization = {
        {"processId", nullptr},
        {"rootUri", uri(root)},
        {"workspaceFolders",workspaceFolders_},
        {"initializationOptions",configuration_},
        {"capabilities",
         {{"general", {{"positionEncodings", Json::array({"utf-16"})}}},
          {"window",{{"workDoneProgress",true}}},
          {"workspace",{{"configuration",true},{"workspaceFolders",true},{"semanticTokens",{{"refreshSupport",true}}}}},
          {"textDocument", {{"completion", {{"completionItem", {{"snippetSupport", false}}}}},
                             {"semanticTokens",{{"dynamicRegistration",false},{"requests",{{"full",true}}},
                                 {"tokenTypes",Json::array({"namespace","type","class","enum","interface","struct","typeParameter","parameter","variable","property","enumMember","event","function","method","macro","keyword","modifier","comment","string","number","regexp","operator","decorator","bracket","label","unknown","concept"})},
                                 {"tokenModifiers",Json::array({"declaration","definition","readonly","static","deprecated","abstract","async","modification","documentation","defaultLibrary","deduced","virtual","dependentName","usedAsMutableReference","usedAsMutablePointer","constructorOrDestructor","userDefined","functionScope","classScope","fileScope","globalScope"})},
                                 {"formats",Json::array({"relative"})},{"overlappingTokenSupport",false},{"multilineTokenSupport",false},{"augmentsSyntaxTokens",true}}},
                             {"documentSymbol", {{"hierarchicalDocumentSymbolSupport", true}}}}}}}};
    request("initialize", std::move(initialization), [this](const Json &result) {
        if (!result.contains("error")) {
            if (result.value("capabilities", Json::object()).value("positionEncoding", "utf-16") !=
                "utf-16") {
                std::lock_guard lock(mutex_);
                error_ = "Unsupported language server position encoding";
                running_ = false;
                return;
            }
            initialized_ = true;
            capabilities_ = result.value("capabilities",Json::object());
            notify("initialized", Json::object());
            notify("workspace/didChangeConfiguration",{{"settings",configuration_}});
        }
        else { std::lock_guard lock(mutex_); error_ = "Language server initialization failed: " + result.dump(); running_ = false; }
    });
    return true;
}
void LanguageClient::stop() {
    capabilities_ = Json::object(); ++semanticGeneration_;
    running_ = false;
    outgoingReady_.notify_all();
    if (child_) {
#ifdef _WIN32
        if (child_->process) {
            if (child_->job)
                TerminateJobObject(child_->job, 0);
            else
                TerminateProcess(child_->process, 0);
            WaitForSingleObject(child_->process, 3000);
        }
#else
        if (child_->process > 0) {
            kill(-child_->process, SIGKILL);
            while (waitpid(child_->process, nullptr, 0) < 0 && errno == EINTR) {
            }
        }
#endif
    }
    if (writer_.joinable())
        writer_.join();
    if (reader_.joinable())
        reader_.join();
    if (child_) {
#ifdef _WIN32
        for (auto handle : {child_->input, child_->output, child_->errors, child_->process, child_->job})
            if (handle)
                CloseHandle(handle);
#else
        for (auto descriptor : {child_->input, child_->output, child_->errors})
            if (descriptor >= 0)
                close(descriptor);
#endif
    }
    child_.reset();
    callbacks_.clear();
    initialized_ = false;
    diagnostics_ = Json::object();
    ++diagnosticGeneration_;
}
std::string LanguageClient::error() const {
    std::lock_guard lock(mutex_);
    return error_;
}
bool LanguageClient::enqueue(Json message) {
    auto body = message.dump();
    std::lock_guard lock(mutex_);
    if (outgoing_.size() >= 128 || outgoingBytes_ + body.size() > 32 * 1024 * 1024)
        return false;
    outgoingBytes_ += body.size();
    outgoing_.push_back("Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body);
    outgoingReady_.notify_one();
    return true;
}
bool LanguageClient::notify(const std::string &method, Json params) {
    return running_ && enqueue({{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}});
}
int LanguageClient::request(const std::string &method, Json params,
                            std::function<void(const Json &)> callback) {
    if (!running_)
        return 0;
    int id = nextId_++;
    callbacks_[id] = std::move(callback);
    if (!enqueue({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}})) {
        callbacks_.erase(id);
        return 0;
    }
    return id;
}
void LanguageClient::cancel(int id) {
    callbacks_.erase(id);
    notify("$/cancelRequest", {{"id", id}});
}
void LanguageClient::poll() {
    std::deque<Json> messages;
    {
        std::lock_guard lock(mutex_);
        messages.swap(incoming_);
    }
    for (auto &message : messages) {
        if (message.contains("id") && message["id"].is_number_integer() && !message.contains("method")) {
            auto found = callbacks_.find(message["id"].get<int>());
            if (found != callbacks_.end()) {
                auto callback = std::move(found->second);
                callbacks_.erase(found);
                callback(message.contains("error") ? Json{{"error", message["error"]}}
                                                   : message.value("result", Json()));
            }
        } else if (message.value("method", std::string{}) == "textDocument/publishDiagnostics") {
            auto &params = message["params"];
            diagnostics_[params["uri"].get<std::string>()] = params["diagnostics"];
            ++diagnosticGeneration_;
        } else if (message.value("method",std::string{}) == "$/progress" && waitForWorkspace_) {
            const auto params = message.value("params",Json::object()), value = params.value("value",Json::object());
            const auto kind = value.value("kind","");
            if (kind == "begin" && value.value("title","").starts_with("Loading ")) {
                workspaceLoadToken_ = params.value("token",Json()); workspaceReady_ = false;
            } else if (kind == "end" && !workspaceLoadToken_.is_null() && params.value("token",Json()) == workspaceLoadToken_) {
                const auto message = value.value("message","");
                if (message.find("could not be loaded") != std::string::npos) { std::lock_guard lock(mutex_); error_ = message; running_ = false; }
                else workspaceReady_ = true;
                workspaceLoadToken_ = nullptr;
            }
        } else if ((message.value("method",std::string{}) == "window/logMessage" || message.value("method",std::string{}) == "window/showMessage") && message.value("params",Json::object()).value("type",0) == 1) {
            std::lock_guard lock(mutex_); error_ += message.at("params").value("message","")+"\n";
            if (error_.size()>65536) error_.erase(0,error_.size()-65536);
        } else if (message.contains("id") && message.contains("method")) {
            const auto method = message.value("method", ""); Json reply = {{"jsonrpc","2.0"},{"id",message["id"]}};
            if (method == "workspace/configuration") {
                Json values = Json::array();
                for (const auto &item : message.value("params",Json::object()).value("items",Json::array())) {
                    Json value = configuration_; const auto section = item.value("section","");
                    std::istringstream parts(section); std::string part;
                    while (std::getline(parts,part,'.')) { if (!value.is_object() || !value.contains(part)) { value = nullptr; break; } value = Json(value.at(part)); }
                    values.push_back(std::move(value));
                }
                reply["result"] = values;
            } else if (method == "workspace/workspaceFolders") reply["result"] = workspaceFolders_;
            else if (method == "window/workDoneProgress/create") reply["result"] = nullptr;
            else if (method == "workspace/semanticTokens/refresh") { ++semanticGeneration_; reply["result"] = nullptr; }
            else if (method == "workspace/applyEdit") reply["result"] = {{"applied",false},{"failureReason","Use the revision-checked change preview API"}};
            else if (method == "client/registerCapability" || method == "client/unregisterCapability")
                reply["error"] = {{"code",-32601},{"message","Dynamic capability registration is not supported"}};
            else reply["error"] = {{"code",-32601},{"message","Unsupported client request"}};
            enqueue(std::move(reply));
        }
    }
}
void LanguageClient::writeLoop() {
#ifndef _WIN32
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &signals, nullptr);
#endif
    while (running_) {
        std::string message;
        {
            std::unique_lock lock(mutex_);
            outgoingReady_.wait(lock, [this] { return !running_ || !outgoing_.empty(); });
            if (!running_)
                return;
            message = std::move(outgoing_.front());
            outgoing_.pop_front();
            // Queue accounting excludes framing bytes, as in enqueue().
            outgoingBytes_ -= message.size() - message.find("\r\n\r\n") - 4;
        }
        size_t offset = 0;
        while (running_ && offset < message.size()) {
#ifdef _WIN32
            DWORD sent = 0;
            if (!WriteFile(child_->input, message.data() + offset,
                           static_cast<DWORD>(std::min<size_t>(65536, message.size() - offset)), &sent,
                           nullptr) ||
                !sent) {
                running_ = false;
                break;
            }
#else
            auto sent = write(child_->input, message.data() + offset, message.size() - offset);
            if (sent <= 0) {
                if (errno == EINTR)
                    continue;
                running_ = false;
                break;
            }
#endif
            offset += sent;
        }
    }
}
void LanguageClient::readLoop() {
    std::string buffer;
    while (running_) {
        char bytes[8192];
        std::string stderrText;
#ifdef _WIN32
        DWORD available = 0, count = 0;
        if (PeekNamedPipe(child_->output, nullptr, 0, nullptr, &available, nullptr) && available) {
            if (ReadFile(child_->output, bytes, std::min<DWORD>(available, sizeof(bytes)), &count, nullptr))
                buffer.append(bytes, count);
        }
        if (PeekNamedPipe(child_->errors, nullptr, 0, nullptr, &available, nullptr) && available) {
            if (ReadFile(child_->errors, bytes, std::min<DWORD>(available, sizeof(bytes)), &count, nullptr))
                stderrText.assign(bytes, count);
        }
        if (WaitForSingleObject(child_->process, 0) == WAIT_OBJECT_0)
            running_ = false;
#else
        ssize_t count = read(child_->output, bytes, sizeof(bytes));
        if (count > 0)
            buffer.append(bytes, static_cast<size_t>(count));
        else if (count == 0)
            running_ = false;
        count = read(child_->errors, bytes, sizeof(bytes));
        if (count > 0)
            stderrText.assign(bytes, static_cast<size_t>(count));
#endif
        if (!stderrText.empty()) {
            std::lock_guard lock(mutex_);
            error_ += stderrText;
            if (error_.size() > 65536)
                error_.erase(0, error_.size() - 65536);
        }
        if (buffer.size() > 16 * 1024 * 1024) {
            std::lock_guard lock(mutex_);
            error_ = "Language server message exceeds limit";
            running_ = false;
            break;
        }
        for (;;) {
            auto end = buffer.find("\r\n\r\n");
            if (end == std::string::npos)
                break;
            auto lengthAt = buffer.find("Content-Length:");
            if (lengthAt == std::string::npos || lengthAt > end) {
                running_ = false;
                break;
            }
            size_t length = 0;
            try {
                length = std::stoull(buffer.substr(lengthAt + 15));
            } catch (...) {
                running_ = false;
                break;
            }
            if (length > 16 * 1024 * 1024) {
                running_ = false;
                break;
            }
            if (buffer.size() < end + 4 + length)
                break;
            auto message = Json::parse(buffer.substr(end + 4, length), nullptr, false);
            if (!message.is_discarded()) {
                std::lock_guard lock(mutex_);
                if (raw_ && (incoming_.size() >= 256 || incomingBytes_ + length > 8*1024*1024)) { error_ = "Debug adapter incoming queue exceeded its limit"; running_ = false; }
                else if (incoming_.size() < 1024) {
                    if (raw_) incomingBytes_ += length;
                    incoming_.push_back(std::move(message));
                }
            }
            buffer.erase(0, end + 4 + length);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    outgoingReady_.notify_all();
}
} // namespace cycode
