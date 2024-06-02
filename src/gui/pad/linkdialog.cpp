#include "gui/pad/linkdialog.hpp"
#include <imgui.h>
#include <unordered_map>

namespace Toolbox::UI {

    void CreateLinkDialog::setup() {}

    void CreateLinkDialog::render() {
        if (!m_open)
            return;

        ImGuiStyle &style = ImGui::GetStyle();

        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;

        constexpr float window_width = 250;
        const float items_width      = window_width - ImGui::GetStyle().WindowPadding.x * 2;

        ImGui::SetNextWindowSize({window_width, 0});

        if (m_opening) {
            ImGui::OpenPopup("Create Link");
            m_opening = false;
        }

        if (ImGui::BeginPopupModal("Create Link", &m_open, window_flags)) {
            const std::vector<ReplayLinkNode> &link_nodes = m_link_data->linkNodes();

            std::unordered_map<char, std::vector<char>> link_combos;

            for (size_t i = 0; link_nodes.size(); ++i) {
                std::vector<char> target_nodes;
                for (size_t j = 0; j < 3; ++j) {
                    if (link_nodes[i].m_infos[j].isSentinelNode()) {
                        continue;
                    }

                    target_nodes.push_back(link_nodes[i].m_infos[j].m_next_link);
                }
                link_combos['A' + i] = std::move(target_nodes);
            }

            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::BeginCombo("Source Node", &m_from_link, ImGuiComboFlags_PopupAlignLeft)) {
                for (auto &[from_link, targets] : link_combos) {
                    bool selected = from_link == m_from_link;
                    std::string from_link_str(1, from_link);
                    if (ImGui::Selectable(from_link_str.c_str(), &selected)) {
                        m_from_link = from_link;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::BeginCombo("Target Node", &m_to_link, ImGuiComboFlags_PopupAlignLeft)) {
                for (auto &[from_link, targets] : link_combos) {
                    bool is_source = from_link == m_from_link;
                    if (!is_source) {
                        continue;
                    }
                    for (char &to_link : targets) {
                        bool selected = to_link == m_to_link;
                        std::string to_link_str(1, to_link);
                        if (ImGui::Selectable(to_link_str.c_str(), &selected)) {
                            m_to_link = to_link;
                        }
                    }
                }
                ImGui::EndCombo();
            }

            bool state_invalid = isValidForCreate(m_from_link, m_to_link);
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

            ImVec2 create_size = ImGui::CalcTextSize("Create") + ImGui::GetStyle().FramePadding;
            ImVec2 cancel_size = ImGui::CalcTextSize("Cancel") + ImGui::GetStyle().FramePadding;

            ImVec2 window_size = ImGui::GetWindowSize();

            ImGui::SetCursorPosX(window_size.x - (create_size.x + cancel_size.x) - style.WindowPadding.x -
                                 style.ItemSpacing.x);

            if (ImGui::Button("Create")) {
                ImGui::CloseCurrentPopup();
                m_on_accept(m_from_link, m_to_link);
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
                ImGui::CloseCurrentPopup();
                m_on_reject(m_from_link, m_to_link);
                m_open = false;
            }

            ImGui::EndPopup();
        }
    }

    bool CreateLinkDialog::isValidForCreate(char from_link, char to_link) const { 
        if (from_link == '*' || to_link == '*') {
            return false;
        }

        const std::vector<ReplayLinkNode> &link_nodes = m_link_data->linkNodes();
        for (size_t i = 0; i < link_nodes.size(); ++i) {
            const ReplayLinkNode &node = link_nodes[i];

            char it_from_link = 'A' + i;
            if (it_from_link != from_link) {
                continue;
            }

            for (size_t j = 0; j < 3; ++j) {
                char it_to_link = node.m_infos[j].m_next_link;
                if (it_to_link == to_link) {
                    // Matching with existing link
                    return false;
                }
            }
        }

        return true;
    }

}  // namespace Toolbox::UI