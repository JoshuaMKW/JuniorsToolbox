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

#include <ImGuiFileDialog.h>
#include <imgui.h>

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
        ImGui::BeginChild("Controller View", {0, 300}, true);

        ImGui::Text("Controller View");

        ImGui::EndChild();
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

        if (ImGui::BeginCombo("Scene Context", window_preview.c_str(),
                              ImGuiComboFlags_PopupAlignLeft)) {
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

        ImGui::Separator();

        renderLinkDataState();

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