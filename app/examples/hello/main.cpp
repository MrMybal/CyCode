#include <iostream>
#include <string_view>

// Welcome to CyCode. Open this folder to configure and build this example.
// Ctrl+P: open a file. Ctrl+S: save. Ctrl+Space: C++ completion with clangd.
// The MCP bridge reads the same live document that you edit here.
namespace hello {

class Greeting {
public:
    explicit Greeting(std::string_view name) : name_(name) {}

    void print() const {
        std::cout << "Hello, " << name_ << "!\n";
    }

private:
    std::string_view name_;
};

} // namespace hello

int main() {
    hello::Greeting greeting("CyCode");
    greeting.print();
    return 0;
}
