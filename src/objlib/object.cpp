#include "objlib/object.hpp"
#include <expected>

namespace Toolbox::Object {
    size_t ISceneObject::getAnimationFrames() const {
        auto ctrl = getAnimationControl();
        if (ctrl.expired())
            return 0;
        return ctrl.lock()->GetLength();
    }

    float ISceneObject::getAnimationFrame() const {
        auto ctrl = getAnimationControl();
        if (ctrl.expired())
            return 0;
        return ctrl.lock()->GetFrame();
    }

    void ISceneObject::setAnimationFrame(u16 frame) {
        auto ctrl = getAnimationControl();
        if (ctrl.expired())
            return;
        ctrl.lock()->SetFrame(frame);
    }

    bool ISceneObject::startAnimation() {
        auto ctrl = getAnimationControl();
        if (ctrl.expired())
            return false;
        ctrl.lock()->SetPaused(false);
        return true;
    }

    bool ISceneObject::stopAnimation() {
        auto ctrl = getAnimationControl();
        if (ctrl.expired())
            return false;
        ctrl.lock()->SetPaused(true);
        return true;
    }

    std::vector<s8> VirtualSceneObject::getData() const { return {}; }
    size_t VirtualSceneObject::getDataSize() const { return 0; }

    bool VirtualSceneObject::hasMember(const QualifiedName &name) const { return false; }
    MetaStruct::GetMemberT VirtualSceneObject::getMember(const QualifiedName &name) const {
        if (name.empty())
            return {};

        auto current_scope = name[0];
        int array_index    = 0;
        {
            size_t lidx = current_scope.find('[');
            if (lidx != std::string::npos) {
                size_t ridx = current_scope.find(']', lidx);
                if (ridx == std::string::npos)
                    return {};

                auto arystr = current_scope.substr(lidx + 1, ridx - lidx);
                array_index = std::atoi(arystr.data());
            }
        }

        for (auto m : m_members) {
            if (m->isTypeStruct()) {
                auto s = m->value<MetaStruct>(array_index)->lock();
                if (s->name() != current_scope)
                    continue;
                if (name.depth() > 1)
                    return s->getMember(QualifiedName(name.begin() + 1, name.end()));
                return s;
            }
            if (m->formattedName(array_index) != current_scope)
                continue;
            if (name.depth() > 1)
                continue;
            return m;
        }
    }
    size_t VirtualSceneObject::getMemberOffset(const QualifiedName &name) const { return 0; }
    size_t VirtualSceneObject::getMemberSize(const QualifiedName &name) const { return 0; }

    std::expected<void, ObjectError> VirtualSceneObject::performScene() {
        return std::expected<void, ObjectError>();
    }

    void VirtualSceneObject::dump(std::ostream &out, int indention, int indention_width) const {}

    void VirtualSceneObject::serialize(Serializer &out) const {}

    void VirtualSceneObject::deserialize(Deserializer &in) {}

    std::vector<s8> GroupSceneObject::getData() const { return std::vector<s8>(); }

    size_t GroupSceneObject::getDataSize() const { return size_t(); }

    bool GroupSceneObject::hasMember(const QualifiedName &name) const { return false; }

    MetaStruct::GetMemberT GroupSceneObject::getMember(const QualifiedName &name) const {
        return {};
    }

    size_t GroupSceneObject::getMemberOffset(const QualifiedName &name) const { return size_t(); }

    size_t GroupSceneObject::getMemberSize(const QualifiedName &name) const { return size_t(); }

    std::expected<void, ObjectGroupError>
    GroupSceneObject::addChild(std::shared_ptr<ISceneObject> child) {
        m_children.push_back(child);
        return {};
    }

    std::expected<void, ObjectGroupError> GroupSceneObject::removeChild(ISceneObject *child) {
        auto it = std::find_if(m_children.begin(), m_children.end(),
                               [child](const auto &ptr) { return ptr.get() == child; });
        if (it == m_children.end()) {
            ObjectGroupError err = {"Child not found in the group object.",
                                    std::stacktrace::current(), this};
            return std::unexpected(err);
        }
        m_children.erase(it);
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
            return {};
        }

        return it->get()->removeChild(QualifiedName(name.begin() + 1, name.end()));
    }

    std::expected<std::vector<ISceneObject *>, ObjectGroupError> GroupSceneObject::getChildren() {
        std::vector<ISceneObject *> ret;
        ret.reserve(m_children.size());
        for (auto &child : m_children) {
            ret.push_back(child.get());
        }
        return ret;
    }

    std::expected<void, ObjectError> GroupSceneObject::performScene() {
        ObjectGroupError err;
        {
            err.m_message = std::format("ObjectGroupError: {} ({}): There were errors "
                                        "performing the children:",
                                        m_type, m_nameref.name());
            err.m_object  = this;
            err.m_stack   = std::stacktrace::current();
        }

        for (auto child : m_children) {
            auto result = child->performScene();
            if (!result)
                err.m_child_errors.push_back(result.error());
        }

        if (err.m_child_errors.size() > 0)
            return std::unexpected(err);

        return {};
    }

    void GroupSceneObject::dump(std::ostream &out, int indention, int indention_width) const {}

    void GroupSceneObject::serialize(Serializer &out) const {}

    void GroupSceneObject::deserialize(Deserializer &in) {}

}  // namespace Toolbox::Object
