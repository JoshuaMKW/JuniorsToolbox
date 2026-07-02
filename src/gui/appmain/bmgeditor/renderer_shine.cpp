#include "bmg/bmg.hpp"

#include "core/mathutil.hpp"
#include "gui/appmain/application.hpp"
#include "gui/appmain/bmgeditor/window.hpp"
#include "gui/appmain/bmgeditor/p_renderers.hpp"
#include "gui/logging/errors.hpp"
#include "spline.hpp"

namespace Toolbox::UI {

    bool BMGEditorViewShineSelect::canPlayback() const { return false; }
    bool BMGEditorViewShineSelect::isPlayback() const { return false; }
    bool BMGEditorViewShineSelect::startPlayback() {
        return false;
    }
    bool BMGEditorViewShineSelect::stopPlayback() {
        return false;
    }

    bool BMGEditorViewShineSelect::render(TimeStep delta_time, const ImRect &render_rect,
                                        const BMG::MessageData::Entry &message) {
        return false;
    }

}  // namespace Toolbox::UI