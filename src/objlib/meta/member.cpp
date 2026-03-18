#include "objlib/meta/member.hpp"
#include "objlib/meta/enum.hpp"
#include "objlib/meta/struct.hpp"
#include "objlib/template.hpp"
#include "objlib/transform.hpp"
#include <algorithm>
#include <optional>

namespace Toolbox::Object {

    QualifiedName MetaMember::qualifiedName() const {
        MetaStruct *parent              = m_parent;
        std::vector<std::string> scopes = {m_name};
        while (parent) {
            scopes.push_back(std::string(parent->name()));
            parent = parent->parent();
        }
        std::reverse(scopes.begin(), scopes.end());
        return QualifiedName(scopes);
    }

    bool MetaMember::operator==(const MetaMember &other) const {
        return m_name == other.m_name && m_values == other.m_values &&
               m_arraysize == other.m_arraysize && m_parent == other.m_parent;
    }

    void MetaMember::updateReferenceToList(const std::vector<RefPtr<MetaMember>> &list) {
        if (!std::holds_alternative<ReferenceInfo>(m_arraysize))
            return;

        ReferenceInfo reference = std::get<ReferenceInfo>(m_arraysize);
        auto reference_it =
            std::find_if(list.begin(), list.end(), [&reference](const RefPtr<MetaMember> &m) {
                return m->name() == reference.m_name;
            });
        if (reference_it != list.end()) {
            reference.m_ref = (*reference_it)->value<MetaValue>(0).value();
            m_arraysize     = reference;
        }
    }

    void MetaMember::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        indention_width   = std::min(indention_width, size_t(8));
        auto self_indent  = std::string(indention * indention_width, ' ');
        auto value_indent = std::string((indention + 1) * indention_width, ' ');
        if (!isArray()) {
            if (isEmpty())
                return;

            if (isTypeStruct()) {
                auto _struct = std::get<RefPtr<MetaStruct>>(m_values[0]);
                out << self_indent << _struct->name() << " " << m_name << " ";
                _struct->dump(out, indention, indention_width, true);
                out << ";\n";
            } else if (isTypeEnum()) {
                auto _enum = std::get<RefPtr<MetaEnum>>(m_values[0]);
                out << self_indent << _enum->name() << " " << m_name << " = "
                    << _enum->value()->toString() << ";\n";
            } else if (isTypeValue()) {
                auto _value = std::get<RefPtr<MetaValue>>(m_values[0]);
                out << self_indent << meta_type_name(_value->type()) << " " << m_name << " = "
                    << _value->toString() << ";\n";
            }
            return;
        }

        if (isTypeStruct()) {
            out << self_indent << std::get<RefPtr<MetaStruct>>(m_values[0])->name() << " " << m_name
                << " = [\n";
            for (const auto &v : m_values) {
                auto _struct = std::get<RefPtr<MetaStruct>>(v);
                std::cout << value_indent;
                std::get<RefPtr<MetaStruct>>(v)->dump(out, indention + 1, indention_width, true);
                std::cout << ",\n";
            }
            out << self_indent << "];\n";
        } else if (isTypeEnum()) {
            out << self_indent << std::get<RefPtr<MetaEnum>>(m_values[0])->name() << " " << m_name
                << " = [\n";
            for (const auto &v : m_values) {
                auto _enum = std::get<RefPtr<MetaEnum>>(v);
                out << _enum->value()->toString() << ",\n";
            }
            out << self_indent << "];\n";
        } else if (isTypeValue()) {
            out << self_indent << meta_type_name(std::get<RefPtr<MetaValue>>(m_values[0])->type())
                << m_name << " = [\n";
            for (const auto &v : m_values) {
                auto _value = std::get<RefPtr<MetaValue>>(v);
                out << value_indent << meta_type_name(_value->type()) << m_name << " = "
                    << std::get<RefPtr<MetaValue>>(v)->toString() << ",\n";
            }
            out << self_indent << "];\n";
        }
    }

    Result<void, SerialError> MetaMember::serialize(Serializer &out) const {
        return gameSerialize(out);
    }

    Result<void, SerialError> MetaMember::deserialize(Deserializer &in) {
        return gameDeserialize(in);
    }

    Result<void, SerialError> MetaMember::gameSerialize(Serializer &out) const {
        for (u32 i = 0; i < arraysize(); ++i) {
            if (isTypeStruct()) {
                auto _struct = std::get<RefPtr<MetaStruct>>(m_values[i]);
                auto result  = _struct->gameSerialize(out);
                if (!result) {
                    return std::unexpected(result.error());
                }
            } else if (isTypeEnum()) {
                auto _enum  = std::get<RefPtr<MetaEnum>>(m_values[i]);
                auto result = _enum->gameSerialize(out);
                if (!result) {
                    return std::unexpected(result.error());
                }
            } else if (isTypeValue()) {
                auto _value = std::get<RefPtr<MetaValue>>(m_values[i]);
                auto result = _value->gameSerialize(out);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
        }
        return {};
    }

    Result<void, SerialError> MetaMember::gameDeserialize(Deserializer &in) {
        syncArray();
        for (u32 i = 0; i < arraysize(); ++i) {
            if (isTypeStruct()) {
                auto _struct = std::get<RefPtr<MetaStruct>>(m_values[i]);
                auto result  = _struct->gameDeserialize(in);
                if (!result) {
                    return std::unexpected(result.error());
                }
            } else if (isTypeEnum()) {
                auto _enum  = std::get<RefPtr<MetaEnum>>(m_values[i]);
                auto result = _enum->gameDeserialize(in);
                if (!result) {
                    return std::unexpected(result.error());
                }
            } else if (isTypeValue()) {
                auto _value = std::get<RefPtr<MetaValue>>(m_values[i]);
                auto result = _value->gameDeserialize(in);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }
        }
        return {};
    }

    ScopePtr<ISmartResource> MetaMember::clone(bool deep) const {
        MetaMember member;
        member.m_name      = m_name;
        member.m_parent    = m_parent;
        member.m_arraysize = m_arraysize;
        member.m_default   = m_default;

        if (deep) {
            for (auto &value : m_values) {
                if (isTypeStruct()) {
                    member.m_values.push_back(
                        make_deep_clone<MetaStruct>(std::get<RefPtr<MetaStruct>>(value)));
                } else if (isTypeEnum()) {
                    member.m_values.push_back(
                        make_deep_clone<MetaEnum>(std::get<RefPtr<MetaEnum>>(value)));
                } else {
                    auto _copy = make_referable<MetaValue>(*std::get<RefPtr<MetaValue>>(value));
                    member.m_values.push_back(_copy);
                }
            }
        } else {
            for (auto &value : m_values) {
                if (isTypeStruct()) {
                    member.m_values.push_back(
                        make_clone<MetaStruct>(std::get<RefPtr<MetaStruct>>(value)));
                } else if (isTypeEnum()) {
                    member.m_values.push_back(
                        make_clone<MetaEnum>(std::get<RefPtr<MetaEnum>>(value)));
                } else {
                    auto _copy = make_referable<MetaValue>(*std::get<RefPtr<MetaValue>>(value));
                    member.m_values.push_back(_copy);
                }
            }
        }

        return make_scoped<MetaMember>(member);
    }

}  // namespace Toolbox::Object
