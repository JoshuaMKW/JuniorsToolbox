#include "gui/window.hpp"
#include "gui/application.hpp"
#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuiFileDialog.h>

#include <IconsForkAwesome.h>
#include <gui/modelcache.hpp>

#include "gui/window.hpp"
#include <iconv.h>

namespace Toolbox::UI {

    void ImWindow::onRenderDockspace() {
        if (!m_is_docking_set_up) {
            m_dockspace_id      = onBuildDockspace();
            m_is_docking_set_up = true;
        }

        bool is_dockspace_avail = m_dockspace_id != std::numeric_limits<ImGuiID>::max() &&
                                  ImGui::DockBuilderGetNode(m_dockspace_id) != nullptr;

        if (is_dockspace_avail) {
            ImGui::DockSpace(m_dockspace_id, {}, ImGuiDockNodeFlags_None, nullptr);
        }
    }

    void ImWindow::close() {
        GUIApplication::instance().dispatchEvent<WindowEvent, true>(getUUID(), EVENT_WINDOW_CLOSE,
                                                                    ImVec2{0, 0});
        defocus();
    }

    void ImWindow::defocus() {
        GUIApplication::instance().dispatchEvent<BaseEvent, true>(getUUID(), EVENT_FOCUS_OUT);
    }

    void ImWindow::focus() {
        GUIApplication::instance().dispatchEvent<BaseEvent, true>(getUUID(), EVENT_FOCUS_IN);
    }

    void ImWindow::open() {
        GUIApplication::instance().dispatchEvent<WindowEvent, true>(getUUID(), EVENT_WINDOW_SHOW,
                                                                    ImVec2{0, 0});
    }

    void ImWindow::hide() {
        GUIApplication::instance().dispatchEvent<WindowEvent, true>(getUUID(), EVENT_WINDOW_HIDE,
                                                                    ImVec2{0, 0});
    }

    void ImWindow::show() {
        GUIApplication::instance().dispatchEvent<WindowEvent, true>(getUUID(), EVENT_WINDOW_SHOW,
                                                                    ImVec2{0, 0});
    }

    void ImWindow::onAttach() {
        ImProcessLayer::onAttach();

        ImVec2 init_size = {0, 0};

        std::optional<ImVec2> default_size = defaultSize();
        if (default_size) {
            init_size = *default_size;
        }

        std::optional<ImVec2> min_size = minSize();
        if (min_size) {
            init_size.x = std::max(init_size.x, min_size->x);
            init_size.y = std::max(init_size.y, min_size->y);
        }

        std::optional<ImVec2> max_size = maxSize();
        if (max_size) {
            init_size.x = std::min(init_size.x, max_size->x);
            init_size.y = std::min(init_size.y, max_size->y);
        }

        setSize(init_size);
    }

    void ImWindow::onImGuiRender(TimeStep delta_time) {
        std::string window_name = std::format("{}###{}", title(), getUUID());

        ImVec2 default_min = {0, 0};
        ImVec2 default_max = {FLT_MAX, FLT_MAX};
        ImGui::SetNextWindowSizeConstraints(minSize() ? minSize().value() : default_min,
                                            maxSize() ? maxSize().value() : default_max);

        ImVec2 pos  = getPos();
        ImVec2 size = getSize();

        ImGuiWindow *window = ImGui::FindWindowByName(window_name.c_str());
        if (window) {
            if (size != m_next_size && m_is_resized) {
                if (m_next_size.x >= 0.0f && m_next_size.y >= 0.0f) {
                    ImGui::SetNextWindowSize(size);
                }
                m_is_resized = false;
            }
            if (pos != m_next_pos && m_is_repositioned) {
                ImGui::SetNextWindowPos(m_next_pos);
                m_is_repositioned = false;
            }
        }

        ImGuiWindowFlags flags_ = flags();
        if (unsaved()) {
            flags_ |= ImGuiWindowFlags_UnsavedDocument;
        }

        // TODO: Fix window class causing problems.

        /*const ImGuiWindowClass *window_class = windowClass();
        if (window_class)
            ImGui::SetNextWindowClass(window_class);*/

        bool is_focused = isFocused();
        bool is_hidden  = window ? (window->Hidden || window->Collapsed) : false;
        bool is_open    = isOpen();

        bool was_focused = is_focused;
        bool was_hidden  = isHidden();
        bool was_open    = is_open;

        if ((flags_ & ImGuiWindowFlags_NoBackground)) {
            ImGui::SetNextWindowBgAlpha(0.0f);
        }

        if (ImGui::Begin(window_name.c_str(), &is_open, flags_)) {
            ImGuiViewport *viewport = ImGui::GetCurrentWindow()->Viewport;

            if ((flags_ & ImGuiWindowFlags_NoBackground)) {
                viewport->Flags |= ImGuiViewportFlags_TransparentFrameBuffer;
            }

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
            onRenderDockspace();
            onRenderMenuBar();
            onRenderBody(delta_time);
        } else {
            m_viewport = nullptr;
        }
        ImGui::End();

        // Handle open/close and focus/defocus
        if (is_open) {
            if (!was_open) {
                show();
            }
            if (is_focused && !was_focused) {
                focus();
            } else if (!is_focused && was_focused) {
                defocus();
            }
        } else if (was_open) {
            close();
        }

        // Handle collapse/hide
        if (is_hidden && !was_hidden) {
            hide();
        } else if (!is_hidden && was_hidden) {
            show();
        }
    }

    void ImWindow::onWindowEvent(RefPtr<WindowEvent> ev) {
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
            setLayerSize(win_size);
            ev->accept();
            break;
        }
        default:
            break;
        }
    }

    std::string ImWindow::title() const {
        std::string ctx = context();
        std::string t;
        if (ctx != "") {
            t = std::format("{} - {}", name(), ctx);
        } else {
            t = name();
        }
        return t;
    }

}  // namespace Toolbox::UI