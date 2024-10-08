#include "gui/window.hpp"
#include "gui/application.hpp"
#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include <ImGuiFileDialog.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <iconv.h>
#include <stb/stb_image.h>

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

    void ImWindow::setIcon(const std::string &icon_name) {
        ResourceManager &res_manager = GUIApplication::instance().getResourceManager();
        UUID64 icon_dir              = res_manager.getResourcePathUUID("Images/Icons");

        auto result = res_manager.getRawData(icon_name, icon_dir);
        if (!result) {
            return;
        }

        Buffer data_buf;
        int width, height, channels;

        // Load image data
        {
            stbi_uc *data =
                stbi_load_from_memory(result.value().data(), result.value().size(), &width, &height, &channels, 4);

            data_buf.alloc(static_cast<size_t>(width * height * channels));
            std::memcpy(data_buf.buf<u8>(), data, data_buf.size());

            stbi_image_free(data);
        }

        setIcon(std::move(data_buf), ImVec2(width, height));
    }

    void ImWindow::setIcon(Buffer &&icon_data, const ImVec2 &icon_size) {
        m_is_new_icon = true;
        m_icon_data   = std::move(icon_data);
        m_icon_size   = icon_size;
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
        ImGuiWindow *window     = ImGui::FindWindowByName(window_name.c_str());

        if (m_is_new_icon) {
            if (window && window->Viewport && window->Viewport->PlatformHandle) {
                GLFWwindow *glfw_window = (GLFWwindow *)window->Viewport->PlatformHandle;
                GLFWimage icon          = {m_icon_size.x, m_icon_size.y, m_icon_data.buf<u8>()};
                glfwSetWindowIcon(glfw_window, 1, &icon);
                m_is_new_icon = false;
            }
        }

        bool is_focused = isFocused();
        bool is_hidden  = window ? (window->Hidden || window->Collapsed) : false;
        bool is_open    = isOpen();

        bool was_focused = is_focused;
        bool was_hidden  = isHidden();
        bool was_open    = is_open;

        if (is_open) {
            ImVec2 pos  = getPos();
            ImVec2 size = getSize();

            ImVec2 default_min = {0, 0};
            ImVec2 default_max = {FLT_MAX, FLT_MAX};
            ImGui::SetNextWindowSizeConstraints(minSize() ? minSize().value() : default_min,
                                                maxSize() ? maxSize().value() : default_max);

            // TODO: Fix window class causing problems.

            /*const ImGuiWindowClass *window_class = windowClass();
            if (window_class)
                ImGui::SetNextWindowClass(window_class);*/

            ImGuiWindowFlags flags_ = flags();
            if (unsaved()) {
                flags_ |= ImGuiWindowFlags_UnsavedDocument;
            }

            // Establish window constraints
            if (window) {
                if (size != m_next_size && m_is_resized) {
                    if (m_next_size.x >= 0.0f && m_next_size.y >= 0.0f) {
                        ImGui::SetNextWindowSize(size);
                        GUIApplication::instance().dispatchEvent<WindowEvent, true>(
                            getUUID(), EVENT_WINDOW_RESIZE, m_next_size);
                    }
                    m_is_resized = false;
                }
                if (pos != m_next_pos && m_is_repositioned) {
                    ImGui::SetNextWindowPos(m_next_pos);
                    GUIApplication::instance().dispatchEvent<WindowEvent, true>(
                        getUUID(), EVENT_WINDOW_MOVE, m_next_pos);
                    m_is_repositioned = false;
                }
            }

            if ((flags_ & ImGuiWindowFlags_NoBackground)) {
                ImGui::SetNextWindowBgAlpha(0.0f);
            }

            // Render the window
            if (ImGui::Begin(window_name.c_str(), &is_open, flags_)) {
                ImGuiViewport *viewport = ImGui::GetCurrentWindow()->Viewport;

                if ((flags_ & ImGuiWindowFlags_NoBackground)) {
                    viewport->Flags |= ImGuiViewportFlags_TransparentFrameBuffer;
                }

                is_focused = ImGui::IsWindowFocused();

                m_viewport = ImGui::GetWindowViewport();
                onRenderDockspace();
                onRenderMenuBar();
                onRenderBody(delta_time);
            } else {
                m_viewport = nullptr;
            }
            ImGui::End();
        }

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