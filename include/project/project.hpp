#pragma once

#include <string>

#include "fsystem.hpp"
#include "scene/layout.hpp"

namespace Toolbox {

    class ProjectManager {
    public:
        ProjectManager()                       = default;
        ProjectManager(const ProjectManager &) = delete;
        ProjectManager(ProjectManager &&)      = default;

        ~ProjectManager() = default;

        bool isInitialized();
        bool loadProjectFolder(const fs_path &path);

        fs_path getProjectFolder() const;
        Scene::SceneLayoutManager &getSceneLayoutManager();
        const Scene::SceneLayoutManager &getSceneLayoutManager() const;

        ProjectManager &operator=(const ProjectManager &) = delete;
        ProjectManager &operator=(ProjectManager &&)      = default;

    private:
        bool m_is_initialized;
        fs_path m_project_folder;

        Scene::SceneLayoutManager m_scene_layout_manager;
    };

}  // namespace Toolbox