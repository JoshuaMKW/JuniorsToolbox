#pragma once

#include <string>
#include <thread>

#include "core/application/layer.hpp"
#include "core/clipboard.hpp"
#include "core/event/event.hpp"
#include "core/time/timepoint.hpp"
#include "core/time/timestep.hpp"

#ifdef TOOLBOX_PLATFORM_WINDOWS

#define EXIT_CODE_OK              0
#define EXIT_CODE_FAILED_RUNTIME  (1 << 28) | 1
#define EXIT_CODE_FAILED_SETUP    (1 << 28) | 2
#define EXIT_CODE_FAILED_TEARDOWN (1 << 28) | 3

#elif TOOLBOX_PLATFORM_LINUX

#define EXIT_CODE_OK              0
#define EXIT_CODE_FAILED_RUNTIME  1
#define EXIT_CODE_FAILED_SETUP    2
#define EXIT_CODE_FAILED_TEARDOWN 3

#endif

namespace Toolbox {

    class CoreApplication : public IEventProcessor {
    protected:
        CoreApplication();

    public:
        CoreApplication(const std::string &app_name);

        CoreApplication(const CoreApplication &) = delete;
        CoreApplication(CoreApplication &&)      = delete;

        virtual ~CoreApplication() {}

        void run(int argc, const char **argv);
        void stop();

        const std::string &getAppName() const { return m_app_name; }
        int getExitCode() const { return m_exit_code; }

        // Override these functions to implement your application

        virtual void onInit(int argc, const char **argv) {}

        virtual void onUpdate(TimeStep delta_time) {
            for (auto &layer : m_layers) {
                layer->onUpdate(m_delta_time);
            }
        }

        virtual void onExit() {}

        // ------------------------------------------------------

        virtual void onEvent(RefPtr<BaseEvent> ev) override final;

        template <typename _Event, bool _Queue, typename... _Args>
        void dispatchEvent(_Args &&...args) {
            static_assert(std::is_base_of_v<BaseEvent, _Event>,
                          "Event must be derived from BaseEvent");
            RefPtr<_Event> ev = make_referable<_Event>(std::forward<_Args>(args)...);
            if constexpr (_Queue) {
                m_event_mutex.lock();
                m_events.emplace_back(ev);
                m_event_mutex.unlock();
            } else {
                onEvent(ev);
            }
        }

        void addLayer(RefPtr<ProcessLayer> layer);
        void removeLayer(RefPtr<ProcessLayer> layer);

        static SystemClipboard &GetClipboard() { return SystemClipboard::instance(); }

        static std::thread::id GetMainThreadId();

        CoreApplication &operator=(const CoreApplication &) = delete;
        CoreApplication &operator=(CoreApplication &&)      = delete;

    protected:
        void setExitCode(int exit_code) { m_exit_code = exit_code; }

    private:
        void setup(int argc, const char **argv);
        void teardown();

    private:
        bool m_is_running = false;
        int m_exit_code   = EXIT_CODE_OK;

        std::string m_app_name;

        size_t m_frame_counter = 0;
        TimePoint m_last_frame_time;
        TimeStep m_delta_time;

        std::mutex m_event_mutex;

        std::vector<RefPtr<ProcessLayer>> m_layers;
        std::vector<RefPtr<BaseEvent>> m_events;
    };

}  // namespace Toolbox