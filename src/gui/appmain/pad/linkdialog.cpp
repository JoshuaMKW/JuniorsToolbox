#include "gui/appmain/pad/linkdialog.hpp"
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
            ImGui::OpenPopup("Connect Nodes");
            m_opening = false;
        }

        if (ImGui::BeginPopupModal("Connect Nodes", &m_open, window_flags)) {
            const std::vector<ReplayLinkNode> &link_nodes = m_link_data->linkNodes();

            std::unordered_map<char, std::vector<char>> link_combos;

            if (m_from_link == m_to_link) {
                m_to_link = '*';
            }

            for (size_t i = 0; i < link_nodes.size(); ++i) {
                std::vector<char> target_nodes;

                for (size_t j = 0; j < 3; ++j) {
                    if (link_nodes[i].m_infos[j].isSentinelNode()) {
                        continue;
                    }

                    target_nodes.push_back(link_nodes[i].m_infos[j].m_next_link);
                }

                if (!target_nodes.empty()) {
                    link_combos['A' + i] = std::move(target_nodes);
                }
            }

#if TOOLBOX_ENABLE_FUTURE_PAD_LINKS
            link_combos['A' + link_nodes.size()] = {};
#endif

            char selected_from_str[2] = {m_from_link, '\0'};
            char selected_to_str[2]   = {m_to_link, '\0'};

            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::BeginCombo("Source Node", selected_from_str,
                                  ImGuiComboFlags_PopupAlignLeft)) {
                for (auto &[from_link, targets] : link_combos) {
                    bool selected         = from_link == m_from_link;
                    char from_link_str[2] = {from_link, '\0'};
                    if (ImGui::Selectable(from_link_str, &selected)) {
                        m_from_link = from_link;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::BeginCombo("Target Node", selected_to_str, ImGuiComboFlags_PopupAlignLeft)) {
                for (auto &[from_link, targets] : link_combos) {
                    bool is_source = from_link == m_from_link;
                    if (!is_source) {
                        continue;
                    }

                    // Add existing nodes as targets
                    for (size_t i = 0; i < link_nodes.size(); ++i) {
                        char to_link  = 'A' + i;
                        if (to_link == m_from_link) {
                            continue;
                        }

                        bool is_taken =
                            std::any_of(targets.begin(), targets.end(),
                                        [to_link](char target) { return target == to_link; });
                        if (is_taken) {
                            continue;
                        }

                        bool selected       = to_link == m_to_link;
                        char to_link_str[2] = {to_link, '\0'};
                        if (ImGui::Selectable(to_link_str, &selected)) {
                            m_to_link = to_link;
                        }
                    }

#if TOOLBOX_ENABLE_FUTURE_PAD_LINKS
                    // Add a future node as a target
                    {
                        char to_link = 'A' + link_nodes.size();
                        if (to_link == m_from_link) {
                            continue;
                        }

                        bool is_taken =
                            std::any_of(targets.begin(), targets.end(),
                                        [to_link](char target) { return target == to_link; });
                        if (is_taken) {
                            continue;
                        }

                        bool selected       = to_link == m_to_link;
                        char to_link_str[2] = {to_link, '\0'};
                        if (ImGui::Selectable(to_link_str, &selected)) {
                            m_to_link = to_link;
                        }
                    }
#endif
                }
                ImGui::EndCombo();
            }

            bool state_valid = isValidForCreate(m_from_link, m_to_link);
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

            ImVec2 create_size = ImGui::CalcTextSize("Connect") + ImGui::GetStyle().FramePadding * 2;
            ImVec2 cancel_size = ImGui::CalcTextSize("Cancel") + ImGui::GetStyle().FramePadding * 2;

            ImVec2 window_size = ImGui::GetWindowSize();

            ImGui::SetCursorPosX(window_size.x - (create_size.x + cancel_size.x) -
                                 style.WindowPadding.x - style.ItemSpacing.x);

            if (ImGui::Button("Connect")) {
                ImGui::CloseCurrentPopup();
                m_on_accept(m_from_link, m_to_link);
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