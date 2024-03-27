#include "gui/event/windowevent.hpp"

namespace Toolbox::UI {

    WindowEvent::WindowEvent(const UUID64 &target_id, TypeID type, const ImVec2 &geo_data)
        : BaseEvent(target_id, type), m_geo_data(geo_data) {}

    ScopePtr<ISmartResource> WindowEvent::clone(bool deep) const {
        return std::make_unique<WindowEvent>(*this);
    }

}  // namespace Toolbox