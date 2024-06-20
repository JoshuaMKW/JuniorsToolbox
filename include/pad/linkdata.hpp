#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "core/types.hpp"
#include "objlib/nameref.hpp"
#include "serial.hpp"

using namespace Toolbox::Object;

namespace Toolbox {

    struct ReplayNodeInfo {
        u16 m_unk_0;
        u8 m_next_link;

        [[nodiscard]] bool isSentinelNode() const noexcept { return m_next_link == '*'; }
    };

    struct ReplayLinkNode {
        NameRef m_link_name = {"Link"};
        NameRef m_node_name = {"(null)"};
        ReplayNodeInfo m_infos[3];
    };

    class ReplayLinkData : public ISerializable {
    public:
        ReplayLinkData()  = default;
        ~ReplayLinkData() = default;

        [[nodiscard]] std::vector<ReplayLinkNode> &linkNodes() noexcept {
            return m_link_nodes;
        }

        [[nodiscard]] const std::vector<ReplayLinkNode> &linkNodes() const noexcept {
            return m_link_nodes;
        }

        [[nodiscard]] std::string_view getReplayLinkName() const noexcept {
            return m_replay_link_name.name();
        }

        [[nodiscard]] std::string_view getReplaySceneName() const noexcept {
            return m_replay_scene_name.name();
        }

        void setReplayLinkName(std::string_view name) { m_replay_link_name.setName(name); }
        void setReplaySceneName(std::string_view name) { m_replay_scene_name.setName(name); }

        [[nodiscard]] bool hasLinkNode(char from_link, char to_link);

        size_t addLinkNode(ReplayLinkNode &&node);
        size_t addLinkNode(const ReplayLinkNode &node);

        void insertLinkNode(size_t index, ReplayLinkNode &&node);
        void insertLinkNode(size_t index, const ReplayLinkNode &node);

        void removeLinkNode(size_t index);
        void removeLinkNode(char from_link, char to_link);

        void clearLinkNodes();

        void modifyLinkNode(size_t index, char *link_a, char *link_b, char *link_c);

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

    private:
        NameRef m_replay_link_name;
        NameRef m_replay_scene_name;
        std::vector<ReplayLinkNode> m_link_nodes = {};
    };

}  // namespace Toolbox