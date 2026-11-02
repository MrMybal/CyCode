#include "cycode/Process.h"
#include "cycode/Tools.h"
#include <algorithm>
#include <map>
#include <thread>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif
namespace cycode {
#ifdef _WIN32
static std::wstring wide(const std::string &s) {
    int n =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring result(n, L' ');
    if (n)
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), result.data(), n);
    return result;
}
static std::wstring quote(const std::string &s) {
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : wide(s)) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'\"')
            result.append(slashes * 2 + 1, L'\\');
        else
            result.append(slashes, L'\\');
        slashes = 0;
        result += c;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}
#endif
ProcessResult runProcess(const std::vector<std::string> &argv, const std::filesystem::path &cwd,
                         std::chrono::milliseconds timeout, std::size_t maxOutput,
                         const std::atomic<bool> *cancellation,
                         const std::map<std::string,std::string> &overrides) {
    ProcessResult result;
    if (argv.empty()) {
        result.output = "Missing executable";
        return result;
    }
    for (const auto &[name,value] : overrides) if (name.empty() || name.find('=')!=std::string::npos || name.find('\0')!=std::string::npos || value.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid process environment override");
    auto commandArgs = argv;
    if (argv[0] == "rg" || argv[0] == "git") {
        auto resolved = toolPath(argv[0]);
        if (resolved.empty()) { result.output = "Required tool is missing: " + argv[0] + ". Repair the CyCode runtime tools installation."; return result; }
        commandArgs[0] = resolved;
    }
    auto start = std::chrono::steady_clock::now();
    auto append = [&](const char *data, std::size_t n) {
        auto kept = std::min(n, maxOutput - result.output.size());
        result.output.append(data, kept);
        if (kept < n)
            result.truncated = true;
    };
#ifdef _WIN32
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE reader = nullptr, writer = nullptr;
    if (!CreatePipe(&reader, &writer, &security, 0)) {
        result.output = "Cannot create process pipe";
        return result;
    }
    SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                               OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = input;
    si.hStdOutput = writer;
    si.hStdError = writer;
    PROCESS_INFORMATION pi{};
    std::wstring command;
    for (const auto &arg : commandArgs) {
        if (!command.empty())
            command += L' ';
        command += quote(arg);
    }
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job)
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    // Some desktop hosts provide duplicate Path/PATH entries. .NET/MSBuild
    // rejects those even though the Windows environment lookup accepts them.
    auto insensitive = [](const std::wstring &a, const std::wstring &b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    };
    std::map<std::wstring, std::wstring, decltype(insensitive)> variables(insensitive);
    auto inherited = GetEnvironmentStringsW();
    if (inherited) {
        for (auto entry = inherited; *entry; entry += wcslen(entry) + 1) {
            std::wstring value(entry);
            auto separator = value.find(L'=', 1);
            if (separator != std::wstring::npos)
                variables[value.substr(0, separator)] = value;
        }
        FreeEnvironmentStringsW(inherited);
    }
    for (const auto &[name,value] : overrides) variables[wide(name)]=wide(name+"="+value);
    std::vector<wchar_t> environment;
    for (const auto &[key, value] : variables) {
        environment.insert(environment.end(), value.begin(), value.end());
        environment.push_back(0);
    }
    environment.push_back(0);
    BOOL created = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                                  CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                                  environment.data(), cwd.empty() ? nullptr : cwd.c_str(), &si, &pi);
    CloseHandle(writer);
    CloseHandle(input);
    if (!created) {
        result.output = "Cannot start " + argv[0] + " (Windows error " + std::to_string(GetLastError()) + ")";
        CloseHandle(reader);
        if (job)
            CloseHandle(job);
        return result;
    }
    if (job)
        AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    for (;;) {
        DWORD available = 0;
        char buffer[8192];
        while (PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr) && available) {
            DWORD read = 0;
            if (!ReadFile(reader, buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr) ||
                !read)
                break;
            append(buffer, read);
        }
        if (WaitForSingleObject(pi.hProcess, 10) == WAIT_OBJECT_0) {
            // A descendant may still own a writer: never wait for EOF here.
            while (PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr) && available) {
                DWORD read = 0;
                if (!ReadFile(reader, buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr) ||
                    !read)
                    break;
                append(buffer, read);
            }
            break;
        }
        if (std::chrono::steady_clock::now() - start > timeout || result.truncated ||
            (cancellation && *cancellation)) {
            result.cancelled = cancellation && *cancellation;
            result.timedOut = !result.truncated && !result.cancelled;
            if (job)
                TerminateJobObject(job, 124);
            else
                TerminateProcess(pi.hProcess, 124);
            WaitForSingleObject(pi.hProcess, 5000);
            break;
        }
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    result.exitCode = static_cast<int>(code);
    CloseHandle(pi.hProcess);
    CloseHandle(reader);
    if (job)
        CloseHandle(job);
#else
    int pipes[2];
    if (pipe(pipes) != 0) {
        result.output = "Cannot create process pipe";
        return result;
    }
    std::vector<char *> args;
    for (const auto &arg : commandArgs)
        args.push_back(const_cast<char *>(arg.c_str()));
    args.push_back(nullptr);
    std::vector<std::string> environmentValues;
    std::vector<char *> environmentPointers;
    std::string executable=commandArgs.front();
    if (!overrides.empty()) {
        std::map<std::string,std::string> variables;
        for (auto entry=environ;entry && *entry;++entry) { const std::string value(*entry);const auto at=value.find('=');if (at!=std::string::npos) variables[value.substr(0,at)]=value.substr(at+1); }
        for (const auto &[name,value] : overrides) variables[name]=value;
        for (const auto &[name,value] : variables) environmentValues.push_back(name+"="+value);
        for (auto &value : environmentValues) environmentPointers.push_back(value.data());
        environmentPointers.push_back(nullptr);
        if (executable.find('/')==std::string::npos) {
            std::istringstream paths(variables["PATH"]);std::string directory;
            while (std::getline(paths,directory,':')) { const auto candidate=std::filesystem::path(directory)/executable;
                if (access(candidate.c_str(),X_OK)==0) { executable=candidate.string();break; } }
        }
    }
    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        close(pipes[0]);
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[1]);
        int input = open("/dev/null", O_RDONLY);
        if (input >= 0) {
            dup2(input, STDIN_FILENO);
            close(input);
        }
        if (!cwd.empty() && chdir(cwd.c_str()) != 0)
            _exit(126);
        if (overrides.empty()) execvp(args[0], args.data());
        else execve(executable.c_str(),args.data(),environmentPointers.data());
        _exit(127);
    }
    close(pipes[1]);
    if (pid < 0) {
        close(pipes[0]);
        result.output = "Cannot fork process";
        return result;
    }
    setpgid(pid, pid);
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    int status = 0;
    for (;;) {
        char buffer[8192];
        ssize_t count;
        while ((count = read(pipes[0], buffer, sizeof(buffer))) > 0)
            append(buffer, static_cast<std::size_t>(count));
        if (waitpid(pid, &status, WNOHANG) == pid) {
            while ((count = read(pipes[0], buffer, sizeof(buffer))) > 0)
                append(buffer, static_cast<std::size_t>(count));
            break;
        }
        if (std::chrono::steady_clock::now() - start > timeout || result.truncated ||
            (cancellation && *cancellation)) {
            result.cancelled = cancellation && *cancellation;
            result.timedOut = !result.truncated && !result.cancelled;
            kill(-pid, SIGKILL);
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            };
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    close(pipes[0]);
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
    return result;
}
} // namespace cycode
