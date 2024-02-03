#pragma once

#include <entt.hpp>
#include "scene/scene.hpp"

namespace Toolbox::Scene {

    class Entity {
    public:
        Entity(entt::entity handle, SceneInstance *scene);

        template <typename T, typename... Args> T& addComponent(Args &&...args) {
            return m_scene->registry().emplace(m_handle, std::forward<Args>(args)...);
        }
        template <typename T> bool hasComponent() const {
            return m_scene->registry().any_of<T>(m_handle);
        }
        template <typename T> T &getComponent() { return m_scene->registry().get<T>(m_handle); }

    private:
        entt::entity m_handle;
        SceneInstance *m_scene;
    };

}  // namespace Toolbox::Scene