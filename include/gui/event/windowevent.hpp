#pragma once

#include "core/input/input.hpp"
#include "gui/event/event.hpp"
#include <string>

namespace Toolbox::UI {

    class WindowEvent final : public BaseEvent {
    private:
        WindowEvent() = default;

    public:
        WindowEvent(const WindowEvent &)     = default;
        WindowEvent(WindowEvent &&) noexcept = default;

        WindowEvent(const UUID64 &target_id, TypeID type, const ImVec2 &geo_data);

        [[nodiscard]] bool isHideEvent() const noexcept { return getType() == EVENT_WINDOW_HIDE; }

        [[nodiscard]] bool isMoveEvent() const noexcept { return getType() == EVENT_WINDOW_MOVE; }

        [[nodiscard]] bool isResizeEvent() const noexcept {
            return getType() == EVENT_WINDOW_RESIZE;
        }

        [[nodiscard]] bool isShowEvent() const noexcept { return getType() == EVENT_WINDOW_SHOW; }

        [[nodiscard]] ImVec2 getGlobalPoint() const noexcept { return m_geo_data; }
        [[nodiscard]] ImVec2 getSize() const noexcept { return m_geo_data; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        WindowEvent &operator=(const WindowEvent &)     = default;
        WindowEvent &operator=(WindowEvent &&) noexcept = default;

    private:
        ImVec2 m_geo_data;
    };

}  // namespace Toolbox::UI