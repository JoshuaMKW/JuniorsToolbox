#include <J3D/Animation/J3DAnimationLoader.hpp>
#include <J3D/Material/J3DMaterialTableLoader.hpp>
#include <J3D/Texture/J3DTextureLoader.hpp>

#include <bstream.h>
#include <expected>
#include <gui/modelcache.hpp>
#include <include/decode.h>
#include <string>
#include <unordered_set>

#include "objlib/meta/errors.hpp"
#include "objlib/object.hpp"

using namespace Toolbox::Object;

static constexpr std::array s_monte_types = {
    NameRef::calcKeyCode("NPCMonteM"),  NameRef::calcKeyCode("NPCMonteMA"),
    NameRef::calcKeyCode("NPCMonteMB"), NameRef::calcKeyCode("NPCMonteMC"),
    NameRef::calcKeyCode("NPCMonteMD"), NameRef::calcKeyCode("NPCMonteME"),
    NameRef::calcKeyCode("NPCMonteMF"), NameRef::calcKeyCode("NPCMonteMG"),
    NameRef::calcKeyCode("NPCMonteMH"), NameRef::calcKeyCode("NPCMonteW"),
    NameRef::calcKeyCode("NPCMonteWA"), NameRef::calcKeyCode("NPCMonteWB"),
    NameRef::calcKeyCode("NPCMonteWC"),
};

namespace Toolbox::Object {

    /* INTERFACE */

    QualifiedName ISceneObject::getQualifiedName() const {
        auto parent = getParent();
        if (parent)
            return QualifiedName(getNameRef().name(), parent->getQualifiedName());
        return QualifiedName(getNameRef().name());
    }

    size_t ISceneObject::getAnimationFrames(AnimationType type) const {
        auto ctrl = getAnimationControl(type);
        if (ctrl.expired())
            return 0;
        return ctrl.lock()->GetLength();
    }

    float ISceneObject::getAnimationFrame(AnimationType type) const {
        auto ctrl = getAnimationControl(type);
        if (ctrl.expired())
            return 0;
        return ctrl.lock()->GetFrame();
    }

    void ISceneObject::setAnimationFrame(size_t frame, AnimationType type) {
        auto ctrl = getAnimationControl(type);
        if (ctrl.expired())
            return;
        ctrl.lock()->SetFrame(static_cast<u16>(frame));
    }

    bool ISceneObject::startAnimation(AnimationType type) {
        auto ctrl = getAnimationControl(type);
        if (ctrl.expired())
            return false;
        ctrl.lock()->SetPaused(false);
        return true;
    }

    bool ISceneObject::stopAnimation(AnimationType type) {
        auto ctrl = getAnimationControl(type);
        if (ctrl.expired())
            return false;
        ctrl.lock()->SetPaused(true);
        return true;
    }

    /* VIRTUAL SCENE OBJECT */

    std::span<u8> VirtualSceneObject::getData() const {
        std::stringstream ostr;

        // Lol
        Serializer out(ostr.rdbuf());
        Deserializer in(ostr.rdbuf());

        serialize(out);

        m_data.resize(in.size());
        in.seek(0);

        in.readBytes({reinterpret_cast<char *>(&m_data[0]), m_data.size()});

        return {m_data.data(), m_data.size()};
    }

    size_t VirtualSceneObject::getDataSize() const { return getData().size(); }

    bool VirtualSceneObject::hasMember(const QualifiedName &name) const {
        return getMember(name).has_value();
    }

    MetaStruct::GetMemberT VirtualSceneObject::getMember(const QualifiedName &name) const {
        if (name.empty())
            return {};

        const auto name_str = name.toString();

        if (m_member_cache.contains(name_str))
            return m_member_cache.at(name_str);

        auto current_scope = name[0];
        auto array_index   = getArrayIndex(name, 0);
        if (!array_index) {
            return std::unexpected(array_index.error());
        }

        for (auto m : m_members) {
            if (m->name() != current_scope)
                continue;

            if (name.depth() == 1) {
                m_member_cache[name_str] = m;
                return m;
            }

            if (m->isTypeStruct()) {
                auto s      = m->value<MetaStruct>(*array_index).value();
                auto member = s->getMember(QualifiedName(name.begin() + 1, name.end()));
                if (member.has_value()) {
                    m_member_cache[name_str] = member.value();
                }
                return member;
            }
        }

        return {};
    }

    size_t VirtualSceneObject::getMemberOffset(const QualifiedName &name, int index) const {
        size_t offset = 0;
        return offset;
    }

    size_t VirtualSceneObject::getMemberSize(const QualifiedName &name, int index) const {
        size_t size = 0;
        return size;
    }

    Result<void, ObjectError> VirtualSceneObject::performScene(float, bool,
                                                               std::vector<RenderInfo> &,
                                                               ResourceCache &,
                                                               std::vector<J3DLight> &) {
        return {};
    }

    void VirtualSceneObject::dump(std::ostream &out, size_t indention,
                                  size_t indention_width) const {
        indention_width          = std::min(indention_width, size_t(8));
        std::string self_indent  = std::string(indention * indention_width, ' ');
        std::string value_indent = std::string((indention + 1) * indention_width, ' ');
        out << self_indent << m_type << " (" << m_nameref.name() << ") {\n";
        out << self_indent << "members:\n";
        for (auto m : m_members) {
            m->dump(out, indention + 1, indention_width);
        }
        out << self_indent << "}\n";
    }

    void VirtualSceneObject::applyWizard(const TemplateWizard &wizard) {
        m_nameref.setName(wizard.m_name);
        for (auto &member : wizard.m_init_members) {
            RefPtr<MetaMember> new_member = make_deep_clone<MetaMember>(member);
            new_member->updateReferenceToList(m_members);
            m_members.emplace_back(new_member);
        }
    }

    Result<void, SerialError> VirtualSceneObject::serialize(Serializer &out) const {
        return gameSerialize(out);
    }

    Result<void, SerialError> VirtualSceneObject::deserialize(Deserializer &in) {
        return gameDeserialize(in);
    }

    Result<void, SerialError> VirtualSceneObject::gameSerialize(Serializer &out) const {
        std::streampos start = out.tell();

        out.pushBreakpoint();
        {
            // Write the size marker for now
            out.write<u32>(0);

            NameRef type_ref(m_type);
            type_ref.serialize(out);

            m_nameref.serialize(out);

            for (auto &member : m_members) {
                auto result = member->gameSerialize(out);
                if (!result) {
                    return result;
                }
            }
        }

        u32 obj_size = static_cast<u32>(out.size() - start);

        out.popBreakpoint();

        // Write the size marker
        out.write<u32, std::endian::big>(obj_size);
        out.seek(0, std::ios::end);
        return {};
    }

    Result<void, SerialError> VirtualSceneObject::gameDeserialize(Deserializer &in) {
        // Metadata
        auto length           = in.read<u32, std::endian::big>();
        std::streampos endpos = static_cast<std::size_t>(in.tell()) + length - 4;

        // Type
        NameRef type;
        {
            auto result = type.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Name
        NameRef name;
        {
            auto result = name.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        m_type = type.name();
        setNameRef(name);

        const char *debug_name = name.name().data();

        auto template_result = TemplateFactory::create(m_type, m_include_custom);
        if (!template_result) {
            auto error_v = template_result.error();
            if (std::holds_alternative<FSError>(error_v)) {
                auto error = std::get<FSError>(error_v);
                return make_serial_error<void>(in, error.m_message[0]);
            } else {
                auto error = std::get<JSONError>(error_v);
                return make_serial_error<void>(in, error.m_message[0]);
            }
        }

        auto template_ = std::move(template_result.value());
        auto wizard    = template_->getWizard(name.name());
        if (!wizard) {
            wizard = template_->getWizard("Default");
            if (!wizard) {
                wizard = template_->getWizard();
            }
        }

        // Members
        for (size_t i = 0; i < wizard->m_init_members.size(); ++i) {
            if (in.tell() >= endpos) {
                /*auto err = make_serial_error(
                    in, std::format(
                            "Unexpected end of file. {} ({}) expected {} members but only found {}",
                            m_type, m_nameref.name(), wizard->m_init_members.size(), i + 1));
                return std::unexpected(err);*/
                break;
            }
            auto &m                        = wizard->m_init_members[i];
            RefPtr<MetaMember> this_member = ref_cast<MetaMember>(make_deep_clone<MetaMember>(m));
            this_member->updateReferenceToList(m_members);
            auto result = this_member->gameDeserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            m_members.push_back(this_member);
        }

        in.seek(endpos, std::ios::beg);
        return {};
    }

    /* GROUP SCENE OBJECT */

    std::span<u8> GroupSceneObject::getData() const {
        std::stringstream ostr;

        // Lol
        Serializer out(ostr.rdbuf());
        Deserializer in(ostr.rdbuf());

        serialize(out);

        m_data.resize(in.size());
        in.seek(0);

        in.readBytes({reinterpret_cast<char *>(&m_data[0]), m_data.size()});

        return {m_data.data(), m_data.size()};
    }

    size_t GroupSceneObject::getDataSize() const { return getData().size(); }

    Result<void, ObjectGroupError> GroupSceneObject::addChild(RefPtr<ISceneObject> child) {
        return insertChild(m_children.size(), std::move(child));
    }

    Result<void, ObjectGroupError> GroupSceneObject::insertChild(size_t index,
                                                                 RefPtr<ISceneObject> child) {
        if (index > m_children.size()) {
            ObjectGroupError err = {std::format("Insertion index {} is out of bounds (end: {})",
                                                index, m_children.size()),
                                    std::stacktrace::current(),
                                    this,
                                    {}};
            return std::unexpected(err);
        }
        ISceneObject *parent = child->getParent();
        if (parent) {
            parent->removeChild(child);
        }
        auto result = child->_setParent(this);
        if (!result) {
            ObjectGroupError err = {"Cannot add a child to the group object:",
                                    std::stacktrace::current(),
                                    this,
                                    {result.error()}};
            return std::unexpected(err);
        }

        m_children.insert(m_children.begin() + index, std::move(child));
        updateGroupSize();
        return {};
    }

    Result<void, ObjectGroupError> GroupSceneObject::removeChild(RefPtr<ISceneObject> child) {
        auto it = std::find_if(m_children.begin(), m_children.end(),
                               [child](const auto &ptr) { return ptr == child; });
        if (it == m_children.end()) {
            ObjectGroupError err = {"Child not found in the group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }
        m_children.erase(it);
        updateGroupSize();
        return {};
    }

    Result<void, ObjectGroupError> GroupSceneObject::removeChild(const QualifiedName &name) {
        if (name.empty())
            return std::unexpected(ObjectGroupError{"Cannot remove a child with an empty name.",
                                                    std::stacktrace::current(), this});

        auto current_scope = name[0];

        auto it = std::find_if(m_children.begin(), m_children.end(), [&](const auto &ptr) {
            return ptr->getNameRef().name() == current_scope;
        });

        if (it == m_children.end()) {
            ObjectGroupError err = {"Child not found in the group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        if (name.depth() == 1) {
            m_children.erase(it);
            updateGroupSize();
            return {};
        }

        return it->get()->removeChild(QualifiedName(name.begin() + 1, name.end()));
    }

    Result<void, ObjectGroupError> GroupSceneObject::removeChild(size_t index) {
        if (index >= m_children.size()) {
            ObjectGroupError err = {
                std::format("Index {} is out of bounds (end: {})", index, m_children.size()),
                std::stacktrace::current(), this};
            return std::unexpected(err);
        }

        m_children.erase(m_children.begin() + index);
        updateGroupSize();
        return {};
    }

    std::vector<RefPtr<ISceneObject>> GroupSceneObject::getChildren() { return m_children; }

    RefPtr<ISceneObject> GroupSceneObject::getChild(const QualifiedName &name) {
        auto scope = name[0];
        auto it    = std::find_if(m_children.begin(), m_children.end(),
                                  [&](const auto &ptr) { return ptr->getNameRef().name() == scope; });
        if (it == m_children.end())
            return nullptr;
        if (name.depth() == 1)
            return *it;
        return it->get()->getChild(QualifiedName(name.begin() + 1, name.end()));
    }

    // Not recommended if you know the object key
    RefPtr<ISceneObject> GroupSceneObject::getChild(UUID64 id) {
        for (RefPtr<ISceneObject> child : getChildren()) {
            if (!child) {
                continue;
            }
            if (child->getUUID() == id) {
                return child;
            }
            RefPtr<ISceneObject> result = child->getChild(id);
            if (result) {
                return result;
            }
        }
        return nullptr;
    }

    RefPtr<ISceneObject> GroupSceneObject::getChildByType(std::string_view type,
                                                          std::optional<std::string_view> name) {
        for (RefPtr<ISceneObject> child : getChildren()) {
            if (!child) {
                continue;
            }
            if (child->type() == type) {
                if (name.has_value()) {
                    if (child->getNameRef().name() == name.value()) {
                        return child;
                    }
                } else {
                    return child;
                }
            }
            RefPtr<ISceneObject> result = child->getChildByType(type, name);
            if (result) {
                return result;
            }
        }
        return nullptr;
    }

    Result<void, ObjectError> GroupSceneObject::performScene(float delta_time, bool animate,
                                                             std::vector<RenderInfo> &renderables,
                                                             ResourceCache &resource_cache,
                                                             std::vector<J3DLight> &scene_lights) {
        if (!getIsPerforming()) {
            return {};
        }

        std::vector<ObjectError> child_errors;

        for (auto &child : m_children) {
            auto result =
                child->performScene(delta_time, animate, renderables, resource_cache, scene_lights);
            if (!result)
                child_errors.push_back(result.error());
        }

        if (child_errors.size() > 0) {
            ObjectGroupError err;
            {
                err.m_message      = std::format("ObjectGroupError: {} ({}): There were errors "
                                                      "performing the children:",
                                                 m_type, m_nameref.name());
                err.m_object       = this;
                err.m_stacktrace   = std::stacktrace::current();
                err.m_child_errors = child_errors;
            }
            return std::unexpected(err);
        }

        return {};
    }

    void GroupSceneObject::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        indention_width          = std::min(indention_width, size_t(8));
        std::string self_indent  = std::string(indention * indention_width, ' ');
        std::string value_indent = std::string((indention + 1) * indention_width, ' ');
        out << self_indent << m_type << " (" << m_nameref.name() << ") {\n";
        out << self_indent << "members:\n";
        for (auto m : m_members) {
            m->dump(out, indention + 1, indention_width);
        }
        out << "\n" << self_indent << "children:\n";
        if (m_children.size() > 0) {
            for (size_t i = 0; i < m_children.size() - 1; ++i) {
                m_children[i]->dump(out, indention + 1, indention_width);
                out << "\n";
            }
            m_children.back()->dump(out, indention + 1, indention_width);
        }
        out << self_indent << "}\n";
    }

    Result<void, SerialError> GroupSceneObject::serialize(Serializer &out) const {
        return gameSerialize(out);
    }

    Result<void, SerialError> GroupSceneObject::deserialize(Deserializer &in) {
        return gameDeserialize(in);
    }

    Result<void, SerialError> GroupSceneObject::gameSerialize(Serializer &out) const {
        std::streampos start = out.tell();

        out.pushBreakpoint();
        {
            // Write the size marker for now
            out.write<u32>(0);

            NameRef type_ref(m_type);
            type_ref.serialize(out);

            m_nameref.serialize(out);

            // Members
            bool late_group_size = (type_ref.code() == 15406 || type_ref.code() == 9858);
            if (!late_group_size) {
                auto result = m_group_size->gameSerialize(out);
                if (!result) {
                    return result;
                }
            }

            for (auto &member : m_members) {
                auto result = member->gameSerialize(out);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }

            if (late_group_size) {
                auto result = m_group_size->gameSerialize(out);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }

            for (auto &child : m_children) {
                auto result = child->gameSerialize(out);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
        }

        u32 obj_size = static_cast<u32>(out.size() - start);

        out.popBreakpoint();

        // Write the size marker
        out.write<u32, std::endian::big>(obj_size);
        out.seek(0, std::ios::end);
        return {};
    }

    Result<void, SerialError> GroupSceneObject::gameDeserialize(Deserializer &in) {
        // Metadata
        auto length           = in.read<u32, std::endian::big>();
        std::streampos endpos = static_cast<std::size_t>(in.tell()) + length - 4;

        // Type
        NameRef obj_type;
        {
            auto result = obj_type.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Name
        NameRef obj_name;
        {
            auto result = obj_name.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        m_type = obj_type.name();
        setNameRef(obj_name);

        auto template_result = TemplateFactory::create(m_type, m_include_custom);
        if (!template_result) {
            auto error_v = template_result.error();
            if (std::holds_alternative<FSError>(error_v)) {
                auto error = std::get<FSError>(error_v);
                return make_serial_error<void>(in, error.m_message[0]);
            } else {
                auto error = std::get<JSONError>(error_v);
                return make_serial_error<void>(in, error.m_message[0]);
            }
        }

        auto template_ = std::move(template_result.value());
        auto wizard    = template_->getWizard(obj_name.name());
        if (!wizard) {
            wizard = template_->getWizard("Default");
            if (!wizard) {
                wizard = template_->getWizard();
            }
        }

        // Members
        bool late_group_size = (obj_type.code() == 15406 || obj_type.code() == 9858);
        if (!late_group_size) {
            m_group_size->gameDeserialize(in);
        }

        for (size_t i = 0; i < wizard->m_init_members.size(); ++i) {
            if (in.tell() >= endpos) {
                /*auto err = make_serial_error(
                    in, std::format(
                            "Unexpected end of file. {} ({}) expected {} members but only found {}",
                            m_type, m_nameref.name(), wizard->m_init_members.size(), i + 1));
                return std::unexpected(err);*/
                break;
            }
            auto &m                        = wizard->m_init_members[i];
            RefPtr<MetaMember> this_member = ref_cast<MetaMember>(make_deep_clone<MetaMember>(m));
            this_member->updateReferenceToList(m_members);
            auto result = this_member->gameDeserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            m_members.push_back(this_member);
        }

        if (late_group_size) {
            m_group_size->gameDeserialize(in);
        }

        size_t num_children = getGroupSize();

        // Children
        for (size_t i = 0; i < num_children; ++i) {
            if (in.tell() >= endpos) {
                return make_serial_error<void>(
                    in,
                    std::format(
                        "Unexpected end of file. {} ({}) expected {} children but only found {}",
                        m_type, m_nameref.name(), num_children, i + 1));
            }
            ObjectFactory::create_t result = ObjectFactory::create(in, m_include_custom);
            if (!result) {
                return std::unexpected(result.error());
            }
            addChild(std::move(result.value()));
        }

        in.seek(endpos, std::ios::beg);
        return {};
    }

    size_t GroupSceneObject::getGroupSize() const {
        auto group_size = m_group_size->value<MetaValue>(0);
        if (!group_size) {
            return 0;
        }
        return group_size.value()->get<u32>().value();
    }

    void GroupSceneObject::setGroupSize(size_t size) {
        auto group_size = m_group_size->value<MetaValue>(0);
        if (!group_size) {
            return;
        }
        group_size.value()->set(static_cast<u32>(size));
    }

    void GroupSceneObject::updateGroupSize() { setGroupSize(m_children.size()); }

    /* PHYSICAL SCENE OBJECT */

    std::span<u8> PhysicalSceneObject::getData() const {
        std::stringstream ostr;

        // Lol
        Serializer out(ostr.rdbuf());
        Deserializer in(ostr.rdbuf());

        serialize(out);

        m_data.resize(in.size());
        in.seek(0, std::ios::beg);

        in.readBytes({reinterpret_cast<char *>(&m_data[0]), m_data.size()});

        return {m_data.data(), m_data.size()};
    }

    size_t PhysicalSceneObject::getDataSize() const { return getData().size(); }

    bool PhysicalSceneObject::hasMember(const QualifiedName &name) const {
        auto member = getMember(name);
        return member.has_value() && member.value() != nullptr;
    }

    MetaStruct::GetMemberT PhysicalSceneObject::getMember(const QualifiedName &name) const {
        if (name.empty())
            return make_meta_error<MetaStruct::MemberT>(name, 0, "Empty name for getMember");

        const auto name_str = name.toString();

        if (m_member_cache.contains(name_str))
            return m_member_cache.at(name_str);

        auto current_scope = name[0];
        auto array_index   = getArrayIndex(name, 0);
        if (!array_index) {
            return std::unexpected(array_index.error());
        }

        for (auto m : m_members) {
            if (m->name() != current_scope)
                continue;

            if (name.depth() == 1) {
                m_member_cache[name_str] = m;
                return m;
            }

            if (m->isTypeStruct()) {
                auto s      = m->value<MetaStruct>(*array_index).value();
                auto member = s->getMember(QualifiedName(name.begin() + 1, name.end()));
                if (member.has_value()) {
                    m_member_cache[name_str] = member.value();
                }
                return member;
            }
        }

        return nullptr;
    }

    size_t PhysicalSceneObject::getMemberOffset(const QualifiedName &name, int index) const {
        size_t offset = 0;
        return offset;
    }

    size_t PhysicalSceneObject::getMemberSize(const QualifiedName &name, int index) const {
        size_t size = 0;
        return size;
    }

    Result<void, ObjectError> PhysicalSceneObject::performScene(
        float delta_time, bool animate, std::vector<RenderInfo> &renderables,
        ResourceCache &resource_cache, std::vector<J3DLight> &scene_lights) {
        if (!getIsPerforming()) {
            return {};
        }

        if (m_type == "Light") {
            auto position_value_ptr  = getMember("Position").value();
            glm::vec3 position_value = getMetaValue<glm::vec3>(position_value_ptr).value();

            auto color_value_ptr      = getMember("Color").value();
            Color::RGBA32 color_value = getMetaValue<Color::RGBA32>(color_value_ptr).value();

            f32 r, g, b, a;
            color_value.getColor(r, g, b, a);

            auto intensity_value_ptr = getMember("Intensity").value();
            f32 intensity_value      = getMetaValue<f32>(intensity_value_ptr).value();

            J3DLight light  = DEFAULT_LIGHT;
            light.Position  = {position_value, 1};
            light.Color     = {r, g, b, a};
            light.DistAtten = {1 / intensity_value, 0, 0, 1};
            light.Direction = {0, -1, 0, 1};
            scene_lights.push_back(light);
        }

        if (!m_model_instance) {
            return {};
        }

        Transform render_transform = {
            {0, 0, 0},
            {0, 0, 0},
            {1, 1, 1}
        };

        auto transform_value_ptr = getMember("Transform").value();
        if (transform_value_ptr) {
            Transform transform = getMetaValue<Transform>(transform_value_ptr).value();
            m_model_instance->SetTranslation(transform.m_translation);
            m_model_instance->SetRotation(transform.m_rotation);
            m_model_instance->SetScale(transform.m_scale);
            m_transform      = transform;
            render_transform = transform;
        }

        if (m_type == "SunModel") {
            m_model_instance->SetScale({1, 1, 1});
        }

        if (m_type == "NPCKinopio") {
            HelperUpdateKinopioRender();
        }

        if (std::any_of(s_monte_types.begin(), s_monte_types.end(),
                        [&](u16 code) { return code == m_type.code(); })) {
            HelperUpdateMonteRender();
        }

            if (!scene_lights.empty()) {
                m_model_instance->SetLight(scene_lights[SCENE_LIGHT_OBJECT_SUN], 0);
                if (scene_lights.size() > 1) {
                    m_model_instance->SetLight(scene_lights[SCENE_LIGHT_OBJECT_SUN_SECONDARY], 1);
                } else {
                    m_model_instance->SetLight(DEFAULT_LIGHT, 1);
                }
                // m_model_instance->SetLight(scene_lights[SCENE_LIGHT_OBJECT_SPECULAR], 2);  //
                // Specular light
            } else {
                m_model_instance->SetLight(DEFAULT_LIGHT, 0);
                m_model_instance->SetLight(DEFAULT_LIGHT, 1);
            }

        if (animate)
            m_model_instance->UpdateAnimations(delta_time);

        renderables.emplace_back(get_shared_ptr<PhysicalSceneObject>(*this), m_model_instance,
                                 render_transform);

        return {};
    }

    void PhysicalSceneObject::dump(std::ostream &out, size_t indention,
                                   size_t indention_width) const {
        indention_width          = std::min(indention_width, size_t(8));
        std::string self_indent  = std::string(indention * indention_width, ' ');
        std::string value_indent = std::string((indention + 1) * indention_width, ' ');
        out << self_indent << m_type.name() << " (" << m_nameref.name() << ") {\n";
        out << self_indent << "members:\n";
        for (auto m : m_members) {
            m->dump(out, indention + 1, indention_width);
        }
        out << self_indent << "}\n";
    }

    void PhysicalSceneObject::applyWizard(const TemplateWizard &wizard) {
        m_nameref.setName(wizard.m_name);
        for (auto &member : wizard.m_init_members) {
            RefPtr<MetaMember> new_member =
                ref_cast<MetaMember>(make_deep_clone<MetaMember>(member));
            new_member->updateReferenceToList(m_members);
            m_members.emplace_back(new_member);
        }
    }

    Result<void, FSError>
    PhysicalSceneObject::loadRenderData(const std::filesystem::path &asset_path,
                                        const TemplateRenderInfo &info,
                                        ResourceCache &resource_cache) {
        J3DModelLoader bmdLoader;
        J3DMaterialTableLoader bmtLoader;
        J3DTextureLoader btiLoader;

        RefPtr<J3DModelData> model_data;
        RefPtr<J3DMaterialTable> mat_table;
        RefPtr<J3DAnimationInstance> anim_data;

        std::optional<std::string> model_file;
        std::optional<std::string> mat_file = info.m_file_materials;

        m_scene_resource_path = asset_path;

        // Get model variable that exists in some objects
        auto model_member_result = getMember("Model");
        if (model_member_result) {
            if (model_member_result.value()) {
                std::string model_member_value =
                    getMetaValue<std::string>(model_member_result.value()).value();
                model_file = std::format("mapobj/{}.bmd", model_member_value);
            }
        } else {
            return make_fs_error<void>(std::error_code(), model_member_result.error().m_message);
        }

        if (info.m_file_model) {
            model_file = info.m_file_model;
        }

        // Early return since no model to animate etc.
        if (!model_file) {
            return {};
        }

        // Load model data

        std::filesystem::path model_path = asset_path / model_file.value();
        std::string model_name           = model_path.stem().string();
        std::transform(model_name.begin(), model_name.end(), model_name.begin(), ::tolower);

        if (resource_cache.m_model.count(model_name) == 0) {
            auto model_path_exists_res = Toolbox::Filesystem::is_regular_file(model_path);
            if (!model_path_exists_res || !model_path_exists_res.value()) {
                return {};
            }

            bStream::CFileStream model_stream(model_path.string(), bStream::Endianess::Big,
                                              bStream::OpenMode::In);

            model_data = bmdLoader.Load(&model_stream, 0);
            // resource_cache.m_model.insert({model_name, *model_data});
        } else {
            model_data = make_referable<J3DModelData>(resource_cache.m_model[model_name]);
        }

        m_model_data = model_data;

        if (!mat_file) {
            mat_file = model_file->replace(model_file->size() - 3, 3, "bmt");
        }

        // Load material data

        std::filesystem::path mat_path = asset_path / mat_file.value();
        std::string mat_name           = mat_path.stem().string();
        std::transform(mat_name.begin(), mat_name.end(), mat_name.begin(), ::tolower);

        auto mat_path_exists_res = Toolbox::Filesystem::is_regular_file(mat_path);
        if (mat_path_exists_res && mat_path_exists_res.value()) {
            bStream::CFileStream mat_stream(mat_path.string(), bStream::Endianess::Big,
                                            bStream::OpenMode::In);

            mat_table = bmtLoader.Load(&mat_stream, model_data);
            if (mat_name == "nozzlebox") {
                RefPtr<J3DMaterial> nozzle_mat = mat_table->GetMaterial("_mat1");
                auto nozzle_type_member        = getMember("Spawn").value();
                std::string nozzle_type = getMetaValue<std::string>(nozzle_type_member).value();
                if (nozzle_type == "normal_nozzle_item") {
                    nozzle_mat->TevBlock->mTevColors[1] = {0, 0, 255, 255};
                } else if (nozzle_type == "rocket_nozzle_item") {
                    nozzle_mat->TevBlock->mTevColors[1] = {255, 0, 0, 255};
                } else if (nozzle_type == "back_nozzle_item") {
                    nozzle_mat->TevBlock->mTevColors[1] = {90, 90, 120, 255};
                }
            }

            if (mat_name == "sky") {
                // RefPtr<J3DTexture> sky_tex_01 = mat_table->GetTexture("B_sky_kumo_s");
                // RefPtr<J3DTexture> sky_tex_02 = mat_table->GetTexture("B_cloud_s");
                //////RefPtr<J3DTexture> sky_tex_03 = mat_table->GetTexture("B_hikoukigumo_s");

                // if (sky_tex_01) {
                //     sky_tex_01->WrapS = EGXWrapMode::Repeat;
                // }

                // if (sky_tex_02) {
                //     sky_tex_02->WrapS = EGXWrapMode::Repeat;
                // }

                // RefPtr<J3DMaterial> mat = mat_table->GetMaterial("_01_nyudougumo");
                // EGXBlendMode blah       = mat->PEBlock.mBlendMode.Type;
                // mat->PEBlock.mZMode.Enable   = false;
                // mat->PEBlock.mBlendMode.Type = EGXBlendMode::None;
            }
        }

        if (type() == "NPCKinopio") {
            HelperUpdateKinopioRender();
        }

        if (std::any_of(s_monte_types.begin(), s_monte_types.end(),
                        [&](u16 code) { return code == m_type.code(); })) {
            HelperUpdateMonteRender();
        }

        // TODO: Load texture data

        m_model_instance = model_data->CreateInstance();
        if (mat_table) {
            m_model_instance->SetInstanceMaterialTable(mat_table);
            m_model_instance->SetUseInstanceMaterialTable(true);
        }

        if (type() == "Sky") {
            // Force render behind everything
            m_model_instance->SetSortBias(255);
            m_model_instance->SetInstanceMaterialTable(mat_table);
        }

        for (auto &[tex_name, new_tex_path] : info.m_texture_swap_map) {
            std::filesystem::path tex_path = asset_path / new_tex_path;
            std::string tex_name_lower     = tex_name;
            std::transform(tex_name_lower.begin(), tex_name_lower.end(), tex_name_lower.begin(),
                           ::tolower);
            bool tex_path_exists = Filesystem::is_regular_file(tex_path).value_or(false);
            if (tex_path_exists) {
                bStream::CFileStream tex_stream(tex_path.string(), bStream::Endianess::Big,
                                                bStream::OpenMode::In);
                RefPtr<J3DTexture> new_texture = btiLoader.Load(tex_name, &tex_stream);
                model_data->SetTexture(tex_name, new_texture);
            }
        }

        for (auto &anim_file : info.m_file_animations) {
            std::filesystem::path anim_path = asset_path / anim_file;
            std::string anim_name           = anim_path.stem().string();

            auto anim_path_exists_res = Filesystem::is_regular_file(anim_path);
            if (anim_path_exists_res && anim_path_exists_res.value()) {
                J3DAnimationLoader anmLoader;
                bStream::CFileStream anim_stream(anim_path.string(), bStream::Endianess::Big,
                                                 bStream::OpenMode::In);

                if (anim_file.ends_with(".brk")) {
                    m_model_instance->SetRegisterColorAnimation(
                        std::reinterpret_pointer_cast<J3DColorAnimationInstance>(
                            anmLoader.LoadAnimation(anim_stream)));
                }

                if (anim_file.ends_with(".btp")) {
                    m_model_instance->SetTexIndexAnimation(
                        std::reinterpret_pointer_cast<J3DTexIndexAnimationInstance>(
                            anmLoader.LoadAnimation(anim_stream)));
                }

                if (anim_file.ends_with(".btk")) {
                    m_model_instance->SetTexMatrixAnimation(
                        std::reinterpret_pointer_cast<J3DTexMatrixAnimationInstance>(
                            anmLoader.LoadAnimation(anim_stream)));
                }
            }
        }

        return {};
    }

    Result<void, SerialError> PhysicalSceneObject::serialize(Serializer &out) const {
        return gameSerialize(out);
    }

    Result<void, SerialError> PhysicalSceneObject::deserialize(Deserializer &in) {
        return gameDeserialize(in);
    }

    Result<void, SerialError> PhysicalSceneObject::gameSerialize(Serializer &out) const {
        std::streampos start = out.tell();

        out.pushBreakpoint();
        {
            // Write the size marker for now
            out.write<u32>(0);

            NameRef type_ref(m_type);
            type_ref.serialize(out);

            m_nameref.serialize(out);

            bool is_map_obj_base = m_type == "MapObjBase";

            for (auto &member : m_members) {
                if (is_map_obj_base) {
                    if (member->name() == "PoleLength" && getMetaValue<f32>(member, 0) == 0.0f) {
                        continue;
                    }
                }
                auto result = member->gameSerialize(out);
                if (!result) {
                    return result;
                }
            }
        }

        u32 obj_size = static_cast<u32>(out.size() - start);

        out.popBreakpoint();

        // Write the size marker
        out.write<u32, std::endian::big>(obj_size);
        out.seek(0, std::ios::end);
        return {};
    }

    Result<void, SerialError> PhysicalSceneObject::gameDeserialize(Deserializer &in) {
        auto scene_path = std::filesystem::path(in.filepath()).parent_path();

        // Metadata
        auto length           = in.read<u32, std::endian::big>();
        std::streampos endpos = static_cast<std::size_t>(in.tell()) + length - 4;

        // Type
        NameRef type;
        {
            auto result = type.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        // Name
        NameRef name;
        {
            auto result = name.deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        m_type = type.name();
        setNameRef(name);

        auto template_result = TemplateFactory::create(m_type.name(), m_include_custom);
        if (!template_result) {
            auto error_v = template_result.error();
            if (std::holds_alternative<FSError>(error_v)) {
                auto error = std::get<FSError>(error_v);
                return make_serial_error<void>(in, error.m_message[0]);
            } else {
                auto error = std::get<JSONError>(error_v);
                return make_serial_error<void>(in, error.m_message[0]);
            }
        }

        auto template_ = std::move(template_result.value());
        auto wizard    = template_->getWizard(name.name());
        if (!wizard) {
            wizard = template_->getWizard("Default");
            if (!wizard) {
                wizard = template_->getWizard();
            }
        }

        const char *debug_str = template_->type().data();

        // Members
        for (size_t i = 0; i < wizard->m_init_members.size(); ++i) {
            auto &m                        = wizard->m_init_members[i];
            RefPtr<MetaMember> this_member = ref_cast<MetaMember>(make_deep_clone<MetaMember>(m));
            this_member->updateReferenceToList(m_members);
            if (in.tell() < endpos) {
                auto result = this_member->gameDeserialize(in);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
            m_members.push_back(this_member);
        }

        // Skip padding/unknown data
        in.seek(endpos, std::ios::beg);

        std::filesystem::path asset_path = scene_path.parent_path();
        auto load_result = loadRenderData(asset_path, wizard->m_render_info, getResourceCache());
        if (!load_result) {
            return make_serial_error<void>(
                in, std::format("Failed to load render data for object {} ({})!", m_type.name(),
                                m_nameref.name()));
        }

        return {};
    }

    ObjectFactory::create_t ObjectFactory::create(Deserializer &in, bool include_custom) {
        if (isGroupObject(in)) {
            auto obj              = make_scoped<GroupSceneObject>();
            obj->m_include_custom = include_custom;
            auto result           = obj->gameDeserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            return obj;
        } else {
            auto obj              = make_scoped<PhysicalSceneObject>();
            obj->m_include_custom = include_custom;
            auto result           = obj->gameDeserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            return obj;
        }
    }

    ObjectFactory::create_ret_t ObjectFactory::create(const Template &template_,
                                                      std::string_view wizard_name, const fs_path &resource_path) {
        if (isGroupObject(template_.type())) {
            return make_scoped<GroupSceneObject>(template_, wizard_name);
        } else if (isPhysicalObject(template_.type())) {
            ScopePtr<PhysicalSceneObject> obj =
                make_scoped<PhysicalSceneObject>(template_, wizard_name);

            auto wizard = template_.getWizard(wizard_name);
            if (!wizard) {
                TOOLBOX_WARN_V("[OBJECT_FACTORY] Failed to fetch wizard for {}, which may cause "
                               "undefined behavior!", template_.type());
                return obj;
            }

            obj->loadRenderData(resource_path, wizard->m_render_info, getResourceCache());
            return obj;
        } else {
            return make_scoped<VirtualSceneObject>(template_, wizard_name);
        }
    }

    static std::vector<u16> s_group_hashes = {16824, 15406, 28318, 18246, 43971, 9858, 25289, 33769,
                                              49941, 13756, 65459, 38017, 47488, 8719, 22637};

    bool ObjectFactory::isGroupObject(std::string_view type) {
        u16 hash = NameRef::calcKeyCode(type);
        return std::find(s_group_hashes.begin(), s_group_hashes.end(), hash) !=
               s_group_hashes.end();
    }

    bool ObjectFactory::isGroupObject(Deserializer &in) {
        in.pushBreakpoint();
        in.seek(4);
        u16 hash = in.read<u16, std::endian::big>();
        in.popBreakpoint();
        return std::find(s_group_hashes.begin(), s_group_hashes.end(), hash) !=
               s_group_hashes.end();
    }

    bool ObjectFactory::isPhysicalObject(std::string_view type) { return !isGroupObject(type); }

    bool ObjectFactory::isPhysicalObject(Deserializer &in) { return !isGroupObject(in); }

}  // namespace Toolbox::Object
