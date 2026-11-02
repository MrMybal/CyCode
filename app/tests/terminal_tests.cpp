#include "cycode/Terminal.h"
#include <fstream>
#include <iostream>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif
using namespace cycode;
static void require(bool value, const char *why) { if (!value) throw std::runtime_error(why); }
template<class F> static void waitFor(F test) {
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!test()) { if (std::chrono::steady_clock::now() > until) throw std::runtime_error("Terminal acceptance timed out"); std::this_thread::sleep_for(std::chrono::milliseconds(15)); }
}
int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "--probe") {
#ifdef _WIN32
        SetConsoleOutputCP(CP_UTF8); SetConsoleCP(CP_UTF8);
        DWORD mode = 0; GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        std::cout << "TTY=" << (GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode) != 0) << '\n';
#else
        std::cout << "TTY=" << isatty(0) << '\n';
#endif
        std::cout << "\x1b[31mRED\x1b[0m café 漢字\nREADY\n" << std::flush;
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line == "exit") return 7;
            if (line == "size") {
#ifdef _WIN32
                CONSOLE_SCREEN_BUFFER_INFO info{}; GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
                std::cout << "SIZE=" << info.dwSize.X << '\n';
#else
                winsize size{}; ioctl(0, TIOCGWINSZ, &size); std::cout << "SIZE=" << size.ws_col << '\n';
#endif
            } else if (line == "flood") for (int i = 0; i < 2500; ++i) std::cout << "ROW_" << i << '\n';
            else std::cout << "REPLY=" << line << '\n';
            std::cout << "DONE\n" << std::flush;
        }
        return 0;
    }
    try {
        Terminals terminals;
        auto root = std::filesystem::current_path();
        const auto executable = pathUtf8(std::filesystem::absolute(utf8Path(argv[0])));
        auto created = terminals.create(root, {executable, "--probe"}, 90, 18);
        const auto id = created.at("id").get<std::string>();
        auto text = [&] { std::string out; for (const auto &line : terminals.snapshot(id, 0, 2000).lines) out += Terminals::text(line) + '\n'; return out; };
        waitFor([&] { return text().find("READY") != std::string::npos; });
        require(text().find("TTY=1") != std::string::npos, "Child did not receive a real terminal");
        require(text().find("café 漢字") != std::string::npos, "Unicode terminal text was corrupted");
        bool red = false; for (const auto &line : terminals.snapshot(id).lines) for (const auto &cell : line) if (cell.chars[0] == 'R' && cell.foreground.index != -1) red = true;
        require(red, "SGR foreground was discarded");
        terminals.write(id, "hello\r"); waitFor([&] { return text().find("REPLY=hello") != std::string::npos; });
        terminals.resize(id, 110, 25); terminals.write(id, "size\r");
        waitFor([&] { return text().find("SIZE=110") != std::string::npos; });
        auto second = terminals.create(root, {executable, "--probe"}); const auto other = second.at("id").get<std::string>();
        require(terminals.list().at("sessions").size() == 2, "Independent terminal sessions missing");
        terminals.write(id, "flood\r"); waitFor([&] { return text().find("ROW_2499") != std::string::npos; });
        require(terminals.snapshot(id, 0, 0).total <= 2025, "Scrollback exceeded its bound");
        std::string independent; for (const auto &line : terminals.snapshot(other).lines) independent += Terminals::text(line);
        require(independent.find("ROW_2499") == std::string::npos, "Output leaked between terminals");
        terminals.write(id, "exit\r"); waitFor([&] { return !terminals.snapshot(id, 0, 0).running; });
        waitFor([&] { auto list = terminals.list(); for (const auto &s : list.at("sessions")) if (s.at("id") == id) return s.at("exitCode") == 7; return false; });
        auto started = std::chrono::steady_clock::now(); terminals.close(other); terminals.close(id);
        require(std::chrono::steady_clock::now() - started < std::chrono::seconds(4), "Terminal teardown blocked");
        require(terminals.list().at("sessions").empty(), "Closed sessions survived");
        auto shell = terminals.create(root, Terminals::shell()); auto shellId = shell.at("id").get<std::string>();
#ifdef _WIN32
        terminals.write(shellId, "Write-Output ('CYCODE_' + (6*7))\r");
#else
        terminals.write(shellId, "printf 'CYCODE_%s\\n' $((6*7))\r");
#endif
        waitFor([&] { for (const auto &line : terminals.snapshot(shellId).lines) if (Terminals::text(line).find("CYCODE_42") != std::string::npos) return true; return false; });
        terminals.closeAll();
#ifdef _WIN32
        auto cmd = terminals.create(root, Terminals::shell("cmd")); const auto cmdId = cmd.at("id").get<std::string>();
        terminals.write(cmdId, "set /a CmdAnswer=6*7\recho CMD_%CmdAnswer%\r");
        waitFor([&] { for (const auto &line : terminals.snapshot(cmdId).lines) if (Terminals::text(line).find("CMD_42") != std::string::npos) return true; return false; });
        terminals.closeAll();
#endif
        std::cout << "Real PTY: interactive shell, Unicode/ANSI, resize, bounded scrollback, isolation, exit codes and teardown passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
