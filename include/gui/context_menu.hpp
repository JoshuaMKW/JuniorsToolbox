#pragma once

#include "core/error.hpp"

#include <algorithm>
#include <expected>
#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "IconsForkAwesome.h"

#include "imgui_ext.hpp"
#include <imgui.h>

#include "core/input/input.hpp"
#include "core/keybind/keybind.hpp"
#include "gui/font.hpp"

using namespace Toolbox;

namespace Toolbox::UI {

    namespace {

        template <typename _DataT> struct ContextEntry;
        template <typename _DataT> struct ContextOp;
        template <typename _DataT> struct ContextGroup;

        template <typename _DataT> struct ContextEntry {
            enum Type {
                TYPE_OP,
                TYPE_GROUP,
                TYPE_DIV,
            };

            union {
                ContextOp<_DataT> *m_op;
                ContextGroup<_DataT> *m_group;
            };

            Type m_type;
        };

        template <typename _DataT> struct ContextOp {
            using operator_t   = std::function<void(_DataT)>;
            using condition_t  = std::function<bool(_DataT)>;
            using open_event_t = std::function<void(_DataT)>;

            std::string m_name;
            KeyBind m_keybind;
            condition_t m_condition;
            operator_t m_op;
            bool m_keybind_used = false;
        };

        template <typename _DataT> struct ContextGroup {
            std::string m_name;
            std::vector<ContextEntry<_DataT>> m_ops;
            std::set<size_t> m_dividers;
        };

    }  // namespace

    template <typename _DataT> class ContextMenu {
    public:
        using context_t = ContextEntry<_DataT>;
        using group_t   = ContextGroup<_DataT>;
        using option_t  = ContextOp<_DataT>;

        ContextMenu()  = default;
        ~ContextMenu() = default;

        group_t *addGroup(group_t *parent_group, std::string_view label);

        void addOption(group_t *parent_group, std::string_view label, const KeyBind &keybind,
                       option_t::operator_t op);

        void addOption(group_t *parent_group, std::string_view label, const KeyBind &keybind,
                       option_t::condition_t condition, option_t::operator_t op);

        void addDivider(group_t *parent_group);

        const ImRect &rect() const { return m_rect; }
        bool isOpen() const { return m_was_open; }
        void setOpenRef(ImGuiID id) { m_hovered_id = id; }

        const std::string &getLabel() const { return m_label; }
        void setLabel(const std::string &label) { m_label = label; }

        void setCanOpen(bool can_open) { m_can_open = can_open; }

        void tryOpen(ImGuiID item_id,
                     ImGuiPopupFlags popup_flags = ImGuiPopupFlags_MouseButtonRight);
        void tryRender(const _DataT &ctx,
                       ImGuiHoveredFlags hover_flags = ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        void applyDeferredCmds();

        void
        renderForCond(std::optional<std::string> label, const _DataT &ctx, bool condition,
                      ImGuiHoveredFlags hover_flags = ImGuiHoveredFlags_AllowWhenBlockedByPopup);

        void
        renderForItem(std::optional<std::string> label, const _DataT &ctx,
                      ImGuiHoveredFlags hover_flags = ImGuiHoveredFlags_AllowWhenBlockedByPopup);

        void
        renderForRect(std::optional<std::string> label, const ImRect &rect, const _DataT &ctx,
                      ImGuiHoveredFlags hover_flags = ImGuiHoveredFlags_AllowWhenBlockedByPopup);

        void onOpen(option_t::open_event_t open) { m_open_event = open; }

    protected:
        bool renderGroup(const group_t &group, const _DataT &ctx, bool allows_hovered);
        bool renderOption(const option_t &option, const _DataT &ctx);
        void processKeybinds(const _DataT &ctx);
        bool processKeybindsGroup(group_t &group, const _DataT &ctx);
        bool processKeybindsOption(option_t &option, const _DataT &ctx);

    private:
        std::string m_label;
        ImGuiID m_id = 0;

        group_t m_root_group;
        option_t::open_event_t m_open_event;
        bool m_was_open = false;
        bool m_can_open = true;
        ImRect m_rect   = {};

        float m_delta_hovered = 0.0f;
        ImGuiID m_hovered_id  = 0;

        _DataT m_deferred_ctx;
        std::vector<typename option_t::operator_t> m_deferred_cmds;

        std::vector<option_t> m_matched_keybinds;
    };

    template <typename _DataT>
    inline ContextMenu<_DataT>::group_t *ContextMenu<_DataT>::addGroup(group_t *parent_group,
                                                                       std::string_view label) {
        if (!parent_group) {
            parent_group = &m_root_group;
        }

        group_t *group = new group_t;
        group->m_name  = label;

        context_t entry = {};
        entry.m_type    = context_t::TYPE_GROUP;
        entry.m_group   = group;
        parent_group->m_ops.emplace_back(entry);

        return group;
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(group_t *parent_group, std::string_view name,
                                               const KeyBind &keybind, option_t::operator_t op) {
        if (parent_group == nullptr) {
            parent_group = &m_root_group;
        }
        option_t *option = new option_t{.m_name      = std::string(name),
                                        .m_keybind   = keybind,
                                        .m_condition = [](_DataT) { return true; },
                                        .m_op        = op};
        context_t entry  = {};
        entry.m_type     = context_t::TYPE_OP;
        entry.m_op       = option;
        parent_group->m_ops.emplace_back(entry);
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::addOption(group_t *parent_group, std::string_view name,
                                               const KeyBind &keybind,
                                               option_t::condition_t condition,
                                               option_t::operator_t op) {
        if (parent_group == nullptr) {
            parent_group = &m_root_group;
        }
        option_t *option = new option_t{.m_name      = std::string(name),
                                        .m_keybind   = keybind,
                                        .m_condition = condition,
                                        .m_op        = op};
        context_t entry  = {};
        entry.m_type     = context_t::TYPE_OP;
        entry.m_op       = option;
        parent_group->m_ops.emplace_back(entry);
    }

    template <typename _DataT> inline void ContextMenu<_DataT>::addDivider(group_t *parent_group) {
        if (parent_group == nullptr) {
            parent_group = &m_root_group;
        }

        context_t entry = {};
        entry.m_type    = context_t::TYPE_DIV;
        parent_group->m_ops.emplace_back(entry);
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::tryOpen(ImGuiID item_id, ImGuiPopupFlags popup_flags) {
        if (!m_can_open || m_was_open) {
            return;
        }

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        // if (window->SkipItems)
        //     return;
        ImGuiID id = window->GetID(m_label.c_str());
        IM_ASSERT(id != 0);  // You cannot pass a NULL str_id if the last item has no identifier
                             // (e.g. a Text() item)
        int mouse_button = (popup_flags & ImGuiPopupFlags_MouseButtonMask_);
        if (ImGui::IsMouseReleased(mouse_button)) {
            ImGui::OpenPopupEx(id, popup_flags);
            m_id = id;
        }
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::tryRender(const _DataT &ctx, ImGuiHoveredFlags hover_flags) {
        if (!m_can_open && !m_was_open) {
            return;
        }

        m_deferred_ctx = ctx;
        processKeybinds(ctx);

        // Since windows can push their own font, we temporarily restore the
        // desired font.
        FontManager &font_manager = FontManager::instance();
        ImFont *font              = font_manager.getCurrentFont();
        ImGui::PushFont(font, font_manager.getCurrentFontSize());

        // Set up the context menu style
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 5.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f, 8.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8.0f, 8.0f});

        ImGuiWindow *window = ImGui::GetCurrentWindow();
        if (!ImGui::BeginFlatPopupEx(m_id, ImGuiWindowFlags_AlwaysAutoResize |
                                               ImGuiWindowFlags_NoTitleBar |
                                               ImGuiWindowFlags_NoSavedSettings)) {
            m_was_open = false;
            m_rect     = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});
            m_id       = 0;
            ImGui::PopStyleVar(3);
            ImGui::PopFont();
            return;
        }

        if (!m_can_open) {
            m_was_open = false;
            m_rect     = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});

            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            ImGui::PopStyleVar(3);
            ImGui::PopFont();
            return;
        }

        m_rect.Min = ImGui::GetWindowPos();
        m_rect.Max = m_rect.Min + ImGui::GetWindowSize();

        if (!m_was_open && m_open_event) {
            m_open_event(ctx);
        }
        m_was_open = true;

        renderGroup(m_root_group, ctx, true);

        ImGui::EndPopup();
        ImGui::PopStyleVar(3);
        ImGui::PopFont();
        return;
    }

    template <typename _DataT> inline void ContextMenu<_DataT>::applyDeferredCmds() {
        for (typename option_t::operator_t &cmd : m_deferred_cmds) {
            cmd(m_deferred_ctx);
        }
        m_deferred_cmds.clear();
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::renderForCond(std::optional<std::string> label,
                                                   const _DataT &ctx, bool condition,
                                                   ImGuiHoveredFlags hover_flags) {
        if (!m_can_open && !m_was_open) {
            return;
        }

        processKeybinds(ctx);

        if (!ImGui::BeginPopupContextConditional(label ? label->c_str() : nullptr, 1, hover_flags,
                                                 condition)) {
            m_was_open = false;
            m_rect     = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});
            return;
        }

        if (!m_can_open) {
            m_was_open = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            m_rect = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});
            return;
        }

        m_rect.Min = ImGui::GetWindowPos();
        m_rect.Max = m_rect.Min + ImGui::GetWindowSize();

        if (!m_was_open && m_open_event) {
            m_open_event(ctx);
        }
        m_was_open = true;

        renderGroup(m_root_group, ctx, true);

        ImGui::EndPopup();
        return;
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::renderForItem(std::optional<std::string> label,
                                                   const _DataT &ctx,
                                                   ImGuiHoveredFlags hover_flags) {
        if (!m_can_open && !m_was_open) {
            return;
        }

        processKeybinds(ctx);

        if (m_hovered_id != 0) {
            if (!ImGui::BeginPopupContextItem(m_hovered_id, 1, hover_flags)) {
                m_was_open = false;
                m_rect     = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});
                return;
            }
        } else {
            if (!ImGui::BeginPopupContextItem(label ? label->c_str() : nullptr, 1, hover_flags)) {
                m_was_open = false;
                m_rect     = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});
                return;
            }
        }

        if (!m_can_open) {
            m_was_open = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            m_rect = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});
            return;
        }

        m_rect.Min = ImGui::GetWindowPos();
        m_rect.Max = m_rect.Min + ImGui::GetWindowSize();

        if (!m_was_open && m_open_event) {
            m_open_event(ctx);
        }
        m_was_open = true;

        renderGroup(m_root_group, ctx, true);

        ImGui::EndPopup();
        return;
    }

    template <typename _DataT>
    inline void ContextMenu<_DataT>::renderForRect(std::optional<std::string> label,
                                                   const ImRect &rect, const _DataT &ctx,
                                                   ImGuiHoveredFlags hover_flags) {
        if (!m_can_open && !m_was_open) {
            return;
        }

        processKeybinds(ctx);

        if (!ImGui::BeginPopupContextForRect(label ? label->c_str() : nullptr, rect, 1,
                                             hover_flags)) {
            m_was_open = true;
            m_rect     = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});
            return;
        }

        if (!m_can_open) {
            m_was_open = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            m_rect = ImRect({-1.0f, -1.0f}, {-1.0f, -1.0f});
            return;
        }

        m_rect.Min = ImGui::GetWindowPos();
        m_rect.Max = m_rect.Min + ImGui::GetWindowSize();

        if (!m_was_open && m_open_event) {
            m_open_event(ctx);
        }
        m_was_open = true;

        renderGroup(m_root_group, ctx, true);

        ImGui::EndPopup();
        return;
    }

    template <typename _DataT>
    inline bool ContextMenu<_DataT>::renderGroup(const group_t &group, const _DataT &ctx,
                                                 bool is_root) {
        bool last_was_sep = false;
        int64_t last_item = -1;

        if (is_root) {
            for (size_t i = 0; i < group.m_ops.size(); ++i) {
                const context_t &entry = group.m_ops.at(i);
                if (entry.m_type == context_t::TYPE_GROUP) {
                    if (renderGroup(*entry.m_group, ctx, false)) {
                        return true;
                    }
                    last_item    = i;
                    last_was_sep = false;
                } else if (entry.m_type == context_t::TYPE_OP) {
                    if (renderOption(*entry.m_op, ctx)) {
                        return true;
                    }
                    last_item    = i;
                    last_was_sep = false;
                } else if (!last_was_sep && 0 <= last_item && i < group.m_ops.size() - 1) {
                    ImGui::Separator();
                    last_was_sep = true;
                }
            }
            return false;
        }

        if (ImGui::BeginMenu(group.m_name.c_str(), true)) {
            for (size_t i = 0; i < group.m_ops.size(); ++i) {
                const context_t &entry = group.m_ops.at(i);
                if (entry.m_type == context_t::TYPE_GROUP) {
                    if (renderGroup(*entry.m_group, ctx, false)) {
                        return true;
                    }
                    last_item    = i;
                    last_was_sep = false;
                } else if (entry.m_type == context_t::TYPE_OP) {
                    if (renderOption(*entry.m_op, ctx)) {
                        return true;
                    }
                    last_item    = i;
                    last_was_sep = false;
                } else if (!last_was_sep && 0 <= last_item && i < group.m_ops.size() - 1) {
                    ImGui::Separator();
                    last_was_sep = true;
                }
            }
            ImGui::EndMenu();
        }
        return false;
    }

    template <typename _DataT>
    inline bool ContextMenu<_DataT>::renderOption(const option_t &option, const _DataT &ctx) {
        const bool is_valid_state = option.m_condition(ctx);

        if (!is_valid_state) {
            ImGui::BeginDisabled();
        }

        std::string keybind_name = option.m_keybind.toString();

        std::string display_name = keybind_name.empty()
                                       ? option.m_name
                                       : std::format("{} ({})", option.m_name, keybind_name);

        bool clicked = false;
        if (ImGui::MenuItem(display_name.c_str())) {
            m_deferred_cmds.emplace_back(option.m_op);
            clicked = true;
        }

        if (!is_valid_state) {
            ImGui::EndDisabled();
        }

        return clicked;
    }

    template <typename _DataT> inline void ContextMenu<_DataT>::processKeybinds(const _DataT &ctx) {
        processKeybindsGroup(m_root_group, ctx);

        // Discard similar command bindings when one is already digested
        // ---
        // Example: CTRL+A -> CTRL+SHIFT+A
        // In this case the second command should be discarded
        // because shift is a soft differentiation.
        const bool already_digested =
            std::any_of(m_matched_keybinds.begin(), m_matched_keybinds.end(),
                        [](const option_t &matched) { return matched.m_keybind_used; });
        if (!already_digested && !m_matched_keybinds.empty()) {
            // Do a reverse sort
            std::stable_sort(m_matched_keybinds.begin(), m_matched_keybinds.end(),
                             [](const option_t &l, const option_t &r) {
                                 return l.m_keybind.size() >= r.m_keybind.size();
                             });

            // The first element will be the longest keybind that is matching.
            option_t &option = m_matched_keybinds[0];
            if (option.m_condition(ctx)) {
                if (m_open_event) {
                    m_open_event(ctx);
                }
                option.m_keybind_used = true;
                m_deferred_cmds.emplace_back(option.m_op);
            } else {
                option.m_keybind_used = false;
            }
        }

        m_matched_keybinds.clear();
    }

    template <typename _DataT>
    inline bool ContextMenu<_DataT>::processKeybindsGroup(group_t &group, const _DataT &ctx) {
        for (size_t i = 0; i < group.m_ops.size(); ++i) {
            context_t &entry = group.m_ops.at(i);
            if (entry.m_type == context_t::TYPE_GROUP) {
                if (processKeybindsGroup(*entry.m_group, ctx)) {
                    return true;
                }
            } else if (entry.m_type == context_t::TYPE_OP) {
                if (processKeybindsOption(*entry.m_op, ctx)) {
                    return true;
                }
            }
        }
        return false;
    }

    template <typename _DataT>
    inline bool ContextMenu<_DataT>::processKeybindsOption(option_t &option, const _DataT &ctx) {

        if (option.m_keybind.isInputMatching()) {
            // Even though we could filter by digested commands,
            // we need to later discard similar command bindings
            // ---
            // Example: CTRL+A -> CTRL+SHIFT+A
            // In this case the second command should be discarded
            // because shift is a soft differentiation.
            m_matched_keybinds.emplace_back(option);
        } else {
            option.m_keybind_used = false;
        }

        return false;
    }

    template <typename _DataT> class ContextMenuBuilder {
        using menu_t      = ContextMenu<_DataT>;
        using operator_t  = menu_t::option_t::operator_t;
        using condition_t = menu_t::option_t::condition_t;

    public:
        ContextMenuBuilder(menu_t *menu) : m_menu(menu) {}

        ContextMenuBuilder &beginGroup(std::string_view group_name) {
            m_group_stack.emplace_back(m_menu->addGroup(group_name));
            return *this;
        }
        ContextMenuBuilder &endGroup() {
            m_group_stack.pop_back();
            return *this;
        }

        ContextMenuBuilder &addOption(std::string_view label, const KeyBind &keybind,
                                      operator_t op);

        ContextMenuBuilder &addOption(std::string_view label, const KeyBind &keybind,
                                      condition_t condition, operator_t op);

        ContextMenuBuilder &addDivider();

    private:
        menu_t *m_menu;
        std::vector<typename menu_t::group_t *> m_group_stack;
    };

    template <typename _DataT>
    inline ContextMenuBuilder<_DataT> &ContextMenuBuilder<_DataT>::addOption(std::string_view label,
                                                                             const KeyBind &keybind,
                                                                             operator_t op) {
        if (!m_menu) {
            return *this;
        }

        typename menu_t::group_t *parent_group = nullptr;
        if (!m_group_stack.empty()) {
            parent_group = m_group_stack.back();
        }

        m_menu->addOption(parent_group, label, keybind, op);
        return *this;
    }

    template <typename _DataT>
    inline ContextMenuBuilder<_DataT> &
    ContextMenuBuilder<_DataT>::addOption(std::string_view label, const KeyBind &keybind,
                                          condition_t condition, operator_t op) {
        if (!m_menu) {
            return *this;
        }

        typename menu_t::group_t *parent_group = nullptr;
        if (!m_group_stack.empty()) {
            parent_group = m_group_stack.back();
        }

        m_menu->addOption(parent_group, label, keybind, condition, op);
        return *this;
    }

    template <typename _DataT>
    inline ContextMenuBuilder<_DataT> &ContextMenuBuilder<_DataT>::addDivider() {
        if (!m_menu) {
            return *this;
        }

        typename menu_t::group_t *parent_group = nullptr;
        if (!m_group_stack.empty()) {
            parent_group = m_group_stack.back();
        }

        m_menu->addDivider(parent_group);
        return *this;
    }

}  // namespace Toolbox::UI