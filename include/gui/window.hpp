#pragma once

#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "serial.hpp"
#include "smart_resource.hpp"
#include "unique.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include "gui/dragdrop/target.hpp"
#include "gui/layer/imlayer.hpp"

namespace Toolbox::UI {

    class ImWindow : public ImProcessLayer {
    protected:
        virtual ImGuiID onBuildDockspace() { return std::numeric_limits<ImGuiID>::max(); }
        virtual void onRenderDockspace();
        virtual void onRenderMenuBar() {}
        virtual void onRenderBody(TimeStep delta_time) {}

    public:
        explicit ImWindow(const std::string &name) : ImProcessLayer(name) {}
        ImWindow(const std::string &name, std::optional<ImVec2> default_size)
            : ImProcessLayer(name), m_default_size(default_size) {}
        ImWindow(const std::string &name, std::optional<ImVec2> min_size,
                 std::optional<ImVec2> max_size)
            : ImProcessLayer(name), m_min_size(min_size), m_max_size(max_size) {}
        ImWindow(const std::string &name, std::optional<ImVec2> default_size,
                 std::optional<ImVec2> min_size, std::optional<ImVec2> max_size)
            : ImProcessLayer(name), m_default_size(default_size), m_min_size(min_size),
              m_max_size(max_size) {}
        ImWindow(const std::string &name, std::optional<ImVec2> default_size,
                 std::optional<ImVec2> min_size, std::optional<ImVec2> max_size,
                 ImGuiWindowClass window_class)
            : ImProcessLayer(name), m_default_size(default_size), m_min_size(min_size),
              m_max_size(max_size), m_window_class(window_class) {}
        ImWindow(const std::string &name, std::optional<ImVec2> default_size,
                 std::optional<ImVec2> min_size, std::optional<ImVec2> max_size,
                 ImGuiWindowClass window_class, ImGuiWindowFlags flags)
            : ImProcessLayer(name), m_default_size(default_size), m_min_size(min_size),
              m_max_size(max_size), m_window_class(window_class), m_flags(flags) {}

        virtual ~ImWindow() = default;

        [[nodiscard]] virtual ImWindow *parent() const { return m_parent; }
        virtual void setParent(ImWindow *parent) { m_parent = parent; }

        virtual const ImGuiWindowClass *windowClass() const {
            return m_parent ? m_parent->windowClass() : &m_window_class;
        }

        virtual ImGuiWindowFlags flags() const {
            ImGuiWindowFlags flags_ = m_flags;
#ifdef IMGUI_ENABLE_VIEWPORT_WORKAROUND
            if (m_viewport && m_viewport->ID != ImGui::GetMainViewport()->ID) {
                flags_ |= (ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
            }
#endif
            return flags_;
        }

        [[nodiscard]] void setSize(const ImVec2 &size) noexcept override {
            m_next_size  = size;
            m_is_resized = true;
        }
        [[nodiscard]] void setPos(const ImVec2 &pos) noexcept override {
            m_next_pos        = pos;
            m_is_repositioned = true;
        }

        void setIcon(const std::string &icon_name);
        void setIcon(Buffer &&icon_data, const ImVec2 &icon_size);

        [[nodiscard]] virtual std::optional<ImVec2> defaultSize() const { return m_default_size; }
        [[nodiscard]] virtual std::optional<ImVec2> minSize() const { return m_min_size; }
        [[nodiscard]] virtual std::optional<ImVec2> maxSize() const { return m_max_size; }

        [[nodiscard]] virtual std::string context() const { return "(OVERRIDE THIS)"; }
        [[nodiscard]] virtual bool unsaved() const { return false; }

        // Returns the supported file type, or empty if designed for a folder.
        [[nodiscard]] virtual std::vector<std::string> extensions() const { return {}; }

        void close();
        void defocus();
        void focus();
        void open();

        void hide();
        void show();

        [[nodiscard]] virtual bool onLoadData(const std::filesystem::path &path) { return false; }
        [[nodiscard]] virtual bool onSaveData(std::optional<std::filesystem::path> path) {
            return false;
        }

        void onAttach() override;
        void onImGuiRender(TimeStep delta_time) override final;
        void onWindowEvent(RefPtr<WindowEvent> ev) override;

        [[nodiscard]] std::string title() const;

    protected:
        void setLayerSize(const ImVec2 &size) noexcept { ImProcessLayer::setSize(size); }
        void setLayerPos(const ImVec2 &pos) noexcept { ImProcessLayer::setPos(pos); }

        static void privDropCallback(GLFWwindow *window, int path_count, const char *paths[]);

        UUID64 m_UUID64;
        ImGuiID m_sibling_id = 0;

        ImWindow *m_parent = nullptr;

        ImGuiViewport *m_viewport               = nullptr;
        ImGuiWindowFlags m_flags                = 0;
        mutable ImGuiWindowClass m_window_class = ImGuiWindowClass();

        std::optional<ImVec2> m_default_size = {};
        std::optional<ImVec2> m_min_size     = {};
        std::optional<ImVec2> m_max_size     = {};

    private:
        ImGuiID m_dockspace_id = std::numeric_limits<ImGuiID>::max();

        bool m_is_docking_set_up = false;
        bool m_is_resized        = false;
        bool m_is_repositioned   = false;

        bool m_is_new_icon = false;
        ImVec2 m_icon_size = {};
        Buffer m_icon_data = {};

        ImVec2 m_next_size = {};
        ImVec2 m_next_pos  = {};
    };

    inline std::string ImWindowComponentTitle(const ImWindow &window_layer,
                                              const std::string &component_name) {
        return std::format("{}##{}", component_name, window_layer.getUUID());
    }

}  // namespace Toolbox::UI