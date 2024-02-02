#pragma once

#include "bmg/bmg.hpp"
#include "smart_resource.hpp"
#include "objlib/object.hpp"
#include "rail/rail.hpp"
#include "raildata.hpp"
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <entt.hpp>

namespace Toolbox {

    class ObjectHierarchy : public ISerializable, public ISmartResource {
    public:
        ObjectHierarchy() = default;
        ObjectHierarchy(std::string_view name) : m_name(name), m_root() {}
        ObjectHierarchy(RefPtr<Object::GroupSceneObject> root) : m_root(root) {}
        ObjectHierarchy(std::string_view name, RefPtr<Object::GroupSceneObject> root)
            : m_name(name), m_root(root) {}

        ObjectHierarchy(const ObjectHierarchy &) = default;
        ObjectHierarchy(ObjectHierarchy &&)      = default;
        ~ObjectHierarchy()                       = default;

        std::string_view name() const { return m_name; }
        void setName(std::string_view name) { m_name = name; }

        RefPtr<Object::GroupSceneObject> getRoot() const { return m_root; }
        void setRoot(RefPtr<Object::GroupSceneObject> root) { m_root = root; }

        std::optional<RefPtr<Object::ISceneObject>>
        findObject(std::string_view name) const {
            return m_root->getChild(name);
        }
        std::optional<RefPtr<Object::ISceneObject>>
        findObject(const QualifiedName &name) const {
            return m_root->getChild(name);
        }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            return m_root->serialize(out);
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            return m_root->deserialize(in);
        }

        ScopePtr<ISmartResource> clone(bool deep) const override {
            if (deep) {
                ObjectHierarchy obj_hierarchy(make_deep_clone<Object::GroupSceneObject>(m_root));
                return make_scoped<ObjectHierarchy>(std::move(obj_hierarchy));
            }

            ObjectHierarchy obj_hierarchy(make_clone<Object::GroupSceneObject>(m_root));
            return make_scoped<ObjectHierarchy>(*this);
        }

        ObjectHierarchy &operator=(const ObjectHierarchy &) = default;

        void dump(std::ostream &os, size_t indent, size_t indent_size) const;
        void dump(std::ostream &os, size_t indent) const { dump(os, indent, 4); }
        void dump(std::ostream &os) const { dump(os, 0); }

    private:
        std::string m_name = "ObjectHierarchy";
        RefPtr<Object::GroupSceneObject> m_root;
    };

    class SceneInstance : public ISmartResource {
        friend class Entity;

    public:
        SceneInstance();
        SceneInstance(const SceneInstance &);

    public:
        ~SceneInstance();

        
        static std::expected<ScopePtr<SceneInstance>, SerialError>
        FromPath(const std::filesystem::path &root);

        [[nodiscard]] static ScopePtr<SceneInstance> EmptyScene() {
            SceneInstance scene;
            return make_scoped<SceneInstance>(std::move(scene));
        }
        [[nodiscard]] static ScopePtr<SceneInstance> BasicScene();

        [[nodiscard]] std::optional<std::filesystem::path> rootPath() const { return m_root_path; }

        [[nodiscard]] entt::registry &registry() { return m_registry; }

        [[nodiscard]] ObjectHierarchy getObjHierarchy() const { return m_map_objects; }
        void setObjHierarchy(const ObjectHierarchy &obj_root) { m_map_objects = obj_root; }

        [[nodiscard]] ObjectHierarchy getTableHierarchy() const { return m_table_objects; }
        void setTableHierarchy(const ObjectHierarchy &table_root) { m_table_objects = table_root; }
        [[nodiscard]] RailData getRailData() const { return m_rail_info; }
        void setRailData(RailData &rails) { m_rail_info = rails; }

        [[nodiscard]] BMG::MessageData getMessageData() const { return m_message_data; }
        void setMessageData(BMG::MessageData &message_data) { m_message_data = message_data; }

        std::expected<void, SerialError> saveToPath(const std::filesystem::path &root);

        void dump(std::ostream &os, size_t indent, size_t indent_size) const;
        void dump(std::ostream &os, size_t indent) const { dump(os, indent, 4); }
        void dump(std::ostream &os) const { dump(os, 0); }

        ScopePtr<ISmartResource> clone(bool deep) const override;

    protected:
        entt::registry m_registry;

    private:
        std::optional<std::filesystem::path> m_root_path = {};
        ObjectHierarchy m_map_objects;
        ObjectHierarchy m_table_objects;
        RailData m_rail_info;
        BMG::MessageData m_message_data;
    };

}  // namespace Toolbox::Scene