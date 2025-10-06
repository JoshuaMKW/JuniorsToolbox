#include "gui/debugger/dialog.hpp"
#include "gui/imgui_ext.hpp"

namespace Toolbox::UI {

    void FillBytesDialog::setup() {
        m_byte_value    = 0;
        m_insert_policy = InsertPolicy::INSERT_CONSTANT;
    }

    void FillBytesDialog::render(const AddressSpan &span) {
        if (m_opening) {
            ImGui::OpenPopup("Fill Bytes");
            m_open = true;
        }

        if (ImGui::BeginPopupModal("Fill Bytes", &m_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            m_opening = false;

            ImGui::TextAndWidth(150, "Hex Value: ");
            ImGui::SameLine();
            //ImGui::SetNextItemWidth(200.0f);

            ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal;

            ImGui::InputInt("##fill_byte_input", &m_byte_value, 1, 16, flags);
            m_byte_value = std::clamp<int>(m_byte_value, 0, 255);

            // Render the watch type selection combo box
            // ---
            {
                ImGui::TextAndWidth(150, "Fill Method: ");
                ImGui::SameLine();

                const char *watch_type_items[] = {"CONSTANT", "INCREMENT", "DECREMENT"};
                const char *current_watch_type =
                    watch_type_items[static_cast<int>(m_insert_policy)];

                // ImGui::SetNextItemWidth(150.0f);
                if (ImGui::BeginCombo("##watch_type_combo", current_watch_type)) {
                    for (int i = 0; i < IM_ARRAYSIZE(watch_type_items); ++i) {
                        if (ImGui::Selectable(watch_type_items[i],
                                              m_insert_policy == static_cast<InsertPolicy>(i))) {
                            m_insert_policy = static_cast<InsertPolicy>(i);
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            if (ImGui::Button("Apply")) {
                m_on_accept(span, m_insert_policy, (u8)m_byte_value);
                m_open = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel")) {
                if (m_on_reject) {
                    m_on_reject(span);
                }
                m_open = false;
            }

            ImGui::EndPopup();
        }
    }

}  // namespace Toolbox::UI