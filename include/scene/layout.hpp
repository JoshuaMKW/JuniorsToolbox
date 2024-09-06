#pragma once

#include "scene/scene.hpp"

namespace Toolbox::Scene {

    class SceneLayoutManager {
    public:
        SceneLayoutManager()                           = default;
        SceneLayoutManager(const SceneLayoutManager &) = default;
        SceneLayoutManager(SceneLayoutManager &&)      = default;

        [[nodiscard]] size_t sceneCount() const;
        [[nodiscard]] size_t scenarioCount(size_t scene) const;

        bool loadFromPath(const fs_path &path);
        bool saveToPath(const fs_path &path) const;

        [[nodiscard]] std::string getFileName(size_t scene, size_t scenario) const;
        bool setFileName(const std::string &filename, size_t scene, size_t scenario);

        bool getScenarioForFileName(const std::string &filename, size_t &scene_out,
                                    size_t &scenario_out) const;

        [[nodiscard]] std::optional<size_t> addScenario(const std::string &filename, size_t scene);
        [[nodiscard]] std::optional<size_t> addScene();

        bool moveScene(size_t src_scene, size_t dst_scene);
        bool moveScenario(size_t src_scene, size_t src_scenario, size_t dst_scene,
                          size_t dst_scenario);

        bool removeScene(size_t scene);
        bool removeScenario(size_t scene, size_t scenario);

        SceneLayoutManager &operator=(const SceneLayoutManager &) = default;
        SceneLayoutManager &operator=(SceneLayoutManager &&)      = default;

        bool operator==(const SceneLayoutManager &) const = default;

    private:
        ScopePtr<ObjectHierarchy> m_scene_layout;
    };

}  // namespace Toolbox::Scene