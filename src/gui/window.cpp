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

    void ImWindow::onRenderDockspace() {
        if (!m_is_docking_set_up) {
            m_dockspace_id = onBuildDockspace();
            m_is_docking_set_up = true;
        }

        bool is_dockspace_avail = m_dockspace_id != std::numeric_limits<ImGuiID>::max() &&
                                  ImGui::DockBuilderGetNode(m_dockspace_id) != nullptr;
        
        if (is_dockspace_avail) {
            ImGui::DockSpace(m_dockspace_id, {}, ImGuiDockNodeFlags_None, nullptr);
        }
    }

}  // namespace Toolbox::UI