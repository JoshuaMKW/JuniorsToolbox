#include "clone.hpp"
#include "objlib/meta/struct.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/error.hpp"
#include "objlib/template.hpp"
#include "objlib/transform.hpp"
#include <expected>
#include <optional>

namespace Toolbox::Object {
    MetaStruct::MetaStruct(std::string_view name, std::vector<MetaMember> members) : m_name(name) {
        for (const auto &m : members) {
            auto p = std::make_shared<MetaMember>(m);
            m_members.push_back(std::move(p));
        }
    }

    MetaStruct::GetMemberT MetaStruct::getMember(std::string_view name) const {
        return getMember(QualifiedName(name));
    }

    MetaStruct::GetMemberT MetaStruct::getMember(const QualifiedName &name) const {
        if (name.empty())
            return {};

        const auto name_str = name.toString();

        if (m_member_cache.contains(name_str))
            return m_member_cache.at(name_str);

        auto current_scope = name[0];
        auto array_result  = getArrayIndex(name, 0);
        if (!array_result) {
            return std::unexpected(array_result.error());
        }

        size_t array_index = array_result.value();
        if (array_index != std::string_view::npos) {
            current_scope = current_scope.substr(0, current_scope.find_first_of('['));
        } else {
            array_index = 0;
        }

        for (auto m : m_members) {
            if (m->name() != current_scope)
                continue;

            if (name.depth() == 1) {
                m_member_cache[name_str] = m;
                return m;
            }

            if (m->isTypeStruct()) {
                auto s      = m->value<MetaStruct>(array_index)->lock();
                auto member = s->getMember(QualifiedName(name.begin() + 1, name.end()));
                if (member.has_value()) {
                    m_member_cache[name_str] = member.value();
                }
                return member;
            }
        }

        return {};
    }

    constexpr QualifiedName MetaStruct::getQualifiedName() const {
        MetaStruct *parent              = m_parent;
        std::vector<std::string> scopes = {m_name};
        while (parent) {
            scopes.push_back(parent->m_name);
            parent = parent->m_parent;
        }
        std::reverse(scopes.begin(), scopes.end());
        return QualifiedName(scopes);
    }

    void MetaStruct::dump(std::ostream &out, size_t indention, size_t indention_width,
                          bool naked) const {
        indention_width          = std::min(indention_width, size_t(8));
        std::string self_indent  = std::string(indention * indention_width, ' ');
        std::string value_indent = std::string((indention + 1) * indention_width, ' ');
        if (!naked)
            out << self_indent << "struct " << m_name << " {\n";
        else
            out << "{\n";
        for (auto m : m_members) {
            m->dump(out, indention + 1, indention_width);
        }
        out << self_indent << "}";
        if (!naked)
            out << "\n";
    }

    bool MetaStruct::operator==(const MetaStruct &other) const = default;

    std::unique_ptr<IClonable> MetaStruct::clone(bool deep) const {
        struct protected_ctor_handler : public MetaStruct {};

        std::unique_ptr<MetaStruct> struct_ = std::make_unique<protected_ctor_handler>();
        struct_->m_name                     = m_name;
        struct_->m_parent                   = m_parent;

        if (deep) {
            for (auto &member : m_members) {
                auto copy =
                    std::reinterpret_pointer_cast<MetaMember, IClonable>(member->clone(true));
                struct_->m_members.push_back(copy);
            }
        } else {
            for (auto &member : m_members) {
                auto copy = std::make_shared<MetaMember>(*member);
                struct_->m_members.push_back(copy);
            }
        }

        return struct_;
    }

}  // namespace Toolbox::Object