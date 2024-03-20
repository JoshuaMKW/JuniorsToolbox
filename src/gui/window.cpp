#include "gui/window.hpp"
#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuiFileDialog.h>

#include <IconsForkAwesome.h>
#include <gui/modelcache.hpp>

#include "gui/window.hpp"
#include <iconv.h>

namespace Toolbox::UI {

    void DockWindow::renderDockspace() {
        m_dockspace_id = ImGui::GetID(getWindowUID(*this).c_str());
        if (!ImGui::DockBuilderGetNode(m_dockspace_id)) {
            ImGui::DockBuilderAddNode(m_dockspace_id);

            buildDockspace(m_dockspace_id);

            ImGui::DockBuilderFinish(m_dockspace_id);
        }
        ImGui::DockSpace(m_dockspace_id, {}, ImGuiDockNodeFlags_None, nullptr);
    }

}  // namespace Toolbox::UI