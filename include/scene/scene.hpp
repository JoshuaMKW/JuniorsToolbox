#pragma once

#include "clone.hpp"
#include "objlib/object.hpp"
#include "rail.hpp"
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Toolbox::Scene {

    class ObjectHierarchy : public ISerializable, public IClonable {
    public:
        ObjectHierarchy() = default;
        ObjectHierarchy(std::shared_ptr<Object::GroupSceneObject> root) : m_root(root) {}

        ObjectHierarchy(const ObjectHierarchy &) = default;
        ObjectHierarchy(ObjectHierarchy &&)      = default;
        ~ObjectHierarchy()                       = default;

        std::shared_ptr<Object::GroupSceneObject> getRoot() const { return m_root; }
        void setRoot(std::shared_ptr<Object::GroupSceneObject> root) { m_root = root; }

        std::shared_ptr<Object::ISceneObject> findObject(std::string_view name) const;
        std::shared_ptr<Object::ISceneObject> findObject(const QualifiedName &name) const;

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        std::unique_ptr<IClonable> clone(bool deep) const override {
            if (deep) {
                auto root = make_deep_clone<Object::GroupSceneObject>(m_root);
                ObjectHierarchy obj_hierarchy;
            }
            return std::make_unique<ObjectHierarchy>(*this);
        }

    private:
        std::shared_ptr<Object::GroupSceneObject> m_root;
    };

    class SceneInstance : public IClonable {
    public:
        SceneInstance(const std::filesystem::path &root);
        SceneInstance(std::shared_ptr<Object::GroupSceneObject> root_obj);
        SceneInstance(std::shared_ptr<Object::GroupSceneObject> root_obj,
                      const std::vector<std::shared_ptr<Rail>> &rails);
        SceneInstance(std::shared_ptr<Object::GroupSceneObject> obj_root,
                      std::shared_ptr<Object::GroupSceneObject> table_root,
                      const std::vector<std::shared_ptr<Rail>> &rails);

    protected:
        SceneInstance() = default;

    public:
        ~SceneInstance() = default;

        [[nodiscard]] static std::unique_ptr<SceneInstance> BasicScene();

        [[nodiscard]] std::shared_ptr<Object::GroupSceneObject> getObjRoot() const {
            return m_obj_root;
        }
        void setObjRoot(std::shared_ptr<Object::GroupSceneObject> obj_root) {
            m_obj_root = obj_root;
        }

        [[nodiscard]] std::shared_ptr<Object::GroupSceneObject> getTableRoot() const {
            return m_table_root;
        }
        void setTableRoot(std::shared_ptr<Object::GroupSceneObject> table_root) {
            m_table_root = table_root;
        }
        [[nodiscard]] std::vector<std::shared_ptr<Rail>> getRails() const { return m_rails; }
        void setRails(const std::vector<std::shared_ptr<Rail>> &rails) { m_rails = rails; }

        void dump(std::ostream &os, int indent, int indent_size) const;
        void dump(std::ostream &os, int indent) const { dump(os, indent, 4); }
        void dump(std::ostream &os) const { dump(os, 0); }

    private:
        std::shared_ptr<Object::GroupSceneObject> m_obj_root;
        std::shared_ptr<Object::GroupSceneObject> m_table_root;
        std::vector<std::shared_ptr<Rail>> m_rails;
    };

}  // namespace Toolbox::Scene