#include "gui/scene/raildialog.hpp"
#include "objlib/template.hpp"
#include <gui/imgui_ext.hpp>
#include <imgui.h>

namespace Toolbox::UI {

    void CreateRailDialog::setup() {
        m_rail_name  = {};
        m_node_count = 0;
    }

    void CreateRailDialog::render(SelectionNodeInfo<Rail::Rail> node_info) {
        if (!m_open)
            return;

        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;

        constexpr float window_width = 300;
        const float items_width      = window_width - ImGui::GetStyle().WindowPadding.x * 2;

        ImGui::SetNextWindowSize({window_width, 0});

        if (m_opening) {
            ImGui::OpenPopup("New Rail");
            m_opening = false;
        }

        if (ImGui::BeginPopupModal("New Rail", &m_open, window_flags)) {
            const float label_width =
                std::max(ImGui::CalcTextSize("Name").x, ImGui::CalcTextSize("Nodes").x);

            std::string proposed_name =
                m_rail_name.empty()
                    ? ""
                    : std::string(m_rail_name.data(), std::strlen(m_rail_name.data()));

            const char *hint_text = proposed_name.c_str();
            if (proposed_name.empty()) {
                hint_text = "Enter unique name here...";
            }

            ImGui::Text("Name");

            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize("Name").x, 0});
            ImGui::SameLine();

            ImGui::InputTextWithHint("##name_input", hint_text, m_rail_name.data(),
                                     m_rail_name.size(), ImGuiInputTextFlags_AutoSelectAll);

            bool state_invalid = m_rail_name.empty();

            ImGui::Text("Nodes");

            ImGui::SameLine();
            ImGui::Dummy({label_width - ImGui::CalcTextSize("Nodes").x, 0});
            ImGui::SameLine();

            u16 node_step = 1, node_step_fast = 10;
            ImGui::InputScalarCompact(
                "##NodeCount", ImGuiDataType_U16, &m_node_count, &node_step, &node_step_fast,
                nullptr, ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);

            if (state_invalid) {
                ImVec4 disabled_color = ImGui::GetStyle().Colors[ImGuiCol_Button];
                disabled_color.x -= 0.1f;
                disabled_color.y -= 0.1f;
                disabled_color.z -= 0.1f;

                // Change button color to make it look "disabled"
                ImGui::PushStyleColor(ImGuiCol_Button, disabled_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disabled_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, disabled_color);

                // Begin disabled block
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Create")) {
                m_on_accept(proposed_name, m_node_count);
                m_open = false;
            }

            if (state_invalid) {
                // End disabled block
                ImGui::EndDisabled();

                // Restore the modified style colors
                ImGui::PopStyleColor(3);
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                m_on_reject(node_info);
                m_open = false;
            }
            ImGui::EndPopup();
        }
    }

    void RenameRailDialog::setup() {}

    void RenameRailDialog::render(SelectionNodeInfo<Rail::Rail> node_info) {
        if (!m_open)
            return;

        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;

        constexpr float window_width = 250;
        const float items_width      = window_width - ImGui::GetStyle().WindowPadding.x * 2;

        ImGui::SetNextWindowSize({window_width, 0});

        if (m_opening) {
            ImGui::OpenPopup("Rename Object");
            m_opening = false;
        }

        if (ImGui::BeginPopupModal("Rename Object", &m_open, window_flags)) {
            std::string original_name =
                m_original_name.empty()
                    ? ""
                    : std::string(m_original_name.data(), std::strlen(m_original_name.data()));

            std::string object_name =
                m_rail_name.empty()
                    ? ""
                    : std::string(m_rail_name.data(), std::strlen(m_rail_name.data()));

            bool state_invalid    = object_name.empty();
            const char *hint_text = original_name.c_str();

            ImGui::Text("New Name");

            ImGui::SameLine();

            ImGui::SetNextItemWidth(items_width - ImGui::GetItemRectSize().x -
                                    ImGui::GetStyle().ItemSpacing.x);

            ImGui::InputTextWithHint("##name_input", hint_text, m_rail_name.data(),
                                     m_rail_name.size(), ImGuiInputTextFlags_AutoSelectAll);

            if (state_invalid) {
                ImVec4 disabled_color = ImGui::GetStyle().Colors[ImGuiCol_Button];
                disabled_color.x -= 0.1f;
                disabled_color.y -= 0.1f;
                disabled_color.z -= 0.1f;

                // Change button color to make it look "disabled"
                ImGui::PushStyleColor(ImGuiCol_Button, disabled_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disabled_color);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, disabled_color);

                // Begin disabled block
                ImGui::BeginDisabled();
            }

            if (ImGui::Button("Rename")) {
                m_on_accept(object_name, node_info);
                m_open = false;
            }

            if (state_invalid) {
                // End disabled block
                ImGui::EndDisabled();

                // Restore the modified style colors
                ImGui::PopStyleColor(3);
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                m_on_reject(node_info);
                m_open = false;
            }

            ImGui::EndPopup();
        }
    }

}  // namespace Toolbox::UI