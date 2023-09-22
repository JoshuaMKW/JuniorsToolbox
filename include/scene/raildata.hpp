#pragma once

#include "boundbox.hpp"
#include "clone.hpp"
#include "objlib/meta/member.hpp"
#include "objlib/meta/value.hpp"
#include "rail/node.hpp"
#include "rail/rail.hpp"
#include "serial.hpp"
#include <optional>
#include <string>
#include <vector>

using namespace Toolbox::Rail;

namespace Toolbox::Scene {

    class RailData : public ISerializable, public IClonable {
    public:
        using rail_ptr_t = std::shared_ptr<Rail::Rail>;

        RailData()                          = default;
        RailData(const std::vector<rail_ptr_t> &rails) : m_rails(rails) {}
        RailData(const RailData &other)     = default;
        RailData(RailData &&other) noexcept = default;
        ~RailData()                         = default;

        [[nodiscard]] std::vector<rail_ptr_t> rails() const { return m_rails; }

        [[nodiscard]] size_t getDataSize() const;
        [[nodiscard]] size_t getRailCount() const { return m_rails.size(); }
        [[nodiscard]] std::optional<size_t> getRailIndex(const Rail::Rail &rail) const;
        [[nodiscard]] std::optional<size_t> getRailIndex(std::string_view name) const;
        [[nodiscard]] std::optional<rail_ptr_t> getRail(size_t index) const;
        [[nodiscard]] std::optional<rail_ptr_t> getRail(std::string_view name) const;

        void addRail(const Rail::Rail &rail);
        void insertRail(size_t index, const Rail::Rail &rail);
        void removeRail(size_t index);
        void removeRail(std::string_view name);

        [[nodiscard]] std::vector<rail_ptr_t>::const_iterator begin() const {
            return m_rails.begin();
        }
        [[nodiscard]] std::vector<rail_ptr_t>::const_iterator end() const { return m_rails.end(); }

        [[nodiscard]] std::vector<rail_ptr_t>::iterator begin() { return m_rails.begin(); }
        [[nodiscard]] std::vector<rail_ptr_t>::iterator end() { return m_rails.end(); }

        [[nodiscard]] const rail_ptr_t operator[](size_t index) const { return m_rails[index]; }
        [[nodiscard]] rail_ptr_t operator[](size_t index) { return m_rails[index]; }

        RailData &operator=(const RailData &other) = default;

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        std::unique_ptr<IClonable> clone(bool deep) const override;

    private:
        std::vector<rail_ptr_t> m_rails = {};
    };

}  // namespace Toolbox::Scene