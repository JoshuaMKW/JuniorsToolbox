#pragma once

#include "core/event/fileevent.hpp"
#include "core/event/event.hpp"
#include "core/event/keyevent.hpp"
#include "core/event/shortcutevent.hpp"
#include "core/event/timerevent.hpp"
#include "core/time/timestep.hpp"
#include "unique.hpp"

namespace Toolbox {

    class ProcessLayer : public IEventProcessor, public IUnique {
    public:
        ProcessLayer()                         = default;
        ProcessLayer(const ProcessLayer &)     = delete;
        ProcessLayer(ProcessLayer &&) noexcept = delete;

        explicit ProcessLayer(const std::string &name) : ProcessLayer() { m_name = name; }

        virtual ~ProcessLayer() = default;

        [[nodiscard]] const std::string &name() const { return m_name; }
        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool isTargetOfEvent(const RefPtr<BaseEvent>& ev) const noexcept {
            return ev->getTargetId() == m_uuid;
        }

        virtual void onAttach() {}
        virtual void onDetach() {}
        virtual void onUpdate(TimeStep delta_time) {}

        virtual void onEvent(RefPtr<BaseEvent> ev);

        ProcessLayer &operator=(const ProcessLayer &)     = delete;
        ProcessLayer &operator=(ProcessLayer &&) noexcept = delete;

        bool operator==(const ProcessLayer &other) const { return m_uuid == other.m_uuid; }

    protected:
        virtual void onFileEvent(RefPtr<FileEvent> ev) {}
        virtual void onKeyEvent(RefPtr<KeyEvent> ev) {}
        virtual void onShortcutEvent(RefPtr<ShortcutEvent> ev) {}
        virtual void onTimerEvent(RefPtr<TimerEvent> ev) {}

    private:
        void propogateEvent(RefPtr<BaseEvent> ev);

        UUID64 m_uuid;
        std::string m_name = "Unnamed Layer";
        std::vector<RefPtr<ProcessLayer>> m_sublayers;
    };

}  // namespace Toolbox