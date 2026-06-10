#include "gui/appmain/status/modal_option.hpp"
#include "gui/appmain/application.hpp"

#include "platform/audio.hpp"

namespace Toolbox::UI {

    bool OptionModal::open() {
        if (!m_is_open && !m_is_closed) {
            ImGui::OpenPopup(m_name.c_str());
            Platform::PlaySystemSound(Platform::SystemSound::S_ERROR);
            m_is_open = true;
            return true;
        }
        return false;
    }

    bool OptionModal::render() {
        const ImGuiStyle &style = ImGui::GetStyle();

        ImGuiWindowFlags modal_flags = ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoMove;

        ImGuiWindowClass modal_class;
        modal_class.ViewportFlagsOverrideSet =
            ImGuiViewportFlags_NoAutoMerge | ImGuiViewportFlags_TopMost;
        ImGui::SetNextWindowClass(&modal_class);

        const float modal_scalar = ImGui::GetFontSize() / 16.0f;
        ImVec2 modal_size        = {500.0f * modal_scalar,
                             m_options.empty() ? 0.0f : 25.0f + (95.0f * modal_scalar)};
        ImGui::SetNextWindowSize(modal_size);

        ImVec2 modal_pos = m_parent ? m_parent->getPos() + m_parent->getSize() / 2.0f
                                    : ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(modal_pos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(m_name.c_str(), &m_is_open, modal_flags)) {
            ImGui::TextWrapped("%s", m_message.c_str());

            const ImVec2 avail_size = ImGui::GetContentRegionAvail();
            const ImVec2 panel_size = {avail_size.x, avail_size.y};

            int64_t option_index = -1;
            if (ImGui::BeginChild("##ChangesPanel", panel_size, ImGuiChildFlags_NavFlattened)) {
                int64_t i = 0;
                for (const auto &info : m_options) {
                    if (ImGui::Button(info.c_str(), ImVec2(100.0f * modal_scalar, 0))) {
                        option_index = i;
                        break;
                    }
                    i++;
                    ImGui::SameLine();
                }
            }
            ImGui::EndChild();

            if (option_index != -1) {
                m_option_cb(option_index);
                close();
            }

            ImGui::EndPopup();
            return true;
        }
        return false;
    }

    void OptionModal::close() {
        if (m_is_open) {
            ImGui::CloseCurrentPopup();
        }
        m_is_closed = true;
        m_is_open   = false;
    }

}  // namespace Toolbox::UI