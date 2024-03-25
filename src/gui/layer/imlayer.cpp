#include "gui/layer/imlayer.hpp"

using namespace Toolbox;

namespace Toolbox::UI {

    ImLayer::ImLayer(const std::string &name) : ProcessLayer(name) {}

    void ImLayer::onUpdate(TimeStep delta_time) {
        onImGuiUpdate(delta_time);

        const std::string name = std::format("{}##{}", getName(), getUUID());
        if (ImGui::Begin(name.c_str())) {
            onImGuiRender(delta_time);
        }
        ImGui::End();
    }

    void ImLayer::onEvent(RefPtr<BaseEvent> ev) {
        ProcessLayer::onEvent(ev);
        if (ev->isAccepted() || ev->isIgnored()) {
            return;
        }
        switch (ev->getType()) {
        case EVENT_FOCUS_IN:
        case EVENT_FOCUS_OUT:
            // TODO: Call focus event
            break;
        case EVENT_WINDOW_HIDE:
        case EVENT_WINDOW_MOVE:
        case EVENT_WINDOW_RESIZE:
        case EVENT_WINDOW_SHOW:
            onWindowEvent(ref_cast<WindowEvent>(ev));
            break;
        }
    }

}  // namespace Toolbox::UI