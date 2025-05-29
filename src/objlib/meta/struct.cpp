#include "objlib/meta/struct.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/errors.hpp"
#include "objlib/template.hpp"
#include "objlib/transform.hpp"
#include "smart_resource.hpp"
#include <expected>
#include <optional>

namespace Toolbox::Object {
    MetaStruct::MetaStruct(std::string_view name, std::vector<MetaMember> members) : m_name(name) {
        for (const auto &m : members) {
            auto p = make_referable<MetaMember>(m);
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
                auto s      = m->value<MetaStruct>(array_index).value();
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

    size_t MetaStruct::computeSize() const {
        return std::accumulate(
            m_members.begin(), m_members.end(), (size_t)0,
            [](size_t value, RefPtr<MetaMember> m) { return value + m->computeSize(); });
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

    bool MetaStruct::operator==(const MetaStruct &other) const {
        return m_name == other.m_name && m_members == other.m_members && m_parent == other.m_parent;
    }

    Result<void, SerialError> MetaStruct::serialize(Serializer &out) const {
        for (auto m : m_members) {
            auto result = m->serialize(out);
            if (!result) {
                return std::unexpected(result.error());
            }
        }
        return {};
    }

    Result<void, SerialError> MetaStruct::deserialize(Deserializer &in) {
        for (auto m : m_members) {
            auto result = m->deserialize(in);
            if (!result) {
                return std::unexpected(result.error());
            }
        }
        return {};
    }

    ScopePtr<ISmartResource> MetaStruct::clone(bool deep) const {
        MetaStruct struct_;
        struct_.m_name   = m_name;
        struct_.m_parent = m_parent;

        if (deep) {
            for (auto &member : m_members) {
                RefPtr<MetaMember> copy = ref_cast<MetaMember>(make_deep_clone<MetaMember>(member));
                struct_.m_members.push_back(copy);
            }
        } else {
            for (auto &member : m_members) {
                RefPtr<MetaMember> copy = ref_cast<MetaMember>(make_clone<MetaMember>(member));
                struct_.m_members.push_back(copy);
            }
        }

        return make_scoped<MetaStruct>(struct_);
    }

}  // namespace Toolbox::Object