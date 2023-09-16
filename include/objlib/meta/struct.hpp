#pragma once

#include "clone.hpp"
#include "error.hpp"
#include "objlib/qualname.hpp"
#include <expected>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Toolbox::Object {

    class MetaMember;

    class MetaStruct : public IClonable {
    public:
        using MemberT = std::variant<std::shared_ptr<MetaMember>, std::shared_ptr<MetaStruct>>;
        using GetMemberT = std::expected<MemberT, MetaScopeError>;
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

        [[nodiscard]] constexpr std::vector<std::shared_ptr<MetaMember>> members() const {
            std::vector<std::shared_ptr<MetaMember>> ret;
            ret.reserve(m_members.size());
            for (auto &member : m_members) {
                ret.emplace_back(member);
            }
            return ret;
        }

        [[nodiscard]] GetMemberT getMember(std::string_view name) const;
        [[nodiscard]] GetMemberT getMember(const QualifiedName &name) const;

        [[nodiscard]] constexpr MetaStruct *parent() const { return m_parent; }

        [[nodiscard]] constexpr QualifiedName getQualifiedName() const;

        void dump(std::ostream &out, size_t indention, size_t indention_width,
                  bool naked = false) const;
        void dump(std::ostream &out, size_t indention, bool naked = false) const {
            dump(out, indention, 2, naked);
        }
        void dump(std::ostream &out) const { dump(out, 0, 2, false); }

        bool operator==(const MetaStruct &other) const;

        std::unique_ptr<IClonable> clone(bool deep) const override;

    private:
        std::string m_name;
        std::vector<std::shared_ptr<MetaMember>> m_members = {};
        MetaStruct *m_parent                               = nullptr;

        mutable CacheMemberT m_member_cache;
    };

}  // namespace Toolbox::Object