#pragma once
#include <array>
#include <string_view>
#include <vector>

namespace aurora {
struct Scene {
    std::string_view name;
    std::array<float, 3> position;
    bool visible = true;
};

class SceneIndex {
public:
    void add(Scene scene);
    [[nodiscard]] std::vector<Scene> find(std::string_view query) const;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::vector<Scene> scenes_;
};
} // namespace aurora
