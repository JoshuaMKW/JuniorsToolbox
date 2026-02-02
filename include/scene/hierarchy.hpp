#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "bmg/bmg.hpp"
#include "objlib/object.hpp"
#include "rail/rail.hpp"
#include "raildata.hpp"
#include "smart_resource.hpp"

namespace Toolbox::Scene {

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

        bool includeCustomObjects() const { return m_include_custom; }
        void setIncludeCustomObjects(bool include) { m_include_custom = include; }

        std::string_view name() const { return m_name; }
        void setName(std::string_view name) { m_name = name; }

        size_t getSize() const { return 1 + m_root->getTotalChildren(); }

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

        RefPtr<Object::ISceneObject>
        findObjectByType(std::string_view type,
                         std::optional<std::string_view> name = std::nullopt) const {
            if (m_root->type() == name) {
                return m_root;
            }
            return m_root->getChildByType(type, name);
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

            ObjectFactory::create(in, m_include_custom)
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
        bool m_include_custom = false;
    };

}