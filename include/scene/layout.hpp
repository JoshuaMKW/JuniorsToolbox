#pragma once

#include "scene/scene.hpp"

namespace Toolbox::Scene {

    class SceneLayout {
    public:
        SceneLayout()                    = default;
        SceneLayout(const SceneLayout &) = default;
        SceneLayout(SceneLayout &&)      = default;

        [[nodiscard]] size_t sceneCount() const;
        [[nodiscard]] size_t scenarioCount(size_t scene) const;

        [[nodiscard]] Result<std::string, BaseError> getFileName(size_t scene,
                                                                 size_t scenario) const;
        bool setFileName(const std::string &filename, size_t scene, size_t scenario);

        bool getScenarioForFileName(const std::string &filename, size_t &scene_out,
                                    size_t &scenario_out) const;

        [[nodiscard]] size_t addScenario(const std::string &filename, size_t scene);
        [[nodiscard]] size_t addScene();

        bool moveScene(size_t src_scene, size_t dst_scene);
        bool moveScenario(size_t src_scene, size_t src_scenario, size_t dst_scene,
                          size_t dst_scenario);

        SceneLayout &operator=(const SceneLayout &) = default;
        SceneLayout &operator=(SceneLayout &&)      = default;

        bool operator==(const SceneLayout &) const = default;

    private:
        ScopePtr<ObjectHierarchy> m_scene_layout;
    };

}  // namespace Toolbox::Scene