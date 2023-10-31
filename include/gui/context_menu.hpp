#pragma once

#include "error.hpp"

#include <algorithm>
#include <expected>
#include <functional>
#include <set>
#include <string>
#include <string_view>

#include <imgui.h>

namespace Toolbox::UI {

    template <typename _DataT> class ContextMenu {
    public:
        using result_t   = std::expected<void, BaseError>;
        using operator_t = std::function<result_t(_DataT)>;

        ContextMenu()  = default;
        ~ContextMenu() = default;

        void addOption(std::string_view label, operator_t op);
        void addDivider();

        result_t render(std::optional<std::string> label, _DataT ctx);

    private:
        std::vector<std::pair<std::string, operator_t>> m_options;
        std::set<size_t> m_dividers;
    };

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(std::string_view label, operator_t op) {
        m_options.emplace_back(label, op);
    }

    template <typename _DataT> inline void ContextMenu<_DataT>::addDivider() {
        m_dividers.emplace(m_options.size());
    }

    template <typename _DataT>
    inline ContextMenu<_DataT>::result_t ContextMenu<_DataT>::render(std::optional<std::string> label,
                                                                     _DataT ctx) {
        if (!ImGui::BeginPopupContextItem(label ? label->c_str() : nullptr))
            return {};

        for (size_t i = 0; i < m_options.size(); ++i) {
            auto &option = m_options.at(i);
            if (m_dividers.find(i) != m_dividers.end()) {
                ImGui::Separator();
            }
            if (ImGui::MenuItem(option.first.c_str())) {
                auto result = option.second(ctx);
                if (!result) {
                    ImGui::EndPopup();
                    return std::unexpected(result.error());
                }
            }
        }

        ImGui::EndPopup();
        return {};
    }

}  // namespace Toolbox::UI