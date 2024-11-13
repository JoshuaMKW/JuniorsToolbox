#include "gui/messages/window.hpp"
#include "gui/application.hpp"
#include "resource/resource.hpp"
#include <iostream>

namespace Toolbox::UI {

    BMGWindow::BMGWindow(const std::string &name) : ImWindow(name) {}
    void BMGWindow::onRenderMenuBar() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File", true)) {
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Region", true)) {
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Packet Size", true)) {
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Other", true)) {
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }
    void BMGWindow::onRenderBody(TimeStep delta_time) {
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

    void BMGWindow::renderIndexPanel() {
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
                if (ImGui::BeginListBox("##messages", {225, 0})) {
                    ImGui::Selectable("Message 1");
                    ImGui::Selectable("Message 2");
                    ImGui::Selectable("Message 3");
                    ImGui::EndListBox();
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();
    }
    void BMGWindow::renderSoundFrame() {
        if (ImGui::BeginChild("Sound Frame", {250, 0})) {
            ImGui::Text("Sound ID:");
            ImGui::SameLine();
            if (ImGui::BeginCombo("##Sound ID", "MALE_PIANTA_SURPRISE")) {
                ImGui::Selectable("MALE_PIANTA_SURPRISE");
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
    void BMGWindow::renderBackgroundPanel() {
        if (ImGui::BeginChild("Background Panel", {185, 0})) {
            ImGui::Text("Background");
            ImGui::SetNextItemWidth(170);
            if (ImGui::BeginCombo("##character", "Pianta")) {
                ImGui::Selectable("Pianta");
                ImGui::EndCombo();
            }
            ImGui::SetNextItemWidth(170);
            if (ImGui::BeginCombo("##role", "NPC")) {
                ImGui::Selectable("NPC");
                ImGui::EndCombo();
            }
        }
        ImGui::EndChild();
    }
    void BMGWindow::renderDialogText() {
        if (ImGui::BeginChild("Dialog Text", {200, 0}, ImGuiChildFlags_Borders)) {
            ImGui::Text("This is some placeholder dialog");
        }
        ImGui::EndChild();
    }
    void BMGWindow::renderDialogMockup() {
        const ImGuiStyle &style = ImGui::GetStyle();
        if (ImGui::BeginChild("Dialog Mockup", {0, 0})) {
            ImVec2 pos = ImGui::GetCursorScreenPos() + style.WindowPadding;
            ImVec2 image_size(m_background_image->size().first, m_background_image->size().second);
            float aspect_ratio = image_size.y / image_size.x;
            ImVec2 avail       = ImGui::GetContentRegionAvail() - (style.WindowPadding * 2);
            ImVec2 computed_size(avail.x, avail.x * aspect_ratio);
            m_image_painter.render(*m_background_image, pos, computed_size);
        }
        ImGui::EndChild();
    }

    bool BMGWindow::onLoadData(std::filesystem::path data_path) {
        const ResourceManager &res_manager = GUIApplication::instance().getResourceManager();
        UUID64 fs_icons_uuid = res_manager.getResourcePathUUID("Images/medit/backgrounds");
        auto result = res_manager.getImageHandle("bmg_preview_shades_pianta.png", fs_icons_uuid);
        if (result) {
            m_background_image = result.value();
        } else {
            TOOLBOX_ERROR_V("[Medit] Failed to load background: {}, {}",
                            "bmg_preview_shades_pianta.png", result.error().m_message[0]);
        }
        return true;
    }
}  // namespace Toolbox::UI
