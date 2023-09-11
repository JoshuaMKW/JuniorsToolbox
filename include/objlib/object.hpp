#pragma once

#include "nameref.hpp"
#include "types.hpp"
#include <expected>
#include <stacktrace>
#include <string>
#include <variant>
#include <vector>

namespace Toolbox::Object {
    struct ISceneObject;

    struct ObjectGroupError {
        std::string m_message;
        std::stacktrace m_stack;
        ISceneObject *m_object;
    };

    struct ObjectCorruptedError {
        std::string m_message;
        std::stacktrace m_stack;
        ISceneObject *m_object;
    };

    using ObjectError = std::variant<ObjectGroupError, ObjectCorruptedError>;

    // A scene object capable of performing in a rendered context and
    // holding modifiable and exotic values
    struct ISceneObject {
        virtual ~ISceneObject() = default;

        [[nodiscard]] virtual bool isGroupObject() const = 0;

        virtual std::string type() const = 0;

        virtual NameRef getNameRef() const      = 0;
        virtual ISceneObject *getParent() const = 0;
        virtual std::vector<s8> getData() const = 0;
        virtual size_t getDataSize() const      = 0;

        virtual bool hasMember(const QualifiedName &name) const              = 0;
        virtual void *getMemberPtr(const QualifiedName &name) const          = 0;
        virtual size_t getMemberOffset(const QualifiedName &name) const = 0;
        virtual size_t getMemberSize(const QualifiedName &name) const   = 0;

        virtual std::expected<void, ObjectGroupError> addChild(ISceneObject *child)        = 0;
        virtual std::expected<void, ObjectGroupError> removeChild(ISceneObject *child)     = 0;
        virtual std::expected<std::vector<ISceneObject *>, ObjectGroupError> getChildren() = 0;

        virtual std::expected<void, ObjectError> performScene() = 0;

        virtual void dump(std::ostream &out, int indention = 0, int indention_width = 2) const = 0;
    };

    class BasicSceneObject : public ISceneObject, public ISerializable {
    public:
        ~BasicSceneObject() = default;

        [[nodiscard]] bool isGroupObject() const override { return false; }

        std::string type() const override { return m_type; }

        NameRef getNameRef() const override { return m_nameref; }
        ISceneObject *getParent() const override { return m_parent; }
        std::vector<s8> getData() const override;
        size_t getDataSize() const override;

        bool hasMember(const QualifiedName &name) const override;
        void *getMemberPtr(const QualifiedName &name) const override;
        size_t getMemberOffset(const QualifiedName &name) const override;
        size_t getMemberSize(const QualifiedName &name) const override;

        std::expected<void, ObjectGroupError> addChild(ISceneObject *child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.", std::stacktrace::current(),
                                    this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError> removeChild(ISceneObject *child) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.", std::stacktrace::current(),
                                    this};
            return std::unexpected(err);
        }

        std::expected<std::vector<ISceneObject *>, ObjectGroupError> getChildren() override {
            ObjectGroupError err = {"Cannot get the children of a non-group object.", std::stacktrace::current(),
                                    this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectError> performScene() override;

        void dump(std::ostream &out, int indention = 0, int indention_width = 2) const override;

        // Inherited via ISerializable
        void serialize(Serializer &out) const override;
        void deserialize(Deserializer &in) override;

    private:
        NameRef m_nameref;
        std::string m_type;

        ISceneObject *m_parent;
    };

}  // namespace Toolbox::Object