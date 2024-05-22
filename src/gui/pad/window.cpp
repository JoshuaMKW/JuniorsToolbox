#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/clipboard.hpp"
#include "core/memory.hpp"
#include "fsystem.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "smart_resource.hpp"

#include "game/task.hpp"
#include "gui/application.hpp"
#include "gui/context_menu.hpp"
#include "gui/image/imagepainter.hpp"
#include "gui/pad/window.hpp"
#include "gui/property/property.hpp"
#include "gui/scene/billboard.hpp"
#include "gui/scene/camera.hpp"
#include "gui/scene/nodeinfo.hpp"
#include "gui/scene/objdialog.hpp"
#include "gui/scene/path.hpp"
#include "gui/scene/renderer.hpp"
#include "gui/window.hpp"

#include "gui/imgui_ext.hpp"
#include <ImGuiFileDialog.h>
#include <imgui.h>

static const std::vector<ImVec2> s_xy_button_points = {
    {-0.4577f, 0.1615f },
    {-0.4477f, 0.0415f },
    {-0.4077f, -0.0305f},
    {-0.2977f, -0.1505f},
    {-0.0877f, -0.2705f},
    {0.0423f,  -0.3065f},
    {0.1923f,  -0.3185f},
    {0.3023f,  -0.3065f},
    {0.4023f,  -0.2825f},
    {0.4723f,  -0.1985f},
    {0.4923f,  -0.0905f},
    {0.4623f,  0.0175f },
    {0.3923f,  0.0895f },
    {0.3123f,  0.1255f },
    {0.1623f,  0.1495f },
    {0.0423f,  0.1975f },
    {-0.0277f, 0.2215f },
    {-0.1277f, 0.2935f },
    {-0.2277f, 0.3295f },
    {-0.3277f, 0.3295f },
    {-0.3777f, 0.2935f },
    {-0.4177f, 0.2455f },
};

static const std::vector<ImVec2> s_z_button_points = {
    ImVec2(-0.92, -0.49), ImVec2(-0.75, -0.5),  ImVec2(-0.65, -0.495), ImVec2(-0.45, -0.46),
    ImVec2(-0.35, -0.43), ImVec2(-0.15, -0.38), ImVec2(0.08, -0.3),    ImVec2(0.28, -0.2),
    ImVec2(0.45, -0.1),   ImVec2(0.58, 0.0),    ImVec2(0.70, 0.1),     ImVec2(0.82, 0.2),
    ImVec2(0.84, 0.25),   ImVec2(0.85, 0.30),   ImVec2(0.82, 0.4),     ImVec2(0.79, 0.45),
    ImVec2(0.76, 0.5),    ImVec2(-1.0, -0.4),   ImVec2(-0.97, -0.48),
};

static const std::vector<ImVec2> s_lr_button_points = {
    ImVec2(-0.95, 0.70), ImVec2(-0.90, 0.45),  ImVec2(-0.80, 0.20),  ImVec2(-0.67, -0.05),
    ImVec2(-0.49, -0.23), ImVec2(-0.25, -0.40), ImVec2(-0.05, -0.46), ImVec2(0.13, -0.48),
    ImVec2(0.35, -0.46), ImVec2(0.55, -0.40),  ImVec2(0.68, -0.32),  ImVec2(0.83, -0.22),
    ImVec2(0.92, -0.08), ImVec2(0.97, 0.02),   ImVec2(0.99, 0.14),   ImVec2(1.00, 0.30),
};

namespace Toolbox::UI {

    PadInputWindow::PadInputWindow() : ImWindow("Pad Recorder"), m_pad_rail("mariomodoki") {
        m_pad_recorder.tStart(false, nullptr);
        m_pad_recorder.onCreateLink(
            [this](const ReplayLinkNode &node) { tryReuseOrCreateRailNode(node); });
    }

    PadInputWindow::~PadInputWindow() {}

    void PadInputWindow::onRenderMenuBar() {
        if (ImGui::BeginMenuBar()) {
            bool is_recording = m_pad_recorder.isRecording();

            if (ImGui::BeginMenu("File")) {
                bool is_record_complete = !is_recording ? m_pad_recorder.isRecordComplete() : false;

                if (is_recording) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::MenuItem("Open")) {
                    m_is_open_dialog_open = true;
                }

                if (!m_load_path) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::MenuItem("Save")) {
                    if (!is_record_complete) {
                        TOOLBOX_ERROR("[PAD RECORD] Record is not complete. Please check remaining "
                                      "links for missing pad data.");
                    } else {
                        m_is_save_default_ready = true;
                        m_is_save_dialog_open   = true;
                    }
                }

                if (!m_load_path) {
                    ImGui::EndDisabled();
                }

                if (ImGui::MenuItem("Save As")) {
                    if (!is_record_complete) {
                        TOOLBOX_ERROR("[PAD RECORD] Record is not complete. Please check remaining "
                                      "links for missing pad data.");
                    } else {
                        m_is_save_dialog_open = true;
                    }
                }

                if (is_recording) {
                    ImGui::EndDisabled();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                if (is_recording) {
                    ImGui::BeginDisabled();
                }

                bool inverse_state = m_pad_recorder.isCameraInversed();
                if (ImGui::Checkbox("Inverse Camera Transform", &inverse_state)) {
                    m_pad_recorder.setCameraInversed(inverse_state);
                }

                if (is_recording) {
                    ImGui::EndDisabled();
                }

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Help")) {
                // TODO: Show help dialog
            }

            ImGui::EndMenuBar();
        }
    }

    void PadInputWindow::onRenderBody(TimeStep delta_time) {
        renderRecordPanel();
        renderControllerView();
        renderRecordedInputData();
        renderFileDialogs();
    }

    void PadInputWindow::renderRecordPanel() {
        float window_bar_height =
            ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeight();
        window_bar_height *= 2.0f;
        ImGui::SetCursorPos({0, window_bar_height + 5});

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

        if (!is_recording) {
            ImGui::EndDisabled();
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
    }

    void PadInputWindow::renderControllerView() {
        ImGui::PushStyleColor(ImGuiCol_WindowBg, {0.8, 0.2, 0.7, 1.0});

        ImGui::BeginChild("Controller View", {0, 300}, true);

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            ImGui::Text("Dolphin not connected.");
            ImGui::EndChild();
            ImGui::PopStyleColor();
            return;
        }

        u32 application_ptr = 0x803E9700;
        u32 gamepad_ptr =
            communicator.read<u32>(application_ptr + 0x20 + (m_pad_recorder.getPort() << 2))
                .value();

        bool is_connected = communicator.read<u8>(gamepad_ptr + 0x7A).value() != 0xFF;
        if (!is_connected) {
            TOOLBOX_ERROR("[PAD RECORD] Controller is not connected. Please ensure that "
                          "the controller is "
                          "connected and the input is being read by Dolphin.");
            return;
        }

        PadButtons held_buttons =
            static_cast<PadButtons>(communicator.read<u32>(gamepad_ptr + 0x18).value());
        PadButtons pressed_buttons =
            static_cast<PadButtons>(communicator.read<u32>(gamepad_ptr + 0x1C).value());

        u8 trigger_l = communicator.read<u8>(gamepad_ptr + 0x26).value();
        u8 trigger_r = communicator.read<u8>(gamepad_ptr + 0x27).value();

        f32 stick_x = communicator.read<f32>(gamepad_ptr + 0x48).value();
        f32 stick_y = communicator.read<f32>(gamepad_ptr + 0x4C).value();

        f32 c_stick_x = communicator.read<f32>(gamepad_ptr + 0x58).value();
        f32 c_stick_y = communicator.read<f32>(gamepad_ptr + 0x5C).value();

        f32 stick_mag   = communicator.read<f32>(gamepad_ptr + 0x50).value();
        s16 stick_angle = communicator.read<s16>(gamepad_ptr + 0x54).value();

        // L button
        {
            bool is_l_pressed = (held_buttons & PadButtons::BUTTON_L) != PadButtons::BUTTON_NONE;
            ImVec2 l_button_position = {65, 37};
            float l_button_radius    = 27.0f;
            ImU32 l_button_color  = is_l_pressed ? IM_COL32(240, 240, 240, 255)
                                                 : IM_COL32(160, 160, 160, 255);

            std::vector<ImVec2> points = s_lr_button_points;

            for (ImVec2 &point : points) {
                point *= l_button_radius;
                point += l_button_position;
                point.y += 10.0f * (trigger_l / 150.0f);
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK, l_button_color,
                                      2.0f);
        }

        // R button
        {
            bool is_r_pressed = (held_buttons & PadButtons::BUTTON_R) != PadButtons::BUTTON_NONE;
            ImVec2 r_button_position = {278, 37};
            float r_button_radius    = 28.0f;
            ImU32 r_button_color     = is_r_pressed ? IM_COL32(240, 240, 240, 255)
                                                    : IM_COL32(160, 160, 160, 255);

            std::vector<ImVec2> points = s_lr_button_points;

            for (ImVec2 &point : points) {
                point.x *= -1;
                point *= r_button_radius;
                point += r_button_position;
                point.y += 10.0f * (trigger_r / 150.0f);
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK, r_button_color,
                                      2.0f);
        }

        // Z button
        {
            bool is_z_pressed = (held_buttons & PadButtons::BUTTON_Z) != PadButtons::BUTTON_NONE;
            ImVec2 z_button_position = {280, 45};
            float z_button_radius    = 28.0f;
            ImU32 z_button_color     = is_z_pressed ? IM_COL32(140, 80, 240, 255)
                                                    : IM_COL32(80, 20, 200, 255);

            std::vector<ImVec2> points = s_z_button_points;

            if (!is_z_pressed) {
                for (ImVec2 &point : points) {
                    f32 x   = point.x + 1;
                    f32 y   = point.y + 1;
                    point.x = x * std::cos(-IM_PI / 64.0f) - y * std::sin(-IM_PI / 64.0f) - 1;
                    point.y = x * std::sin(-IM_PI / 64.0f) + y * std::cos(-IM_PI / 64.0f) - 1;
                }
            }

            for (ImVec2 &point : points) {
                point *= z_button_radius;
                point += z_button_position;
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK, z_button_color,
                                      2.0f);
        }

        // Control stick
        {
            float stick_radius    = 15.0f;
            ImVec2 stick_position = {70, 100};
            ImVec2 stick_tilted_position =
                stick_position + ImVec2(stick_x * stick_radius, -stick_y * stick_radius);
            ImU32 stick_color = IM_COL32(160, 160, 160, 255);

            ImGui::DrawNgon(8, stick_position, stick_radius + 7.5f, IM_COL32_BLACK, stick_color,
                            2.0f, 0.0f);
            ImGui::DrawCircle(stick_tilted_position, stick_radius, IM_COL32_BLACK, stick_color,
                              2.0f);
        }

        // C stick
        {
            float c_stick_radius    = 11.00f;
            ImVec2 c_stick_position = {230, 170};
            ImVec2 c_stick_tilted_position =
                c_stick_position + ImVec2(c_stick_x * c_stick_radius, -c_stick_y * c_stick_radius);
            ImU32 c_stick_color = IM_COL32(200, 160, 30, 255);

            ImGui::DrawNgon(8, c_stick_position, c_stick_radius + 11.50f, IM_COL32_BLACK,
                            c_stick_color, 2.0f, 0.0f);
            ImGui::DrawCircle(c_stick_tilted_position, c_stick_radius, IM_COL32_BLACK,
                              c_stick_color, 2.0f);
        }

        // A button
        {
            bool is_a_pressed = (held_buttons & PadButtons::BUTTON_A) != PadButtons::BUTTON_NONE;
            ImVec2 a_button_position = {270, 102};
            ImU32 a_button_color     = is_a_pressed ? IM_COL32(90, 210, 150, 255)
                                                    : IM_COL32(20, 170, 90, 255);

            ImGui::DrawCircle(a_button_position, 16.0f, IM_COL32_BLACK, a_button_color, 2.0f);
        }

        // B button
        {
            bool is_b_pressed = (held_buttons & PadButtons::BUTTON_B) != PadButtons::BUTTON_NONE;
            ImVec2 b_button_position = {238, 120};
            ImU32 b_button_color     = is_b_pressed ? IM_COL32(240, 130, 150, 255)
                                                    : IM_COL32(210, 30, 50, 255);

            ImGui::DrawCircle(b_button_position, 9.0f, IM_COL32_BLACK, b_button_color, 2.0f);
        }

        // X button
        {
            bool is_x_pressed = (held_buttons & PadButtons::BUTTON_X) != PadButtons::BUTTON_NONE;
            ImVec2 x_button_position = {300, 92};
            float x_button_radius    = 32.0f;
            ImU32 x_button_color     = is_x_pressed ? IM_COL32(240, 240, 240, 255)
                                                    : IM_COL32(160, 160, 160, 255);

            std::vector<ImVec2> points = s_xy_button_points;

            // Rotate clockwise by 45 degrees
            for (auto &point : points) {
                f32 x   = point.x;
                f32 y   = point.y;
                point.x = x * std::cos(IM_PI / 2.0f) - y * std::sin(IM_PI / 2.0f);
                point.y = x * std::sin(IM_PI / 2.0f) + y * std::cos(IM_PI / 2.0f);
            }

            for (auto &point : points) {
                point *= x_button_radius;
                point += x_button_position;
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK, x_button_color,
                                      2.0f);
        }

        // Y button
        {
            bool is_y_pressed = (held_buttons & PadButtons::BUTTON_Y) != PadButtons::BUTTON_NONE;
            ImVec2 y_button_position = {258, 71};
            float y_button_radius    = 32.0f;
            ImU32 y_button_color     = is_y_pressed ? IM_COL32(240, 240, 240, 255)
                                                    : IM_COL32(160, 160, 160, 255);

            std::vector<ImVec2> points = s_xy_button_points;
            for (ImVec2 &point : points) {
                point *= y_button_radius;
                point += y_button_position;
            }

            ImGui::DrawConcavePolygon(points.data(), points.size(), IM_COL32_BLACK, y_button_color,
                                      2.0f);
        }

        // Start button
        {
            bool is_start_pressed =
                (held_buttons & PadButtons::BUTTON_START) != PadButtons::BUTTON_NONE;
            ImVec2 start_button_position = {170, 105};
            ImU32 start_button_color     = is_start_pressed ? IM_COL32(240, 240, 240, 255)
                                                            : IM_COL32(160, 160, 160, 255);

            ImGui::DrawCircle(start_button_position, 7.0f, IM_COL32_BLACK, start_button_color,
                              2.0f);
        }

        // D-pad
        {
            bool is_up_pressed = (held_buttons & PadButtons::BUTTON_UP) != PadButtons::BUTTON_NONE;
            bool is_down_pressed =
                (held_buttons & PadButtons::BUTTON_DOWN) != PadButtons::BUTTON_NONE;
            bool is_left_pressed =
                (held_buttons & PadButtons::BUTTON_LEFT) != PadButtons::BUTTON_NONE;
            bool is_right_pressed =
                (held_buttons & PadButtons::BUTTON_RIGHT) != PadButtons::BUTTON_NONE;

            ImVec2 dpad_center = {110, 170};
            float dpad_radius  = 7.5f;
            float dpad_size    = 5.0f;

            ImU32 dpad_color         = IM_COL32(160, 160, 160, 255);
            ImU32 dpad_color_pressed = IM_COL32(240, 240, 240, 255);

            float angle_offset = IM_PI / 4.0f;

            ImVec2 up_position    = dpad_center + ImVec2(0, -dpad_radius * 2);
            ImVec2 down_position  = dpad_center + ImVec2(0, dpad_radius * 2);
            ImVec2 left_position  = dpad_center + ImVec2(-dpad_radius * 2, 0);
            ImVec2 right_position = dpad_center + ImVec2(dpad_radius * 2, 0);

            // Draw outline
            {
                // Draw each direction
                ImGui::DrawSquare(up_position, dpad_radius * 2, IM_COL32_BLACK, 0, 2.0f);
                ImGui::DrawSquare(down_position, dpad_radius * 2, IM_COL32_BLACK, 0, 2.0f);
                ImGui::DrawSquare(left_position, dpad_radius * 2, IM_COL32_BLACK, 0, 2.0f);
                ImGui::DrawSquare(right_position, dpad_radius * 2, IM_COL32_BLACK, 0, 2.0f);
            }

            // Draw center
            ImGui::DrawSquare(dpad_center, dpad_radius * 2, IM_COL32_BLACK, dpad_color, 0.0f);

            // Draw up
            {
                ImU32 up_color = is_up_pressed ? dpad_color_pressed : dpad_color;
                ImGui::DrawSquare(up_position, dpad_radius * 2, IM_COL32_BLACK, up_color, 2.0f);
                ImGui::DrawNgon(3, up_position - ImVec2(0.5f, 0.5f), dpad_size, IM_COL32_BLACK,
                                dpad_color, 0.0f, IM_PI * 1.5f);
            }

            // Draw down
            {
                ImU32 down_color = is_down_pressed ? dpad_color_pressed : dpad_color;
                ImGui::DrawSquare(down_position, dpad_radius * 2, IM_COL32_BLACK, down_color, 2.0f);
                ImGui::DrawNgon(3, down_position - ImVec2(0.5f, 0.5f), dpad_size, IM_COL32_BLACK,
                                dpad_color, 0.0f, IM_PI * 0.5f);
            }

            // Draw left
            {
                ImU32 left_color = is_left_pressed ? dpad_color_pressed : dpad_color;
                ImGui::DrawSquare(left_position, dpad_radius * 2, IM_COL32_BLACK, left_color, 2.0f);
                ImGui::DrawNgon(3, left_position - ImVec2(0.5f, 0.5f), dpad_size, IM_COL32_BLACK,
                                dpad_color, 0.0f, IM_PI);
            }

            // Draw right
            {
                ImU32 right_color = is_right_pressed ? dpad_color_pressed : dpad_color;
                ImGui::DrawSquare(right_position, dpad_radius * 2, IM_COL32_BLACK, right_color,
                                  2.0f);
                ImGui::DrawNgon(3, right_position - ImVec2(0.5f, 0.5f), dpad_size, IM_COL32_BLACK,
                                dpad_color, 0.0f, 0.0f);
            }
        }

        ImGui::EndChild();

        ImGui::PopStyleColor();
    }

    void PadInputWindow::renderRecordedInputData() {
        ImGui::BeginChild("Recorded Input Data", {0, 0}, true);

        std::string window_preview = "Scene not selected.";

        const std::vector<RefPtr<ImWindow>> &windows = GUIApplication::instance().getWindows();
        auto window_it =
            std::find_if(windows.begin(), windows.end(), [this](RefPtr<ImWindow> window) {
                return window->getUUID() == m_attached_scene_uuid;
            });
        if (window_it != windows.end()) {
            window_preview = (*window_it)->context();
        }

        ImGui::Text("Scene Context");
        ImGui::PushID("Scene Context Combo");
        if (ImGui::BeginCombo("", window_preview.c_str(), ImGuiComboFlags_PopupAlignLeft)) {
            for (RefPtr<const ImWindow> window : windows) {
                if (window->name() != "Scene Editor") {
                    continue;
                }
                bool selected = window->getUUID() == m_attached_scene_uuid;
                if (ImGui::Selectable(window->context().c_str(), &selected)) {
                    m_attached_scene_uuid = window->getUUID();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();

        ImGui::SameLine();

        if (m_attached_scene_uuid == 0) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Make Scene Rail")) {
            GUIApplication::instance().dispatchEvent<SceneCreateRailEvent, true>(
                m_attached_scene_uuid, m_pad_rail);
        }

        if (m_attached_scene_uuid == 0) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();

        renderLinkDataState();

        ImGui::EndChild();
    }

    void PadInputWindow::renderFileDialogs() {
        if (m_is_open_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog(
                "OpenPadDialog", "Choose Folder", nullptr,
                m_load_path ? m_load_path->string().c_str() : ".", "");
        }

        if (ImGuiFileDialog::Instance()->Display("OpenPadDialog")) {
            m_is_open_dialog_open = false;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();

                auto file_result = Toolbox::is_directory(path);
                if (!file_result) {
                    ImGuiFileDialog::Instance()->Close();
                    return;
                }

                if (!file_result.value()) {
                    ImGuiFileDialog::Instance()->Close();
                    return;
                }

                m_load_path = path;

                m_pad_recorder.resetRecording();
                m_pad_recorder.loadFromFolder(*m_load_path);

                ImGuiFileDialog::Instance()->Close();
                return;
            }
        }

        if (m_is_save_dialog_open) {
            if (!m_is_save_default_ready) {
                ImGuiFileDialog::Instance()->OpenDialog(
                    "SavePadDialog", "Choose Folder", nullptr,
                    m_load_path ? m_load_path->string().c_str() : ".", "");
            } else {
                m_pad_recorder.saveToFolder(*m_load_path);
                return;
            }
        }

        if (ImGuiFileDialog::Instance()->Display("SavePadDialog")) {
            m_is_save_dialog_open = false;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                m_load_path = ImGuiFileDialog::Instance()->GetFilePathName();

                m_pad_recorder.saveToFolder(*m_load_path);

                ImGuiFileDialog::Instance()->Close();
                return;
            }
        }

#if 0
        if (m_is_save_text_dialog_open) {
            ImGuiFileDialog::Instance()->OpenDialog(
                "SavePadTextDialog", "Choose File", ".txt",
                m_load_path ? m_load_path->string().c_str() : ".", "");
        }

        if (ImGuiFileDialog::Instance()->Display("SavePadTextDialog")) {
            m_is_save_dialog_open = false;
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::filesystem::path path = ImGuiFileDialog::Instance()->GetFilePathName();

                m_load_path = path.parent_path();
                m_file_path = path;

                if (m_file_path->extension() == ".txt") {
                    std::ofstream outstr = std::ofstream(*m_file_path, std::ios::out);
                    if (!outstr.is_open()) {
                        TOOLBOX_ERROR("[PAD RECORD] Failed to save .txt file.");
                        ImGuiFileDialog::Instance()->Close();
                        return;
                    }
                    m_pad_data.toText(outstr);
                    ImGuiFileDialog::Instance()->Close();
                    return;
                }

                TOOLBOX_ERROR("[PAD RECORD] Unsupported file type.");
                ImGuiFileDialog::Instance()->Close();
                return;
            }
        }
#endif
    }

    void PadInputWindow::renderLinkDataState() {
        ImGuiID id = ImGui::GetID("Link Data State");
        if (ImGui::BeginChildPanel(id, {0, 0})) {
            const std::vector<ReplayLinkNode> &link_nodes = m_pad_recorder.linkData().linkNodes();
            for (size_t i = 0; i < link_nodes.size(); ++i) {
                for (size_t j = 0; j < 3; ++j) {
                    char from_link = 'A' + i;
                    char to_link   = link_nodes[i].m_infos[j].m_next_link;
                    if (to_link == '*') {
                        continue;
                    }
                    bool is_recording = m_pad_recorder.isRecording(from_link, to_link);
                    if (m_pad_recorder.hasRecordData(from_link, to_link)) {
                        ImGui::Text("Link %c -> %c", from_link, to_link);
                        ImGui::SameLine();
                        if (ImGui::Button(is_recording ? ICON_FK_SQUARE : ICON_FK_UNDO)) {
                            is_recording ? m_pad_recorder.stopRecording()
                                         : m_pad_recorder.startRecording(from_link, to_link);
                        }
                    } else {
                        ImGui::Text("Link %c -> %c", from_link, to_link);
                        ImGui::SameLine();
                        if (ImGui::Button(is_recording ? ICON_FK_SQUARE : ICON_FK_CIRCLE)) {
                            is_recording ? m_pad_recorder.stopRecording()
                                         : m_pad_recorder.startRecording(from_link, to_link);
                        }
                    }
                }
            }
            ImGui::EndChildPanel();
        }
    }

    void PadInputWindow::loadMimePadData(Buffer &buffer) {}

    void PadInputWindow::tryReuseOrCreateRailNode(const ReplayLinkNode &link_node) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        u32 player_ptr                    = communicator.read<u32>(0x8040E108).value();
        if (!player_ptr) {
            if (m_pad_recorder.isCameraInversed()) {
                TOOLBOX_ERROR("[PAD RECORD] Player pointer is null. Please ensure that the game is "
                              "running and the player is loaded.");
                RefPtr<RailNode> node = make_referable<RailNode>(0, 0, 0);
                m_pad_rail.addNode(node);
                if (m_pad_rail.nodes().size() > 1) {
                    size_t prev_node_index = m_pad_rail.nodes().size() - 2;
                    if (prev_node_index == 0) {
                        m_pad_rail.connectNodeToNext(prev_node_index);
                    } else {
                        m_pad_rail.connectNodeToNeighbors(prev_node_index, true);
                    }
                    m_pad_rail.connectNodeToPrev(node);
                }
                return;
            }
            return;
        }
        glm::vec3 player_position = {communicator.read<f32>(player_ptr + 0x10).value(),
                                     communicator.read<f32>(player_ptr + 0x14).value(),
                                     communicator.read<f32>(player_ptr + 0x18).value()};

        std::vector<Rail::Rail::node_ptr_t> rail_nodes = m_pad_rail.nodes();
        auto node_it = std::find_if(rail_nodes.begin(), rail_nodes.end(),
                                    [&player_position](const Rail::Rail::node_ptr_t &node) {
                                        glm::vec3 node_pos = node->getPosition();
                                        glm::vec3 pos_diff = node_pos - player_position;
                                        return std::sqrtf(glm::dot(pos_diff, pos_diff)) < 100.0f;
                                    });
        if (node_it != rail_nodes.end()) {
            TOOLBOX_INFO("[PAD RECORD] Reusing existing rail node and snapping player to node.");
            glm::vec3 node_position = (*node_it)->getPosition();
            communicator.write<f32>(player_ptr + 0x10, node_position.x);
            communicator.write<f32>(player_ptr + 0x14, node_position.y);
            communicator.write<f32>(player_ptr + 0x18, node_position.z);
            return;
        }

        TOOLBOX_INFO("[PAD RECORD] Creating new rail node and snapping player to node.");

        glm::vec<3, s16> node_position = {static_cast<s16>(player_position.x),
                                          static_cast<s16>(player_position.y),
                                          static_cast<s16>(player_position.z)};
        RefPtr<RailNode> node =
            make_referable<RailNode>(node_position.x, node_position.y, node_position.z);
        m_pad_rail.addNode(node);
        if (m_pad_rail.nodes().size() > 1) {
            size_t prev_node_index = m_pad_rail.nodes().size() - 2;
            if (prev_node_index == 0) {
                m_pad_rail.connectNodeToNext(prev_node_index);
            } else {
                m_pad_rail.connectNodeToNeighbors(prev_node_index, true);
            }
            m_pad_rail.connectNodeToPrev(node);
        }

        communicator.write<f32>(player_ptr + 0x10, static_cast<f32>(node_position.x));
        communicator.write<f32>(player_ptr + 0x14, static_cast<f32>(node_position.y));
        communicator.write<f32>(player_ptr + 0x18, static_cast<f32>(node_position.z));
    }

    bool PadInputWindow::onLoadData(const std::filesystem::path &path) {
        auto file_result = Toolbox::is_directory(path);
        if (!file_result) {
            return false;
        }

        if (!file_result.value()) {
            return false;
        }

        if (Toolbox::is_directory(path)) {
            if (!path.filename().string().starts_with("pad")) {
                return false;
            }

            return m_pad_recorder.loadFromFolder(path);
        }

        // TODO: Implement opening from archives.
        return false;
    }

    bool PadInputWindow::onSaveData(std::optional<std::filesystem::path> path) {
        if (!path && !m_load_path) {
            return false;
        }
        return m_pad_recorder.saveToFolder(path ? *path : *m_load_path);
    }

    void PadInputWindow::onImGuiUpdate(TimeStep delta_time) {}

    void PadInputWindow::onImGuiPostUpdate(TimeStep delta_time) {}

    void PadInputWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void PadInputWindow::onDragEvent(RefPtr<DragEvent> ev) {}

    void PadInputWindow::onDropEvent(RefPtr<DropEvent> ev) {}

}  // namespace Toolbox::UI