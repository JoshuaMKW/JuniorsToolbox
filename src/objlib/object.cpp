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

    Result<void, BaseError> VirtualSceneObject::loadDependencies(const fs_path &dependencies_path) {
        return Result<void, BaseError>();
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
        m_wizard = wizard.m_name;
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

        m_template                           = *template_result.value();
        std::optional<TemplateWizard> wizard = m_template.getWizard(name.name());
        if (!wizard) {
            const std::vector<TemplateWizard> &wizards = m_template.wizards();
            for (const TemplateWizard &wz : wizards) {
                if (wz.m_obj_name == name.name()) {
                    wizard = wz;
                    break;
                }
            }
            if (!wizard) {
                wizard = m_template.getWizard("Default");
                if (!wizard) {
                    wizard = m_template.getWizard();
                }
            }
        }

        m_wizard = wizard->m_name;

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

    const std::vector<RefPtr<ISceneObject>> &GroupSceneObject::getChildren() { return m_children; }

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
                if (name.has_value() && !name->empty()) {
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

        m_template                           = *template_result.value();
        std::optional<TemplateWizard> wizard = m_template.getWizard(obj_name.name());
        if (!wizard) {
            const std::vector<TemplateWizard> &wizards = m_template.wizards();
            for (const TemplateWizard &wz : wizards) {
                if (wz.m_obj_name == obj_name.name()) {
                    wizard = wz;
                    break;
                }
            }
            if (!wizard) {
                wizard = m_template.getWizard("Default");
                if (!wizard) {
                    wizard = m_template.getWizard();
                }
            }
        }

        m_wizard = wizard->m_name;

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

    Result<void, BaseError>
    PhysicalSceneObject::loadDependencies(const fs_path &dependencies_path) {
        if (dependencies_path.empty()) {
            return {};
        }

        auto result = loadRenderData(dependencies_path, getResourceCache());
        if (!result) {
            return std::unexpected(result.error());
        }

        return {};
    }

    void PhysicalSceneObject::refreshRenderState() {
        // If this executes, the wizard has been reassigned
        if (reassignWizardBasedOnFields()) {
            m_render_controller->clear();
            loadRenderData(m_scene_resource_path, getResourceCache());
        }

        bareRefreshRenderState_();
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

            auto color_value_ptr     = getMember("Color").value();
            Color::RGBA8 color_value = getMetaValue<Color::RGBA8>(color_value_ptr).value();

            f32 r, g, b, a;
            color_value.getColor(r, g, b, a);

            auto intensity_value_ptr = getMember("Unknown1").value();  // Intensity (?)
            f32 intensity_value      = getMetaValue<f32>(intensity_value_ptr).value();

            J3DLight light  = DEFAULT_LIGHT;
            light.Position  = {position_value, 1};
            light.Color     = {r, g, b, a};
            light.DistAtten = {1 / intensity_value, 0, 0, 1};
            light.Direction = {0, -1, 0, 1};
            scene_lights.push_back(light);
        }

        RefPtr<J3DModelInstance> selected_model = m_render_controller->getRenderModel();
        if (!selected_model) {
            return {};
        }

        Transform render_transform = {
            {0, 0, 0},
            {0, 0, 0},
            {1, 1, 1}
        };

        if (m_transform.has_value()) {
            render_transform = m_transform.value();
        }

        if (m_type == "SunModel") {
            selected_model->SetScale({1, 1, 1});
        }

        if (!scene_lights.empty()) {
            selected_model->SetLight(scene_lights[SCENE_LIGHT_OBJECT_SUN], 0);
            if (scene_lights.size() > 1) {
                selected_model->SetLight(scene_lights[SCENE_LIGHT_OBJECT_SUN_SECONDARY], 1);
            } else {
                selected_model->SetLight(DEFAULT_LIGHT, 1);
            }
            // selected_model->SetLight(scene_lights[SCENE_LIGHT_OBJECT_SPECULAR], 2);  //
            // Specular light
        } else {
            selected_model->SetLight(DEFAULT_LIGHT, 0);
            selected_model->SetLight(DEFAULT_LIGHT, 1);
        }

        renderables.emplace_back(get_shared_ptr<PhysicalSceneObject>(*this), selected_model,
                                 render_transform);

        return {};
    }

    void PhysicalSceneObject::sync() {
        const bool wizard_reassigned = reassignWizardBasedOnFields();

        auto transform_value_ptr = getMember("Transform").value();
        if (transform_value_ptr) {
            m_transform = getMetaValue<Transform>(transform_value_ptr).value();
        }

        std::filesystem::path asset_path = m_scene_resource_path;
        auto load_result                 = loadRenderData(asset_path, getResourceCache());
        if (!load_result) {
            return;
        }
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
        m_wizard = wizard.m_name;
        m_nameref.setName(wizard.m_name);
        for (auto &member : wizard.m_init_members) {
            RefPtr<MetaMember> new_member =
                ref_cast<MetaMember>(make_deep_clone<MetaMember>(member));
            new_member->updateReferenceToList(m_members);
            m_members.emplace_back(new_member);
        }
    }

    bool PhysicalSceneObject::reassignWizardBasedOnFields() {
        const std::vector<TemplateWizard> &wizards = m_template.wizards();
        if (wizards.empty()) {
            return false;
        }

        auto object_member_result = getMember("Object");
        if (!object_member_result) {
            return false;
        }

        RefPtr<MetaMember> object_member = object_member_result.value();
        if (!object_member) {
            return false;
        }

        std::string object_str = getMetaValue<std::string>(object_member, 0).value();

        for (const TemplateWizard &wizard : wizards) {
            bool has_object_field = false;
            for (const MetaMember &member : wizard.m_init_members) {
                if (member.name() == "Object") {
                    has_object_field = true;

                    auto value_result = member.value<MetaValue>(0);
                    if (!value_result) {
                        return false;
                    }

                    auto v_result = value_result.value()->get<std::string>();
                    if (!v_result) {
                        return false;
                    }

                    if (v_result.value() == object_str) {
                        bool updated = m_wizard != wizard.m_name;
                        if (updated) {
                            m_wizard = wizard.m_name;
                        }
                        return updated;
                    }
                }
            }

            if (!has_object_field) {
                if (wizard.m_obj_name == m_nameref.name()) {
                    bool updated = m_wizard != wizard.m_name;
                    if (updated) {
                        m_wizard = wizard.m_name;
                    }
                    return updated;
                }
            }
        }

        bool updated = m_wizard != "default_init";
        if (updated) {
            m_wizard = "default_init";
        }
        return updated;
    }

    Result<void, FSError>
    PhysicalSceneObject::loadRenderData(const std::filesystem::path &asset_path,
                                        ResourceCache &resource_cache) {
        m_render_controller = make_referable<ObjectRenderController>();

        if (m_template.type().empty() || m_wizard.empty()) {
            return make_fs_error<void>(std::error_code(),
                                       {"[Object] Object has no template and wizard data!"});
        }

        J3DModelLoader bmdLoader;
        J3DMaterialTableLoader bmtLoader;
        J3DTextureLoader btiLoader;

        RefPtr<J3DModelData> model_data;
        RefPtr<J3DMaterialTable> mat_table;
        RefPtr<J3DAnimationInstance> anim_data;

        ScopePtr<TemplateRenderInfo> render_info;

        std::optional<std::string> model_file;
        std::optional<std::string> mat_file;

        m_scene_resource_path = asset_path;

        const fs_path global_path = asset_path.parent_path()
                                        .parent_path()
                                        .parent_path()
                                        .parent_path();  // files/data/scene/X/scene -> files/

        // Special Better Sunshine Engine object that has dynamic render paths
        if (type() == "GenericRailObj") {
            auto model_member_result = getMember("Model");
            if (model_member_result.value_or(nullptr)) {
                const std::string model_field =
                    getMetaValue<std::string>(model_member_result.value()).value();

                render_info = make_scoped<TemplateRenderInfo>();
                render_info->m_file_model = std::format("mapobj/{}.bmd", model_field);
                render_info->m_file_animations = {
                    std::format("mapobj/{}.bck", model_field),
                    std::format("mapobj/{}.blk", model_field),
                    std::format("mapobj/{}.bpk", model_field),
                    std::format("mapobj/{}.brk", model_field),
                    std::format("mapobj/{}.btp", model_field),
                    std::format("mapobj/{}.btk", model_field),
                };
            }
        } else {
            auto object_member_result = getMember("Object");
            if (object_member_result.value_or(nullptr)) {
                const std::string object_field =
                    getMetaValue<std::string>(object_member_result.value()).value();
                render_info = TemplateFactory::findRenderInfo(object_field);
            }

            if (!render_info) {
                std::optional<TemplateWizard> wizard = m_template.getWizard(m_wizard);
                if (!wizard) {
                    return make_fs_error<void>(
                        std::error_code(),
                        {std::format("[PhysicalObject] Failed to load render data for object {}",
                                     m_type.name())});
                }

                render_info = make_scoped<TemplateRenderInfo>(wizard->m_render_info);
            }
        }

        mat_file = render_info->m_file_materials;

        if (render_info->m_file_model) {
            model_file = render_info->m_file_model;
        }

        // Early return since no model to animate etc.
        if (!model_file) {
            return {};
        }

        // Load model data

        const std::filesystem::path model_path = model_file.value().starts_with('/')
                                                     ? global_path / model_file.value().substr(1)
                                                     : asset_path / model_file.value();

        std::string model_name = model_path.stem().string();
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

        const fs_path mat_path = mat_file.value().starts_with('/')
                                     ? global_path / mat_file.value().substr(1)
                                     : asset_path / mat_file.value();

        std::string mat_name = mat_path.stem().string();
        std::transform(mat_name.begin(), mat_name.end(), mat_name.begin(), ::tolower);

        auto mat_path_exists_res = Toolbox::Filesystem::is_regular_file(mat_path);
        if (mat_path_exists_res.value_or(false)) {
            bStream::CFileStream mat_stream(mat_path.string(), bStream::Endianess::Big,
                                            bStream::OpenMode::In);

            mat_table = bmtLoader.Load(&mat_stream, model_data);
            if (mat_name == "nozzlebox") {
                RefPtr<J3DMaterial> nozzle_mat = mat_table->GetMaterial("_mat1");
                auto nozzle_type_member        = getMember("Nozzle").value();
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

        // TODO: Load texture data

        RefPtr<J3DModelInstance> render_model = model_data->CreateInstance();
        if (mat_table) {
            render_model->SetInstanceMaterialTable(mat_table);
            render_model->SetUseInstanceMaterialTable(true);
        }

        if (type() == "Sky") {
            // Force render behind everything
            render_model->SetSortBias(255);
            render_model->SetInstanceMaterialTable(mat_table);
        }

        for (auto &[tex_name, new_tex_path] : render_info->m_texture_swap_map) {
            const fs_path tex_path     = new_tex_path.starts_with('/')
                                             ? global_path / new_tex_path.substr(1)
                                             : asset_path / new_tex_path;
            std::string tex_name_lower = tex_name;
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

        for (auto &anim_file : render_info->m_file_animations) {
            const fs_path anim_path = anim_file.starts_with('/') ? global_path / anim_file.substr(1)
                                                                 : asset_path / anim_file;
            std::string anim_name   = anim_path.stem().string();

            auto anim_path_exists_res = Filesystem::is_regular_file(anim_path);
            if (anim_path_exists_res && anim_path_exists_res.value()) {
                J3DAnimationLoader anmLoader;
                bStream::CFileStream anim_stream(anim_path.string(), bStream::Endianess::Big,
                                                 bStream::OpenMode::In);
                RefPtr<J3DAnimationInstance> animation = anmLoader.LoadAnimation(anim_stream);

                if (anim_file.ends_with(".bck")) {
                    m_render_controller->addAnimation(AnimationType::BCK, animation);
                }

                if (anim_file.ends_with(".brk")) {
                    m_render_controller->addAnimation(AnimationType::BRK, animation);
                }

                if (anim_file.ends_with(".btp")) {
                    m_render_controller->addAnimation(AnimationType::BTP, animation);
                }

                if (anim_file.ends_with(".btk")) {
                    m_render_controller->addAnimation(AnimationType::BTK, animation);
                }
            }
        }

        m_render_controller->addRenderModel(render_model);

        m_render_controller->startAnimation(AnimationType::BCK, 0);
        m_render_controller->startAnimation(AnimationType::BLK, 0);
        m_render_controller->startAnimation(AnimationType::BPK, 0);
        m_render_controller->startAnimation(AnimationType::BRK, 0);
        m_render_controller->startAnimation(AnimationType::BTK, 0);
        m_render_controller->startAnimation(AnimationType::BTP, 0);

        // Initialize the render state
        bareRefreshRenderState_();
        return {};
    }

    void PhysicalSceneObject::bareRefreshRenderState_() {
        RefPtr<J3DModelInstance> selected_model = m_render_controller->getRenderModel();
        if (!selected_model) {
            return;
        }

        if (m_transform.has_value()) {
            selected_model->SetTranslation(m_transform.value().m_translation);
            selected_model->SetRotation(m_transform.value().m_rotation);
            selected_model->SetScale(m_transform.value().m_scale);
        }

        if (m_type == "HideObjPictureTwin") {
            HelperUpdateHideObjPictureTwinRender();
            return;
        }

        if (m_type == "WaterHitPictureHideObj") {
            HelperUpdateWaterHitPictureHideObjRender();
            return;
        }

        if (m_type == "WoodBlock") {
            HelperUpdateWoodblockRender();
            return;
        }

        if (m_type == "NPCKinopio") {
            HelperUpdateKinopioRender();
            return;
        }

        if (Object::IsObjectMonte(this)) {
            HelperUpdateMonteRender();
            return;
        }
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
        m_scene_resource_path = scene_path.parent_path();

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

        m_template                           = *template_result.value();
        std::optional<TemplateWizard> wizard = m_template.getWizard(name.name());
        if (!wizard) {
            const std::vector<TemplateWizard> &wizards = m_template.wizards();
            for (const TemplateWizard &wz : wizards) {
                if (wz.m_obj_name == name.name()) {
                    wizard = wz;
                    break;
                }
            }
            if (!wizard) {
                wizard = m_template.getWizard("Default");
                if (!wizard) {
                    wizard = m_template.getWizard();
                }
            }
        }

        m_wizard              = wizard->m_name;
        const char *debug_str = m_template.type().data();

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

        sync();
        return {};
    }

    ScopePtr<ISmartResource> PhysicalSceneObject::clone(bool deep) const {
        auto obj         = make_scoped<PhysicalSceneObject>();
        obj->m_type      = m_type;
        obj->m_nameref   = m_nameref;
        obj->m_parent    = nullptr;
        obj->m_transform = m_transform;

        obj->m_scene_resource_path = m_scene_resource_path;

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

        auto result = obj->loadRenderData(obj->m_scene_resource_path, getResourceCache());
        if (!result) {
            TOOLBOX_ERROR_V("[PhysicalObject] {}", result.error().m_message);
        }
        return obj;
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
                                                      std::string_view wizard_name,
                                                      const fs_path &resource_path) {
        if (isGroupObject(template_.type())) {
            return make_scoped<GroupSceneObject>(template_, wizard_name);
        } else if (isPhysicalObject(template_.type())) {
            ScopePtr<PhysicalSceneObject> obj =
                make_scoped<PhysicalSceneObject>(template_, wizard_name);

            obj->loadRenderData(resource_path, getResourceCache());
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

    RefPtr<J3DAnimationInstance>
    ObjectRenderController::getAnimationControl(AnimationType type) const {
        if (m_active_animation_map.contains(type)) {
            const int animation_id = m_active_animation_map.at(type);
            switch (type) {
            case AnimationType::BCK:
                return m_animations_bck[animation_id];
            case AnimationType::BLK:
                return m_animations_blk[animation_id];
            case AnimationType::BPK:
                return m_animations_bpk[animation_id];
            case AnimationType::BRK:
                return m_animations_brk[animation_id];
            case AnimationType::BTK:
                return m_animations_btk[animation_id];
            case AnimationType::BTP:
                return m_animations_btp[animation_id];
            }
        }
        return nullptr;
    }

    ObjectRenderController &ObjectRenderController::clear() {
        m_model_instances.clear();
        m_animations_bck.clear();
        m_animations_blk.clear();
        m_animations_bpk.clear();
        m_animations_brk.clear();
        m_animations_btp.clear();
        m_animations_btk.clear();
        m_active_animation_map.clear();
        m_selected_model_idx = -1;
        return *this;
    }

    ObjectRenderController &ObjectRenderController::addRenderModel(RefPtr<J3DModelInstance> model) {
        m_model_instances.emplace_back(std::move(model));
        if (m_selected_model_idx == -1) {
            m_selected_model_idx = m_model_instances.size() - 1;
        }
        return *this;
    }

    ObjectRenderController &
    ObjectRenderController::addAnimation(AnimationType type,
                                         RefPtr<J3DAnimationInstance> animation) {
        switch (type) {
        case AnimationType::BCK:
            m_animations_bck.emplace_back(std::move(animation));
            return *this;
        case AnimationType::BLK:
            return *this;
        case AnimationType::BPK:
            return *this;
        case AnimationType::BRK:
            m_animations_brk.emplace_back(std::move(animation));
            return *this;
        case AnimationType::BTK:
            m_animations_btk.emplace_back(std::move(animation));
            return *this;
        case AnimationType::BTP:
            m_animations_btp.emplace_back(std::move(animation));
            return *this;
        }
        return *this;
    }

    ObjectRenderController &ObjectRenderController::selectRenderModel(int model_idx) {
        m_selected_model_idx = model_idx;
        return *this;
    }

    J3DLight ObjectRenderController::getLightData(int index) {
        return getRenderModel()->GetLight(index);
    }

    RefPtr<J3DModelInstance> ObjectRenderController::getRenderModel() const {
        return getRenderModel(m_selected_model_idx);
    }

    RefPtr<J3DModelInstance> ObjectRenderController::getRenderModel(int model_index) const {
        return model_index < 0 || m_model_instances.empty() ? nullptr
                                                            : m_model_instances[model_index];
    }

    size_t ObjectRenderController::getAnimationFrames(AnimationType type) const {
        RefPtr<J3DAnimationInstance> animation = getAnimationControl(type);
        return animation->GetLength();
    }

    float ObjectRenderController::getAnimationFrame(AnimationType type) const {
        RefPtr<J3DAnimationInstance> animation = getAnimationControl(type);
        return animation->GetFrame();
    }

    void ObjectRenderController::setAnimationFrame(size_t frame, AnimationType type) {
        RefPtr<J3DAnimationInstance> animation = getAnimationControl(type);
        animation->SetFrame(static_cast<u16>(frame));
    }

    bool ObjectRenderController::startAnimation(AnimationType type, int anim_idx) {
        RefPtr<J3DModelInstance> selected_model = getRenderModel();
        if (!selected_model) {
            return false;
        }

        RefPtr<J3DAnimationInstance> animation, last_animation;
        const int last_anim_id =
            m_active_animation_map.contains(type) ? m_active_animation_map[type] : -1;

        if (anim_idx == last_anim_id) {
            return true;
        }

        switch (type) {
        case AnimationType::BCK:
            animation = anim_idx < m_animations_bck.size() ? m_animations_bck[anim_idx] : nullptr;
            if (!animation) {
                return false;
            }
            last_animation = last_anim_id < m_animations_bck.size() ? m_animations_bck[last_anim_id]
                                                                    : nullptr;
            if (last_animation) {
                last_animation->SetFrame(0, true);
            }
            animation->SetFrame(0, false);
            selected_model->SetJointAnimation(ref_cast<J3DJointAnimationInstance>(animation));
            break;
        case AnimationType::BLK:
            return false;
        case AnimationType::BPK:
            return false;
        case AnimationType::BRK:
            animation = anim_idx < m_animations_brk.size() ? m_animations_brk[anim_idx] : nullptr;
            if (!animation) {
                return false;
            }
            last_animation = last_anim_id < m_animations_brk.size() ? m_animations_brk[last_anim_id]
                                                                    : nullptr;
            if (last_animation) {
                last_animation->SetFrame(0, true);
            }
            animation->SetFrame(0, false);
            selected_model->SetRegisterColorAnimation(
                ref_cast<J3DColorAnimationInstance>(animation));
            break;
        case AnimationType::BTK:
            animation = anim_idx < m_animations_btk.size() ? m_animations_btk[anim_idx] : nullptr;
            if (!animation) {
                return false;
            }
            last_animation = last_anim_id < m_animations_btk.size() ? m_animations_btk[last_anim_id]
                                                                    : nullptr;
            if (last_animation) {
                last_animation->SetFrame(0, true);
            }
            animation->SetFrame(0, false);
            selected_model->SetTexMatrixAnimation(
                ref_cast<J3DTexMatrixAnimationInstance>(animation));
            break;
        case AnimationType::BTP:
            animation = anim_idx < m_animations_btp.size() ? m_animations_btp[anim_idx] : nullptr;
            if (!animation) {
                return false;
            }
            last_animation = last_anim_id < m_animations_btp.size() ? m_animations_btp[last_anim_id]
                                                                    : nullptr;
            if (last_animation) {
                last_animation->SetFrame(0, true);
            }
            animation->SetFrame(0, false);
            selected_model->SetTexIndexAnimation(ref_cast<J3DTexIndexAnimationInstance>(animation));
            break;
        }

        m_active_animation_map[type] = anim_idx;
        return true;
    }

    bool ObjectRenderController::startAnimation(AnimationType type, const std::string &anim_name) {
        return false;
    }

    bool ObjectRenderController::stopAnimation(AnimationType type) {
        RefPtr<J3DAnimationInstance> animation = getAnimationControl(type);
        if (!animation) {
            return false;
        }
        animation->SetPaused(true);
        return true;
    }

    bool IsObjectMonte(ISceneObject *object) {
        const u16 type_code = NameRef(object->type()).code();
        return std::any_of(s_monte_types.begin(), s_monte_types.end(),
                           [&](u16 code) { return code == type_code; });
    }

}  // namespace Toolbox::Object
