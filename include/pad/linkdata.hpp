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
        NameRef m_link_name;
        NameRef m_node_name;
        ReplayNodeInfo m_infos[3];
    };

    class ReplayLinkData : public ISerializable {
    public:
        ReplayLinkData()  = default;
        ~ReplayLinkData() = default;

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

        [[nodiscard]] bool hasLinkNode(char from_link, char to_link) {
            for (size_t i = 0; i < m_link_nodes.size(); ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    char from_link = 'A' + i;
                    char to_link   = m_link_nodes[i].m_infos[j].m_next_link;
                    if (to_link == '*') {
                        continue;
                    }
                    if (from_link == from_link && to_link == to_link) {
                        return true;
                    }
                }
            }
            return false;
        }

        size_t addLinkNode(ReplayLinkNode &&node) {
            m_link_nodes.push_back(std::move(node));
            return m_link_nodes.size() - 1;
        }
        size_t addLinkNode(const ReplayLinkNode &node) {
            m_link_nodes.push_back(node);
            return m_link_nodes.size() - 1;
        }

        void insertLinkNode(size_t index, ReplayLinkNode &&node) {
            m_link_nodes.insert(m_link_nodes.begin() + index, std::move(node));
        }
        void insertLinkNode(size_t index, const ReplayLinkNode &node) {
            m_link_nodes.insert(m_link_nodes.begin() + index, node);
        }

        void removeLinkNode(size_t index) { m_link_nodes.erase(m_link_nodes.begin() + index); }
        void removeLinkNode(char from_link, char to_link) {
            for (size_t i = 0; i < m_link_nodes.size(); ++i) {
                ReplayLinkNode &node = m_link_nodes[i];
                for (size_t j = 0; j < 3; ++j) {
                    char from_link = 'A' + i;
                    char to_link   = node.m_infos[j].m_next_link;
                    if (to_link == '*') {
                        continue;
                    }
                    if (from_link == from_link && to_link == to_link) {
                        node.m_infos[j].m_next_link = '*';
                        if (node.m_infos[0].isSentinelNode() && node.m_infos[1].isSentinelNode() &&
                            node.m_infos[2].isSentinelNode()) {
                            removeLinkNode(i);
                        }
                        return;
                    }
                }
            }
        }

        void clearLinkNodes() { m_link_nodes.clear(); }

        void modifyLinkNode(size_t index, char *link_a, char *link_b, char *link_c) {
            if (link_a) {
                m_link_nodes[index].m_infos[0].m_next_link = *link_a;
            }
            if (link_b) {
                m_link_nodes[index].m_infos[1].m_next_link = *link_b;
            }
            if (link_c) {
                m_link_nodes[index].m_infos[2].m_next_link = *link_c;
            }
        }

        Result<void, SerialError> serialize(Serializer &out) const override {
            out.pushBreakpoint();
            out.write<u32, std::endian::big>(0);

            m_replay_link_name.serialize(out);
            m_replay_scene_name.serialize(out);

            out.write<u32, std::endian::big>(static_cast<u32>(m_link_nodes.size()));

            for (const ReplayLinkNode &node : m_link_nodes) {
                out.write<u32>(0x15 + node.m_link_name.name().length() +
                               node.m_node_name.name().length());
                node.m_link_name.serialize(out);
                node.m_node_name.serialize(out);
                for (size_t i = 0; i < 3; ++i) {
                    out.write<u16, std::endian::big>(node.m_infos[i].m_unk_0);
                    out.write<u8, std::endian::big>(node.m_infos[i].m_next_link);
                }
            }

            std::streampos end = out.tell();
            out.popBreakpoint();
            std::streampos beg = out.tell();

            out.write<u32, std::endian::big>(static_cast<u32>(end - beg));

            out.seek(0, std::ios::end);

            return {};
        }

        Result<void, SerialError> deserialize(Deserializer &in) override {
            u32 rel_start = in.tell();
            u32 file_size = in.read<u32, std::endian::big>();

            m_replay_link_name.deserialize(in);
            m_replay_scene_name.deserialize(in);

            u32 node_count = in.read<u32, std::endian::big>();

            for (u32 i = 0; i < node_count; ++i) {
                ReplayLinkNode link_node;

                u32 link_size = in.read<u32, std::endian::big>();

                link_node.m_link_name.deserialize(in);
                link_node.m_node_name.deserialize(in);
                for (size_t i = 0; i < 3; ++i) {
                    link_node.m_infos[i].m_unk_0     = in.read<u16, std::endian::big>();
                    link_node.m_infos[i].m_next_link = in.read<u8, std::endian::big>();
                }

                m_link_nodes.push_back(std::move(link_node));
            }

            return {};
        }

    private:
        NameRef m_replay_link_name;
        NameRef m_replay_scene_name;
        std::vector<ReplayLinkNode> m_link_nodes;
    };

}  // namespace Toolbox