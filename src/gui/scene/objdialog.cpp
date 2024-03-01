#include "gui/scene/objdialog.hpp"
#include "gui/settings.hpp"
#include "objlib/template.hpp"
#include <algorithm>
#include <imgui.h>

namespace Toolbox::UI {

    void CreateObjDialog::setup() {
        m_templates.clear();
        m_templates = Object::TemplateFactory::createAll();

        std::sort(m_templates.begin(), m_templates.end(), [](auto &l, auto &r) {
            std::string_view l_type = l->type();
            std::string_view r_type = r->type();

            std::string l_lower(l_type.size(), '\0'), r_lower(r_type.size(), '\0');

            std::transform(l_type.begin(), l_type.end(), l_lower.begin(),
                           [](char c) { return std::tolower(c); });

            std::transform(r_type.begin(), r_type.end(), r_lower.begin(),
                           [](char c) { return std::tolower(c); });

            return l_lower < r_lower;
        });
        m_template_index = -1;
        // m_object_name.reserve(128);
    }

    static std::vector<std::string> s_better_sms_objects = {"GenericRailObj", "ParticleBox",
                                                            "SoundBox"};

    void CreateObjDialog::render(SelectionNodeInfo<Object::ISceneObject> node_info) {
        if (!m_open)
            return;

        constexpr ImGuiWindowFlags window_flags =
            ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize;

        constexpr float window_width = 400;
        const float items_width      = window_width - ImGui::GetStyle().WindowPadding.x * 2;

        ImGui::SetNextWindowSize({window_width, 0});

        if (m_opening) {
            ImGui::OpenPopup("New Object");
            m_opening = false;
        }

        if (ImGui::BeginPopupModal("New Object", &m_open, window_flags)) {
            ImGui::PushID("Search Templates");

            ImGui::Text("Find Template");

            ImGui::SetNextItemWidth(items_width - ImGui::GetItemRectSize().x -
                                    ImGui::GetStyle().ItemSpacing.x);

            ImGui::SameLine();
            m_template_filter.Draw("##search");
            ImGui::PopID();

            ImGui::SetNextItemWidth(items_width);

            // TODO: Render list of selectable templates to make an object from.
            if (ImGui::BeginListBox("##Template List")) {
                for (size_t i = 0; i < m_templates.size(); ++i) {
                    bool is_selected               = i == m_template_index;
                    std::string_view template_type = m_templates.at(i)->type();

                    const AppSettings &settings = SettingsManager::instance().getCurrentProfile();
                    if (!settings.m_is_better_obj_allowed) {
                        // Selectively remove extended objects for naive user sake
                        bool is_better_object =
                            std::any_of(s_better_sms_objects.begin(), s_better_sms_objects.end(),
                                        [&](const std::string &better_obj) {
                                            return template_type == better_obj;
                                        });
                        if (is_better_object) {
                            if (is_selected) {
                                m_template_index = -1;
                            }
                            continue;
                        }
                    }

                    if (!m_template_filter.PassFilter(
                            template_type.data(), template_type.data() + template_type.size())) {
                        if (is_selected) {
                            m_template_index = -1;
                        }
                        continue;
                    }

                    if (ImGui::Selectable(m_templates.at(i)->type().data(), &is_selected,
                                          ImGuiSelectableFlags_AllowDoubleClick)) {
                        if (m_template_index != i) {
                            m_wizard_index = -1;
                        }
                        m_template_index = static_cast<int>(i);
                    }

                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndListBox();
            }

            ImGui::SetNextItemWidth((items_width / 2) - (ImGui::GetStyle().ItemSpacing.x / 2));

            bool pre_state_invalid = m_template_index == -1 || m_wizard_index == -1;

            // TODO: Render list of selectable templates to make an object from.
            if (ImGui::BeginCombo("##wizard_select",
                                  pre_state_invalid ? "Instance Not Selected"
                                                    : m_templates.at(m_template_index)
                                                          ->wizards()
                                                          .at(m_wizard_index)
                                                          .m_name.c_str(),
                                  ImGuiComboFlags_PopupAlignLeft)) {
                if (m_template_index != -1) {
                    auto wizards                = m_templates.at(m_template_index)->wizards();
                    bool needs_internal_default = wizards.size() <= 1;

                    // True default should be skipped if a custom default is configured
                    for (size_t i = needs_internal_default ? 0 : 1; i < wizards.size(); ++i) {
                        bool is_selected             = i == m_wizard_index;
                        std::string_view wizard_name = wizards.at(i).m_name;

                        if (ImGui::Selectable(wizard_name.data(), &is_selected,
                                              ImGuiSelectableFlags_AllowDoubleClick)) {
                            m_wizard_index = static_cast<int>(i);
                        }
                    }
                }
                ImGui::EndCombo();
            }

            bool state_invalid = m_template_index == -1 || m_wizard_index == -1;

            std::string proposed_name =
                m_object_name.empty()
                    ? ""
                    : std::string(m_object_name.data(), std::strlen(m_object_name.data()));
            bool name_empty = proposed_name.empty();

            const char *hint_text = proposed_name.c_str();
            if (name_empty && !state_invalid) {
                proposed_name =
                    m_templates.at(m_template_index)->wizards().at(m_wizard_index).m_name;
                hint_text = proposed_name.c_str();
            } else if (name_empty) {
                hint_text = "Enter unique name here...";
            }

            ImGui::SameLine();

            ImGui::SetNextItemWidth((items_width / 2) - (ImGui::GetStyle().ItemSpacing.x / 2));

            // Used for duplicate name warning at end
            ImVec2 name_input_pos = ImGui::GetCursorPos();

            ImGui::InputTextWithHint("##name_input", hint_text, m_object_name.data(),
                                     m_object_name.size(), ImGuiInputTextFlags_AutoSelectAll);

            ImVec2 name_input_size = ImGui::GetItemRectSize();

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
                m_on_accept(0, proposed_name, *m_templates.at(m_template_index),
                            m_templates.at(m_template_index)->wizards().at(m_wizard_index).m_name,
                            node_info);
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

            // Detect duplicate name
            ISceneObject *this_parent;
            if (node_info.m_selected->isGroupObject()) {
                this_parent = node_info.m_selected.get();
            } else {
                this_parent = node_info.m_selected->getParent();
            }

            std::string name_state_msg = "Name is unique and has no collisions!";
            ImVec4 name_state_color    = {0.2f, 0.8f, 0.2f, 1.0f};

            if (this_parent) {
                auto children = std::move(this_parent->getChildren());
                auto sibling_it =
                    std::find_if(children.begin(), children.end(), [&](const auto &child) {
                        return child->getNameRef().name() == proposed_name;
                    });
                if (sibling_it != children.end()) {
                    name_state_msg   = "Name already exists! Proceed with caution.";
                    name_state_color = {0.8f, 0.2f, 0.1f, 1.0f};
                }
            }

            ImVec2 name_state_text_size = ImGui::CalcTextSize(name_state_msg.c_str());
            ImVec2 name_warning_pos     = {
                window_width - ImGui::GetStyle().WindowPadding.x - name_state_text_size.x,
                name_input_pos.y + name_input_size.y + ImGui::GetStyle().ItemSpacing.y};

            ImGui::SetCursorPos(name_warning_pos);
            ImGui::TextColored(name_state_color, name_state_msg.c_str());

            ImGui::EndPopup();
        }
    }

    void RenameObjDialog::setup() {}

    void RenameObjDialog::render(SelectionNodeInfo<Object::ISceneObject> node_info) {
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
                m_object_name.empty()
                    ? ""
                    : std::string(m_object_name.data(), std::strlen(m_object_name.data()));

            bool state_invalid    = object_name.empty();
            const char *hint_text = original_name.c_str();

            ImGui::Text("New Name");

            ImGui::SameLine();

            ImGui::SetNextItemWidth(items_width - ImGui::GetItemRectSize().x -
                                    ImGui::GetStyle().ItemSpacing.x);

            ImGui::InputTextWithHint("##name_input", hint_text, m_object_name.data(),
                                     m_object_name.size(), ImGuiInputTextFlags_AutoSelectAll);

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