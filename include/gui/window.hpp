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

namespace Toolbox::UI {

    class IWindow : public IUnique {
    protected:
        static ImGuiID s_next_window_uid;

        virtual void renderMenuBar()            = 0;
        virtual void renderBody(f32 delta_time) = 0;

        virtual void onDeleteKey()   = 0;
        virtual void onPageDownKey() = 0;
        virtual void onPageUpKey()   = 0;
        virtual void onHomeKey()     = 0;

    public:
        virtual ~IWindow() = default;

        virtual void open() = 0;

        [[nodiscard]] virtual IWindow *parent() const = 0;
        virtual void setParent(IWindow *parent)       = 0;

        virtual const ImGuiWindowClass *windowClass() const = 0;

        virtual ImGuiWindowFlags flags() const = 0;

        [[nodiscard]] virtual std::optional<ImVec2> defaultSize() const = 0;
        [[nodiscard]] virtual std::optional<ImVec2> size() const        = 0;
        [[nodiscard]] virtual std::optional<ImVec2> minSize() const     = 0;
        [[nodiscard]] virtual std::optional<ImVec2> maxSize() const     = 0;

        [[nodiscard]] virtual std::string context() const = 0;
        [[nodiscard]] virtual std::string name() const    = 0;
        [[nodiscard]] virtual bool unsaved() const        = 0;

        // Returns the supported file type, or empty if designed for a folder.
        [[nodiscard]] virtual std::vector<std::string> extensions() const = 0;

        [[nodiscard]] virtual bool loadData(const std::filesystem::path &path)         = 0;
        [[nodiscard]] virtual bool saveData(std::optional<std::filesystem::path> path) = 0;

        [[nodiscard]] virtual bool update(f32 delta_time)     = 0;
        [[nodiscard]] virtual bool postUpdate(f32 delta_time) = 0;

        [[nodiscard]] virtual std::string title() const = 0;

        virtual void render(f32 delta_time) = 0;
    };

    class SimpleWindow : public IWindow {
    protected:
        void renderMenuBar() override {}
        void renderBody(f32 delta_time) override {}

        void onDeleteKey() override {}
        void onPageDownKey() override {}
        void onPageUpKey() override {}
        void onHomeKey() override {}

    public:
        SimpleWindow() = default;
        SimpleWindow(std::optional<ImVec2> default_size) : m_default_size(default_size) {}
        SimpleWindow(std::optional<ImVec2> min_size, std::optional<ImVec2> max_size)
            : m_min_size(min_size), m_max_size(max_size) {}
        SimpleWindow(std::optional<ImVec2> default_size, std::optional<ImVec2> min_size,
                     std::optional<ImVec2> max_size)
            : m_default_size(default_size), m_min_size(min_size), m_max_size(max_size) {}
        SimpleWindow(std::optional<ImVec2> default_size, std::optional<ImVec2> min_size,
                     std::optional<ImVec2> max_size, ImGuiWindowClass window_class)
            : m_default_size(default_size), m_min_size(min_size), m_max_size(max_size),
              m_window_class(window_class) {}
        SimpleWindow(std::optional<ImVec2> default_size, std::optional<ImVec2> min_size,
                     std::optional<ImVec2> max_size, ImGuiWindowClass window_class,
                     ImGuiWindowFlags flags)
            : m_default_size(default_size), m_min_size(min_size), m_max_size(max_size),
              m_window_class(window_class), m_flags(flags) {}
        ~SimpleWindow() override = default;

        void open() override { m_is_open = true; }

        [[nodiscard]] UUID64 getUUID() const override { return m_UUID64; }

        [[nodiscard]] IWindow *parent() const override { return m_parent; }
        void setParent(IWindow *parent) override { m_parent = parent; }

        const ImGuiWindowClass *windowClass() const override {
            return m_parent ? m_parent->windowClass() : &m_window_class;
        }

        ImGuiWindowFlags flags() const override {
            ImGuiWindowFlags flags_ = m_flags;
#ifdef IMGUI_ENABLE_VIEWPORT_WORKAROUND
            if (m_viewport && m_viewport->ID != ImGui::GetMainViewport()->ID) {
                flags_ |= (ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
            }
#endif
            return flags_;
        }

        [[nodiscard]] std::optional<ImVec2> defaultSize() const override { return m_default_size; }
        [[nodiscard]] std::optional<ImVec2> size() const override { return m_size; }
        [[nodiscard]] std::optional<ImVec2> minSize() const override { return m_min_size; }
        [[nodiscard]] std::optional<ImVec2> maxSize() const override { return m_max_size; }

        [[nodiscard]] std::string context() const override { return "(OVERRIDE THIS)"; }
        [[nodiscard]] std::string name() const override { return "Base Window"; }
        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file type, or empty if designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool loadData(const std::filesystem::path &path) override { return false; }
        [[nodiscard]] bool saveData(std::optional<std::filesystem::path> path) override {
            return false;
        }

        [[nodiscard]] bool update(f32 delta_time) override { return true; }
        [[nodiscard]] bool postUpdate(f32 delta_time) override { return true; }

        [[nodiscard]] std::string title() const override {
            std::string ctx = context();
            std::string t;
            if (ctx != "") {
                t = std::format("{} - {}", name(), ctx);
            } else {
                t = name();
            }
            return t;
        }

        void render(f32 delta_time) override final {
            if (!m_is_open)
                return;

            if (defaultSize())
                ImGui::SetNextWindowSize(defaultSize().value(), ImGuiCond_Once);

            ImVec2 default_min = {0, 0};
            ImVec2 default_max = {FLT_MAX, FLT_MAX};
            ImGui::SetNextWindowSizeConstraints(minSize() ? minSize().value() : default_min,
                                                maxSize() ? maxSize().value() : default_max);

            std::string window_name = std::format("{}###{}", title(), getUUID());

            ImGuiWindowFlags flags_ = flags();
            if (unsaved()) {
                flags_ |= ImGuiWindowFlags_UnsavedDocument;
            }

            /*const ImGuiWindowClass *window_class = windowClass();
            if (window_class)
                ImGui::SetNextWindowClass(window_class);*/

            if (ImGui::Begin(window_name.c_str(), &m_is_open, flags_)) {
                m_size     = ImGui::GetWindowSize();
                m_viewport = ImGui::GetWindowViewport();
                renderMenuBar();
                renderBody(delta_time);
            } else {
                m_viewport = nullptr;
            }
            ImGui::End();
        }

    protected:
        UUID64 m_UUID64;
        ImGuiID m_sibling_id = 0;

        IWindow *m_parent = nullptr;

        bool m_is_open                          = false;
        ImGuiViewport *m_viewport               = nullptr;
        ImGuiWindowFlags m_flags                = 0;
        mutable ImGuiWindowClass m_window_class = ImGuiWindowClass();

        bool m_is_docking_set_up = false;

        std::optional<ImVec2> m_default_size = {};
        std::optional<ImVec2> m_size         = {};
        std::optional<ImVec2> m_min_size     = {};
        std::optional<ImVec2> m_max_size     = {};
    };

    class DockWindow : public IWindow {
    protected:
        virtual void buildDockspace(ImGuiID dockspace_id){};

        void renderDockspace();
        void renderMenuBar() override {}
        void renderBody(f32 delta_time) override {}

        void onDeleteKey() override {}
        void onPageDownKey() override {}
        void onPageUpKey() override {}
        void onHomeKey() override {}

    public:
        DockWindow() = default;
        explicit DockWindow(std::optional<ImVec2> default_size) : m_default_size(default_size) {}
        DockWindow(std::optional<ImVec2> min_size, std::optional<ImVec2> max_size)
            : m_min_size(min_size), m_max_size(max_size) {}
        DockWindow(std::optional<ImVec2> default_size, std::optional<ImVec2> min_size,
                   std::optional<ImVec2> max_size)
            : m_default_size(default_size), m_min_size(min_size), m_max_size(max_size) {}
        DockWindow(std::optional<ImVec2> default_size, std::optional<ImVec2> min_size,
                   std::optional<ImVec2> max_size, ImGuiWindowClass window_class)
            : m_default_size(default_size), m_min_size(min_size), m_max_size(max_size),
              m_window_class(window_class) {}
        DockWindow(std::optional<ImVec2> default_size, std::optional<ImVec2> min_size,
                   std::optional<ImVec2> max_size, ImGuiWindowClass window_class,
                   ImGuiWindowFlags flags)
            : m_default_size(default_size), m_min_size(min_size), m_max_size(max_size),
              m_window_class(window_class), m_flags(flags) {}
        ~DockWindow() override = default;

        void open() override { m_is_open = true; }

        [[nodiscard]] UUID64 getUUID() const override { return m_UUID64; }

        [[nodiscard]] IWindow *parent() const override { return m_parent; }
        void setParent(IWindow *parent) override { m_parent = parent; }

        const ImGuiWindowClass *windowClass() const override {
            return m_parent ? m_parent->windowClass() : &m_window_class;
        }

        ImGuiWindowFlags flags() const override {
            ImGuiWindowFlags flags_ = m_flags;
#ifdef IMGUI_ENABLE_VIEWPORT_WORKAROUND
            if (m_viewport && m_viewport->ID != ImGui::GetMainViewport()->ID) {
                flags_ |= (ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
            }
#endif
            return flags_;
        }

        [[nodiscard]] std::optional<ImVec2> defaultSize() const override { return m_default_size; }
        [[nodiscard]] std::optional<ImVec2> size() const override { return m_size; }
        [[nodiscard]] std::optional<ImVec2> minSize() const override { return m_min_size; }
        [[nodiscard]] std::optional<ImVec2> maxSize() const override { return m_max_size; }

        [[nodiscard]] std::string context() const override { return "(OVERRIDE THIS)"; }
        [[nodiscard]] std::string name() const override { return "Base Window"; }
        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file type, or empty if designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override { return {}; }

        [[nodiscard]] bool loadData(const std::filesystem::path &path) override { return false; }
        [[nodiscard]] bool saveData(std::optional<std::filesystem::path> path) override {
            return false;
        }

        [[nodiscard]] bool update(f32 delta_time) override { return true; }
        [[nodiscard]] bool postUpdate(f32 delta_time) override { return true; }

        [[nodiscard]] std::string title() const override {
            std::string ctx = context();
            std::string t;
            if (ctx != "") {
                t = std::format("{} - {}", name(), ctx);
            } else {
                t = name();
            }
            return t;
        }

        void render(f32 delta_time) override final {
            if (!m_is_open)
                return;

            if (defaultSize())
                ImGui::SetNextWindowSize(defaultSize().value(), ImGuiCond_Once);

            ImVec2 default_min = {0, 0};
            ImVec2 default_max = {FLT_MAX, FLT_MAX};
            ImGui::SetNextWindowSizeConstraints(minSize() ? minSize().value() : default_min,
                                                maxSize() ? maxSize().value() : default_max);

            std::string window_name = std::format("{}###{}", title(), getUUID());

            ImGuiWindowFlags flags_ = flags();
            if (unsaved()) {
                flags_ |= ImGuiWindowFlags_UnsavedDocument;
            }

            /*const ImGuiWindowClass *window_class = windowClass();
            if (window_class)
                ImGui::SetNextWindowClass(window_class);*/

            if (ImGui::Begin(window_name.c_str(), &m_is_open, flags_)) {
                m_size     = ImGui::GetWindowSize();
                m_viewport = ImGui::GetWindowViewport();

                renderDockspace();
                renderMenuBar();
                renderBody(delta_time);
            } else {
                m_viewport = nullptr;
            }
            ImGui::End();
        }

    protected:
        UUID64 m_UUID64;

        IWindow *m_parent = nullptr;

        bool m_is_open                          = false;
        ImGuiViewport *m_viewport               = nullptr;
        ImGuiWindowFlags m_flags                = 0;
        mutable ImGuiWindowClass m_window_class = ImGuiWindowClass();

        ImGuiID m_dockspace_id = 0;
        bool m_is_docking_set_up = false;

        std::optional<ImVec2> m_default_size = {};
        std::optional<ImVec2> m_size         = {};
        std::optional<ImVec2> m_min_size     = {};
        std::optional<ImVec2> m_max_size     = {};
    };

    inline std::string getWindowUID(const IWindow &window) {
        return std::to_string(window.getUUID());
    }

    inline std::string getWindowChildUID(const IWindow &window, const std::string &child_name) {
        return std::format("{}##{}", child_name, getWindowUID(window));
    }

}  // namespace Toolbox::UI