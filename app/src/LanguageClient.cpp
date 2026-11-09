#include "cycode/LanguageClient.h"
#include "cycode/LoopbackStream.h"
#include "cycode/Tools.h"
#include <array>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include "cycode/WindowsChildHandles.h"
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
LanguageClient::LanguageClient(std::function<void()> wake) : wakeHandler_(std::move(wake)) {}
void LanguageClient::signalWake() noexcept {
    // Signals never own protocol dispatch; a host callback cannot unwind a thread.
    try { if (wakeHandler_) wakeHandler_(); } catch (...) {}
}
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
std::string LanguageClient::uriKey(const std::string &uri) {
    auto key=pathUtf8(fromUri(uri).lexically_normal());
#ifdef _WIN32
    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c) { return char(std::tolower(c)); });
#endif
    return key;
}
bool LanguageClient::start(const std::filesystem::path &root, const std::string &executable,
                           const std::filesystem::path &compilationDatabase, Json settings) {
    waitForWorkspace_ = false;
    if (!compilationDatabase.empty()) settings["compilationDatabasePath"] = pathUtf8(compilationDatabase);
    return startImpl(root, {executable, "--background-index", "--rename-file-limit=0", "--use-dirty-headers", "--log=error", "--header-insertion=never", "--completion-style=detailed"}, settings, false);
}
bool LanguageClient::startServer(const std::filesystem::path &root, const std::vector<std::string> &argv, Json configuration, bool waitForWorkspace, Json environment) {
    waitForWorkspace_ = waitForWorkspace;
    return startImpl(root, argv, std::move(configuration), false, environment);
}
bool LanguageClient::startRaw(const std::filesystem::path &root, const std::string &executable) { waitForWorkspace_ = false; return startImpl(root, {executable}, Json::object(), true); }
bool LanguageClient::startRaw(const std::filesystem::path &root, const std::vector<std::string> &argv, Json environment) { waitForWorkspace_ = false; return startImpl(root,argv,Json::object(),true,environment); }
std::deque<Json> LanguageClient::receiveRaw() {
    std::lock_guard lock(mutex_); std::deque<Json> result; result.swap(incoming_); incomingBytes_ = 0; return result;
}
bool LanguageClient::connectRaw(std::uint16_t port) {
    stop();
    raw_ = true; waitForWorkspace_ = false; workspaceReady_ = false;
    {
        std::lock_guard lock(mutex_);
        error_.clear(); logs_.clear(); incoming_.clear(); outgoing_.clear();
        incomingBytes_ = 0; outgoingBytes_ = 0;
    }
    socket_ = std::make_unique<LoopbackStream>();
    if (!socket_->connect(port)) {
        socket_.reset();
        std::lock_guard lock(mutex_);
        error_ = "Cannot connect to the local Java debug adapter";
        return false;
    }
    running_ = true;
    reader_ = std::thread([this] { readLoop(); });
    writer_ = std::thread([this] { writeLoop(); });
    return true;
}
bool LanguageClient::startImpl(const std::filesystem::path &root, const std::vector<std::string> &argv,
                              Json configuration, bool raw,const Json &overrides) {
    stop();
    workspaceReady_ = false; workspaceLoadToken_ = nullptr;
    if (argv.empty() || argv.front().empty()) { error_ = "Language server executable is missing"; return false; }
    configuration_ = std::move(configuration);
    workspaceReadyNotification_=configuration_.value("$workspaceReadyNotification","");
    configuration_.erase("$workspaceReadyNotification");
    if (!workspaceReadyNotification_.empty()) waitForWorkspace_=true;
    initializationOptions_ = configuration_.value("$initializationOptions",configuration_);
    configuration_.erase("$initializationOptions");
    workspaceFolders_ = Json::array({{{"uri",uri(root)},{"name",pathUtf8(root.filename())}}});
    raw_ = raw; incomingBytes_ = 0;
    child_ = std::make_unique<Child>();
    initialized_ = false;serverName_.clear();
    diagnostics_ = Json::object();diagnosticVersions_=Json::object();
    ++diagnosticGeneration_;
    {
        std::lock_guard lock(mutex_);
        error_.clear();
        logs_.clear();
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
        for(auto handle:{childIn,childOut,childErr})if(handle)CloseHandle(handle);
        stop();
        return false;
    }
    SetHandleInformation(child_->input, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(child_->output, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(child_->errors, HANDLE_FLAG_INHERIT, 0);
    WindowsChildHandles inheritedHandles{childIn,childOut,childErr};
    if(!inheritedHandles.get()) {
        error_="Cannot restrict language-server handle inheritance (Windows error "+std::to_string(inheritedHandles.error())+")";
        for(auto handle:{childIn,childOut,childErr})CloseHandle(handle);
        stop();return false;
    }
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = childIn;
    si.StartupInfo.hStdOutput = childOut;
    si.StartupInfo.hStdError = childErr;
    si.lpAttributeList=inheritedHandles.get();
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
    if (!child_->job || !SetInformationJobObject(child_->job, JobObjectExtendedLimitInformation, &limit, sizeof(limit))) {
        error_="Cannot create the language-server process job (Windows error "+std::to_string(GetLastError())+")";
        for(auto handle:{childIn,childOut,childErr})CloseHandle(handle);
        stop();return false;
    }
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
        CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
                       environment.data(), root.c_str(), &si.StartupInfo, &process) != FALSE;
    const auto creationError=created?ERROR_SUCCESS:GetLastError();
    CloseHandle(childIn);
    CloseHandle(childOut);
    CloseHandle(childErr);
    if (!created) {
        error_ = "Cannot start language server: " + argv.front()+" (Windows error "+std::to_string(creationError)+")";
        stop();
        return false;
    }
    child_->process = process.hProcess;
    if (!AssignProcessToJobObject(child_->job, process.hProcess)) {
        error_="Cannot own the language-server process tree (Windows error "+std::to_string(GetLastError())+")";
        // Only the child handle returned by our own CreateProcess is terminated.
        // It is still suspended and has not executed server/project code.
        TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,INFINITE);
        CloseHandle(process.hThread);stop();return false;
    }
    if(ResumeThread(process.hThread)==static_cast<DWORD>(-1)) {
        error_="Cannot resume the owned language server (Windows error "+std::to_string(GetLastError())+")";
        TerminateJobObject(child_->job,1);WaitForSingleObject(process.hProcess,INFINITE);
        CloseHandle(process.hThread);stop();return false;
    }
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
        {"initializationOptions",initializationOptions_},
        {"capabilities",
         {{"general", {{"positionEncodings", Json::array({"utf-16"})}}},
          {"experimental",{{"serverStatusNotification",true}}},
          {"window",{{"workDoneProgress",true}}},
          {"workspace",{{"configuration",true},{"workspaceFolders",true},
              {"workspaceEdit",{{"documentChanges",true},{"failureHandling","textOnlyTransactional"},
                  {"normalizesLineEndings",true},{"changeAnnotationSupport",{{"groupsOnLabel",false}}}}},
              {"symbol",{{"dynamicRegistration",false},{"tagSupport",{{"valueSet",Json::array({1})}}},
                  {"symbolKind",{{"valueSet",Json::array({1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26})}}},
                  {"resolveSupport",{{"properties",Json::array({"location.range"})}}}}},
              {"semanticTokens",{{"refreshSupport",true}}}}},
          {"textDocument", {{"documentHighlight",{{"dynamicRegistration",false}}},
                             {"definition",{{"dynamicRegistration",false},{"linkSupport",true}}},
                             {"declaration",{{"dynamicRegistration",false},{"linkSupport",true}}},
                             {"typeDefinition",{{"dynamicRegistration",false},{"linkSupport",true}}},
                             {"implementation",{{"dynamicRegistration",false},{"linkSupport",true}}},
                             {"completion", {{"dynamicRegistration",false},{"insertTextMode",1},{"completionItem", {{"snippetSupport", true},{"insertReplaceSupport",true},{"insertTextModeSupport",{{"valueSet",Json::array({1,2})}}},{"resolveSupport",{{"properties",Json::array({"documentation","detail","additionalTextEdits","insertTextMode"})}}}}},{"completionList",{{"itemDefaults",Json::array({"commitCharacters","editRange","insertTextFormat","insertTextMode","data"})}}}}},
                             {"hover",{{"dynamicRegistration",false},{"contentFormat",Json::array({"plaintext"})}}},
                             {"signatureHelp",{{"dynamicRegistration",false},{"contextSupport",true},
                                 {"signatureInformation",{{"documentationFormat",Json::array({"plaintext"})},
                                     {"parameterInformation",{{"labelOffsetSupport",true}}},{"activeParameterSupport",true}}}}},
                             {"publishDiagnostics",{{"relatedInformation",false},{"versionSupport",true}}},
                             {"codeAction",{{"dynamicRegistration",false},{"isPreferredSupport",true},{"disabledSupport",true},{"dataSupport",true},
                                 {"codeActionLiteralSupport",{{"codeActionKind",{{"valueSet",Json::array({"","quickfix","refactor","refactor.extract","refactor.inline","refactor.rewrite","source","source.organizeImports","source.fixAll"})}}}}},
                                 {"resolveSupport",{{"properties",Json::array({"edit"})}}},{"honorsChangeAnnotations",true}}},
                             {"rename",{{"prepareSupport",true},{"dynamicRegistration",false},{"honorsChangeAnnotations",true}}},
                             {"callHierarchy",{{"dynamicRegistration",false}}},
                             {"typeHierarchy",{{"dynamicRegistration",false}}},
                             {"semanticTokens",{{"dynamicRegistration",false},{"requests",{{"full",{{"delta",true}}}}},
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
            const auto info=result.value("serverInfo",Json());
            serverName_=info.is_object()&&info.value("name",Json()).is_string()?info.at("name").get<std::string>():"";
            notify("initialized", Json::object());
            notify("workspace/didChangeConfiguration",{{"settings",configuration_}});
        }
        else { std::lock_guard lock(mutex_); error_ = "Language server initialization failed: " + result.dump(); running_ = false; }
    });
    return true;
}
void LanguageClient::stop() {
    capabilities_ = Json::object(); ++semanticGeneration_; ++semanticRefreshGeneration_;
    running_ = false;
    outgoingReady_.notify_all();
    if (socket_) socket_->shutdown();
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
    socket_.reset();
    callbacks_.clear();
    initialized_ = false;serverName_.clear();
    diagnostics_ = Json::object();diagnosticVersions_=Json::object();
    ++diagnosticGeneration_;
}
std::string LanguageClient::error() const {
    std::lock_guard lock(mutex_);
    const auto &text=error_.empty()&&!running_?logs_:error_;
    return Json::parse(Json(text).dump(-1,' ',false,Json::error_handler_t::replace)).get<std::string>();
}
std::string LanguageClient::logs() const {std::lock_guard lock(mutex_);return Json::parse(Json(logs_).dump(-1,' ',false,Json::error_handler_t::replace)).get<std::string>();}
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
    const bool document=method=="textDocument/didOpen" || method=="textDocument/didChange" || method=="textDocument/didClose";
    const auto uri=document ? params.at("textDocument").at("uri").get<std::string>() : "";
    const bool sent=running_ && enqueue({{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}});
    if (sent && document) {
        const auto key=uriKey(uri);
        for (auto i=diagnosticVersions_.begin();i!=diagnosticVersions_.end();) {
            bool matches=false;try { matches=uriKey(i.key())==key; } catch (const std::exception &) {}
            if (matches) {
                // Keep the last displayed publication during typing. In particular,
                // C++ completion must not wait for fresh diagnostics on every key.
                // Its eligibility as code-action context expires immediately.
                if (method!="textDocument/didChange") diagnostics_.erase(i.key());
                i=diagnosticVersions_.erase(i);
            } else ++i;
        }
        if (method!="textDocument/didChange") ++diagnosticGeneration_;
    }
    return sent;
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
            diagnosticVersions_[params["uri"].get<std::string>()]=params.value("version",Json());
            ++diagnosticGeneration_;
        } else if (message.value("method",std::string{})==workspaceReadyNotification_ && waitForWorkspace_) {
            const auto params=message.value("params",Json::object());
            if (workspaceReadyNotification_=="experimental/serverStatus") {
                workspaceReady_=workspaceReady_ || params.value("quiescent",false);
                if (params.value("health","")=="error") { std::lock_guard lock(mutex_);error_=params.value("message","Rust workspace analysis failed"); }
            } else if (workspaceReadyNotification_=="language/status" && params.value("type","")=="Started") workspaceReady_=true;
        } else if (message.value("method",std::string{}) == "$/progress" && waitForWorkspace_ && workspaceReadyNotification_.empty()) {
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
            else if (method == "workspace/semanticTokens/refresh") { ++semanticGeneration_; ++semanticRefreshGeneration_; reply["result"] = nullptr; }
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
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (running_ && offset < message.size()) {
            if (socket_) {
                const auto sent = socket_->write(message.data() + offset, message.size() - offset);
                if (sent < 0 || std::chrono::steady_clock::now() >= deadline) {
                    { std::lock_guard lock(mutex_); error_ = "Java debug adapter write failed or timed out"; }
                    running_ = false; outgoingReady_.notify_all(); break;
                }
                if (!sent) std::this_thread::sleep_for(std::chrono::milliseconds(2));
                else offset += static_cast<size_t>(sent);
                continue;
            }
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
        bool received = false;
        char bytes[8192];
        std::string stderrText;
        if (socket_) {
            const auto count = socket_->read(bytes, sizeof(bytes));
            if (count > 0) buffer.append(bytes, static_cast<size_t>(count));
            else if (count < 0) running_ = false;
        } else {
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
        }
        if (!stderrText.empty()) {
            std::lock_guard lock(mutex_);
            logs_ += stderrText;
            if (logs_.size() > 65536)
                logs_.erase(0, logs_.size() - 65536);
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
                    received = true;
                }
            }
            buffer.erase(0, end + 4 + length);
        }
        // Publish only after releasing the incoming queue mutex. The owner may
        // guard its condition predicate without inverting the transport locks.
        if (received) signalWake();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    signalWake(); // EOF, malformed frame or transport failure also wakes pending requests.
    outgoingReady_.notify_all();
}
} // namespace cycode
