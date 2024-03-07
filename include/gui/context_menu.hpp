#pragma once

#include "core/error.hpp"

#include <algorithm>
#include <expected>
#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "IconsForkAwesome.h"

#include <imgui.h>

#include "gui/input.hpp"
#include "gui/keybind.hpp"

namespace Toolbox::UI {

    template <typename _DataT> class ContextMenu {
    public:
        using operator_t  = std::function<void(_DataT)>;
        using condition_t = std::function<bool()>;

        struct ContextOp {
            std::string m_name;
            std::vector<int> m_keybind;
            condition_t m_condition;
            operator_t m_op;
            bool m_keybind_used = false;
        };

        ContextMenu()  = default;
        ~ContextMenu() = default;

        void addOption(std::string_view label, std::vector<int> keybind, operator_t op);
        void addOption(std::string_view label, std::initializer_list<int> keybind, operator_t op);

        void addOption(std::string_view label, std::vector<int> keybind, condition_t condition,
                       operator_t op);
        void addOption(std::string_view label, std::initializer_list<int> keybind,
                       condition_t condition, operator_t op);

        void addDivider();

        void render(std::optional<std::string> label, _DataT ctx);

    protected:
        void processKeybinds(_DataT ctx);

    private:
        std::vector<ContextOp> m_options;
        std::set<size_t> m_dividers;
    };

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(std::string_view name, std::vector<int> keybind,
                                               operator_t op) {
        ContextOp option = {.m_name      = std::string(name),
                            .m_keybind   = keybind,
                            .m_condition = []() { return true; },
                            .m_op        = op};
        m_options.push_back(option);
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(std::string_view name,
                                               std::initializer_list<int> keybind, operator_t op) {
        ContextOp option = {.m_name      = std::string(name),
                            .m_keybind   = keybind,
                            .m_condition = []() { return true; },
                            .m_op        = op};
        m_options.push_back(option);
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(std::string_view name, std::vector<int> keybind,
                                               condition_t condition, operator_t op) {
        ContextOp option = {.m_name      = std::string(name),
                            .m_keybind   = keybind,
                            .m_condition = condition,
                            .m_op        = op};
        m_options.push_back(option);
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(std::string_view name,
                                               std::initializer_list<int> keybind,
                                               condition_t condition, operator_t op) {
        ContextOp option = {.m_name      = std::string(name),
                            .m_keybind   = keybind,
                            .m_condition = condition,
                            .m_op        = op};
        m_options.push_back(option);
    }

    template <typename _DataT> inline void ContextMenu<_DataT>::addDivider() {
        m_dividers.emplace(m_options.size());
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::render(std::optional<std::string> label, _DataT ctx) {
        processKeybinds(ctx);

        if (!ImGui::BeginPopupContextItem(label ? label->c_str() : nullptr))
            return;

        for (size_t i = 0; i < m_options.size(); ++i) {
            ContextOp &option = m_options.at(i);
            if (!option.m_condition()) {
                continue;
            }

            if (m_dividers.find(i) != m_dividers.end()) {
                ImGui::Separator();
            }

            std::string keybind_name = KeyBindToString(option.m_keybind);

            std::string display_name = keybind_name.empty()
                                           ? option.m_name
                                           : std::format("{} ({})", option.m_name, keybind_name);

            if (ImGui::MenuItem(display_name.c_str())) {
                option.m_op(ctx);
            }
        }

        ImGui::EndPopup();
        return;
    }

    template <typename _DataT> inline void ContextMenu<_DataT>::processKeybinds(_DataT ctx) {
        ImGuiIO &io = ImGui::GetIO();
        for (size_t i = 0; i < m_options.size(); ++i) {
            ContextOp &option = m_options.at(i);

            bool keybind_pressed = std::all_of(option.m_keybind.begin(), option.m_keybind.end(),
                                               [io](int key) { return Input::GetKey(key); });

            if (keybind_pressed && !option.m_keybind_used) {
                option.m_keybind_used = true;
                option.m_op(ctx);
            }

            if (!keybind_pressed) {
                option.m_keybind_used = false;
            }
        }
        return;
    }

}  // namespace Toolbox::UI