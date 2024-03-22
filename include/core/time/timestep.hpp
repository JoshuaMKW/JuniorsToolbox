#pragma once

#include "core/time/timepoint.hpp"

namespace Toolbox {

    class TimeStep {
    public:
        TimeStep()                          = default;
        TimeStep(const TimeStep &other)     = default;
        TimeStep(TimeStep &&other) noexcept = default;

        explicit TimeStep(double elapsed_seconds) : m_duration(elapsed_seconds) {}
        TimeStep(TimePoint start, TimePoint end) : m_duration(end - start) {}

        float seconds() const { return m_duration.count(); }
        float milliseconds() const { return m_duration.count() * 1000.0; }

        operator float() const { return m_duration.count(); }

        TimeStep &operator=(const TimeStep &other)     = default;
        TimeStep &operator=(TimeStep &&other) noexcept = default;

    private:
        std::chrono::duration<double> m_duration;
    };

}  // namespace Toolbox