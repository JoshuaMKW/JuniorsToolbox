#include "objlib/object.hpp"
#include "objlib/meta/errors.hpp"
#include <expected>
#include <gui/modelcache.hpp>
#include <string>

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

    std::vector<s8> VirtualSceneObject::getData() const { return {}; }

    size_t VirtualSceneObject::getDataSize() const { return 0; }

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

    std::expected<void, ObjectError>
    VirtualSceneObject::performScene(std::vector<std::shared_ptr<J3DModelInstance>> &,
                                     ResourceCache &) {
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
            std::shared_ptr<MetaMember> new_member = make_deep_clone<MetaMember>(member);
            new_member->updateReferenceToList(m_members);
            m_members.emplace_back(new_member);
        }
    }

    std::expected<void, SerialError> VirtualSceneObject::serialize(Serializer &out) const {
        return {};
    }

    std::expected<void, SerialError> VirtualSceneObject::deserialize(Deserializer &in) {
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

        auto template_result = TemplateFactory::create(m_type);
        if (!template_result) {
            auto error_v = template_result.error();
            if (std::holds_alternative<FSError>(error_v)) {
                auto error = std::get<FSError>(error_v);
                return make_serial_error<void>(in, error.m_message);
            } else {
                auto error = std::get<JSONError>(error_v);
                return make_serial_error<void>(in, error.m_message);
            }
        }

        auto template_ = std::move(template_result.value());
        auto wizard    = template_->getWizard();

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
            auto &m          = wizard->m_init_members[i];
            auto this_member = make_deep_clone<MetaMember>(m);
            this_member->updateReferenceToList(m_members);
            auto result = this_member->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            m_members.push_back(this_member);
        }

        in.seek(endpos, std::ios::beg);
        return {};
    }

    /* GROUP SCENE OBJECT */

    std::vector<s8> GroupSceneObject::getData() const { return std::vector<s8>(); }

    size_t GroupSceneObject::getDataSize() const { return size_t(); }

    std::expected<void, ObjectGroupError>
    GroupSceneObject::addChild(std::shared_ptr<ISceneObject> child) {
        auto child_it = std::find_if(
            m_children.begin(), m_children.end(), [&](std::shared_ptr<ISceneObject> other) {
                return other->getNameRef().name() == child->getNameRef().name();
            });

        if (child_it != m_children.end()) {
            ObjectGroupError err = {
                "Child already in the group object:", std::stacktrace::current(), this, {}};
            return std::unexpected(err);
        }

        ISceneObject *parent = child->getParent();
        if (parent) {
            parent->removeChild(child->getNameRef().name());
        }
        auto result = child->_setParent(this);
        if (!result) {
            ObjectGroupError err = {"Cannot add a child to the group object:",
                                    std::stacktrace::current(),
                                    this,
                                    {result.error()}};
            return std::unexpected(err);
        }
        m_children.push_back(child);
        updateGroupSize();
        return {};
    }

    std::expected<void, ObjectGroupError>
    GroupSceneObject::removeChild(std::shared_ptr<ISceneObject> child) {
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

    std::expected<void, ObjectGroupError> GroupSceneObject::removeChild(const QualifiedName &name) {
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

    std::expected<std::vector<std::shared_ptr<ISceneObject>>, ObjectGroupError>
    GroupSceneObject::getChildren() {
        return m_children;
    }

    std::optional<std::shared_ptr<ISceneObject>>
    GroupSceneObject::getChild(const QualifiedName &name) {
        auto scope = name[0];
        auto it    = std::find_if(m_children.begin(), m_children.end(),
                                  [&](const auto &ptr) { return ptr->getNameRef().name() == scope; });
        if (it == m_children.end())
            return {};
        if (name.depth() == 1)
            return *it;
        return it->get()->getChild(QualifiedName(name.begin() + 1, name.end()));
    }

    std::expected<void, ObjectError>
    GroupSceneObject::performScene(std::vector<std::shared_ptr<J3DModelInstance>> &renderables,
                                   ResourceCache &resource_cache) {

        std::vector<ObjectError> child_errors;

        for (auto child : m_children) {
            auto result = child->performScene(renderables, resource_cache);
            if (!result)
                child_errors.push_back(result.error());
        }

        if (child_errors.size() > 0) {
            ObjectGroupError err;
            {
                err.m_message = std::format("ObjectGroupError: {} ({}): There were errors "
                                            "performing the children:",
                                            m_type, m_nameref.name());
                err.m_object  = this;
                err.m_stack   = std::stacktrace::current();
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

    std::expected<void, SerialError> GroupSceneObject::serialize(Serializer &out) const {
        return {};
    }

    std::expected<void, SerialError> GroupSceneObject::deserialize(Deserializer &in) {
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

        auto template_result = TemplateFactory::create(m_type);
        if (!template_result) {
            auto error_v = template_result.error();
            if (std::holds_alternative<FSError>(error_v)) {
                auto error = std::get<FSError>(error_v);
                return make_serial_error<void>(in, error.m_message);
            } else {
                auto error = std::get<JSONError>(error_v);
                return make_serial_error<void>(in, error.m_message);
            }
        }

        auto template_ = std::move(template_result.value());
        auto wizard    = template_->getWizard();

        // Members
        bool late_group_size = (obj_type.code() == 15406 || obj_type.code() == 9858);
        if (!late_group_size) {
            m_group_size->deserialize(in);
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
            auto &m          = wizard->m_init_members[i];
            auto this_member = make_deep_clone<MetaMember>(m);
            this_member->updateReferenceToList(m_members);
            auto result = this_member->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            m_members.push_back(this_member);
        }

        if (late_group_size) {
            m_group_size->deserialize(in);
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
            ObjectFactory::create_t result = ObjectFactory::create(in);
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

    std::vector<s8> PhysicalSceneObject::getData() const { return {}; }

    size_t PhysicalSceneObject::getDataSize() const { return 0; }

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

    std::expected<void, ObjectError>
    PhysicalSceneObject::performScene(std::vector<std::shared_ptr<J3DModelInstance>> &renderables,
                                      ResourceCache &resource_cache) {

        if (m_model_instance) {
            auto transform_value_ptr = getMember(std::string("Transform")).value();
            if (transform_value_ptr) {
                Transform transform = getMetaValue<Transform>(transform_value_ptr).value();
                m_model_instance->SetTranslation(transform.m_translation);
                m_model_instance->SetRotation(transform.m_rotation);
                m_model_instance->SetScale(transform.m_scale);
            }
            renderables.push_back(m_model_instance);
            return {};
        }

        auto model_member_result = getMember(std::string("Model"));
        if (!model_member_result.has_value())
            return {};

        auto model_name_value_ptr = model_member_result.value();
        if (!model_name_value_ptr)
            return {};

        auto model_name = getMetaValue<std::string>(model_name_value_ptr).value();
        std::transform(model_name.begin(), model_name.end(), model_name.begin(), ::tolower);

        if (resource_cache.m_model.count(model_name) == 0)
            return {};

        // TODO: Use json information to load model and material data

        m_model_instance = resource_cache.m_model[model_name]->GetInstance();

        if (resource_cache.m_material.count(model_name) != 0) {
            m_model_instance->SetInstanceMaterialTable(resource_cache.m_material[model_name]);
            m_model_instance->SetUseInstanceMaterialTable(true);
        }

        auto transform_value_ptr = getMember(std::string("Transform")).value();
        if (transform_value_ptr) {
            Transform transform = getMetaValue<Transform>(transform_value_ptr).value();
            m_model_instance->SetTranslation(transform.m_translation);
            m_model_instance->SetRotation(transform.m_rotation);
            m_model_instance->SetScale(transform.m_scale);
        }
        renderables.push_back(m_model_instance);

        return {};
    }

    void PhysicalSceneObject::dump(std::ostream &out, size_t indention,
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

    void PhysicalSceneObject::applyWizard(const TemplateWizard &wizard) {
        m_nameref.setName(wizard.m_name);
        for (auto &member : wizard.m_init_members) {
            auto new_member = make_deep_clone<MetaMember>(member);
            new_member->updateReferenceToList(m_members);
            m_members.emplace_back(new_member);
        }
    }

    std::expected<void, SerialError> PhysicalSceneObject::serialize(Serializer &out) const {
        return std::expected<void, SerialError>();
    }

    std::expected<void, SerialError> PhysicalSceneObject::deserialize(Deserializer &in) {
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

        const char *type_cstr = m_type.c_str();

        auto template_result = TemplateFactory::create(m_type);
        if (!template_result) {
            auto error_v = template_result.error();
            if (std::holds_alternative<FSError>(error_v)) {
                auto error = std::get<FSError>(error_v);
                return make_serial_error<void>(in, error.m_message);
            } else {
                auto error = std::get<JSONError>(error_v);
                return make_serial_error<void>(in, error.m_message);
            }
        }

        auto template_ = std::move(template_result.value());
        auto wizard    = template_->getWizard();

        const char *debug_str = template_->type().data();

        // Members
        for (size_t i = 0; i < wizard->m_init_members.size(); ++i) {
            auto &m          = wizard->m_init_members[i];
            auto this_member = make_deep_clone<MetaMember>(m);
            this_member->updateReferenceToList(m_members);
            if (in.tell() < endpos) {
                auto result = this_member->deserialize(in);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
            m_members.push_back(this_member);
        }

        in.seek(endpos, std::ios::beg);
        return {};
    }

    ObjectFactory::create_t ObjectFactory::create(Deserializer &in) {
        if (isGroupObject(in)) {
            auto obj    = std::make_unique<GroupSceneObject>();
            auto result = obj->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            return obj;
        } else {
            auto obj    = std::make_unique<PhysicalSceneObject>();
            auto result = obj->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
            return obj;
        }
    }

    ObjectFactory::create_ret_t ObjectFactory::create(const Template &template_,
                                                      std::string_view wizard_name) {
        if (isGroupObject(template_.type())) {
            return std::make_unique<GroupSceneObject>(template_, wizard_name);
        } else if (isPhysicalObject(template_.type())) {
            return std::make_unique<PhysicalSceneObject>(template_, wizard_name);
        } else {
            return std::make_unique<VirtualSceneObject>(template_, wizard_name);
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
