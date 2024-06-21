#include "gui/project/window.hpp"

namespace Toolbox::UI {

    ProjectViewWindow::ProjectViewWindow(const std::string &name) : ImWindow(name) {}

    void ProjectViewWindow::onRenderMenuBar() {}

    void ProjectViewWindow::onRenderBody(TimeStep delta_time) {}

    void ProjectViewWindow::renderProjectTreeView() {}

    void ProjectViewWindow::renderProjectFolderView() {}

    void ProjectViewWindow::renderProjectFolderButton() {}

    void ProjectViewWindow::renderProjectFileButton() {}

    void ProjectViewWindow::onAttach() {}

    void ProjectViewWindow::onDetach() {}

    void ProjectViewWindow::onImGuiUpdate(TimeStep delta_time) {}

    void ProjectViewWindow::onContextMenuEvent(RefPtr<ContextMenuEvent> ev) {}

    void ProjectViewWindow::onDragEvent(RefPtr<DragEvent> ev) {}

    void ProjectViewWindow::onDropEvent(RefPtr<DropEvent> ev) {}

} // namespace Toolbox::UI