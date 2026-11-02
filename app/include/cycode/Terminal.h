#pragma once
#include "cycode/Process.h"
#include <array>
#include <memory>
#include <nlohmann/json.hpp>
namespace cycode {
struct TerminalColor {
    int index = -1; // -1 default, -2 RGB, otherwise ANSI index
    unsigned char red = 0, green = 0, blue = 0;
};
struct TerminalCell {
    std::array<uint32_t, 6> chars{};
    TerminalColor foreground, background;
    int width = 1;
    bool bold = false, italic = false, underline = false, reverse = false, strike = false;
};
struct TerminalSnapshot {
    int first = 0, total = 0, columns = 0, rows = 0, cursorRow = 0, cursorColumn = 0;
    bool cursorVisible = true, alternate = false, running = false;
    uint64_t generation = 0;
    std::vector<std::vector<TerminalCell>> lines;
};
// Persistent native PTY sessions, shared by the desktop and MCP.
class Terminals {
  public:
    Terminals();
    ~Terminals();
    nlohmann::json create(const std::filesystem::path &, const std::vector<std::string> &, int columns = 100, int rows = 24,
                          const nlohmann::json &environment = nlohmann::json::object());
    nlohmann::json list() const;
    TerminalSnapshot snapshot(const std::string &, int first = 0, int count = 200) const;
    void write(const std::string &, const std::string &bytes);
    void paste(const std::string &, const std::string &text);
    void key(const std::string &, const std::string &name, int modifiers = 0);
    void character(const std::string &, uint32_t codepoint, int modifiers = 0);
    void resize(const std::string &, int columns, int rows);
    void clear(const std::string &);
    void close(const std::string &);
    void closeAll();
    static std::vector<std::string> shell(const std::string &profile = "default");
    static std::string text(const std::vector<TerminalCell> &);
    static std::string cellText(const TerminalCell &);
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace cycode
