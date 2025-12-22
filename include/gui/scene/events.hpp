#pragma once

#include "core/event/event.hpp"

namespace Toolbox::UI {

    enum ESceneEvent : Toolbox::BaseEvent::TypeID {
        SCENE_CREATE_RAIL_EVENT = Toolbox::BaseEvent::EVENT_USER_BEGIN,
        SCENE_DISABLE_CONTROL_EVENT,
        SCENE_ENABLE_CONTROL_EVENT,
    };

    class SceneCreateRailEvent : public Toolbox::BaseEvent {
    private:
        SceneCreateRailEvent() = default;

    public:
        SceneCreateRailEvent(const SceneCreateRailEvent &)     = default;
        SceneCreateRailEvent(SceneCreateRailEvent &&) noexcept = default;

        SceneCreateRailEvent(const UUID64 &target_id, const Rail::Rail &rail);

        [[nodiscard]] const Rail::Rail &getRail() const noexcept { return m_rail; }

        ScopePtr<ISmartResource> clone(bool deep) const override;

        SceneCreateRailEvent &operator=(const SceneCreateRailEvent &)     = default;
        SceneCreateRailEvent &operator=(SceneCreateRailEvent &&) noexcept = default;

    private:
        Rail::Rail m_rail;
    };

}