#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "core/clipboard.hpp"
#include "game/task.hpp"
#include "gui/application.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/property/property.hpp"
#include "gui/scene/billboard.hpp"
#include "gui/scene/camera.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "gui/scene/objdialog.hpp"
#include "gui/scene/path.hpp"
#include "gui/scene/renderer.hpp"
#include "gui/window.hpp"

#include "gui/pad/window.hpp"
#include <gui/context_menu.hpp>
#include <imgui.h>

namespace Toolbox::UI {

    PadInputWindow::PadInputWindow() : ImWindow("Pad Recorder") {
        m_pad_recorder.tStart(false, nullptr);
    }

    PadInputWindow::~PadInputWindow() {}

    void PadInputWindow::onRenderMenuBar() {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open")) {
                /*auto path = Toolbox::openFileDialog();
                if (path) {
                    onLoadData(*path);
                }*/
            }

            if (ImGui::MenuItem("Save")) {
                /*auto path = Toolbox::saveFileDialog();
                if (path) {
                    onSaveData(*path);
                }*/
            }

            ImGui::EndMenu();
        }
    }

    void PadInputWindow::onRenderBody(TimeStep delta_time) {
        renderRecordPanel();
        renderControllerView();
        renderRecordedInputData();
    }

    void PadInputWindow::renderRecordPanel() {
        float window_bar_height =
            ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight();
        ImGui::SetCursorPos({0, window_bar_height + 10});

        const ImVec2 frame_padding  = ImGui::GetStyle().FramePadding;
        const ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

        const ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 cmd_button_size   = ImGui::CalcTextSize(ICON_FK_UNDO) + frame_padding;
        cmd_button_size.x        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.5f;
        cmd_button_size.y        = std::max(cmd_button_size.x, cmd_button_size.y) * 1.f;

        bool is_recording = m_pad_recorder.isRecording();

        if (is_recording) {
            ImGui::BeginDisabled();
        }

        ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.35f, 0.1f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.7f, 0.2f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.7f, 0.2f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2);
        if (ImGui::AlignedButton(ICON_FK_PLAY, cmd_button_size)) {
            m_pad_recorder.startRecording();
        }

        ImGui::PopStyleColor(3);

        if (is_recording) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.35f, 0.1f, 0.1f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.7f, 0.2f, 0.2f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.7f, 0.2f, 0.2f, 1.0f});

        if (!is_recording) {
            ImGui::BeginDisabled();
        }

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 + cmd_button_size.x);
        if (ImGui::AlignedButton(ICON_FK_STOP, cmd_button_size)) {
            m_pad_recorder.stopRecording();
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, {0.1f, 0.2f, 0.4f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.4f, 0.8f, 0.9f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.2f, 0.4f, 0.8f, 1.0f});

        ImGui::SetCursorPosX(window_size.x / 2 - cmd_button_size.x / 2 - cmd_button_size.x);
        if (ImGui::AlignedButton(ICON_FK_UNDO, cmd_button_size)) {
            m_pad_recorder.resetRecording();
        }

        ImGui::PopStyleColor(3);

        if (!is_recording) {
            ImGui::EndDisabled();
        }
    }

    void PadInputWindow::renderControllerView() {
        ImGui::BeginChild("Controller View", {0, 0}, true);

        ImGui::Text("Controller View");

        ImGui::EndChild();
    }

    void PadInputWindow::renderRecordedInputData() {
        ImGui::BeginChild("Recorded Input Data", {0, 0}, true);

        ImGui::Text("Recorded Input Data");

        ImGui::EndChild();
    }

    void PadInputWindow::loadMimePadData(Buffer &buffer) {}

    bool PadInputWindow::onLoadData(const std::filesystem::path &path) {
        if (!Toolbox::exists(path)) {
            return false;
        }

        if (Toolbox::is_regular_file(path)) {
            if (path.extension().string() != "pad") {
                return false;
            }

            std::ifstream instr = std::ifstream(path, std::ios::binary | std::ios::in);
            if (!instr.is_open()) {
                return false;
            }

            Deserializer in(instr.rdbuf());
            auto result = m_pad_data.deserialize(in);
            if (!result) {
                LogError(result.error());
                return false;
            }

            return true;
        }

        // TODO: Implement opening from archives.
        return false;
    }

    bool PadInputWindow::onSaveData(std::optional<std::filesystem::path> path) {
        if (!path) {
            return false;
        }

        std::ofstream outstr = std::ofstream(*path, std::ios::binary | std::ios::out);
        Serializer out(outstr.rdbuf());

        auto result = m_pad_data.serialize(out);
        if (!result) {
            LogError(result.error());
            return false;
        }

        return true;
    }

    void PadInputWindow::onImGuiUpdate(TimeStep delta_time) {}

    void PadInputWindow::onImGuiPostUpdate(TimeStep delta_time) {}

    void PadInputWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void PadInputWindow::onDragEvent(RefPtr<DragEvent> ev) {}

    void PadInputWindow::onDropEvent(RefPtr<DropEvent> ev) {}

}  // namespace Toolbox::UI