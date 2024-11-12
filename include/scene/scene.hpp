#pragma once

#include "bmg/bmg.hpp"
#include "objlib/object.hpp"
#include "rail/rail.hpp"
#include "raildata.hpp"
#include "smart_resource.hpp"
#include <entt.hpp>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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

        RefPtr<Object::ISceneObject> findObject(std::string_view name) const {
            if (m_root->getNameRef().name() == name) {
                return m_root;
            }
            return m_root->getChild(name);
        }

        RefPtr<Object::ISceneObject> findObject(const QualifiedName &name) const {
            if (m_root->getQualifiedName() == name) {
                return m_root;
            }
            return m_root->getChild(name);
        }

        RefPtr<Object::ISceneObject> findObject(UUID64 id) const {
            if (m_root->getUUID() == id) {
                return m_root;
            }
            return m_root->getChild(id);
        }

        Result<void, SerialError> serialize(Serializer &out) const override {
            if (!m_root) {
                return make_serial_error<void>(out, "Root object is null");
            }
            return m_root->serialize(out);
        }

        Result<void, SerialError> deserialize(Deserializer &in) override {
            SerialError err = {};
            bool error      = false;

            ObjectFactory::create(in)
                .and_then([&](auto &&obj) {
                    m_root =
                        std::static_pointer_cast<GroupSceneObject, ISceneObject>(std::move(obj));
                    return Result<bool, SerialError>();
                })
                .or_else([&](SerialError &&e) {
                    err   = std::move(e);
                    error = true;
                    return Result<bool, SerialError>();
                });

            if (error) {
                return std::unexpected(err);
            }

            return Result<void, SerialError>();
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

        static Result<ScopePtr<SceneInstance>, SerialError>
        FromPath(const std::filesystem::path &root);

        [[nodiscard]] static ScopePtr<SceneInstance> EmptyScene() {
            SceneInstance scene;
            return make_scoped<SceneInstance>(std::move(scene));
        }
        [[nodiscard]] static ScopePtr<SceneInstance> BasicScene();

        [[nodiscard]] std::optional<std::filesystem::path> rootPath() const { return m_root_path; }

        [[nodiscard]] ObjectHierarchy getObjHierarchy() const { return m_map_objects; }
        void setObjHierarchy(const ObjectHierarchy &obj_root) { m_map_objects = obj_root; }

        [[nodiscard]] ObjectHierarchy getTableHierarchy() const { return m_table_objects; }
        void setTableHierarchy(const ObjectHierarchy &table_root) { m_table_objects = table_root; }

        [[nodiscard]] RailData &getRailData() { return m_rail_info; }
        [[nodiscard]] const RailData &getRailData() const { return m_rail_info; }
        void setRailData(const RailData &data) { m_rail_info = data; }

        [[nodiscard]] BMG::MessageData getMessageData() const { return m_message_data; }
        void setMessageData(BMG::MessageData &message_data) { m_message_data = message_data; }

        Result<void, SerialError> saveToPath(const std::filesystem::path &root);

        void dump(std::ostream &os, size_t indent, size_t indent_size) const;
        void dump(std::ostream &os, size_t indent) const { dump(os, indent, 4); }
        void dump(std::ostream &os) const { dump(os, 0); }

        ScopePtr<ISmartResource> clone(bool deep) const override;

    private:
        std::optional<std::filesystem::path> m_root_path = {};
        ObjectHierarchy m_map_objects;
        ObjectHierarchy m_table_objects;
        RailData m_rail_info;
        BMG::MessageData m_message_data;
    };

}  // namespace Toolbox