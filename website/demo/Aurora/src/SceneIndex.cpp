#include "SceneIndex.h"
#include <algorithm>
#include <iterator>

namespace aurora {

// A small scene catalogue for a sample graphics tool.
// Names and positions are illustrative project data.
void SceneIndex::add(Scene scene) {
    const auto existing = std::ranges::find(scenes_, scene.name, &Scene::name);
    if (existing == scenes_.end()) {
        scenes_.push_back(scene);
    }
}

std::vector<Scene> SceneIndex::find(std::string_view query) const {
    std::vector<Scene> matches;
    matches.reserve(scenes_.size());

    // Keep the original scene order while filtering visible entries.
    const auto visibleMatch = [query](const Scene& scene) {
        return scene.visible && scene.name.find(query) != std::string_view::npos;
    };
    std::ranges::copy_if(scenes_, std::back_inserter(matches), visibleMatch);
    return matches;
}

std::size_t SceneIndex::size() const noexcept {
    return scenes_.size();
}

} // namespace aurora
