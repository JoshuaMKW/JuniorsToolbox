#include "gui/window.hpp"
#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuiFileDialog.h>

#include <gui/IconsForkAwesome.h>
#include <gui/modelcache.hpp>

#include "gui/window.hpp"
#include <iconv.h>

namespace Toolbox::UI {

    void DockWindow::renderDockspace() {
        ImGuiID dockspace_id = ImGui::GetID(getWindowUID(*this).c_str());
        if (!ImGui::DockBuilderGetNode(dockspace_id)) {
            ImGui::DockBuilderAddNode(dockspace_id);

            buildDockspace(dockspace_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }
        ImGui::DockSpace(dockspace_id, {}, ImGuiDockNodeFlags_PassthruCentralNode, nullptr);
    }

}  // namespace Toolbox::UI