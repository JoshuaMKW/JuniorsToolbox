#include "scene/scene.hpp"
#include <fstream>

namespace Toolbox::Scene {

    void ObjectHierarchy::dump(std::ostream &os, size_t indent, size_t indent_size) const {
        std::string indent_str(indent * indent_size, ' ');
        os << indent_str << "ObjectHierarchy (" << m_name << ") {\n";
        m_root->dump(os, indent + 1, indent_size);
        os << indent_str << "}\n";
    }

    SceneInstance::SceneInstance(const std::filesystem::path &root) : m_root_path(root) {
        auto scene_bin  = root / "map/scene.bin";
        auto tables_bin = root / "map/tables.bin";

        ObjectFactory::create_t map_root_obj;
        ObjectFactory::create_t table_root_obj;

        {
            std::ifstream file(scene_bin, std::ios::in | std::ios::binary);
            Deserializer in(file.rdbuf());

            map_root_obj = ObjectFactory::create(in);
            if (!map_root_obj) {
                throw std::runtime_error("Failed to create root object");
            }
        }

        {
            std::ifstream file(tables_bin, std::ios::in | std::ios::binary);
            Deserializer in(file.rdbuf());

            table_root_obj = ObjectFactory::create(in);
            if (!table_root_obj) {
                throw std::runtime_error("Failed to create table root object");
            }
        }

        std::shared_ptr<Object::GroupSceneObject> map_root_obj_ptr =
            std::static_pointer_cast<GroupSceneObject, ISceneObject>(
                std::move(map_root_obj.value()));
        std::shared_ptr<Object::GroupSceneObject> table_root_obj_ptr =
            std::static_pointer_cast<GroupSceneObject, ISceneObject>(
                std::move(table_root_obj.value()));

        m_map_objects   = ObjectHierarchy("Map", map_root_obj_ptr);
        m_table_objects = ObjectHierarchy("Table", table_root_obj_ptr);

        m_rails = {};
    }

    SceneInstance::SceneInstance(std::shared_ptr<Object::GroupSceneObject> root_obj) {}

    SceneInstance::SceneInstance(std::shared_ptr<Object::GroupSceneObject> root_obj,
                                 const std::vector<std::shared_ptr<Rail::Rail>> &rails) {}

    SceneInstance::SceneInstance(std::shared_ptr<Object::GroupSceneObject> obj_root,
                                 std::shared_ptr<Object::GroupSceneObject> table_root,
                                 const std::vector<std::shared_ptr<Rail::Rail>> &rails) {}

    std::unique_ptr<SceneInstance> SceneInstance::BasicScene() { return nullptr; }

    void SceneInstance::dump(std::ostream &os, size_t indent, size_t indent_size) const {
        std::string indent_str(indent * indent_size, ' ');
        if (!m_root_path) {
            os << indent_str << "SceneInstance (null) {" << std::endl;
        } else {
            os << indent_str << "SceneInstance (" << m_root_path->parent_path().filename() << ") {"
               << std::endl;
        }
        m_map_objects.dump(os, indent + 1, indent_size);
        m_table_objects.dump(os, indent + 1, indent_size);
        os << indent_str << "}" << std::endl;
    }

    std::unique_ptr<IClonable> SceneInstance::clone(bool deep) const {
        if (deep) {
            SceneInstance scene_instance(
                make_deep_clone<Object::GroupSceneObject>(m_map_objects.getRoot()),
                make_deep_clone<Object::GroupSceneObject>(m_table_objects.getRoot()), m_rails);
            return std::make_unique<SceneInstance>(std::move(scene_instance));
        }

        SceneInstance scene_instance(
            make_clone<Object::GroupSceneObject>(m_map_objects.getRoot()),
            make_clone<Object::GroupSceneObject>(m_table_objects.getRoot()), m_rails);
        return std::make_unique<SceneInstance>(std::move(scene_instance));
    }

}  // namespace Toolbox::Scene