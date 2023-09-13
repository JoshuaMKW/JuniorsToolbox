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

    MetaStruct::GetMemberT
    MetaStruct::getMember(std::string_view name) const {
        return std::weak_ptr<MetaMember>();
    }

    MetaStruct::GetMemberT MetaStruct::getMember(const QualifiedName &name) const {
        if (name.empty())
            return {};

        if (m_member_cache.contains(name))
            return m_member_cache.at(name);

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
                if (name.depth() > 1) {
                    auto member = s->getMember(QualifiedName(name.begin() + 1, name.end()));
                    if (member.has_value()) {
                        m_member_cache[name] = member.value();
                    }
                    return member;
                }
                m_member_cache[name] = s;
                return s;
            }
            if (m->formattedName(array_index) != current_scope)
                continue;
            if (name.depth() > 1)
                continue;
            m_member_cache[name] = m;
            return m;
        }
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

}  // namespace Toolbox::Object