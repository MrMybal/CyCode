#include "cycode/RustToolchain.h"
#include <algorithm>
#include <stdexcept>
namespace cycode {
void validateRustToolchain(const std::string &name) {
    const auto alphanumeric = [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    };
    if (name.size() > 128 || (!name.empty() && !alphanumeric(name.front())) ||
        std::any_of(name.begin(), name.end(),
                    [&](unsigned char c) { return !alphanumeric(c) && c != '-' && c != '_' && c != '.'; }))
        throw std::runtime_error("Rust toolchain must be a name (up to 128 ASCII letters/digits, dots, "
                                 "hyphens or underscores), not a path or option");
}
std::vector<std::string> rustToolchainCommand(const std::string &name, const std::string &rustup,
                                              const std::vector<std::string> &command) {
    validateRustToolchain(name);
    if (name.empty())
        return command;
    if (rustup.empty())
        throw std::runtime_error(
            "The selected Rust toolchain requires installed rustup (PATH or CYCODE_RUSTUP)");
    if (command.empty() || command.front().empty() || command.size() > 512)
        throw std::runtime_error("Invalid Rust toolchain command");
    for (const auto &arg : command)
        if (arg.size() > 8192 || arg.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid Rust toolchain argument");
    std::vector<std::string> result = {rustup, "run", name, "--"}; // Deliberately never --install.
    result.insert(result.end(), command.begin(), command.end());
    return result;
}
nlohmann::json rustToolchainEnvironment(const std::string &name) {
    validateRustToolchain(name);
    if (name.empty())
        return nlohmann::json::object();
    // Rustup configures its proxy PATH and selected-toolchain environment. Avoid
    // inherited absolute Cargo/rustc selections bypassing those proxies.
    return {{"RUSTUP_TOOLCHAIN", name}, {"CARGO", "cargo"}, {"RUSTC", "rustc"}};
}
} // namespace cycode
