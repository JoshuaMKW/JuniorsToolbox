#include "gui/event/windowevent.hpp"

namespace Toolbox::UI {

    WindowEvent::WindowEvent(const UUID64 &target_id, TypeID type, float pos_x, float pos_y,
                             float size_x, float size_y)
        : BaseEvent(target_id, type), m_screen_pos_x(pos_x), m_screen_pos_y(pos_y), m_size_x(size_x),
          m_size_y(size_y) {}

    ScopePtr<ISmartResource> WindowEvent::clone(bool deep) const {
        return std::make_unique<WindowEvent>(*this);
    }

}  // namespace Toolbox