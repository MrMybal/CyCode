#include "cycode/Terminal.h"
#include "cycode/Tools.h"
#include <vterm.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif
extern char **environ;
#endif
namespace cycode {
using Json = nlohmann::json;
static void dimensions(int columns, int rows) {
    if (columns < 10 || columns > 400 || rows < 2 || rows > 120) throw std::runtime_error("Terminal size must be 10..400 columns and 2..120 rows");
}
static void utf8(std::string &out, uint32_t c) {
    if (c < 0x80) out += char(c);
    else if (c < 0x800) { out += char(0xc0 | (c >> 6)); out += char(0x80 | (c & 63)); }
    else if (c < 0x10000) { out += char(0xe0 | (c >> 12)); out += char(0x80 | ((c >> 6) & 63)); out += char(0x80 | (c & 63)); }
    else { out += char(0xf0 | (c >> 18)); out += char(0x80 | ((c >> 12) & 63)); out += char(0x80 | ((c >> 6) & 63)); out += char(0x80 | (c & 63)); }
}
#ifdef _WIN32
static std::wstring wideText(const std::string &text) {
    auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!count && !text.empty()) throw std::runtime_error("Terminal argument is not UTF-8");
    std::wstring value(count, 0); if (count) MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), value.data(), count);
    return value;
}
static std::wstring argument(const std::string &text) {
    auto value = wideText(text);
    if (!value.empty() && value.find_first_of(L" \t\n\v\"") == std::wstring::npos) return value;
    std::wstring result = L"\""; size_t slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(slashes * (c == L'\"' ? 2 : 1) + (c == L'\"' ? 1 : 0), L'\\'); slashes = 0; result += c;
    }
    result.append(slashes * 2, L'\\'); return result + L'\"';
}
#endif
struct TerminalSession {
    mutable std::mutex modelMutex, queueMutex, osMutex;
    std::condition_variable queued;
    std::deque<std::string> output;
    size_t queuedBytes = 0, historyCells = 0;
    std::deque<std::vector<VTermScreenCell>> history;
    VTerm *vt = nullptr;
    VTermScreen *screen = nullptr;
    std::atomic<bool> running{false}, stopping{false};
    std::atomic<int> exitCode{-1};
    std::thread reader, writer, supervisor;
    std::string id, executable, directory, title, pendingTitle;
    uint64_t generation = 0;
    int columns = 0, rows = 0;
    bool cursorVisible = true, alternate = false;
#ifdef _WIN32
    HPCON console = nullptr;
    HANDLE input = nullptr, readPipe = nullptr, process = nullptr, job = nullptr;
    DWORD pid = 0;
#else
    int master = -1;
    pid_t pid = -1;
#endif
    ~TerminalSession() { stop(); if (vt) vterm_free(vt); }
    void enqueue(const std::string &bytes) {
        if (bytes.empty()) return;
        std::lock_guard lock(queueMutex);
        if (queuedBytes + bytes.size() > 262144) throw std::runtime_error("Terminal input queue is full");
        output.push_back(bytes); queuedBytes += bytes.size(); queued.notify_one();
    }
    static int property(VTermProp prop, VTermValue *value, void *user) {
        auto &s = *static_cast<TerminalSession *>(user);
        if (prop == VTERM_PROP_CURSORVISIBLE) s.cursorVisible = value->boolean;
        if (prop == VTERM_PROP_ALTSCREEN) s.alternate = value->boolean;
        if (prop == VTERM_PROP_TITLE) {
            if (value->string.initial) s.pendingTitle.clear();
            s.pendingTitle.append(value->string.str, std::min<size_t>(value->string.len, 512 - std::min<size_t>(512, s.pendingTitle.size())));
            if (value->string.final) s.title = Json::parse(Json(s.pendingTitle).dump(-1,' ',false,Json::error_handler_t::replace)).get<std::string>();
        }
        return 1;
    }
    static int pushLine(int count, const VTermScreenCell *cells, void *user) {
        auto &s = *static_cast<TerminalSession *>(user);
        s.history.emplace_back(cells, cells + count); s.historyCells += count;
        while (s.historyCells > 200000 || s.history.size() > 2000) { s.historyCells -= s.history.front().size(); s.history.pop_front(); }
        return 1;
    }
    static int popLine(int count, VTermScreenCell *cells, void *user) {
        auto &s = *static_cast<TerminalSession *>(user);
        if (s.history.empty()) return 0;
        VTermScreenCell blank{}; blank.width = 1; blank.fg.type = VTERM_COLOR_DEFAULT_FG; blank.bg.type = VTERM_COLOR_DEFAULT_BG;
        std::fill_n(cells, count, blank);
        std::copy_n(s.history.back().data(), std::min<size_t>(count, s.history.back().size()), cells);
        s.historyCells -= s.history.back().size(); s.history.pop_back(); return 1;
    }
    void initialize(int c, int r) {
        dimensions(c, r); columns = c; rows = r;
        vt = vterm_new(r, c); if (!vt) throw std::runtime_error("Cannot allocate terminal emulator");
        vterm_set_utf8(vt, 1); screen = vterm_obtain_screen(vt);
        vterm_screen_enable_altscreen(screen, 1); vterm_screen_enable_reflow(screen, true);
        static const VTermScreenCallbacks callbacks = [] {
            VTermScreenCallbacks result{}; result.settermprop = property; result.sb_pushline = pushLine; result.sb_popline = popLine; return result;
        }();
        vterm_screen_set_callbacks(screen, &callbacks, this);
        vterm_output_set_callback(vt, [](const char *data, size_t length, void *user) {
            auto &s = *static_cast<TerminalSession *>(user);
            try { s.enqueue(std::string(data, length)); } catch (...) { }
        }, this);
        vterm_screen_reset(screen, 1);
    }
    void feed(const char *data, size_t length) {
        std::lock_guard lock(modelMutex);
        vterm_input_write(vt, data, length); vterm_screen_flush_damage(screen); ++generation;
    }
    void start(const std::filesystem::path &cwd, const std::vector<std::string> &argv, const Json &environment);
    void readLoop();
    void writeLoop();
    void stop();
    Json info() const {
        std::lock_guard lock(modelMutex);
        return {{"id", id}, {"pid", pid}, {"executable", executable}, {"cwd", directory}, {"title", title},
                {"running", bool(running)}, {"exitCode", int(exitCode)}, {"columns", columns}, {"rows", rows},
                {"history", history.size()}, {"generation", generation}};
    }
};
void TerminalSession::start(const std::filesystem::path &cwd, const std::vector<std::string> &argv, const Json &overrides) {
    if (argv.empty() || !std::filesystem::is_regular_file(utf8Path(argv.front()))) throw std::runtime_error("Terminal executable is missing");
    for (const auto &arg : argv) if (arg.find('\0') != std::string::npos) throw std::runtime_error("Invalid terminal argument");
    if (!overrides.is_object() || overrides.dump().size() > 65536) throw std::runtime_error("Invalid process environment");
    for (auto i = overrides.begin(); i != overrides.end(); ++i)
        if (i.key().empty() || i.key().find_first_of("=\0", 0, 2) != std::string::npos || (!i->is_null() && (!i->is_string() || i->get_ref<const std::string &>().find('\0') != std::string::npos))) throw std::runtime_error("Invalid environment variable");
    directory = pathUtf8(cwd); executable = argv.front();
#ifdef _WIN32
    HANDLE childInput = nullptr, childOutput = nullptr;
    if (!CreatePipe(&childInput, &input, nullptr, 0) || !CreatePipe(&readPipe, &childOutput, nullptr, 0)) {
        if (childInput) CloseHandle(childInput); if (childOutput) CloseHandle(childOutput);
        throw std::runtime_error("Cannot create terminal pipes");
    }
    auto hr = CreatePseudoConsole({SHORT(columns), SHORT(rows)}, childInput, childOutput, 0, &console);
    CloseHandle(childInput); CloseHandle(childOutput);
    if (FAILED(hr)) throw std::runtime_error("Cannot create ConPTY (Windows 10 1809+ required): " + std::to_string(hr));
    SIZE_T size = 0; InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    std::vector<unsigned char> storage(size);
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
    // Do not inherit redirected standard handles from the IDE/CLI host. Null
    // standard handles are populated from the attached pseudoconsole by Windows.
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &size)) throw std::runtime_error("Cannot initialize terminal process attributes");
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, console, sizeof(console), nullptr, nullptr)) {
        DeleteProcThreadAttributeList(startup.lpAttributeList); throw std::runtime_error("Cannot attach ConPTY");
    }
    // .NET/PowerShell reject duplicate Path/PATH entries inherited from some hosts.
    auto insensitive = [](const std::wstring &a, const std::wstring &b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; };
    std::map<std::wstring, std::wstring, decltype(insensitive)> environment(insensitive);
    auto inherited = GetEnvironmentStringsW();
    if (inherited) {
        for (auto p = inherited; *p; p += wcslen(p) + 1) { std::wstring entry(p); auto at = entry.find(L'=', 1); if (at != std::wstring::npos) environment[entry.substr(0, at)] = entry; }
        FreeEnvironmentStringsW(inherited);
    }
    environment[L"TERM"] = L"TERM=xterm-256color";
    for (auto i = overrides.begin(); i != overrides.end(); ++i) {
        auto key = wideText(i.key());
        if (i->is_null()) environment.erase(key); else environment[key] = key + L'=' + wideText(i->get<std::string>());
    }
    std::vector<wchar_t> block;
    for (const auto &[key, value] : environment) { block.insert(block.end(), value.begin(), value.end()); block.push_back(0); } block.push_back(0);
    // cmd.exe parses its own command line and needs a native, quoted argv[0].
    std::wstring command = L"\"" + utf8Path(argv.front()).make_preferred().wstring() + L"\"";
    for (size_t i = 1; i < argv.size(); ++i) command += L' ' + argument(argv[i]);
    PROCESS_INFORMATION info{};
    auto created = CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, block.data(), cwd.c_str(), &startup.StartupInfo, &info);
    auto failure = GetLastError(); DeleteProcThreadAttributeList(startup.lpAttributeList);
    if (!created) throw std::runtime_error("Cannot start terminal process (Windows error " + std::to_string(failure) + ")");
    process = info.hProcess; pid = info.dwProcessId; job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{}; limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) || !AssignProcessToJobObject(job, process)) {
        TerminateProcess(process, 1); CloseHandle(info.hThread); throw std::runtime_error("Cannot own terminal process tree");
    }
    ResumeThread(info.hThread); CloseHandle(info.hThread);
#else
    std::vector<char *> args; for (const auto &arg : argv) args.push_back(const_cast<char *>(arg.c_str())); args.push_back(nullptr);
    std::vector<std::string> environment;
    for (auto p = environ; *p; ++p) { std::string value(*p); auto equal = value.find('=');
        if (!value.starts_with("TERM=") && !value.starts_with("COLORTERM=") && !overrides.contains(value.substr(0,equal))) environment.push_back(value); }
    environment.emplace_back("TERM=xterm-256color"); environment.emplace_back("COLORTERM=truecolor");
    for (auto i = overrides.begin(); i != overrides.end(); ++i) if (!i->is_null()) environment.push_back(i.key()+"="+i->get<std::string>());
    std::vector<char *> env; for (auto &value : environment) env.push_back(value.data()); env.push_back(nullptr);
    winsize window{static_cast<unsigned short>(rows), static_cast<unsigned short>(columns), 0, 0};
    pid = forkpty(&master, nullptr, nullptr, &window);
    if (!pid) { if (chdir(cwd.c_str())) _exit(126); execve(args[0], args.data(), env.data()); _exit(127); }
    if (pid < 0) throw std::runtime_error("Cannot create pseudo-terminal");
    fcntl(master, F_SETFL, O_NONBLOCK); fcntl(master, F_SETFD, FD_CLOEXEC);
#endif
    running = true;
    reader = std::thread([this] { readLoop(); });
    writer = std::thread([this] { writeLoop(); });
    supervisor = std::thread([this] {
#ifdef _WIN32
        while (WaitForSingleObject(process, 20) == WAIT_TIMEOUT) if (stopping) { TerminateJobObject(job, 1); break; }
        WaitForSingleObject(process, 3000); DWORD code = 0; GetExitCodeProcess(process, &code); exitCode = int(code);
        running = false; queued.notify_all();
        // Keep draining output on the reader thread during ConPTY's final update.
        std::lock_guard lock(osMutex); if (console) { ClosePseudoConsole(console); console = nullptr; }
#else
        int status = 0;
        bool finished = false;
        for (;;) {
            if (stopping) kill(-pid, SIGKILL);
            const auto observed = waitpid(pid, &status, WNOHANG);
            if (observed == pid && (WIFEXITED(status) || WIFSIGNALED(status))) {
                finished = true;
                break;
            }
            if (observed < 0 && errno != EINTR) {
                // A tracer may temporarily own wait notifications (notably on
                // macOS). Keep the terminal live while its issued PID exists.
                const auto waitError=errno;
                if (waitError!=ECHILD) break;
                if (kill(pid,0)!=0 && errno!=EPERM) break;
            }
            // A debugger can report a traced child's stop to its real parent.
            // Neither a stop/continue notification nor EINTR means it exited.
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        exitCode = finished ? (WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status)) : -1;
        if (finished || stopping) kill(-pid, SIGKILL);
        running = false; queued.notify_all();
#endif
    });
}
void TerminalSession::readLoop() {
    char buffer[8192];
#ifdef _WIN32
    DWORD count = 0; while (ReadFile(readPipe, buffer, sizeof(buffer), &count, nullptr) && count) feed(buffer, count);
#else
    for (;;) {
        pollfd event{master, POLLIN, 0}; poll(&event, 1, 20);
        auto count = ::read(master, buffer, sizeof(buffer));
        if (count > 0) feed(buffer, size_t(count));
        else if (!running || (count < 0 && errno != EAGAIN && errno != EINTR)) break;
    }
#endif
}
void TerminalSession::writeLoop() {
    for (;;) {
        std::string bytes;
        { std::unique_lock lock(queueMutex); queued.wait(lock, [&] { return !running || !output.empty(); });
          if (!running) break; bytes = std::move(output.front()); output.pop_front(); queuedBytes -= bytes.size(); }
        size_t offset = 0;
        while (offset < bytes.size() && running) {
#ifdef _WIN32
            DWORD count = 0; if (!WriteFile(input, bytes.data() + offset, DWORD(bytes.size() - offset), &count, nullptr) || !count) return;
            offset += count;
#else
            auto count = ::write(master, bytes.data() + offset, bytes.size() - offset);
            if (count > 0) offset += size_t(count);
            else if (count < 0 && (errno == EAGAIN || errno == EINTR)) { pollfd event{master, POLLOUT, 0}; poll(&event, 1, 20); }
            else return;
#endif
        }
    }
}
void TerminalSession::stop() {
    stopping = true; running = false; queued.notify_all();
#ifdef _WIN32
    if (writer.joinable()) CancelSynchronousIo(writer.native_handle());
#endif
    if (supervisor.joinable()) supervisor.join();
    if (writer.joinable()) writer.join();
#ifdef _WIN32
    if (console) { ClosePseudoConsole(console); console = nullptr; }
    if (reader.joinable()) { CancelSynchronousIo(reader.native_handle()); reader.join(); }
    for (auto handle : {input, readPipe, process, job}) if (handle) CloseHandle(handle);
    input = readPipe = process = job = nullptr;
#else
    if (reader.joinable()) reader.join();
    if (master >= 0) { ::close(master); master = -1; }
#endif
}

struct Terminals::Impl {
    mutable std::mutex mutex;
    std::map<std::string, std::shared_ptr<TerminalSession>> sessions;
    size_t next = 1;
    std::shared_ptr<TerminalSession> get(const std::string &id) const {
        std::lock_guard lock(mutex); auto found = sessions.find(id);
        if (found == sessions.end()) throw std::runtime_error("Unknown terminal session"); return found->second;
    }
};
Terminals::Terminals() : impl_(std::make_unique<Impl>()) {}
Terminals::~Terminals() { closeAll(); }
Json Terminals::create(const std::filesystem::path &cwd, const std::vector<std::string> &argv, int columns, int rows, const Json &environment) {
    if (!std::filesystem::is_directory(cwd)) throw std::runtime_error("Terminal working directory is missing");
    auto session = std::make_shared<TerminalSession>(); session->initialize(columns, rows);
    { std::lock_guard lock(impl_->mutex);
      if (impl_->sessions.size() >= 8) throw std::runtime_error("Close a terminal before opening more than eight sessions");
      session->id = "terminal-" + std::to_string(impl_->next++);
      session->start(cwd, argv, environment); impl_->sessions.emplace(session->id, session); }
    return session->info();
}
Json Terminals::list() const {
    std::vector<std::shared_ptr<TerminalSession>> sessions;
    { std::lock_guard lock(impl_->mutex); for (const auto &[id, session] : impl_->sessions) sessions.push_back(session); }
    Json result = Json::array(); for (const auto &session : sessions) result.push_back(session->info()); return {{"sessions", result}};
}
static TerminalColor colour(VTermColor color) {
    if (VTERM_COLOR_IS_DEFAULT_FG(&color) || VTERM_COLOR_IS_DEFAULT_BG(&color)) return {};
    if (VTERM_COLOR_IS_INDEXED(&color)) return {color.indexed.idx};
    return {-2, color.rgb.red, color.rgb.green, color.rgb.blue};
}
TerminalSnapshot Terminals::snapshot(const std::string &id, int first, int count) const {
    auto session = impl_->get(id); std::lock_guard lock(session->modelMutex); auto &s = *session;
    TerminalSnapshot result; const auto history = s.alternate ? 0 : int(s.history.size());
    result.total = history + s.rows; result.columns = s.columns; result.rows = s.rows;
    result.first = std::clamp(first, 0, result.total); result.running = s.running;
    result.generation = s.generation; result.alternate = s.alternate; result.cursorVisible = s.cursorVisible;
    VTermPos cursor{}; vterm_state_get_cursorpos(vterm_obtain_state(s.vt), &cursor);
    result.cursorRow = history + cursor.row; result.cursorColumn = cursor.col;
    for (int row = result.first; row < std::min(result.total, result.first + std::clamp(count, 0, 2120)); ++row) {
        auto &line = result.lines.emplace_back(); line.reserve(s.columns);
        for (int column = 0; column < s.columns; ++column) {
            VTermScreenCell cell{};
            cell.width = 1; cell.fg.type = VTERM_COLOR_DEFAULT_FG; cell.bg.type = VTERM_COLOR_DEFAULT_BG;
            if (row < history) { if (column < int(s.history[row].size())) cell = s.history[row][column]; }
            else vterm_screen_get_cell(s.screen, {row - history, column}, &cell);
            TerminalCell out; std::copy_n(cell.chars, 6, out.chars.begin());
            out.width = cell.width; out.foreground = colour(cell.fg); out.background = colour(cell.bg);
            out.bold = cell.attrs.bold; out.italic = cell.attrs.italic; out.underline = cell.attrs.underline;
            out.reverse = cell.attrs.reverse; out.strike = cell.attrs.strike;
            if (cell.attrs.conceal) out.chars.fill(0); line.push_back(out);
        }
    }
    return result;
}
std::string Terminals::text(const std::vector<TerminalCell> &line) {
    std::string out;
    for (const auto &cell : line) {
        if (!cell.width || cell.chars[0] == UINT32_MAX) continue;
        if (!cell.chars[0]) out += ' '; else for (auto c : cell.chars) { if (!c) break; utf8(out, c); }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back(); return out;
}
std::string Terminals::cellText(const TerminalCell &cell) {
    std::string out;
    if (!cell.width || cell.chars[0] == UINT32_MAX || !cell.chars[0]) return out;
    for (auto c : cell.chars) { if (!c) break; utf8(out, c); } return out;
}
void Terminals::write(const std::string &id, const std::string &bytes) {
    if (bytes.size() > 65536) throw std::runtime_error("Terminal input exceeds 64 KiB");
    auto s = impl_->get(id); if (!s->running) throw std::runtime_error("Terminal process has exited"); s->enqueue(bytes);
}
void Terminals::paste(const std::string &id, const std::string &text) {
    if (text.size() > 65536 || text.find('\x1b') != std::string::npos) throw std::runtime_error("Paste exceeds 64 KiB or contains an escape character");
    auto s = impl_->get(id); std::lock_guard lock(s->modelMutex);
    if (!s->running) throw std::runtime_error("Terminal process has exited");
    vterm_keyboard_start_paste(s->vt); s->enqueue(text); vterm_keyboard_end_paste(s->vt);
}
void Terminals::character(const std::string &id, uint32_t codepoint, int modifiers) {
    if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff)) throw std::runtime_error("Invalid Unicode terminal character");
    auto s = impl_->get(id); std::lock_guard lock(s->modelMutex);
    if (s->running) vterm_keyboard_unichar(s->vt, codepoint, VTermModifier(modifiers & 7));
}
void Terminals::key(const std::string &id, const std::string &name, int modifiers) {
    static const std::map<std::string, VTermKey> keys = {{"Enter", VTERM_KEY_ENTER}, {"Tab", VTERM_KEY_TAB}, {"Backspace", VTERM_KEY_BACKSPACE},
        {"Escape", VTERM_KEY_ESCAPE}, {"Up", VTERM_KEY_UP}, {"Down", VTERM_KEY_DOWN}, {"Left", VTERM_KEY_LEFT}, {"Right", VTERM_KEY_RIGHT},
        {"Home", VTERM_KEY_HOME}, {"End", VTERM_KEY_END}, {"Insert", VTERM_KEY_INS}, {"Delete", VTERM_KEY_DEL}, {"PageUp", VTERM_KEY_PAGEUP}, {"PageDown", VTERM_KEY_PAGEDOWN}};
    VTermKey key;
    if (auto found = keys.find(name); found != keys.end()) key = found->second;
    else if (name.size() >= 2 && name.front() == 'F') { int value = std::stoi(name.substr(1)); if (value < 1 || value > 12) throw std::runtime_error("Unknown terminal key"); key = VTermKey(VTERM_KEY_FUNCTION(value)); }
    else throw std::runtime_error("Unknown terminal key");
    auto s = impl_->get(id); std::lock_guard lock(s->modelMutex); if (s->running) vterm_keyboard_key(s->vt, key, VTermModifier(modifiers & 7));
}
void Terminals::resize(const std::string &id, int columns, int rows) {
    dimensions(columns, rows); auto s = impl_->get(id);
    std::lock_guard os(s->osMutex);
    { std::lock_guard model(s->modelMutex); if (columns == s->columns && rows == s->rows) return; }
#ifdef _WIN32
    if (s->console && FAILED(ResizePseudoConsole(s->console, {SHORT(columns), SHORT(rows)}))) throw std::runtime_error("Cannot resize ConPTY");
#else
    winsize size{static_cast<unsigned short>(rows), static_cast<unsigned short>(columns), 0, 0};
    if (s->master >= 0 && ioctl(s->master, TIOCSWINSZ, &size)) throw std::runtime_error("Cannot resize pseudo-terminal");
#endif
    std::lock_guard model(s->modelMutex);
    s->columns = columns; s->rows = rows; vterm_set_size(s->vt, rows, columns); ++s->generation;
}
void Terminals::clear(const std::string &id) {
    auto s = impl_->get(id); std::lock_guard lock(s->modelMutex);
    s->history.clear(); s->historyCells = 0; ++s->generation;
}
void Terminals::close(const std::string &id) {
    std::shared_ptr<TerminalSession> session;
    { std::lock_guard lock(impl_->mutex); auto found = impl_->sessions.find(id); if (found == impl_->sessions.end()) return; session = std::move(found->second); impl_->sessions.erase(found); }
    session->stop();
}
void Terminals::closeAll() {
    std::map<std::string, std::shared_ptr<TerminalSession>> sessions;
    { std::lock_guard lock(impl_->mutex); sessions.swap(impl_->sessions); }
    for (auto &[id, session] : sessions) session->stop();
}
std::vector<std::string> Terminals::shell(const std::string &profile) {
#ifdef _WIN32
    if (profile == "default" || profile == "powershell") {
        auto modern = toolPath("pwsh"); if (!modern.empty()) return {modern, "-NoLogo"};
        auto windows = toolPath("powershell");
        if (windows.empty()) { wchar_t system[32768]; if (GetSystemDirectoryW(system, 32768)) windows = pathUtf8(std::filesystem::path(system) / "WindowsPowerShell/v1.0/powershell.exe"); }
        return {windows, "-NoLogo"};
    }
    if (profile == "cmd") { wchar_t system[32768]; if (!GetSystemDirectoryW(system, 32768)) throw std::runtime_error("Cannot locate Windows shell"); return {pathUtf8(std::filesystem::path(system) / "cmd.exe"), "/D"}; }
#else
    if (profile == "default") { auto configured = std::getenv("SHELL"); if (configured && std::filesystem::is_regular_file(configured)) return {configured}; }
    if (profile == "default" || profile == "bash") return {"/bin/bash"};
    if (profile == "zsh") return {"/bin/zsh"};
#endif
    throw std::runtime_error("Shell profile is unavailable on this platform");
}
} // namespace cycode
