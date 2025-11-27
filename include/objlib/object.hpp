#pragma once

#include "core/types.hpp"
#include "nameref.hpp"
#include "objlib/errors.hpp"
#include "objlib/meta/member.hpp"
#include "template.hpp"
#include "transform.hpp"
#include "unique.hpp"
#include <J3D/Animation/J3DAnimationInstance.hpp>
#include <J3D/Data/J3DModelData.hpp>
#include <J3D/Data/J3DModelInstance.hpp>
#include <J3D/Material/J3DMaterialTable.hpp>
#include <J3D/Rendering/J3DLight.hpp>
#include <boundbox.hpp>
#include <expected>
#include <filesystem>
#include <stacktrace>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace J3DAnimation;

#define SCENE_LIGHT_PLAYER_SUN              0
#define SCENE_LIGHT_PLAYER_SUN_SECONDARY    1
#define SCENE_LIGHT_PLAYER_SHADOW           2
#define SCENE_LIGHT_PLAYER_SHADOW_SECONDARY 3
#define SCENE_LIGHT_PLAYER_SPECULAR         4
#define SCENE_LIGHT_OBJECT_SUN              5
#define SCENE_LIGHT_OBJECT_SUN_SECONDARY    6
#define SCENE_LIGHT_OBJECT_SHADOW           7
#define SCENE_LIGHT_OBJECT_SHADOW_SECONDARY 8
#define SCENE_LIGHT_OBJECT_SPECULAR         9
#define SCENE_LIGHT_ENEMY_SUN               10
#define SCENE_LIGHT_ENEMY_SUN_SECONDARY     11
#define SCENE_LIGHT_ENEMY_SHADOW            12
#define SCENE_LIGHT_ENEMY_SHADOW_SECONDARY  13
#define SCENE_LIGHT_ENEMY_SPECULAR          14

namespace Toolbox::Object {

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

    using model_cache_t    = std::unordered_map<std::string, J3DModelData>;
    using material_cache_t = std::unordered_map<std::string, J3DMaterialTable>;

    struct ResourceCache {
        model_cache_t m_model;
        material_cache_t m_material;
    };

    namespace {
        static ResourceCache s_resource_cache;
    }

    inline ResourceCache &getResourceCache() { return s_resource_cache; }
    inline void clearResourceCache() { s_resource_cache = ResourceCache(); }

    // A scene object capable of performing in a rendered context and
    // holding modifiable and exotic values
    class ISceneObject : public IGameSerializable, public ISmartResource, public IUnique {
    public:
        friend class ObjectFactory;

        struct RenderInfo {
            RefPtr<ISceneObject> m_object;
            RefPtr<J3DModelInstance> m_model;
            Transform m_transform;
        };

        /* ABSTRACT INTERFACE */
        virtual ~ISceneObject() = default;

        [[nodiscard]] virtual bool isGroupObject() const = 0;

        [[nodiscard]] virtual std::string type() const = 0;

        [[nodiscard]] virtual NameRef getNameRef() const = 0;
        virtual void setNameRef(NameRef name)            = 0;

        [[nodiscard]] virtual ISceneObject *getParent() const = 0;

        // Don't use this, use addChild / removeChild instead.
        virtual Result<void, ObjectGroupError> _setParent(ISceneObject *parent) = 0;

    public:
        [[nodiscard]] virtual std::span<u8> getData() const = 0;
        [[nodiscard]] virtual size_t getDataSize() const    = 0;

        [[nodiscard]] virtual bool hasMember(const QualifiedName &name) const                   = 0;
        [[nodiscard]] virtual MetaStruct::GetMemberT getMember(const QualifiedName &name) const = 0;
        [[nodiscard]] virtual std::vector<RefPtr<MetaMember>> getMembers() const                = 0;
        [[nodiscard]] virtual size_t getMemberOffset(const QualifiedName &name,
                                                     int index) const                           = 0;
        [[nodiscard]] virtual size_t getMemberSize(const QualifiedName &name, int index) const  = 0;

        virtual Result<void, ObjectGroupError> addChild(RefPtr<ISceneObject> child)     = 0;
        virtual Result<void, ObjectGroupError> insertChild(size_t index,
                                                           RefPtr<ISceneObject> child)  = 0;
        virtual Result<void, ObjectGroupError> removeChild(RefPtr<ISceneObject> object) = 0;
        virtual Result<void, ObjectGroupError> removeChild(const QualifiedName &name)   = 0;
        virtual Result<void, ObjectGroupError> removeChild(size_t index)                = 0;
        [[nodiscard]] virtual std::vector<RefPtr<ISceneObject>> getChildren()           = 0;
        [[nodiscard]] virtual RefPtr<ISceneObject> getChild(const QualifiedName &name)  = 0;
        [[nodiscard]] virtual RefPtr<ISceneObject> getChild(UUID64 id)                  = 0;

        [[nodiscard]] virtual std::optional<Transform> getTransform() const      = 0;
        virtual Result<void, MetaError> setTransform(const Transform &transform) = 0;

        [[nodiscard]] virtual std::optional<BoundingBox> getBoundingBox() const = 0;

        [[nodiscard]] virtual std::optional<std::filesystem::path> getAnimationsPath() const = 0;
        [[nodiscard]] virtual std::optional<std::string_view>
        getAnimationName(AnimationType type) const                                = 0;
        virtual bool loadAnimationData(std::string_view name, AnimationType type) = 0;

        [[nodiscard]] virtual J3DLight getLightData(int index) = 0;

        [[nodiscard]] virtual bool getCanPerform() const   = 0;
        [[nodiscard]] virtual bool getIsPerforming() const = 0;
        virtual void setIsPerforming(bool performing)      = 0;

        virtual Result<void, ObjectError> performScene(float delta_time, bool animate,
                                                       std::vector<RenderInfo> &renderables,
                                                       ResourceCache &resource_cache,
                                                       std::vector<J3DLight> &scene_lights) = 0;

        virtual void dump(std::ostream &out, size_t indention, size_t indention_width) const = 0;

    protected:
        /* PROTECTED ABSTRACT INTERFACE */
        [[nodiscard]] virtual std::weak_ptr<J3DAnimationInstance>
        getAnimationControl(AnimationType type) const = 0;

    public:
        [[nodiscard]] QualifiedName getQualifiedName() const;

        [[nodiscard]] RefPtr<ISceneObject> getChild(const std::string &name) {
            return getChild(QualifiedName(name));
        }

        [[nodiscard]] size_t getAnimationFrames(AnimationType type) const;
        [[nodiscard]] float getAnimationFrame(AnimationType type) const;
        void setAnimationFrame(size_t frame, AnimationType type);

        bool startAnimation(AnimationType type);
        bool stopAnimation(AnimationType type);

        virtual u32 getGamePtr() const   = 0;
        virtual void setGamePtr(u32 ptr) = 0;

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
            parent->addChild(RefPtr<VirtualSceneObject>(this));
        }
        VirtualSceneObject(const Template &template_, std::string_view wizard_name,
                           Deserializer &in, ISceneObject *parent)
            : VirtualSceneObject(template_, wizard_name, in) {
            parent->addChild(RefPtr<VirtualSceneObject>(this));
        }
        VirtualSceneObject(const VirtualSceneObject &) = default;
        VirtualSceneObject(VirtualSceneObject &&)      = default;

    public:
        ~VirtualSceneObject() override = default;

        [[nodiscard]] bool isGroupObject() const override { return false; }

        std::string type() const override { return m_type; }

        NameRef getNameRef() const override { return m_nameref; }
        void setNameRef(NameRef nameref) override { m_nameref = nameref; }

        [[nodiscard]] UUID64 getUUID() const override { return m_UUID64; }

        ISceneObject *getParent() const override { return m_parent; }
        Result<void, ObjectGroupError> _setParent(ISceneObject *parent) override {
            m_parent = parent;
            return {};
        }

        std::span<u8> getData() const override;
        size_t getDataSize() const override;

        bool hasMember(const QualifiedName &name) const override;
        MetaStruct::GetMemberT getMember(const QualifiedName &name) const override;
        std::vector<RefPtr<MetaMember>> getMembers() const override { return m_members; }
        size_t getMemberOffset(const QualifiedName &name, int index) const override;
        size_t getMemberSize(const QualifiedName &name, int index) const override;

        Result<void, ObjectGroupError> addChild(RefPtr<ISceneObject> child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        Result<void, ObjectGroupError> insertChild(size_t index,
                                                   RefPtr<ISceneObject> child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        Result<void, ObjectGroupError> removeChild(RefPtr<ISceneObject> object) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        Result<void, ObjectGroupError> removeChild(const QualifiedName &name) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        Result<void, ObjectGroupError> removeChild(size_t index) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        [[nodiscard]] std::vector<RefPtr<ISceneObject>> getChildren() override { return {}; }
        [[nodiscard]] RefPtr<ISceneObject> getChild(const QualifiedName &name) override {
            return nullptr;
        }
        [[nodiscard]] RefPtr<ISceneObject> getChild(UUID64 id) override { return nullptr; }

        [[nodiscard]] std::optional<Transform> getTransform() const override { return {}; }
        Result<void, MetaError> setTransform(const Transform &transform) override { return {}; }

        [[nodiscard]] std::optional<BoundingBox> getBoundingBox() const override { return {}; }

        [[nodiscard]] std::optional<std::filesystem::path> getAnimationsPath() const override {
            return {};
        }
        [[nodiscard]] std::optional<std::string_view>
        getAnimationName(AnimationType type) const override {
            return {};
        }
        bool loadAnimationData(std::string_view name, AnimationType type) override { return false; }

        [[nodiscard]] J3DLight getLightData(int index) override { return {}; }

        [[nodiscard]] bool getCanPerform() const { return false; }
        [[nodiscard]] bool getIsPerforming() const { return false; }
        void setIsPerforming(bool performing) {}

        Result<void, ObjectError> performScene(float delta_time, bool animate,
                                               std::vector<RenderInfo> &renderables,
                                               ResourceCache &resource_cache,
                                               std::vector<J3DLight> &scene_lights) override;

        u32 getGamePtr() const override { return m_game_ptr; }
        void setGamePtr(u32 ptr) override { m_game_ptr = ptr; }

        void dump(std::ostream &out, size_t indention, size_t indention_width) const override;

    protected:
        [[nodiscard]] std::weak_ptr<J3DAnimationInstance>
        getAnimationControl(AnimationType type) const override {
            return {};
        }

        void applyWizard(const TemplateWizard &wizard);

    public:
        // Inherited via IGameSerializable
        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        Result<void, SerialError> gameSerialize(Serializer &out) const override;
        Result<void, SerialError> gameDeserialize(Deserializer &in) override;

        ScopePtr<ISmartResource> clone(bool deep) const override {
            auto obj       = make_scoped<VirtualSceneObject>();
            obj->m_type    = m_type;
            obj->m_nameref = m_nameref;
            obj->m_parent  = nullptr;
            obj->m_members.reserve(m_members.size());

            if (deep) {
                for (const auto &member : m_members) {
                    auto new_member = make_deep_clone<MetaMember>(member);
                    obj->m_members.push_back(std::move(new_member));
                }
            } else {
                for (const auto &member : m_members) {
                    auto new_member = make_clone<MetaMember>(member);
                    obj->m_members.push_back(std::move(new_member));
                }
            }

            return obj;
        }

    protected:
        UUID64 m_UUID64;
        u32 m_sibling_id = 0;

        std::string m_type;
        NameRef m_nameref;
        std::vector<RefPtr<MetaMember>> m_members;
        mutable std::vector<u8> m_data;
        ISceneObject *m_parent = nullptr;

        mutable MetaStruct::CacheMemberT m_member_cache;

        u32 m_game_ptr = 0;

        bool m_include_custom = false;
    };

    class GroupSceneObject : public VirtualSceneObject {
    public:
        friend class ObjectFactory;

        GroupSceneObject() : VirtualSceneObject() {
            m_group_size = make_referable<MetaMember>("GroupSize", MetaValue(static_cast<u32>(0)));
        }
        GroupSceneObject(const Template &template_) : VirtualSceneObject(template_) {
            m_group_size = make_referable<MetaMember>("GroupSize", MetaValue(static_cast<u32>(0)));
        }
        GroupSceneObject(const Template &template_, std::string_view wizard_name)
            : VirtualSceneObject(template_, wizard_name) {
            m_group_size = make_referable<MetaMember>("GroupSize", MetaValue(static_cast<u32>(0)));
        }
        GroupSceneObject(const Template &template_, Deserializer &in)
            : GroupSceneObject(template_) {
            deserialize(in);
        }
        GroupSceneObject(const Template &template_, Deserializer &in, ISceneObject *parent)
            : GroupSceneObject(template_, in) {
            parent->addChild(RefPtr<GroupSceneObject>(this));
        }
        GroupSceneObject(const Template &template_, std::string_view wizard_name,
                         ISceneObject *parent)
            : GroupSceneObject(template_, wizard_name) {
            parent->addChild(RefPtr<GroupSceneObject>(this));
        }
        GroupSceneObject(const GroupSceneObject &) = default;
        GroupSceneObject(GroupSceneObject &&)      = default;

    public:
        ~GroupSceneObject() override = default;

        [[nodiscard]] bool isGroupObject() const override { return true; }

        [[nodiscard]] std::span<u8> getData() const override;
        [[nodiscard]] size_t getDataSize() const override;

        Result<void, ObjectGroupError> addChild(RefPtr<ISceneObject> child) override;
        Result<void, ObjectGroupError> insertChild(size_t index,
                                                   RefPtr<ISceneObject> child) override;
        Result<void, ObjectGroupError> removeChild(RefPtr<ISceneObject> child) override;
        Result<void, ObjectGroupError> removeChild(const QualifiedName &name) override;
        Result<void, ObjectGroupError> removeChild(size_t index) override;
        [[nodiscard]] std::vector<RefPtr<ISceneObject>> getChildren() override;
        [[nodiscard]] RefPtr<ISceneObject> getChild(const QualifiedName &name) override;
        [[nodiscard]] RefPtr<ISceneObject> getChild(UUID64 id) override;

        [[nodiscard]] bool getCanPerform() const { return true; }
        [[nodiscard]] bool getIsPerforming() const { return m_is_performing; }
        void setIsPerforming(bool performing) { m_is_performing = performing; }

        Result<void, ObjectError> performScene(float delta_time, bool animate,
                                               std::vector<RenderInfo> &renderables,
                                               ResourceCache &resource_cache,
                                               std::vector<J3DLight> &scene_lights) override;

        void dump(std::ostream &out, size_t indention, size_t indention_width) const override;

        // Inherited via IGameSerializable
        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        Result<void, SerialError> gameSerialize(Serializer &out) const override;
        Result<void, SerialError> gameDeserialize(Deserializer &in) override;

        ScopePtr<ISmartResource> clone(bool deep) const override {
            auto obj       = make_scoped<GroupSceneObject>();
            obj->m_type    = m_type;
            obj->m_nameref = m_nameref;
            obj->m_parent  = nullptr;
            obj->m_members.reserve(m_members.size());
            if (deep) {
                for (const auto &member : m_members) {
                    auto new_member = make_deep_clone<MetaMember>(member);
                    obj->m_members.push_back(std::move(new_member));
                }
                obj->m_group_size = make_deep_clone<MetaMember>(m_group_size);
                for (const auto &child : m_children) {
                    auto new_child = make_deep_clone<ISceneObject>(child);
                    obj->m_children.push_back(std::move(new_child));
                }
            } else {
                for (const auto &member : m_members) {
                    auto new_member = make_clone<MetaMember>(member);
                    obj->m_members.push_back(std::move(new_member));
                }
                obj->m_group_size = make_clone<MetaMember>(m_group_size);
                for (const auto &child : m_children) {
                    auto new_child = make_clone<ISceneObject>(child);
                    obj->m_children.push_back(std::move(new_child));
                }
            }
            return obj;
        }

        [[nodiscard]] size_t getGroupSize() const;
        [[nodiscard]] RefPtr<MetaMember> getGroupSizeM() const { return m_group_size; }

    protected:
        void setGroupSize(size_t size);
        void updateGroupSize();

    private:
        RefPtr<MetaMember> m_group_size;
        mutable std::vector<u8> m_data;
        std::vector<RefPtr<ISceneObject>> m_children = {};
        bool m_is_performing                         = true;
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
                    std::static_pointer_cast<MetaMember, ISmartResource>(member.clone(true)));
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
            parent->addChild(RefPtr<PhysicalSceneObject>(this));
        }
        PhysicalSceneObject(const Template &template_, std::string_view wizard_name,
                            Deserializer &in, ISceneObject *parent)
            : PhysicalSceneObject(template_, wizard_name, in) {
            parent->addChild(RefPtr<PhysicalSceneObject>(this));
        }
        PhysicalSceneObject(const PhysicalSceneObject &) = default;
        PhysicalSceneObject(PhysicalSceneObject &&)      = default;

    public:
        ~PhysicalSceneObject() override {
            m_data.clear();
            try {
                m_model_instance.reset();
            } catch (...) {
                return;
            }
        }

        [[nodiscard]] bool isGroupObject() const override { return false; }

        std::string type() const override { return std::string(m_type.name()); }

        NameRef getNameRef() const override { return m_nameref; }
        void setNameRef(NameRef nameref) override { m_nameref = nameref; }

        [[nodiscard]] UUID64 getUUID() const override { return m_UUID64; }

        ISceneObject *getParent() const override { return m_parent; }
        Result<void, ObjectGroupError> _setParent(ISceneObject *parent) override {
            m_parent = parent;
            return {};
        }

        std::span<u8> getData() const override;
        size_t getDataSize() const override;

        bool hasMember(const QualifiedName &name) const override;
        MetaStruct::GetMemberT getMember(const QualifiedName &name) const override;
        std::vector<RefPtr<MetaMember>> getMembers() const override { return m_members; }
        size_t getMemberOffset(const QualifiedName &name, int index) const override;
        size_t getMemberSize(const QualifiedName &name, int index) const override;

        Result<void, ObjectGroupError> addChild(RefPtr<ISceneObject> child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        Result<void, ObjectGroupError> insertChild(size_t index,
                                                   RefPtr<ISceneObject> child) override {
            ObjectGroupError err = {"Cannot add child to a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        Result<void, ObjectGroupError> removeChild(RefPtr<ISceneObject> object) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        Result<void, ObjectGroupError> removeChild(const QualifiedName &name) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        Result<void, ObjectGroupError> removeChild(size_t index) override {
            ObjectGroupError err = {"Cannot remove a child from a non-group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        [[nodiscard]] std::vector<RefPtr<ISceneObject>> getChildren() override { return {}; }
        [[nodiscard]] RefPtr<ISceneObject> getChild(const QualifiedName &name) override {
            return {};
        }
        [[nodiscard]] RefPtr<ISceneObject> getChild(UUID64 id) override { return nullptr; }

        [[nodiscard]] std::optional<Transform> getTransform() const override { return m_transform; }
        Result<void, MetaError> setTransform(const Transform &transform) override {
            // TODO: Set the properties transform too
            m_transform = transform;
            if (m_model_instance) {
                m_model_instance->SetTranslation(transform.m_translation);
                m_model_instance->SetRotation(transform.m_rotation);
                m_model_instance->SetScale(transform.m_scale);
            }

            auto transform_value_ptr = getMember("Transform").value();
            if (transform_value_ptr) {
                auto result = setMetaValue<Transform>(transform_value_ptr, 0, transform);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }

            return {};
        }

        [[nodiscard]] std::optional<BoundingBox> getBoundingBox() const override {
            if (!m_model_instance)
                return {};

            std::optional<Transform> transform = getTransform();
            if (!transform)
                return {};

            glm::vec3 center, size;

            glm::vec3 min, max;
            m_model_instance->GetBoundingBox(min, max);

            max.x *= transform->m_scale.x;
            max.y *= transform->m_scale.y;
            max.z *= transform->m_scale.z;

            min.x *= transform->m_scale.x;
            min.y *= transform->m_scale.y;
            min.z *= transform->m_scale.z;

            size   = max - min;
            center = transform->m_translation + min + (size / 2.0f);

            return BoundingBox(center, size, glm::quat(transform->m_rotation));
        }

        [[nodiscard]] std::optional<std::filesystem::path> getAnimationsPath() const override {
            return "./scene/mapobj/";
        }
        std::optional<std::string_view> getAnimationName(AnimationType type) const override {
            return {};
        }
        bool loadAnimationData(std::string_view name, AnimationType type) override { return false; }

        J3DLight getLightData(int index) override { return m_model_instance->GetLight(index); }

        [[nodiscard]] bool getCanPerform() const { return true; }
        [[nodiscard]] bool getIsPerforming() const { return m_is_performing; }
        void setIsPerforming(bool performing) { m_is_performing = performing; }

        Result<void, ObjectError> performScene(float delta_time, bool animate,
                                               std::vector<RenderInfo> &renderables,
                                               ResourceCache &resource_cache,
                                               std::vector<J3DLight> &scene_lights) override;

        u32 getGamePtr() const override { return m_game_ptr; }
        void setGamePtr(u32 ptr) override { m_game_ptr = ptr; }

        void dump(std::ostream &out, size_t indention, size_t indention_width) const override;

    protected:
        std::weak_ptr<J3DAnimationInstance> getAnimationControl(AnimationType type) const override {
            return {};
        }

        void applyWizard(const TemplateWizard &wizard);

        Result<void, FSError> loadRenderData(const std::filesystem::path &asset_path,
                                             const TemplateRenderInfo &info,
                                             ResourceCache &resource_cache);

    public:
        // Inherited via IGameSerializable
        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        Result<void, SerialError> gameSerialize(Serializer &out) const override;
        Result<void, SerialError> gameDeserialize(Deserializer &in) override;

        ScopePtr<ISmartResource> clone(bool deep) const override {
            auto obj         = make_scoped<PhysicalSceneObject>();
            obj->m_type      = m_type;
            obj->m_nameref   = m_nameref;
            obj->m_parent    = nullptr;
            obj->m_transform = m_transform;
            obj->m_members.reserve(m_members.size());

            if (m_model_instance)
                obj->m_model_instance = make_referable<J3DModelInstance>(*m_model_instance);

            if (deep) {
                for (const auto &member : m_members) {
                    auto new_member = make_deep_clone<MetaMember>(member);
                    obj->m_members.push_back(std::move(new_member));
                }
            } else {
                for (const auto &member : m_members) {
                    auto new_member = make_clone<MetaMember>(member);
                    obj->m_members.push_back(std::move(new_member));
                }
            }

            return obj;
        }

    protected:
        void HelperUpdateKinojiRender();
        void HelperUpdateKinopioRender();
        void HelperUpdateMonteRender();

    private:
        UUID64 m_UUID64;

        NameRef m_type;
        NameRef m_nameref;
        std::vector<RefPtr<MetaMember>> m_members;
        mutable std::vector<u8> m_data;
        ISceneObject *m_parent = nullptr;

        mutable MetaStruct::CacheMemberT m_member_cache;

        std::optional<Transform> m_transform;
        RefPtr<J3DModelInstance> m_model_instance;
        RefPtr<J3DModelData> m_model_data;

        bool m_is_performing = true;

        u32 m_game_ptr = 0;

        bool m_include_custom = false;
    };

    class ObjectFactory {
    public:
        using create_ret_t = ScopePtr<ISceneObject>;
        using create_err_t = SerialError;
        using create_t     = Result<create_ret_t, create_err_t>;

        static create_t create(Deserializer &in, bool include_custom);
        static create_ret_t create(const Template &template_, std::string_view wizard_name);

    protected:
        static bool isGroupObject(std::string_view type);
        static bool isGroupObject(Deserializer &in);
        static bool isPhysicalObject(std::string_view type);
        static bool isPhysicalObject(Deserializer &in);
    };

}  // namespace Toolbox::Object