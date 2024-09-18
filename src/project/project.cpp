#include "project/project.hpp"

namespace Toolbox {

    bool ProjectManager::isInitialized() { return m_is_initialized; }

    bool ProjectManager::loadProjectFolder(const fs_path &path) {
        fs_path sys_path   = path / "sys";
        fs_path files_path = path / "files";

        auto sys_result   = Toolbox::Filesystem::is_directory(sys_path);
        auto files_result = Toolbox::Filesystem::is_directory(files_path);

        if (!(sys_result && sys_result.value()) || !(files_result && files_result.value())) {
            TOOLBOX_ERROR_V("Project folder is not valid: {}", path.string());
            return false;
        }

        Toolbox::Filesystem::is_directory(path).and_then([&](bool is_dir) {
            if (is_dir) {
                m_project_folder = path;

                // Process the stageArc.bin
                fs_path layout_path = path / "files" / "data" / "stageArc.bin";
                m_scene_layout_manager.loadFromPath(layout_path);

                m_is_initialized = true;
            } else {
                m_is_initialized = false;
            }

            return Result<bool, FSError>();
        });

        return true;
    }

    fs_path ProjectManager::getProjectFolder() const { return m_project_folder; }

    Scene::SceneLayoutManager &ProjectManager::getSceneLayoutManager() {
        return m_scene_layout_manager;
    }

    const Scene::SceneLayoutManager &ProjectManager::getSceneLayoutManager() const {
        return m_scene_layout_manager;
    }

}  // namespace Toolbox