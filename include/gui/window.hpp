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

        [[nodiscard]] virtual std::optional<ImVec2> defaultSize() const { return m_default_size; }
        [[nodiscard]] virtual std::optional<ImVec2> size() const { return m_size; }
        [[nodiscard]] virtual std::optional<ImVec2> minSize() const { return m_min_size; }
        [[nodiscard]] virtual std::optional<ImVec2> maxSize() const { return m_max_size; }

        [[nodiscard]] virtual std::string context() const { return "(OVERRIDE THIS)"; }
        [[nodiscard]] virtual bool unsaved() const { return false; }

        // Returns the supported file type, or empty if designed for a folder.
        [[nodiscard]] virtual std::vector<std::string> extensions() const { return {}; }

        [[nodiscard]] virtual bool onLoadData(const std::filesystem::path &path) { return false; }
        [[nodiscard]] virtual bool onSaveData(std::optional<std::filesystem::path> path) {
            return false;
        }

        void onImGuiRender(TimeStep delta_time) override final {
            std::optional<ImVec2> default_size = defaultSize();
            if (default_size.has_value())
                ImGui::SetNextWindowSize(default_size.value(), ImGuiCond_Once);

            ImVec2 default_min = {0, 0};
            ImVec2 default_max = {FLT_MAX, FLT_MAX};
            ImGui::SetNextWindowSizeConstraints(minSize() ? minSize().value() : default_min,
                                                maxSize() ? maxSize().value() : default_max);

            std::string window_name = std::format("{}###{}", title(), getUUID());

            ImGuiWindowFlags flags_ = flags();
            if (unsaved()) {
                flags_ |= ImGuiWindowFlags_UnsavedDocument;
            }

            // TODO: Fix window class causing problems.

            /*const ImGuiWindowClass *window_class = windowClass();
            if (window_class)
                ImGui::SetNextWindowClass(window_class);*/

            bool is_open = isOpen();
            if (ImGui::Begin(window_name.c_str(), &is_open, flags_)) {
                m_size     = ImGui::GetWindowSize();
                m_viewport = ImGui::GetWindowViewport();
                onRenderDockspace();
                onRenderMenuBar();
                onRenderBody(delta_time);
            } else {
                m_viewport = nullptr;
            }
            ImGui::End();
        }

        [[nodiscard]] std::string title() const {
            std::string ctx = context();
            std::string t;
            if (ctx != "") {
                t = std::format("{} - {}", name(), ctx);
            } else {
                t = name();
            }
            return t;
        }

    protected:
        UUID64 m_UUID64;
        ImGuiID m_sibling_id = 0;

        ImWindow *m_parent = nullptr;

        ImGuiViewport *m_viewport               = nullptr;
        ImGuiWindowFlags m_flags                = 0;
        mutable ImGuiWindowClass m_window_class = ImGuiWindowClass();

        std::optional<ImVec2> m_default_size = {};
        std::optional<ImVec2> m_size         = {};
        std::optional<ImVec2> m_min_size     = {};
        std::optional<ImVec2> m_max_size     = {};

    private:
        ImGuiID m_dockspace_id   = std::numeric_limits<ImGuiID>::max();
        bool m_is_docking_set_up = false;
    };

    inline std::string ImWindowComponentTitle(const ImWindow &window_layer,
                                              const std::string &component_name) {
        return std::format("{}##{}", component_name, window_layer.getUUID());
    }

}  // namespace Toolbox::UI