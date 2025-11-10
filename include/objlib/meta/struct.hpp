#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "core/memory.hpp"
#include "errors.hpp"
#include "objlib/qualname.hpp"
#include "gameio.hpp"
#include "smart_resource.hpp"

namespace Toolbox::Object {

    class MetaMember;

    class MetaStruct : public IGameSerializable, public ISmartResource {
    public:
        using MemberT      = RefPtr<MetaMember>;
        using GetMemberT   = Result<MemberT, MetaScopeError>;
        using CacheMemberT = std::unordered_map<std::string, MemberT>;

        MetaStruct(std::string_view name) : m_name(name) {}
        MetaStruct(std::string_view name, std::vector<MetaMember> members);
        MetaStruct(const MetaStruct &other) = default;
        MetaStruct(MetaStruct &&other)      = default;
        ~MetaStruct()                       = default;

    protected:
        MetaStruct() = default;

    public:
        [[nodiscard]] constexpr std::string_view name() const { return m_name; }

        [[nodiscard]] constexpr std::vector<MemberT> members() const { return m_members; }

        [[nodiscard]] GetMemberT getMember(std::string_view name) const;
        [[nodiscard]] GetMemberT getMember(const QualifiedName &name) const;

        [[nodiscard]] constexpr MetaStruct *parent() const { return m_parent; }

        [[nodiscard]] constexpr QualifiedName getQualifiedName() const;

        [[nodiscard]] size_t computeSize() const;

        void dump(std::ostream &out, size_t indention, size_t indention_width,
                  bool naked = false) const;
        void dump(std::ostream &out, size_t indention, bool naked = false) const {
            dump(out, indention, 2, naked);
        }
        void dump(std::ostream &out) const { dump(out, 0, 2, false); }

        bool operator==(const MetaStruct &other) const;

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        Result<void, SerialError> gameSerialize(Serializer &out) const override;
        Result<void, SerialError> gameDeserialize(Deserializer &in) override;

        ScopePtr<ISmartResource> clone(bool deep) const override;

    private:
        std::string m_name;
        std::vector<MemberT> m_members = {};
        MetaStruct *m_parent           = nullptr;

        mutable CacheMemberT m_member_cache;
    };

}  // namespace Toolbox::Object