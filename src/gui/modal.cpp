#include "gui/modal.hpp"

namespace Toolbox::UI {

    bool ImModal::onBeginWindow(const std::string &window_name, bool *is_open,
                                ImGuiWindowFlags flags) {
        // TODO: This should be handled by windowClass() once bugs are fixed in ImWindow.
        ImGuiWindowClass modal_class;
        modal_class.ViewportFlagsOverrideSet =
            ImGuiViewportFlags_NoAutoMerge | ImGuiViewportFlags_TopMost;
        ImGui::SetNextWindowClass(&modal_class);

        //ImVec2 modal_size = {500.0f * (ImGui::GetFontSize() / 16.0f), 0.0f};
        //ImGui::SetNextWindowSize(modal_size);

        ImVec2 modal_pos = m_parent ? m_parent->getPos() + m_parent->getSize() / 2.0f
                                    : ImGui::GetIO().DisplaySize / 2.0f;
        ImGui::SetNextWindowPos(modal_pos, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGui::OpenPopup(window_name.c_str());

        return ImGui::BeginPopupModal(window_name.c_str(), is_open, flags);
    }

    void ImModal::onEndWindow(bool did_render) {
        if (did_render) {
            ImGui::EndPopup();
        }
    }

}  // namespace Toolbox::UI
