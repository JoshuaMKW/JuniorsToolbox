#include "scene/scene.hpp"
#include <fstream>

namespace Toolbox {

    void ObjectHierarchy::dump(std::ostream &os, size_t indent, size_t indent_size) const {
        std::string indent_str(indent * indent_size, ' ');
        os << indent_str << "ObjectHierarchy (" << m_name << ") {\n";
        m_root->dump(os, indent + 1, indent_size);
        os << indent_str << "}\n";
    }

    Result<ScopePtr<SceneInstance>, SerialError>
    SceneInstance::FromPath(const std::filesystem::path &root, bool include_custom_objs) {
        ScopePtr<SceneInstance> scene = make_scoped<SceneInstance>();
        scene->m_root_path            = root;

        scene->m_map_objects   = ObjectHierarchy("Map");
        scene->m_table_objects = ObjectHierarchy("Table");

        scene->m_map_objects.setIncludeCustomObjects(include_custom_objs);
        scene->m_table_objects.setIncludeCustomObjects(include_custom_objs);

        fs_path scene_bin   = root / "map/scene.bin";
        fs_path tables_bin  = root / "map/tables.bin";
        fs_path rail_bin    = root / "map/scene.ral";
        fs_path message_bin = root / "map/message.bmg";

        // Load the scene data
        {
            std::ifstream file(scene_bin, std::ios::in | std::ios::binary);
            Deserializer in(file.rdbuf(), scene_bin.string());

            Result<void, SerialError> result = scene->m_map_objects.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Load the table data
        {
            std::ifstream file(tables_bin, std::ios::in | std::ios::binary);
            Deserializer in(file.rdbuf());

            Result<void, SerialError> result = scene->m_table_objects.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Load the rail data
        {
            std::ifstream file(rail_bin, std::ios::in | std::ios::binary);
            Deserializer in(file.rdbuf());

            Result<void, SerialError> result = scene->m_rail_info.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Load the message data
        {
            std::ifstream file(message_bin, std::ios::in | std::ios::binary);
            if (file.is_open()) {  // scene.bmg is optional
                Deserializer in(file.rdbuf());

                Result<void, SerialError> result = scene->m_message_data.deserialize(in);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
        }

        return scene;
    }

    SceneInstance::SceneInstance() {}

    SceneInstance::SceneInstance(const SceneInstance &) {}

    SceneInstance::~SceneInstance() {}

    ScopePtr<SceneInstance> SceneInstance::BasicScene() { return nullptr; }

    Result<void, SerialError> SceneInstance::saveToPath(const std::filesystem::path &root) {
        auto scene_bin   = root / "map/scene.bin";
        auto tables_bin  = root / "map/tables.bin";
        auto rail_bin    = root / "map/scene.ral";
        auto message_bin = root / "map/message.bmg";

        {
            std::ofstream file(scene_bin, std::ios::out | std::ios::binary);
            Serializer out(file.rdbuf(), scene_bin.string());

            auto result = m_map_objects.serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        {
            std::ofstream file(tables_bin, std::ios::out | std::ios::binary);
            Serializer out(file.rdbuf(), scene_bin.string());

            auto result = m_table_objects.serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        {
            std::ofstream file(rail_bin, std::ios::out | std::ios::binary);
            Serializer out(file.rdbuf(), scene_bin.string());

            auto result = m_rail_info.serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        {
            std::ofstream file(message_bin, std::ios::out | std::ios::binary);
            Serializer out(file.rdbuf(), scene_bin.string());

            auto result = m_message_data.serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        m_root_path = root;
        return {};
    }

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
        m_rail_info.dump(os, indent + 1, indent_size);
        m_message_data.dump(os, indent + 1, indent_size);
        os << indent_str << "}" << std::endl;
    }

    ScopePtr<ISmartResource> SceneInstance::clone(bool deep) const {
        auto scene_instance         = make_scoped<SceneInstance>();
        scene_instance->m_root_path = m_root_path;

        if (deep) {
            scene_instance->m_map_objects   = *make_deep_clone(m_map_objects);
            scene_instance->m_table_objects = *make_deep_clone(m_table_objects);
            scene_instance->m_rail_info     = *make_deep_clone(m_rail_info);
            scene_instance->m_message_data  = m_message_data;
            return scene_instance;
        }

        scene_instance->m_map_objects   = *make_clone(m_map_objects);
        scene_instance->m_table_objects = *make_clone(m_table_objects);
        scene_instance->m_rail_info     = *make_clone(m_rail_info);
        scene_instance->m_message_data  = m_message_data;
        return scene_instance;
    }

}  // namespace Toolbox