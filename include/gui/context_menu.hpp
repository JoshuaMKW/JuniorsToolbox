#pragma once

#include "error.hpp"

#include <algorithm>
#include <expected>
#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "IconsForkAwesome.h"

#include <imgui.h>

#include "gui/input.hpp"

namespace Toolbox::UI {

    std::string keyNameFromEnum(int key);

    template <typename _DataT> class ContextMenu {
    public:
        using result_t   = std::expected<void, BaseError>;
        using operator_t = std::function<result_t(_DataT)>;

        struct ContextOp {
            std::string m_name;
            std::vector<int> m_keybind;
            operator_t m_op;
            bool m_keybind_used = false;
        };

        ContextMenu()  = default;
        ~ContextMenu() = default;

        void addOption(std::string_view label, std::vector<int> keybind, operator_t op);
        void addOption(std::string_view label, std::initializer_list<int> keybind, operator_t op);
        void addDivider();

        result_t render(std::optional<std::string> label, _DataT ctx);

    protected:
        result_t processKeybinds(_DataT ctx);

    private:
        std::vector<ContextOp> m_options;
        std::set<size_t> m_dividers;
    };

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(std::string_view name, std::vector<int> keybind,
                                               operator_t op) {
        ContextOp option = {.m_name = std::string(name), .m_keybind = keybind, .m_op = op};
        m_options.push_back(option);
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(std::string_view name,
                                               std::initializer_list<int> keybind, operator_t op) {
        ContextOp option = {.m_name = std::string(name), .m_keybind = keybind, .m_op = op};
        m_options.push_back(option);
    }

    template <typename _DataT> inline void ContextMenu<_DataT>::addDivider() {
        m_dividers.emplace(m_options.size());
    }

    template <typename _DataT>
    inline ContextMenu<_DataT>::result_t
    ContextMenu<_DataT>::render(std::optional<std::string> label, _DataT ctx) {
        processKeybinds(ctx);

        if (!ImGui::BeginPopupContextItem(label ? label->c_str() : nullptr))
            return {};

        for (size_t i = 0; i < m_options.size(); ++i) {
            ContextOp &option = m_options.at(i);
            if (m_dividers.find(i) != m_dividers.end()) {
                ImGui::Separator();
            }

            std::string keybind_name = "";
            for (size_t j = 0; j < option.m_keybind.size(); ++j) {
                int key = option.m_keybind.at(j);
                if ((key & ImGuiMod_Alt)) {
                    keybind_name += "Alt";
                } else if ((key & ImGuiMod_Ctrl)) {
                    keybind_name += "Ctrl";
                } else if ((key & ImGuiMod_Shift)) {
                    keybind_name += "Shift";
                } else if ((key & ImGuiMod_Super)) {
                    keybind_name += "Super";
                } else {
                    keybind_name += keyNameFromEnum(key);
                }
                if (j < option.m_keybind.size() - 1) {
                    keybind_name += "+";
                }
            }

            std::string display_name = keybind_name.empty()
                                           ? option.m_name
                                           : std::format("{} ({})", option.m_name, keybind_name);

            if (ImGui::MenuItem(display_name.c_str())) {
                auto result = option.m_op(ctx);
                if (!result) {
                    ImGui::EndPopup();
                    return std::unexpected(result.error());
                }
            }
        }

        ImGui::EndPopup();
        return {};
    }

    template <typename _DataT>
    inline ContextMenu<_DataT>::result_t ContextMenu<_DataT>::processKeybinds(_DataT ctx) {
        ImGuiIO &io = ImGui::GetIO();
        for (size_t i = 0; i < m_options.size(); ++i) {
            ContextOp &option = m_options.at(i);

            bool keybind_pressed = std::all_of(option.m_keybind.begin(), option.m_keybind.end(),
                                               [io](int key) { return Input::GetKey(key); });

            if (keybind_pressed && !option.m_keybind_used) {
                option.m_keybind_used = true;
                return option.m_op(ctx);
            }

            if (!keybind_pressed) {
                option.m_keybind_used = false;
            }
        }
        return {};
    }

}  // namespace Toolbox::UI