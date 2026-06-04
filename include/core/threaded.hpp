#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace Toolbox {

    template <typename _ExitT> class Threaded {
    protected:
        virtual _ExitT tRun(void *param) = 0;

    public:
        // Use member initializer list for primitives
        Threaded() = default;

        virtual ~Threaded() {
            // ALWAYS wait during destruction, even if detached.
            // If the object is destroyed while a detached thread is running,
            // the thread will use a dangling 'this' pointer and segfault.
            tKill(true);
        }

        // --- Rule of Five ---
        // Classes managing threads that capture 'this' cannot be safely copied or moved.
        // Explicitly deleting these prevents catastrophic use-after-free bugs.
        Threaded(const Threaded &)            = delete;
        Threaded &operator=(const Threaded &) = delete;
        Threaded(Threaded &&)                 = delete;
        Threaded &operator=(Threaded &&)      = delete;

        // Starts the thread
        void tStart(bool detached, void *param) {
            if (!_m_started.load()) {
                // Prevent std::terminate() by joining a previously finished, non-detached thread
                if (_m_thread.joinable()) {
                    _m_thread.join();
                }

                // Reset state
                _m_killed.store(false);
                _m_kill_flag.store(false);
                _m_detached.store(detached);
                _m_started.store(true);

                _m_thread = std::thread(&Threaded::tRun_, this, param);

                if (detached) {
                    _m_thread.detach();
                }
            }
        }

        // Call this from the main thread
        bool tJoin() {
            if (!_m_started.load()) {
                return false;
            }
            if (_m_killed.load()) {
                return false;
            }
            if (_m_detached.load()) {
                return false;
            }

            if (_m_thread.joinable()) {
                _m_thread.join();
                return true;
            }

            return false;
        }

        // Call this from the main thread
        void tKill(bool wait) {
            // Signal the thread to stop
            _m_kill_flag.store(true);

            if (!wait) {
                return;
            }

            if (_m_detached.load()) {
                std::unique_lock<std::mutex> lk(_m_mutex);
                // Fix: Condition variables require a predicate to prevent lost/spurious wakeups
                _m_kill_condition.wait(lk,
                                       [this]() { return _m_killed.load() || !_m_started.load(); });
            } else {
                if (_m_thread.joinable()) {
                    _m_thread.join();
                }
            }
        }

        bool tIsKilled() const { return _m_killed.load(); }

        bool tIsAlive() const { return _m_started.load() && !_m_killed.load(); }

    protected:
        bool tIsSignalKill() const { return _m_kill_flag.load(); }

    private:
        void tRun_(void *param) {
            if constexpr (std::is_void_v<_ExitT>) {
                tRun(param);
                if (_m_exit_cb) {
                    _m_exit_cb();
                }
            } else {
                _ExitT ret = tRun(param);
                if (_m_exit_cb) {
                    _m_exit_cb(ret);
                }
            }

            {
                // Lock the mutex before updating state to properly sync with wait()
                std::lock_guard<std::mutex> lk(_m_mutex);
                _m_started.store(false);
                _m_killed.store(true);
            }
            // Safely notify anyone waiting in tKill()
            _m_kill_condition.notify_all();
        }

    protected:
        std::atomic<bool> _m_started{false};
        std::atomic<bool> _m_detached{false};
        std::atomic<bool> _m_killed{false};

        std::mutex _m_mutex;
        std::thread _m_thread;

        std::atomic<bool> _m_kill_flag{false};
        std::condition_variable _m_kill_condition;

        std::function<void()> _m_start_cb;
        std::function<void(_ExitT)> _m_exit_cb;
    };

    template <typename _ExitT> class TaskThread : public Threaded<_ExitT> {
    public:
        using prog_cb_t = std::function<void(double)>;

        TaskThread() : Threaded<_ExitT>(), _m_progress(0.0) {}

        virtual ~TaskThread() = default;

        // Delete copy/move to adhere to base class constraints
        TaskThread(const TaskThread &)            = delete;
        TaskThread &operator=(const TaskThread &) = delete;
        TaskThread(TaskThread &&)                 = delete;
        TaskThread &operator=(TaskThread &&)      = delete;

        // Progress callback is hit when progress is updated internally.
        // Progress value is between 0 and 1. It is up to the calling thread
        // to understand what this progress value contextually means
        static void RequestProgress(TaskThread &task, prog_cb_t prog_cb) {
            task._m_progress_cb = prog_cb;
            task._m_prog_flag.store(true);
        }

        double getProgress() const { return _m_progress.load(); }

    protected:
        void setProgress(double progress) {
            _m_progress.store(progress);
            if (_m_prog_flag.load()) {
                if (_m_progress_cb) {
                    _m_progress_cb(progress);
                }
            }
        }

        std::atomic<double> _m_progress{0.0};
        prog_cb_t _m_progress_cb;

        std::mutex _m_prog_mutex;
        std::atomic<bool> _m_prog_flag{false};
        std::condition_variable _m_prog_condition;
    };

}  // namespace Toolbox
