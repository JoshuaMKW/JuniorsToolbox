#include "pad/linkdata.hpp"

using namespace Toolbox::Object;

namespace Toolbox {

    bool ReplayLinkData::hasLinkNode(char from_link, char to_link) {
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

    size_t ReplayLinkData::addLinkNode(ReplayLinkNode &&node) {
        m_link_nodes.push_back(std::move(node));
        return m_link_nodes.size() - 1;
    }

    size_t ReplayLinkData::addLinkNode(const ReplayLinkNode &node) {
        m_link_nodes.push_back(node);
        return m_link_nodes.size() - 1;
    }

    void ReplayLinkData::insertLinkNode(size_t index, ReplayLinkNode &&node) {
        m_link_nodes.insert(m_link_nodes.begin() + index, std::move(node));
    }

    void ReplayLinkData::insertLinkNode(size_t index, const ReplayLinkNode &node) {
        m_link_nodes.insert(m_link_nodes.begin() + index, node);
    }

    void ReplayLinkData::removeLinkNode(size_t index) {
        m_link_nodes.erase(m_link_nodes.begin() + index);
    }

    void ReplayLinkData::removeLinkNode(char from_link, char to_link) {
        for (size_t i = 0; i < m_link_nodes.size(); ++i) {
            ReplayLinkNode &node = m_link_nodes[i];
            for (size_t j = 0; j < 3; ++j) {
                char the_from_link = 'A' + i;
                char the_to_link   = node.m_infos[j].m_next_link;
                if (the_to_link == '*') {
                    continue;
                }
                if (the_from_link == from_link && the_to_link == to_link) {
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

    void ReplayLinkData::clearLinkNodes() { m_link_nodes.clear(); }

    void ReplayLinkData::modifyLinkNode(size_t index, char *link_a, char *link_b, char *link_c) {
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

    Result<void, SerialError> ReplayLinkData::serialize(Serializer &out) const {
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

    Result<void, SerialError> ReplayLinkData::deserialize(Deserializer &in) {
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

}  // namespace Toolbox