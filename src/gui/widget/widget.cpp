#include <imgui_internal.h>
#include <imgui.h>

#include "gui/application.hpp"
#include "gui/widget/widget.hpp"

namespace Toolbox::UI {

    void ImWidget::defocus() {
        GUIApplication::instance().dispatchEvent<BaseEvent, true>(getUUID(), EVENT_FOCUS_OUT);
    }

    void ImWidget::focus() {
        GUIApplication::instance().dispatchEvent<BaseEvent, true>(getUUID(), EVENT_FOCUS_IN);
    }

    void ImWidget::onAttach() {}

    void ImWidget::onImGuiRender(TimeStep delta_time) {
        std::string window_name = std::format("{}###{}", title(), getUUID());

        ImVec2 default_min = {0, 0};
        ImVec2 default_max = {FLT_MAX, FLT_MAX};
        ImGui::SetNextWindowSizeConstraints(minSize() ? minSize().value() : default_min,
                                            maxSize() ? maxSize().value() : default_max);

        ImVec2 pos  = getPos();
        ImVec2 size = getSize();

        bool is_focused = isFocused();
        bool was_focused = is_focused;

        ImGuiChildFlags widget_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::BeginChild(window_name.c_str(), size, widget_flags)) {
            ImVec2 updated_size = ImGui::GetWindowSize();
            ImVec2 updated_pos  = ImGui::GetWindowPos();

            is_focused = ImGui::IsWindowFocused();

            if (updated_size != size) {
                GUIApplication::instance().dispatchEvent<WindowEvent, true>(
                    getUUID(), EVENT_WINDOW_RESIZE, updated_size);
            }

            if (updated_pos != pos) {
                GUIApplication::instance().dispatchEvent<WindowEvent, true>(
                    getUUID(), EVENT_WINDOW_MOVE, updated_pos);
            }

            m_viewport = ImGui::GetWindowViewport();
            onRenderBody(delta_time);
        } else {
            m_viewport = nullptr;
        }
        ImGui::End();

        // Handle focus/defocus
        if (is_focused && !was_focused) {
            focus();
        } else if (!is_focused && was_focused) {
            defocus();
        }
    }

    void ImWidget::onWindowEvent(RefPtr<WindowEvent> ev) {
        ImProcessLayer::onWindowEvent(ev);
        switch (ev->getType()) {
        case EVENT_WINDOW_RESIZE: {
            ImVec2 win_size = ev->getSize();
            if (m_min_size) {
                win_size.x = std::max(win_size.x, m_min_size->x);
                win_size.y = std::max(win_size.y, m_min_size->y);
            }
            if (m_max_size) {
                win_size.x = std::min(win_size.x, m_max_size->x);
                win_size.y = std::min(win_size.y, m_max_size->y);
            }
            setSize(win_size);
            break;
        }
        default:
            break;
        }
    }

}

