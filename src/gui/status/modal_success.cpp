#include "gui/status/modal_success.hpp"
#include "platform/audio.hpp"

namespace Toolbox::UI {

    bool SuccessModal::open() {
        if (!m_is_open && !m_is_closed) {
            ImGui::OpenPopup(m_name.c_str());
            Platform::PlaySystemSound(Platform::SystemSound::S_SUCCESS);
            m_is_open = true;
            return true;
        }
        return false;
    }

    bool SuccessModal::render() {
        ImGuiWindowFlags modal_flags = ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoMove;

        ImGuiWindowClass modal_class;
        modal_class.ViewportFlagsOverrideSet =
            ImGuiViewportFlags_NoAutoMerge | ImGuiViewportFlags_TopMost;
        ImGui::SetNextWindowClass(&modal_class);

        ImVec2 modal_size = {500.0f * (ImGui::GetFontSize() / 16.0f), 0.0f};
        ImGui::SetNextWindowSize(modal_size);

        ImVec2 modal_pos = ImGui::GetIO().DisplaySize / 2.0f;
        modal_pos -= modal_size / 2.0f;
        modal_pos.y -= 200.0f;

        ImGui::SetNextWindowPos(modal_pos, ImGuiCond_Always);
        if (ImGui::BeginPopupModal(m_name.c_str(), &m_is_open, modal_flags)) {
            ImGui::TextWrapped("%s", m_message.c_str());
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                close();
            }
            ImGui::EndPopup();
            return true;
        }
        return false;
    }

    void SuccessModal::close() {
        if (m_is_open) {
            ImGui::CloseCurrentPopup();
        }
        m_is_closed = true;
        m_is_open   = false;
    }

}  // namespace Toolbox::UI