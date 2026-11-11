#pragma once
#include "cycode/Engine.h"

namespace cycode {
enum class CommandContext { global, document, editor };
struct Command {
    std::string id, title, category;
    CommandContext context = CommandContext::global;
};
const std::vector<Command> &commands();
const Command *findCommand(const std::string &id);
struct KeyBinding {
    std::string command, shortcut;
    std::vector<std::string> strokes;
};
// Independent of the windowing toolkit so profiles and chord dispatch are testable.
class Keymap {
  public:
    struct Result { std::string command; bool consumed = false; };
    void load(const std::string &profile, const Json &overrides = Json::object());
    void extensions(std::vector<Command> commands, Json defaults = Json::object());
    // Accepts one canonical or display-form stroke; invalid events cancel a pending chord.
    Result press(const std::string &stroke, double time, bool editor, bool document);
    void cancel();
    std::string pending(double time) const;
    std::string hint(const std::string &command) const;
    const std::vector<KeyBinding> &bindings() const { return bindings_; }
    static std::vector<std::string> parse(const std::string &shortcut);
  private:
    std::vector<KeyBinding> bindings_;
    std::vector<Command> extensionCommands_;
    Json extensionDefaults_ = Json::object();
    std::vector<size_t> candidates_;
    size_t step_ = 0;
    double until_ = 0;
};
int fuzzyScore(const std::string &query, const std::string &candidate);
} // namespace cycode
