#pragma once

#include "nameref.hpp"
#include "objlib/meta/member.hpp"
#include "template.hpp"
#include "transform.hpp"
#include "types.hpp"
#include <J3D/Animation/J3DAnimationInstance.hpp>
#include <J3D/J3DLight.hpp>
#include <J3D/J3DModelInstance.hpp>
#include <expected>
#include <filesystem>
#include <stacktrace>
#include <string>
#include <unordered_map>
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

    enum class AnimationType { BCK, BLK, BPK, BTP, BTK, BRK };
    constexpr std::optional<AnimationType> animationTypeFromPath(std::string_view path) {
        auto ext = path.substr(path.find_last_of('.'));
        if (ext == ".bck")
            return AnimationType::BCK;
        if (ext == ".blk")
            return AnimationType::BLK;
        if (ext == ".bpk")
            return AnimationType::BPK;
        if (ext == ".btp")
            return AnimationType::BTP;
        if (ext == ".btk")
            return AnimationType::BTK;
        if (ext == ".brk")
            return AnimationType::BRK;
        return {};
    }

    using model_cache_t = std::unordered_map<std::string, std::shared_ptr<J3DModelData>>;

    // A scene object capable of performing in a rendered context and
    // holding modifiable and exotic values
    class ISceneObject : public ISerializable, public IClonable {
    public:
        friend class ObjectFactory;

        /* ABSTRACT INTERFACE */
        virtual ~ISceneObject() = default;

        [[nodiscard]] virtual bool isGroupObject() const = 0;

        [[nodiscard]] virtual std::string type() const = 0;

        [[nodiscard]] virtual NameRef getNameRef() const = 0;
        virtual void setNameRef(NameRef name)            = 0;

        [[nodiscard]] virtual ISceneObject *getParent() const                         = 0;
        virtual std::expected<void, ObjectGroupError> setParent(ISceneObject *parent) = 0;

        [[nodiscard]] virtual std::vector<s8> getData() const = 0;
        [[nodiscard]] virtual size_t getDataSize() const      = 0;

        [[nodiscard]] virtual bool hasMember(const QualifiedName &name) const                   = 0;
        [[nodiscard]] virtual MetaStruct::GetMemberT getMember(const QualifiedName &name) const = 0;
        [[nodiscard]] virtual std::vector<std::shared_ptr<MetaMember>> getMembers() const       = 0;
        [[nodiscard]] virtual size_t getMemberOffset(const QualifiedName &name,
                                                     int index) const                           = 0;
        [[nodiscard]] virtual size_t getMemberSize(const QualifiedName &name, int index) const  = 0;

        virtual std::expected<void, ObjectGroupError>
        addChild(std::shared_ptr<ISceneObject> child) = 0;
        virtual std::expected<void, ObjectGroupError>
        removeChild(std::shared_ptr<ISceneObject> name)                                      = 0;
        virtual std::expected<void, ObjectGroupError> removeChild(const QualifiedName &name) = 0;
        [[nodiscard]] virtual std::expected<std::vector<std::shared_ptr<ISceneObject>>,
                                            ObjectGroupError>
        getChildren() = 0;
        [[nodiscard]] virtual std::optional<std::shared_ptr<ISceneObject>>
        getChild(const QualifiedName &name) = 0;

        [[nodiscard]] virtual std::optional<J3DTransformInfo> getTransform() const  = 0;
        virtual void setTransform(J3DTransformInfo transform) = 0;

        [[nodiscard]] virtual std::optional<std::filesystem::path> getAnimationsPath() const = 0;
        [[nodiscard]] virtual std::optional<std::string_view>
        getAnimationName(AnimationType type) const                                = 0;
        virtual bool loadAnimationData(std::string_view name, AnimationType type) = 0;

        [[nodiscard]] virtual J3DLight getLightData(int index) = 0;

        virtual std::expected<void, ObjectError>
        performScene(std::vector<std::shared_ptr<J3DModelInstance>> &renderables,
                     model_cache_t &model_cache) = 0;

        virtual void dump(std::ostream &out, size_t indention, size_t indention_width) const = 0;

    protected:
        /* PROTECTED ABSTRACT INTERFACE */
        [[nodiscard]] virtual std::weak_ptr<J3DAnimationInstance>
        getAnimationControl(AnimationType type) const = 0;

    public:
        [[nodiscard]] QualifiedName getQualifiedName() const;

        /* NON-VIRTUAL HELPERS */
        [[nodiscard]] bool hasMember(const std::string &name) const {
            return hasMember(QualifiedName(name));
        }
        [[nodiscard]] MetaStruct::GetMemberT getMember(const std::string &name) const {
            return getMember(QualifiedName(name));
        }

        [[nodiscard]] std::optional<std::shared_ptr<ISceneObject>>
        getChild(const std::string &name) {
            return getChild(QualifiedName(name));
        }

        [[nodiscard]] size_t getAnimationFrames(AnimationType type) const;
        [[nodiscard]] float getAnimationFrame(AnimationType type) const;
        void setAnimationFrame(size_t frame, AnimationType type);

        bool startAnimation(AnimationType type);
        bool stopAnimation(AnimationType type);

        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }
    };

    class VirtualSceneObject : public ISceneObject {
    public:
        friend class ObjectFactory;

        VirtualSceneObject() = default;

        VirtualSceneObject(const Template &template_) : ISceneObject(), m_nameref() {
            m_type = template_.type();

            auto wizard = template_.getWizard();
            if (!wizard)
                return;

            applyWizard(*wizard);
        }

        VirtualSceneObject(const Template &template_, std::string_view wizard_name)
            : ISceneObject(), m_nameref() {
            m_type = template_.type();

            auto wizard = template_.getWizard(wizard_name);
            if (!wizard)
                return;

            applyWizard(*wizard);
        }

        VirtualSceneObject(const Template &template_, Deserializer &in)
            : VirtualSceneObject(template_) {
            deserialize(in);
        }

        VirtualSceneObject(const Template &template_, std::string_view wizard_name,
                           Deserializer &in)
            : VirtualSceneObject(template_, wizard_name) {
            deserialize(in);
        }
        VirtualSceneObject(const Template &template_, Deserializer &in, ISceneObject *parent)
            : VirtualSceneObject(template_, in) {
            parent->addChild(std::shared_ptr<VirtualSceneObject>(this));
        }
        VirtualSceneObject(const Template &template_, std::string_view wizard_name,
                           Deserializer &in, ISceneObject *parent)
            : VirtualSceneObject(template_, wizard_name, in) {
            parent->addChild(std::shared_ptr<VirtualSceneObject>(this));
        }
        VirtualSceneObject(const VirtualSceneObject &) = default;
        VirtualSceneObject(VirtualSceneObject &&)      = default;

    public:
        ~VirtualSceneObject() override = default;

        [[nodiscard]] bool isGroupObject() const override { return false; }

        std::string type() const override { return m_type; }

        NameRef getNameRef() const override { return m_nameref; }
        void setNameRef(NameRef nameref) override { m_nameref = nameref; }

        ISceneObject *getParent() const override { return m_parent; }
        std::expected<void, ObjectGroupError> setParent(ISceneObject *parent) override {
            m_parent = parent;
            return {};
        }

        std::vector<s8> getData() const override;
        size_t getDataSize() const override;

        bool hasMember(const QualifiedName &name) const override;
        MetaStruct::GetMemberT getMember(const QualifiedName &name) const override;
        std::vector<std::shared_ptr<MetaMember>> getMembers() const override { return m_members; }
        size_t getMemberOffset(const QualifiedName &name, int index) const override;
        size_t getMemberSize(const QualifiedName &name, int index) const override;

        std::expected<void, ObjectGroupError>
        addChild(std::shared_ptr<ISceneObject> child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError>
        removeChild(std::shared_ptr<ISceneObject> object) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError> removeChild(const QualifiedName &name) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        [[nodiscard]] std::expected<std::vector<std::shared_ptr<ISceneObject>>, ObjectGroupError>
        getChildren() override {
            ObjectGroupError err = {"Cannot get the children of a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }
        [[nodiscard]] std::optional<std::shared_ptr<ISceneObject>>
        getChild(const QualifiedName &name) override {
            return {};
        }

        [[nodiscard]] std::optional<J3DTransformInfo> getTransform() const override { return {}; }
        void setTransform(J3DTransformInfo transform) override {}

        [[nodiscard]] std::optional<std::filesystem::path> getAnimationsPath() const override {
            return {};
        }
        [[nodiscard]] std::optional<std::string_view>
        getAnimationName(AnimationType type) const override {
            return {};
        }
        bool loadAnimationData(std::string_view name, AnimationType type) override { return false; }

        [[nodiscard]] J3DLight getLightData(int index) override { return {}; }

        std::expected<void, ObjectError>
        performScene(std::vector<std::shared_ptr<J3DModelInstance>> &renderables,
                     model_cache_t &model_cache) override;

        void dump(std::ostream &out, size_t indention, size_t indention_width) const override;

    protected:
        [[nodiscard]] std::weak_ptr<J3DAnimationInstance>
        getAnimationControl(AnimationType type) const override {
            return {};
        }

        void applyWizard(const TemplateWizard &wizard);

    public:
        // Inherited via ISerializable
        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        std::unique_ptr<IClonable> clone(bool deep) const override {
            if (deep) {
                VirtualSceneObject obj;
                obj.m_type    = m_type;
                obj.m_nameref = m_nameref;
                obj.m_parent  = m_parent;
                obj.m_members.reserve(m_members.size());
                for (const auto &member : m_members) {
                    auto new_member = make_deep_clone<MetaMember>(member);
                    obj.m_members.push_back(new_member);
                }
                return std::make_unique<VirtualSceneObject>(std::move(obj));
            }
            return std::make_unique<VirtualSceneObject>(*this);
        }

    protected:
        std::string m_type;
        NameRef m_nameref;
        std::vector<std::shared_ptr<MetaMember>> m_members;
        ISceneObject *m_parent = nullptr;

        mutable MetaStruct::CacheMemberT m_member_cache;
    };

    class GroupSceneObject : public VirtualSceneObject {
    public:
        friend class ObjectFactory;

        GroupSceneObject() : VirtualSceneObject() {
            m_group_size =
                std::make_shared<MetaMember>("GroupSize", MetaValue(static_cast<u32>(0)));
        }
        GroupSceneObject(const Template &template_) : VirtualSceneObject(template_) {
            m_group_size =
                std::make_shared<MetaMember>("GroupSize", MetaValue(static_cast<u32>(0)));
        }
        GroupSceneObject(const Template &template_, std::string_view wizard_name)
            : VirtualSceneObject(template_, wizard_name) {
            m_group_size =
                std::make_shared<MetaMember>("GroupSize", MetaValue(static_cast<u32>(0)));
        }
        GroupSceneObject(const Template &template_, Deserializer &in)
            : GroupSceneObject(template_) {
            deserialize(in);
        }
        GroupSceneObject(const Template &template_, std::string_view wizard_name, Deserializer &in)
            : GroupSceneObject(template_, wizard_name) {
            deserialize(in);
        }
        GroupSceneObject(const Template &template_, Deserializer &in, ISceneObject *parent)
            : GroupSceneObject(template_, in) {
            parent->addChild(std::shared_ptr<GroupSceneObject>(this));
        }
        GroupSceneObject(const Template &template_, std::string_view wizard_name, Deserializer &in,
                         ISceneObject *parent)
            : GroupSceneObject(template_, wizard_name, in) {
            parent->addChild(std::shared_ptr<GroupSceneObject>(this));
        }
        GroupSceneObject(const GroupSceneObject &) = default;
        GroupSceneObject(GroupSceneObject &&)      = default;

    public:
        ~GroupSceneObject() override = default;

        [[nodiscard]] bool isGroupObject() const override { return true; }

        [[nodiscard]] std::vector<s8> getData() const override;
        [[nodiscard]] size_t getDataSize() const override;

        std::expected<void, ObjectGroupError>
        addChild(std::shared_ptr<ISceneObject> child) override;
        std::expected<void, ObjectGroupError>
        removeChild(std::shared_ptr<ISceneObject> child) override;
        std::expected<void, ObjectGroupError> removeChild(const QualifiedName &name) override;
        [[nodiscard]] std::expected<std::vector<std::shared_ptr<ISceneObject>>, ObjectGroupError>
        getChildren() override;
        [[nodiscard]] std::optional<std::shared_ptr<ISceneObject>>
        getChild(const QualifiedName &name) override;

        std::expected<void, ObjectError>
        performScene(std::vector<std::shared_ptr<J3DModelInstance>> &renderables,
                     model_cache_t &model_cache) override;

        void dump(std::ostream &out, size_t indention, size_t indention_width) const override;

        // Inherited via ISerializable
        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        [[nodiscard]] size_t getGroupSize() const;
        [[nodiscard]] std::shared_ptr<MetaMember> getGroupSizeM() const { return m_group_size; }

    protected:
        void setGroupSize(size_t size);
        void updateGroupSize();

    private:
        std::shared_ptr<MetaMember> m_group_size;
        std::vector<std::shared_ptr<ISceneObject>> m_children = {};
    };

    class PhysicalSceneObject : public ISceneObject {
    public:
        friend class ObjectFactory;

        PhysicalSceneObject() = default;

        PhysicalSceneObject(const Template &template_)
            : ISceneObject(), m_nameref(), m_transform() {
            m_type = template_.type();

            auto wizard = template_.getWizard();
            if (!wizard)
                return;

            for (auto &member : wizard->m_init_members) {
                m_members.emplace_back(
                    std::static_pointer_cast<MetaMember, IClonable>(member.clone(true)));
            }
        }

        PhysicalSceneObject(const Template &template_, std::string_view wizard_name)
            : ISceneObject(), m_nameref(), m_transform() {
            m_type = template_.type();

            auto wizard = template_.getWizard(wizard_name);
            if (!wizard)
                return;

            applyWizard(*wizard);
        }

        PhysicalSceneObject(const Template &template_, Deserializer &in)
            : PhysicalSceneObject(template_) {
            deserialize(in);
        }

        PhysicalSceneObject(const Template &template_, std::string_view wizard_name,
                            Deserializer &in)
            : PhysicalSceneObject(template_, wizard_name) {
            deserialize(in);
        }
        PhysicalSceneObject(const Template &template_, Deserializer &in, ISceneObject *parent)
            : PhysicalSceneObject(template_, in) {
            parent->addChild(std::shared_ptr<PhysicalSceneObject>(this));
        }
        PhysicalSceneObject(const Template &template_, std::string_view wizard_name,
                            Deserializer &in, ISceneObject *parent)
            : PhysicalSceneObject(template_, wizard_name, in) {
            parent->addChild(std::shared_ptr<PhysicalSceneObject>(this));
        }
        PhysicalSceneObject(const PhysicalSceneObject &) = default;
        PhysicalSceneObject(PhysicalSceneObject &&)      = default;

    public:
        ~PhysicalSceneObject() override = default;

        [[nodiscard]] bool isGroupObject() const override { return false; }

        std::string type() const override { return m_type; }

        NameRef getNameRef() const override { return m_nameref; }
        void setNameRef(NameRef nameref) override { m_nameref = nameref; }

        ISceneObject *getParent() const override { return m_parent; }
        std::expected<void, ObjectGroupError> setParent(ISceneObject *parent) override {
            m_parent = parent;
            return {};
        }

        std::vector<s8> getData() const override;
        size_t getDataSize() const override;

        bool hasMember(const QualifiedName &name) const override;
        MetaStruct::GetMemberT getMember(const QualifiedName &name) const override;
        std::vector<std::shared_ptr<MetaMember>> getMembers() const override { return m_members; }
        size_t getMemberOffset(const QualifiedName &name, int index) const override;
        size_t getMemberSize(const QualifiedName &name, int index) const override;

        std::expected<void, ObjectGroupError>
        addChild(std::shared_ptr<ISceneObject> child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError>
        removeChild(std::shared_ptr<ISceneObject> object) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<void, ObjectGroupError> removeChild(const QualifiedName &name) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        std::expected<std::vector<std::shared_ptr<ISceneObject>>, ObjectGroupError>
        getChildren() override {
            ObjectGroupError err = {"Cannot get the children of a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }
        [[nodiscard]] std::optional<std::shared_ptr<ISceneObject>>
        getChild(const QualifiedName &name) override {
            return {};
        }

        [[nodiscard]] std::optional<J3DTransformInfo> getTransform() const override { return m_transform; }
        void setTransform(J3DTransformInfo transform) override {
          // TODO: Set the properties transform too
          m_transform = transform;
        }

        [[nodiscard]] std::optional<std::filesystem::path> getAnimationsPath() const override {
            return "./scene/mapobj/";
        }
        std::optional<std::string_view> getAnimationName(AnimationType type) const override {
            return {};
        }
        bool loadAnimationData(std::string_view name, AnimationType type) override { return false; }

        J3DLight getLightData(int index) override { return m_model_instance->GetLight(index); }

        std::expected<void, ObjectError>
        performScene(std::vector<std::shared_ptr<J3DModelInstance>> &renderables,
                     model_cache_t &model_cache) override;

        void dump(std::ostream &out, size_t indention, size_t indention_width) const override;

    protected:
        std::weak_ptr<J3DAnimationInstance> getAnimationControl(AnimationType type) const override {
            return {};
        }

        void applyWizard(const TemplateWizard &wizard);

    public:
        // Inherited via ISerializable
        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        std::unique_ptr<IClonable> clone(bool deep) const override {
            if (deep) {
                PhysicalSceneObject obj;
                obj.m_type    = m_type;
                obj.m_nameref = m_nameref;
                obj.m_parent  = m_parent;
                obj.m_members.reserve(m_members.size());
                for (const auto &member : m_members) {
                    auto new_member = make_deep_clone<MetaMember>(member);
                    obj.m_members.push_back(new_member);
                }
                obj.m_transform      = m_transform;
                obj.m_model_instance = m_model_instance;
                return std::make_unique<PhysicalSceneObject>(std::move(obj));
            }
            return std::make_unique<PhysicalSceneObject>(*this);
        }

    private:
        std::string m_type;
        NameRef m_nameref;
        std::vector<std::shared_ptr<MetaMember>> m_members;
        ISceneObject *m_parent = nullptr;

        mutable MetaStruct::CacheMemberT m_member_cache;

        std::optional<J3DTransformInfo> m_transform;
        std::shared_ptr<J3DModelInstance> m_model_instance = {};
    };

    class ObjectFactory {
    public:
        using create_ret_t = std::unique_ptr<ISceneObject>;
        using create_err_t = SerialError;
        using create_t     = std::expected<create_ret_t, create_err_t>;

        static create_t create(Deserializer &in);

    protected:
        static bool isGroupObject(std::string_view type);
        static bool isGroupObject(Deserializer &in);
        static bool isPhysicalObject(std::string_view type);
        static bool isPhysicalObject(Deserializer &in);
    };

}  // namespace Toolbox::Object