#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "recorder.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "core/clipboard.hpp"
#include "game/task.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/property/property.hpp"
#include "gui/scene/billboard.hpp"
#include "gui/scene/camera.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "gui/scene/objdialog.hpp"
#include "gui/scene/path.hpp"
#include "gui/scene/renderer.hpp"
#include "gui/window.hpp"

#include <gui/context_menu.hpp>
#include <imgui.h>

namespace Toolbox::UI {

    class PadInputWindow final : public ImWindow {
    public:
        PadInputWindow();
        ~PadInputWindow();

    protected:
        void onRenderMenuBar() override;
        void onRenderBody(TimeStep delta_time) override;

        void renderRecordPanel();
        void renderControllerView();
        void renderRecordedInputData();

        void loadMimePadData(Buffer &buffer);

    public:
        ImGuiWindowFlags flags() const override {
            return ImWindow::flags() | ImGuiWindowFlags_MenuBar;
        }

        const ImGuiWindowClass *windowClass() const override {
            if (parent() && parent()->windowClass()) {
                return parent()->windowClass();
            }

            ImGuiWindow *currentWindow           = ImGui::GetCurrentWindow();
            m_window_class.ClassId               = (ImGuiID)getUUID();
            m_window_class.ParentViewportId      = currentWindow->ViewportId;
            m_window_class.DockingAllowUnclassed = true;
            m_window_class.DockingAlwaysTabBar   = false;
            return nullptr;
        }

        std::optional<ImVec2> minSize() const override {
            return {
                {500, 700}
            };
        }
        std::optional<ImVec2> maxSize() const override { return std::nullopt; }

        [[nodiscard]] std::string context() const override {
            if (m_file_name == "")
                return "(unknown)";
            return m_file_name;
        }
        [[nodiscard]] bool unsaved() const override { return false; }

        // Returns the supported file types, empty string is designed for a folder.
        [[nodiscard]] std::vector<std::string> extensions() const override {
            return {"", "arc", "szs"};
        }

        [[nodiscard]] bool onLoadData(const std::filesystem::path &path) override;
        [[nodiscard]] bool onSaveData(std::optional<std::filesystem::path> path) override;

        void onImGuiUpdate(TimeStep delta_time) override;
        void onImGuiPostUpdate(TimeStep delta_time) override;
        void onContextMenuEvent(RefPtr<ContextMenuEvent> ev) override;
        void onDragEvent(RefPtr<DragEvent> ev) override;
        void onDropEvent(RefPtr<DropEvent> ev) override;

    private:
        PadData m_pad_data;
        PadRecorder m_pad_recorder;

        std::string m_file_name = "";

        bool m_is_recording_input = false;
        u32 m_last_recorded_frame = 0;

        size_t m_controller_port = 0;

        bool m_is_save_default_ready  = false;
        bool m_is_save_as_dialog_open = false;
        bool m_is_verify_open         = false;
    };

}  // namespace Toolbox::UI