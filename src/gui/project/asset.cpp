#pragma once

#include "fsystem.hpp"
#include "gui/project/asset.hpp"
#include "model/fsmodel.hpp"

namespace Toolbox::UI {

    void ProjectAsset::onRenderBody(TimeStep delta_time) {
        // Draw the asset icon
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {3, 3});

        if (m_model) {
            m_painter.render(m_model->getDecoration(m_index));
            ImGui::Text("%s", m_model->getDisplayText(m_index).c_str());
        }

        ImGui::PopStyleVar();
    }

}  // namespace Toolbox::UI