#include "scene/raildata.hpp"
#include "smart_resource.hpp"
#include "rail/rail.hpp"
#include <algorithm>
#include <numeric>
#include <optional>

using namespace Toolbox::Rail;

namespace Toolbox {

    size_t RailData::getDataSize() const {
        size_t size = std::accumulate(
            m_rails.begin(), m_rails.end(), static_cast<size_t>(0),
            [](size_t size, RailData::rail_ptr_t rail) { return size + rail->getDataSize(); });
        return (size + 31) & ~31;
    }

    std::optional<size_t> RailData::getRailIndex(std::string_view name) const {
        for (size_t i = 0; i < m_rails.size(); ++i) {
            if (m_rails[i]->name() == name) {
                return i;
            }
        }
        return {};
    }

    std::optional<size_t> RailData::getRailIndex(UUID64 id) const {
        for (size_t i = 0; i < m_rails.size(); ++i) {
            if (m_rails[i]->getUUID() == id) {
                return i;
            }
        }
        return {};
    }

    RailData::rail_ptr_t RailData::getRail(size_t index) const {
        if (index >= m_rails.size())
            return nullptr;
        return m_rails[index];
    }

    RailData::rail_ptr_t RailData::getRail(std::string_view name) const {
        auto index = getRailIndex(name);
        if (!index)
            return nullptr;
        return m_rails[index.value()];
    }

    RailData::rail_ptr_t RailData::getRail(UUID64 id) const {
        auto index = getRailIndex(id);
        if (!index)
            return nullptr;
        return m_rails[index.value()];
    }

    void RailData::addRail(const Rail::Rail &rail) { insertRail(m_rails.size(), rail); }

    void RailData::insertRail(size_t index, const Rail::Rail &rail) {
        auto new_rail = make_referable<Rail::Rail>(rail);
        new_rail->setSiblingID(m_next_sibling_id++);
        auto rail_it = std::find_if(m_rails.begin(), m_rails.end(), [&](const rail_ptr_t& r) {
            return r->name() == rail.name();
        });
        size_t existing_index = rail_it - m_rails.begin();
        if (rail_it != m_rails.end()) {
            m_rails.erase(rail_it);
        }
        if (index > existing_index) {
            --index;
        }
        m_rails.insert(m_rails.begin() + index, std::move(new_rail));
    }

    void RailData::removeRail(size_t index) { m_rails.erase(m_rails.begin() + index); }
    void RailData::removeRail(std::string_view name) {
        auto index = getRailIndex(name);
        if (!index)
            return;
        m_rails.erase(m_rails.begin() + index.value());
    }
    void RailData::removeRail(UUID64 id) {
        auto index = getRailIndex(id);
        if (!index)
            return;
        m_rails.erase(m_rails.begin() + index.value());
    }

    void RailData::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        auto indention_str = std::string(indention * indention_width, ' ');
        out << indention_str << "RailData: {" << std::endl;
        for (const auto &rail : m_rails) {
            rail->dump(out, indention + 1, indention_width);
        }
        out << indention_str << "}" << std::endl;
    }

    Result<void, SerialError> RailData::serialize(Serializer &out) const {
        size_t header_start = 0;

        size_t name_start = 12 + 12 * m_rails.size();

        size_t name_size = std::accumulate(m_rails.begin(), m_rails.end(), static_cast<size_t>(0),
                                           [](size_t accum, RailData::rail_ptr_t rail) {
                                               return accum + rail->name().size() + 1;
                                           });

        size_t data_start = name_start + ((name_size + 3) & ~3);

        for (auto &rail : m_rails) {
            out.seek(header_start, std::ios::beg);
            out.write<u32, std::endian::big>(static_cast<u32>(rail->getNodeCount()));
            out.write<u32, std::endian::big>(static_cast<u32>(name_start));
            out.write<u32, std::endian::big>(static_cast<u32>(data_start));

            out.seek(name_start, std::ios::beg);
            out.writeCString(rail->name());

            out.seek(data_start, std::ios::beg);
            for (auto &node : rail->nodes()) {
                node->serialize(out);
            }

            header_start += 12;
            name_start += rail->name().size() + 1;
            data_start += 68 * rail->getNodeCount();
        }

        return {};
    }

    Result<void, SerialError> RailData::deserialize(Deserializer &in) {
        while (true) {
            size_t node_count = in.read<u32, std::endian::big>();
            size_t name_pos   = in.read<u32, std::endian::big>();
            size_t data_pos   = in.read<u32, std::endian::big>();

            if (node_count == 0 && name_pos == 0 && data_pos == 0)
                return {};

            in.seek(name_pos, std::ios::beg);
            auto name = in.readCString(128);

            auto rail = make_referable<Rail::Rail>(name);

            in.seek(data_pos, std::ios::beg);
            for (size_t i = 0; i < node_count; ++i) {
                auto node = make_referable<Rail::RailNode>();
                node->deserialize(in);
                rail->addNode(node);
            }

            m_rails.push_back(rail);
            in.seek(m_rails.size() * 12, std::ios::beg);
        }
    }

    ScopePtr<ISmartResource> RailData::clone(bool deep) const {
        std::vector<RailData::rail_ptr_t> rails;
        if (deep) {
            for (auto &rail : m_rails) {
                rails.push_back(make_deep_clone<Rail::Rail>(rail));
            }
        } else {
            for (auto &rail : m_rails) {
                rails.push_back(make_clone<Rail::Rail>(rail));
            }
        }
        return make_scoped<RailData>(rails);
    }

}  // namespace Toolbox::Scene
