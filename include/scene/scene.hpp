#pragma once

#include "clone.hpp"
#include "objlib/object.hpp"
#include "rail/rail.hpp"
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
        ObjectHierarchy(std::string_view name) : m_name(name), m_root() {} 
        ObjectHierarchy(std::shared_ptr<Object::GroupSceneObject> root) : m_root(root) {}
        ObjectHierarchy(std::string_view name, std::shared_ptr<Object::GroupSceneObject> root) : m_name(name), m_root(root) {}

        ObjectHierarchy(const ObjectHierarchy &) = default;
        ObjectHierarchy(ObjectHierarchy &&)      = default;
        ~ObjectHierarchy()                       = default;

        std::string_view name() const { return m_name; }
        void setName(std::string_view name) { m_name = name; }

        std::shared_ptr<Object::GroupSceneObject> getRoot() const { return m_root; }
        void setRoot(std::shared_ptr<Object::GroupSceneObject> root) { m_root = root; }

        std::optional<std::shared_ptr<Object::ISceneObject>>
        findObject(std::string_view name) const {
            return m_root->getChild(name);
        }
        std::optional<std::shared_ptr<Object::ISceneObject>>
        findObject(const QualifiedName &name) const {
            return m_root->getChild(name);
        }

        std::expected<void, SerialError> serialize(Serializer &out) const override {
            return m_root->serialize(out);
        }

        std::expected<void, SerialError> deserialize(Deserializer &in) override {
            return m_root->deserialize(in);
        }

        std::unique_ptr<IClonable> clone(bool deep) const override {
            if (deep) {
                ObjectHierarchy obj_hierarchy(make_deep_clone<Object::GroupSceneObject>(m_root));
                return std::make_unique<ObjectHierarchy>(std::move(obj_hierarchy));
            }

            ObjectHierarchy obj_hierarchy(make_clone<Object::GroupSceneObject>(m_root));
            return std::make_unique<ObjectHierarchy>(*this);
        }

        ObjectHierarchy &operator=(const ObjectHierarchy &) = default;

        void dump(std::ostream &os, size_t indent, size_t indent_size) const;
        void dump(std::ostream &os, size_t indent) const { dump(os, indent, 4); }
        void dump(std::ostream &os) const { dump(os, 0); }

    private:
        std::string m_name = "ObjectHierarchy";
        std::shared_ptr<Object::GroupSceneObject> m_root;
    };

    class SceneInstance : public IClonable {
    public:
        SceneInstance(const std::filesystem::path &root);
        SceneInstance(std::shared_ptr<Object::GroupSceneObject> root_obj);
        SceneInstance(std::shared_ptr<Object::GroupSceneObject> root_obj,
                      const std::vector<std::shared_ptr<Rail::Rail>> &rails);
        SceneInstance(std::shared_ptr<Object::GroupSceneObject> obj_root,
                      std::shared_ptr<Object::GroupSceneObject> table_root,
                      const std::vector<std::shared_ptr<Rail::Rail>> &rails);

    protected:
        SceneInstance() = default;

    public:
        ~SceneInstance() = default;

        [[nodiscard]] static std::unique_ptr<SceneInstance> EmptyScene() {
            SceneInstance scene;
            return std::make_unique<SceneInstance>(std::move(scene));
        }
        [[nodiscard]] static std::unique_ptr<SceneInstance> BasicScene();

        [[nodiscard]] ObjectHierarchy getObjHierarchy() const { return m_map_objects; }
        void setObjHierarchy(const ObjectHierarchy &obj_root) { m_map_objects = obj_root; }

        [[nodiscard]] ObjectHierarchy getTableHierarchy() const { return m_table_objects; }
        void setTableHierarchy(const ObjectHierarchy &table_root) { m_table_objects = table_root; }
        [[nodiscard]] std::vector<std::shared_ptr<Rail::Rail>> getRails() const { return m_rails; }
        void setRails(const std::vector<std::shared_ptr<Rail::Rail>> &rails) { m_rails = rails; }

        void dump(std::ostream &os, size_t indent, size_t indent_size) const;
        void dump(std::ostream &os, size_t indent) const { dump(os, indent, 4); }
        void dump(std::ostream &os) const { dump(os, 0); }

        std::unique_ptr<IClonable> clone(bool deep) const override;

    private:
        std::optional<std::filesystem::path> m_root_path = {};
        ObjectHierarchy m_map_objects;
        ObjectHierarchy m_table_objects;
        std::vector<std::shared_ptr<Rail::Rail>> m_rails;
    };

}  // namespace Toolbox::Scene