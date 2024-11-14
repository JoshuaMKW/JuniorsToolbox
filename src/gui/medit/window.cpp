#include <cstdint>
#include <fstream>

#include "gui/application.hpp"
#include "gui/medit/window.hpp"
#include "resource/resource.hpp"

namespace Toolbox::UI {

    MeditWindow::MeditWindow(const std::string &name) : ImWindow(name) {}
    void MeditWindow::onRenderMenuBar() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Region", true)) {
                if (ImGui::RadioButton("NTSC-U", m_region == NTSCU)) {
                    m_region = NTSCU;
                }
                if (ImGui::RadioButton("PAL", m_region == PAL)) {
                    m_region = PAL;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Packet Size", true)) {
                if (ImGui::RadioButton("4 (System)", m_packet_size == 4)) {
                    m_packet_size = 4;
                }
                if (ImGui::RadioButton("8 (Unknown)", m_packet_size == 8)) {
                    m_packet_size = 8;
                }
                if (ImGui::RadioButton("12 (NPC)", m_packet_size == 12)) {
                    m_packet_size = 12;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }
    void MeditWindow::onRenderBody(TimeStep delta_time) {
        renderIndexPanel();
        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        if (ImGui::BeginChild("Right Panel")) {
            if (ImGui::BeginChild("Options Panel", {500, 100})) {
                renderSoundFrame();
                ImGui::SameLine();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();
                renderBackgroundPanel();
            }
            ImGui::EndChild();
            ImGui::Separator();
            renderDialogText();
            ImGui::SameLine();
            renderDialogMockup();
        }
        ImGui::EndChild();
    }

    void MeditWindow::renderIndexPanel() {
        if (ImGui::BeginChild("Index Panel", {225, 0}, 0, ImGuiWindowFlags_NoDecoration)) {
            if (ImGui::BeginChild("Buttons", {150, 40})) {
                if (ImGui::Button("New")) {
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                }
                ImGui::SameLine();
                if (ImGui::Button("Copy")) {
                }
            }
            ImGui::EndChild();
            if (ImGui::BeginChild("Search", {230, 40})) {
                ImGui::SetNextItemWidth(225);
                ImGui::InputTextWithHint("##search", "*Search for text here*", m_search_buffer,
                                         IM_ARRAYSIZE(m_search_buffer), 0);
            }
            ImGui::EndChild();
            if (ImGui::BeginChild("List", {230, 0})) {
                int unk_msg_num = 0;
                if (ImGui::BeginListBox("##messages", {225, 0})) {
                    for (auto const &[idx, entry] : std::views::enumerate(m_data.entries())) {
                        std::string message_name;
                        if (entry.m_name == "") {
                            message_name = std::format("message_unk_{}", idx);
                        } else {
                            message_name = entry.m_name;
                        }
                        if (ImGui::Selectable(message_name.c_str(), m_selected_msg_idx == idx)) {
                            m_selected_msg_idx = idx;
                        }
                    }
                    ImGui::EndListBox();
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
    void MeditWindow::renderSoundFrame() {
        if (ImGui::BeginChild("Sound Frame", {250, 0})) {
            ImGui::Text("Sound ID:");
            ImGui::SameLine();
            if (ImGui::BeginCombo("##Sound ID", ppMessageSound(m_sound))) {
                for (int i = 0; i < IM_ARRAYSIZE(Toolbox::BMG::ordered_sounds); i++) {
                    if (ImGui::Selectable(ppMessageSound(Toolbox::BMG::ordered_sounds[i]))) {
                        m_sound = Toolbox::BMG::ordered_sounds[i];
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Text("Start Frame");
            ImGui::SameLine();
            ImGui::InputInt("##Start Frame", &m_start_frame_val);
            ImGui::Text("End Frame");
            ImGui::SameLine();
            ImGui::InputInt("##End Frame", &m_end_frame_val);
        }
        ImGui::EndChild();
    }
    void MeditWindow::renderBackgroundPanel() {
        if (ImGui::BeginChild("Background Panel", {185, 0})) {
            ImGui::Text("Background");
            ImGui::SetNextItemWidth(170);
            if (ImGui::BeginCombo("##character", m_selected_background.c_str())) {
                for (auto &[key, value] : BackgroundMap()) {
                    if (ImGui::Selectable(key.c_str())) {
                        m_selected_background = key;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SetNextItemWidth(170);
            if (ImGui::BeginCombo("##role", "NPC")) {
                ImGui::Selectable("NPC");
                ImGui::Selectable("Board");
                ImGui::Selectable("DEBS");
                ImGui::EndCombo();
            }
        }
        ImGui::EndChild();
    }
    void MeditWindow::renderDialogText() {
        if (ImGui::BeginChild("Dialog Text", {200, 0}, ImGuiChildFlags_Borders)) {
            auto entry = m_data.getEntry(m_selected_msg_idx);
            TOOLBOX_ASSERT(entry.has_value(), "selected_msg_index out of bounds!");
            ImGui::Text(entry.value().m_message.getSimpleString().c_str());
        }
        ImGui::EndChild();
    }
    void MeditWindow::renderDialogMockup() {
        const ImGuiStyle &style = ImGui::GetStyle();
        if (ImGui::BeginChild("Dialog Mockup", {0, 0})) {
            ImVec2 pos = ImGui::GetCursorScreenPos() + style.WindowPadding;
            RefPtr<const ImageHandle> background_image = m_background_images[m_selected_background];
            ImVec2 image_size(background_image->size().first, background_image->size().second);
            float aspect_ratio = image_size.y / image_size.x;
            ImVec2 avail       = ImGui::GetContentRegionAvail() - (style.WindowPadding * 2);
            ImVec2 computed_size(avail.x, avail.x * aspect_ratio);
            m_image_painter.render(*background_image, pos, computed_size);
        }
        ImGui::EndChild();
    }

    bool MeditWindow::onLoadData(fs_path data_path) {
        const ResourceManager &res_manager = GUIApplication::instance().getResourceManager();
        UUID64 fs_icons_uuid = res_manager.getResourcePathUUID("Images/Medit/Backgrounds");
        for (auto &[key, value] : BackgroundMap()) {
            auto result = res_manager.getImageHandle(value, fs_icons_uuid);
            if (result) {
                m_background_images[key] = result.value();
            } else {
                TOOLBOX_ERROR_V("[Medit] Failed to load background: {}, {}", value.string(),
                                result.error().m_message[0]);
            }
        }
        m_selected_background = BackgroundMap()[0].first;
        std::ifstream file(data_path, std::ios::in | std::ios::binary);
        Deserializer in(file.rdbuf(), data_path.string());

        if (!m_data.deserialize(in)) {
            return false;
        }
        return true;
    }

    const std::vector<std::pair<std::string, fs_path>> &MeditWindow::BackgroundMap() {
        static std::vector<std::pair<std::string, fs_path>> s_background_map = {
            {"Pianta",  "bmg_preview_shades_pianta.png"},
            {"Tanooki", "bmg_preview_tanooki.png"      },
            {"Noki",    "bmg_preview_old_noki.png"     }
        };
        return s_background_map;
    }
}  // namespace Toolbox::UI
