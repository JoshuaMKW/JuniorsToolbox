#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <condition_variable>

namespace Toolbox {

    template <typename _ExitT> class Threaded {
    protected:
        virtual _ExitT tRun(void *param) = 0;

    public:
        virtual ~Threaded() { tKill(!_m_detached); }

        // Starts the detached thread
        void tStart(bool detached, void *param) {
            if (!_m_started) {
                _m_thread = std::thread(&Threaded::tRun_, this, param);
                _m_detached = detached;
                if (detached)
                    _m_thread.detach();
                _m_started = true;
            }
        }

        // Call this from the main thread
        bool tJoin() {
            if (!_m_started) {
                return false;
            }
            if (_m_killed) {
                return false;
            }
            if (_m_detached) {
                return false;
            }
            _m_thread.join();
            return true;
        }

        // Call this from the main thread
        void tKill(bool wait) {
            if (_m_killed || !_m_started) {
                return;
            }

            _m_kill_flag.store(true);

            if (!wait) {
                return;
            }

            if (_m_detached) {
                std::unique_lock<std::mutex> lk(_m_mutex);
                _m_kill_condition.wait(lk);
            } else {
                _m_thread.join();
            }
        }

        bool tIsKilled() const { return _m_killed; }

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
            _m_killed = true;
            _m_kill_condition.notify_all();
        }

        bool _m_started  = false;
        bool _m_detached = false;
        bool _m_killed   = false;

        std::mutex _m_mutex;
        std::thread _m_thread;

        std::atomic<bool> _m_kill_flag;
        std::condition_variable _m_kill_condition;

        std::function<void()> _m_start_cb;
        std::function<void()> _m_exit_cb;
    };

    template <typename _ExitT> class TaskThread : public Threaded<_ExitT> {
    public:
        // This is blocking. For non-blocking, use RequestProgress(TaskThread &, double *)
        static double RequestProgress(TaskThread &task) {
            task._m_prog_flag.store(true);

            std::unique_lock<std::mutex> lk(task._m_prog_mutex);
            task._m_prog_condition.wait(lk);
        }

        // Progress sent to out is between 0 and 1. It is up to the calling thread
        // to understand what this progress value contextually means
        static void RequestProgress(TaskThread &task, double *out) {
            if (!out)
                return;
            task._m_prog_flag.store(true);
            task._m_progress_out = out;
        }

    protected:
        double getProgress() const { return _m_progress; }
        void setProgress(double progress) {
            _m_progress = progress;
            if (_m_prog_flag.load()) {
                if (_m_progress_out) {
                    *_m_progress_out = progress;
                }
                _m_prog_condition.notify_all();
                if (progress == 1.0) {
                    _m_prog_flag.store(false);
                }
            }
        }

        double *_m_progress_out;
        double _m_progress;

        std::mutex _m_prog_mutex;
        std::atomic<bool> _m_prog_flag;
        std::condition_variable _m_prog_condition;
    };

}  // namespace Toolbox