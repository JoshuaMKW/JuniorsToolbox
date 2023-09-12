#pragma once

#include "nameref.hpp"
#include "objlib/meta/member.hpp"
#include "transform.hpp"
#include "types.hpp"
#include <J3D/Animation/J3DAnimationInstance.hpp>
#include <J3D/J3DLight.hpp>
#include <expected>
#include <filesystem>
#include <stacktrace>
#include <string>
#include <variant>
#include <vector>

using namespace J3DAnimation;

namespace Toolbox::Object {
    class ISceneObject;

    struct ObjectCorruptedError;
    struct ObjectGroupError;

    using ObjectError = std::variant<ObjectGroupError, ObjectCorruptedError>;

    struct ObjectCorruptedError {
        std::string m_message;
        std::stacktrace m_stack;
        ISceneObject *m_object;
    };

    struct ObjectGroupError {
        std::string m_message;
        std::stacktrace m_stack;
        ISceneObject *m_object;
        std::vector<ObjectError> m_child_errors = {};
    };

    // A scene object capable of performing in a rendered context and
    // holding modifiable and exotic values
    class ISceneObject {
    public:
        /* ABSTRACT INTERFACE */
        virtual ~ISceneObject() = default;

        [[nodiscard]] virtual bool isGroupObject() const = 0;

        [[nodiscard]] virtual std::string type() const = 0;

        [[nodiscard]] virtual NameRef getNameRef() const      = 0;
        [[nodiscard]] virtual ISceneObject *getParent() const = 0;
        [[nodiscard]] virtual std::vector<s8> getData() const = 0;
        [[nodiscard]] virtual size_t getDataSize() const      = 0;

        [[nodiscard]] virtual bool hasMember(const QualifiedName &name) const                   = 0;
        [[nodiscard]] virtual MetaStruct::GetMemberT getMember(const QualifiedName &name) const = 0;
        [[nodiscard]] virtual size_t getMemberOffset(const QualifiedName &name) const           = 0;
        [[nodiscard]] virtual size_t getMemberSize(const QualifiedName &name) const             = 0;

        virtual std::expected<void, ObjectGroupError>
        addChild(std::shared_ptr<ISceneObject> child)                                        = 0;
        virtual std::expected<void, ObjectGroupError> removeChild(ISceneObject *name)        = 0;
        virtual std::expected<void, ObjectGroupError> removeChild(const QualifiedName &name) = 0;
        virtual std::expected<std::vector<ISceneObject *>, ObjectGroupError> getChildren()   = 0;

        [[nodiscard]] virtual Transform getTransform() const  = 0;
        virtual void setTransform(const Transform &transform) = 0;

        [[nodiscard]] virtual std::optional<std::filesystem::path> getAnimationsPath() const = 0;
        virtual std::optional<std::string_view> getAnimationName() const                     = 0;
        virtual bool loadAnimationData(std::string_view name)                                = 0;

        virtual std::weak_ptr<J3DLight> getLightData(int index) = 0;

        virtual std::expected<void, ObjectError> performScene() = 0;

        virtual void dump(std::ostream &out, int indention, int indention_width) const = 0;

    protected:
        /* PROTECTED ABSTRACT INTERFACE */
        virtual std::weak_ptr<J3DAnimationInstance> getAnimationControl() const = 0;

    public:
        /* NON-VIRTUAL HELPERS */
        bool hasMember(const std::string &name) const { return hasMember(QualifiedName(name)); }
        MetaStruct::GetMemberT getMember(const std::string &name) const {
            return getMember(QualifiedName(name));
        }

        [[nodiscard]] size_t getAnimationFrames() const;
        [[nodiscard]] float getAnimationFrame() const;
        void setAnimationFrame(u16 frame);

        bool startAnimation();
        bool stopAnimation();

        void dump(std::ostream &out, int indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }
    };

    class VirtualSceneObject : public ISceneObject, public ISerializable {
    public:
        ~VirtualSceneObject() = default;

        [[nodiscard]] bool isGroupObject() const override { return false; }

        std::string type() const override { return m_type; }

        NameRef getNameRef() const override { return m_nameref; }
        ISceneObject *getParent() const override { return m_parent; }
        std::vector<s8> getData() const override;
        size_t getDataSize() const override;

        bool hasMember(const QualifiedName &name) const override;
        MetaStruct::GetMemberT getMember(const QualifiedName &name) const override;
        size_t getMemberOffset(const QualifiedName &name) const override;
        size_t getMemberSize(const QualifiedName &name) const override;

        std::expected<void, ObjectGroupError>
        addChild(std::shared_ptr<ISceneObject> child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError> removeChild(ISceneObject *object) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError> removeChild(const QualifiedName &name) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<std::vector<ISceneObject *>, ObjectGroupError> getChildren() override {
            ObjectGroupError err = {"Cannot get the children of a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        [[nodiscard]] Transform getTransform() const override { return {}; }
        void setTransform(const Transform &transform) override {}

        [[nodiscard]] std::optional<std::filesystem::path> getAnimationsPath() const override {
            return {};
        }
        std::optional<std::string_view> getAnimationName() const override { return {}; }
        bool loadAnimationData(std::string_view name) override { return false; }

        std::expected<void, ObjectError> performScene() override;

        void dump(std::ostream &out, int indention, int indention_width) const override;

    protected:
        std::weak_ptr<J3DAnimationInstance> getAnimationControl() const override { return {}; }

    public:
        // Inherited via ISerializable
        void serialize(Serializer &out) const override;
        void deserialize(Deserializer &in) override;

    protected:
        NameRef m_nameref;
        std::string m_type;
        std::vector<std::shared_ptr<MetaMember>> m_members;
        ISceneObject *m_parent = nullptr;
    };

    class GroupSceneObject final : public VirtualSceneObject {
    public:
        ~GroupSceneObject() = default;

        [[nodiscard]] bool isGroupObject() const override { return true; }

        std::vector<s8> getData() const override;
        size_t getDataSize() const override;

        bool hasMember(const QualifiedName &name) const override;
        MetaStruct::GetMemberT getMember(const QualifiedName &name) const override;
        size_t getMemberOffset(const QualifiedName &name) const override;
        size_t getMemberSize(const QualifiedName &name) const override;

        std::expected<void, ObjectGroupError>
        addChild(std::shared_ptr<ISceneObject> child) override;
        std::expected<void, ObjectGroupError> removeChild(ISceneObject *child) override;
        std::expected<void, ObjectGroupError> removeChild(const QualifiedName &name) override;
        std::expected<std::vector<ISceneObject *>, ObjectGroupError> getChildren() override;
        std::expected<void, ObjectError> performScene() override;

        void dump(std::ostream &out, int indention, int indention_width) const override;

        // Inherited via ISerializable
        void serialize(Serializer &out) const override;
        void deserialize(Deserializer &in) override;

    private:
        std::vector<std::shared_ptr<ISceneObject>> m_children;
    };

    class PhysicalSceneObject : public ISceneObject, public ISerializable {
    public:
        ~PhysicalSceneObject() = default;

        [[nodiscard]] bool isGroupObject() const override { return false; }

        std::string type() const override { return m_type; }

        NameRef getNameRef() const override { return m_nameref; }
        ISceneObject *getParent() const override { return m_parent; }
        std::vector<s8> getData() const override;
        size_t getDataSize() const override;

        bool hasMember(const QualifiedName &name) const override;
        MetaStruct::GetMemberT getMember(const QualifiedName &name) const override;
        size_t getMemberOffset(const QualifiedName &name) const override;
        size_t getMemberSize(const QualifiedName &name) const override;

        std::expected<void, ObjectGroupError>
        addChild(std::shared_ptr<ISceneObject> child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError> removeChild(ISceneObject *object) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError> removeChild(const QualifiedName &name) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<std::vector<ISceneObject *>, ObjectGroupError> getChildren() override {
            ObjectGroupError err = {"Cannot get the children of a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        [[nodiscard]] Transform getTransform() const override { return m_transform; }
        void setTransform(const Transform &transform) override { m_transform = transform; }

        [[nodiscard]] std::optional<std::filesystem::path> getAnimationsPath() const override {
            return {};
        }
        std::optional<std::string_view> getAnimationName() const override { return {}; }
        bool loadAnimationData(std::string_view name) override { return false; }

        std::expected<void, ObjectError> performScene() override;

        void dump(std::ostream &out, int indention, int indention_width) const override;

    protected:
        std::weak_ptr<J3DAnimationInstance> getAnimationControl() const override { return {}; }

    public:
        // Inherited via ISerializable
        void serialize(Serializer &out) const override;
        void deserialize(Deserializer &in) override;

    private:
        NameRef m_nameref;
        std::string m_type;
        std::vector<std::shared_ptr<MetaMember>> m_members;
        ISceneObject *m_parent = nullptr;

        Transform m_transform;
        std::shared_ptr<J3DAnimationInstance> m_anim_ctrl = {};
    };

}  // namespace Toolbox::Object