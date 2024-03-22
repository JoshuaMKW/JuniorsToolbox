#pragma once

#include "core/event/event.hpp"
#include "core/time/timestep.hpp"
#include "unique.hpp"

namespace Toolbox {

    class ProcessLayer : public IEventProcessor, public IUnique {
    public:
        ProcessLayer()                         = default;
        ProcessLayer(const ProcessLayer &)     = delete;
        ProcessLayer(ProcessLayer &&) noexcept = delete;

        ProcessLayer(const std::string &name) : ProcessLayer() { m_name = name; }

        virtual ~ProcessLayer() = default;

        [[nodiscard]] const std::string &getName() const { return m_name; }
        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        virtual void onAttach() {}
        virtual void onDetach() {}
        virtual void onUpdate(TimeStep delta_time) {}
        virtual void onEvent(BaseEvent &ev) {}

        ProcessLayer &operator=(const ProcessLayer &)     = delete;
        ProcessLayer &operator=(ProcessLayer &&) noexcept = delete;

        bool operator==(const ProcessLayer &other) const { return m_uuid == other.m_uuid; }

    private:
        UUID64 m_uuid;
        std::string m_name = "Unnamed Layer";
    };

}  // namespace Toolbox