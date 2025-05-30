#include "gui/debugger/dialog.hpp"

namespace Toolbox::UI {

    void AddGroupDialog::setup() { memset(m_group_name.data(), 0, m_group_name.size()); }

    void AddGroupDialog::render(size_t selection_index) {
        const bool state_valid =
            m_filter_predicate ? m_filter_predicate(std::string_view(m_group_name.data(),
                                                                     strlen(m_group_name.data())))
                               : true;

        if (ImGui::BeginPopupModal("Add Group", &m_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Name: ");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

            ImGui::InputTextWithHint("##group_name", "Enter unique name here...",
                                     m_group_name.data(), m_group_name.size(),
                                     ImGuiInputTextFlags_AutoSelectAll);

            size_t group_name_length = strlen(m_group_name.data());
            std::string_view group_name_view(m_group_name.data(), group_name_length);

            if (!state_valid) {
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
                m_on_accept(selection_index, m_insert_policy, group_name_view);
                m_open = false;
            }

            if (!state_valid) {
                // End disabled block
                ImGui::EndDisabled();
                // Restore the modified style colors
                ImGui::PopStyleColor(3);
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                m_on_reject(selection_index);
                m_open = false;
            }

            ImGui::EndPopup();
        }
    }

}  // namespace Toolbox::UI