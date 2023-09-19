#include "scene/scene.hpp"

namespace Toolbox::Scene {
    SceneInstance::SceneInstance(const std::filesystem::path &root) {
        
    }
    SceneInstance::SceneInstance(std::shared_ptr<Object::GroupSceneObject> root_obj) {}

    SceneInstance::SceneInstance(std::shared_ptr<Object::GroupSceneObject> root_obj,
                                      const std::vector<std::shared_ptr<Rail>> &rails) {}

    SceneInstance::SceneInstance(std::shared_ptr<Object::GroupSceneObject> obj_root,
                                      std::shared_ptr<Object::GroupSceneObject> table_root,
                                      const std::vector<std::shared_ptr<Rail>> &rails) {}

    std::unique_ptr<SceneInstance> SceneInstance::BasicScene() { return nullptr; }

    void SceneInstance::dump(std::ostream &os, int indent, int indent_size) const {}
}  // namespace Toolbox::Scene