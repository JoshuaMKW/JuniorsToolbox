#include "gui/messages/window.hpp"

namespace Toolbox::UI {

    BMGWindow::BMGWindow(const std::string &name) : ImWindow(name) {
    }
    void BMGWindow::onRenderMenuBar() {
    }
    void BMGWindow::onRenderBody(TimeStep delta_time) {
    }

    bool BMGWindow::onLoadData(std::filesystem::path data_path) {
        return true;
    }
}
