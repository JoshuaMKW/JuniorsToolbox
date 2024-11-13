#include "gui/messages/window.hpp"

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
        renderSoundFrame();
        ImGui::SameLine();
        renderBackgroundPanel();
        renderDialogText();
        ImGui::SameLine();
        renderDialogMockup();
    }

    void BMGWindow::renderIndexPanel() {
        if (ImGui::BeginChild("Index Panel", {350, 0}, 0, ImGuiWindowFlags_NoDecoration)) {
            if (ImGui::BeginChild("Buttons", {150, 40}, 0)) {
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
            if (ImGui::BeginChild("Search", {0, 30})) {
                ImGui::InputText("##search", m_search_buffer, IM_ARRAYSIZE(m_search_buffer), 0);
            }
            ImGui::EndChild();
            if (ImGui::BeginChild("List")) {
                if (ImGui::BeginListBox("##messages")) {
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
        if (ImGui::BeginChild("Sound Frame")) {
            if (ImGui::BeginCombo("Sound ID", "MALE_PIANTA_SURPRISE")) {
                ImGui::Selectable("MALE_PIANTA_SURPRISE");
                ImGui::EndCombo();
            }
            ImGui::InputInt("Start Frame", &m_start_frame_val);
            ImGui::InputInt("End Frame", &m_end_frame_val);
        }
        ImGui::EndChild();
    }
    void BMGWindow::renderBackgroundPanel() {
        if (ImGui::BeginChild("Background Panel")) {
            ImGui::Text("Background");
            if (ImGui::BeginCombo("##character", "Pianta")){
                ImGui::Selectable("Pianta");
                ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("##role", "NPC")){
                ImGui::Selectable("NPC");
                ImGui::EndCombo();
            }
        }
        ImGui::EndChild();
    }
    void BMGWindow::renderDialogText() {
        if (ImGui::BeginChild("Dialog Text")) {
            ImGui::Text("This is some placeholder dialog");
        }
        ImGui::EndChild();
    }
    void BMGWindow::renderDialogMockup() {
        if (ImGui::BeginChild("Dialog Mockup")) {
        }
        ImGui::EndChild();
    }

    bool BMGWindow::onLoadData(std::filesystem::path data_path) { return true; }
}  // namespace Toolbox::UI
