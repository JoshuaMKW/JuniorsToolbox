#include "bmg/bmg.hpp"

#include "core/mathutil.hpp"
#include "gui/appmain/application.hpp"
#include "gui/appmain/bmgeditor/window.hpp"
#include "gui/appmain/bmgeditor/p_renderers.hpp"
#include "gui/logging/errors.hpp"
#include "spline.hpp"

constexpr ImVec4 MessageTintColor = ImVec4(1.0f, 1.0f, 1.0f, 0.8f);

namespace Toolbox::UI {

    bool BMGEditorViewDEBS::canPlayback() const { return true; }
    bool BMGEditorViewDEBS::isPlayback() const { return m_is_playback; }
    bool BMGEditorViewDEBS::startPlayback() {
        m_is_playback = true;
        return true;
    }
    bool BMGEditorViewDEBS::stopPlayback() {
        m_is_playback = false;
        return true;
    }

    bool BMGEditorViewDEBS::render(TimeStep delta_time, const ImRect &render_rect,
                                        const BMG::MessageData::Entry &message) {
        return false;
    }

}  // namespace Toolbox::UI