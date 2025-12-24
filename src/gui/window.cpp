#include "gui/window.hpp"
#include "gui/application.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/event/dropevent.hpp"
#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include <imgui.h>
#include <imgui_internal.h>

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

    bool ImWindow::onBeginWindow(const std::string &window_name, bool *is_open,
                                 ImGuiWindowFlags flags) {
        return ImGui::Begin(window_name.c_str(), is_open, flags);
    }

    void ImWindow::onEndWindow(bool did_render) { ImGui::End(); }

    void ImWindow::setSize(const ImVec2 &size) noexcept {
        if (size == getSize()) {
            return;
        }
        GUIApplication::instance().dispatchEvent<WindowEvent, true>(getUUID(), EVENT_WINDOW_RESIZE,
                                                                    size);
    }
    void ImWindow::setPos(const ImVec2 &pos) noexcept {
        if (pos == getPos()) {
            return;
        }
        GUIApplication::instance().dispatchEvent<WindowEvent, true>(getUUID(), EVENT_WINDOW_MOVE,
                                                                    pos);
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
            stbi_uc *data = stbi_load_from_memory(result.value().data(), result.value().size(),
                                                  &width, &height, &channels, 4);

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

    void ImWindow::onDetach() {
        if (m_low_handle) {
            GUIApplication::instance().deregisterDragDropSource(m_low_handle);
            GUIApplication::instance().deregisterDragDropTarget(m_low_handle);
        }
        m_low_handle = nullptr;
    }

    void ImWindow::onImGuiRender(TimeStep delta_time) {
        std::string window_name = std::format("{}###{}", title(), getUUID());
        ImGuiWindow *window     = ImGui::FindWindowByName(window_name.c_str());
        m_imgui_window          = window;

        if (m_is_new_icon) {
            if (m_imgui_window && m_imgui_window->Viewport &&
                m_imgui_window->Viewport->PlatformHandle) {
                GLFWwindow *glfw_window = (GLFWwindow *)m_imgui_window->Viewport->PlatformHandle;
                GLFWimage icon          = {m_icon_size.x, m_icon_size.y, m_icon_data.buf<u8>()};
                glfwSetWindowIcon(glfw_window, 1, &icon);
                m_is_new_icon = false;
            }
        }

        bool is_focused = isFocused();
        bool is_hidden  = isHidden();
        bool is_open    = isOpen();

        bool was_focused = is_focused;
        bool was_hidden  = is_hidden;
        bool was_open    = is_open;

        if (is_hidden) {
            return;
        }

        if (is_open) {
            m_prev_pos  = getPos();
            m_prev_size = getSize();

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

            if ((flags_ & ImGuiWindowFlags_NoBackground)) {
                ImGui::SetNextWindowBgAlpha(0.0f);
            }

            // Render the window
            bool did_render = onBeginWindow(window_name, &is_open, flags_);
            if (m_imgui_window) {
                is_hidden = m_imgui_window->Hidden && !m_imgui_window->DockIsActive;
            }

            if (did_render) {
                if (m_imgui_window) {
                    m_im_order = m_imgui_window->BeginOrderWithinContext;
                }

                if (m_first_render) {
                    m_prev_pos  = ImGui::GetWindowPos();
                    m_prev_size = ImGui::GetWindowSize();
                    setSize(m_prev_size);
                    setPos(m_prev_pos);
                    m_first_render = false;
                }

                ImGuiViewport *viewport = ImGui::GetCurrentWindow()->Viewport;
                GLFWwindow *window      = static_cast<GLFWwindow *>(viewport->PlatformHandle);
                if (window) {
                    glfwSetWindowUserPointer(window, this);
                    /*glfwSetDropCallback(static_cast<GLFWwindow *>(viewport->PlatformHandle),
                                        privDropCallback);*/
                    m_low_handle =
                        (Platform::LowWindow)viewport->PlatformHandleRaw;
                    if (!Platform::GetWindowZOrder(m_low_handle, m_z_order)) {
                        m_z_order = -1;
                    }

                    GUIApplication::instance().registerDragDropSource(m_low_handle);
                    GUIApplication::instance().registerDragDropTarget(m_low_handle);
                }

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
            onEndWindow(did_render);
        }

        // Establish window constraints
        if (m_imgui_window) {
            if (m_imgui_window->Size != m_prev_size) {
                if (m_next_size.x >= 0.0f && m_next_size.y >= 0.0f) {
                    GUIApplication::instance().dispatchEvent<WindowEvent, true>(
                        getUUID(), EVENT_WINDOW_RESIZE, m_imgui_window->Size);
                }
                ImGui::SetWindowSize(m_imgui_window, m_prev_size, ImGuiCond_Always);
            }

            if (m_imgui_window->Pos != m_prev_pos) {
                GUIApplication::instance().dispatchEvent<WindowEvent, true>(
                    getUUID(), EVENT_WINDOW_MOVE, m_imgui_window->Pos);
                ImGui::SetWindowPos(m_imgui_window, m_prev_pos, ImGuiCond_Always);
            }
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
        case EVENT_WINDOW_MOVE: {
            ImVec2 win_pos = ev->getGlobalPoint();
            setLayerPos(win_pos);
            std::string window_name = std::format("{}###{}", title(), ev->getTargetId());
            ImGui::SetWindowPos(window_name.c_str(), win_pos, ImGuiCond_Always);
            ev->accept();
            break;
        }
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
            std::string window_name = std::format("{}###{}", title(), ev->getTargetId());
            ImGui::SetWindowSize(window_name.c_str(), win_size, ImGuiCond_Always);
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
