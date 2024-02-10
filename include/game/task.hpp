#pragma once

#include <expected>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "core/error.hpp"
#include "core/memory.hpp"
#include "core/types.hpp"
#include "dolphin/process.hpp"
#include "objlib/object.hpp"

using namespace Toolbox;

namespace Toolbox::Game {

    class TaskCommunicator {
    public:
        TaskCommunicator() = default;
        ~TaskCommunicator() { kill(); }

        using transact_complete_cb = std::function<void(u32)>;

        // Starts the detached thread
        void start() {
            if (!m_started) {
                m_thread  = std::thread(&TaskCommunicator::run, this);
                m_started = true;
            }
        }

        // Waits until the running thread is killed
        void kill() {
            m_kill_flag.store(true);
            m_kill_condition.notify_one();

            std::unique_lock<std::mutex> lk(m_mutex);
            m_kill_condition.wait(lk);
        }

        bool isSceneLoaded(u8 stage, u8 scenario);

        Result<void> taskLoadScene(u8 stage, u8 scenario,
                                   transact_complete_cb complete_cb = nullptr);

        Result<void> taskFindActorPtr(RefPtr<ISceneObject> actor,
                                      transact_complete_cb complete_cb = nullptr);

        Result<void> taskAddSceneObject(RefPtr<ISceneObject> object,
                                        RefPtr<GroupSceneObject> parent,
                                        transact_complete_cb complete_cb = nullptr);

        Result<void> taskRemoveSceneObject(RefPtr<ISceneObject> object,
                                           RefPtr<GroupSceneObject> parent,
                                           transact_complete_cb complete_cb = nullptr);

        Result<void> taskPlayCameraDemo(std::string_view demo_name,
                                        transact_complete_cb complete_cb = nullptr);

        Result<void> updateSceneObjectParameter(const QualifiedName &member_name,
                                                size_t member_game_offset,
                                                RefPtr<ISceneObject> object,
                                                RefPtr<GroupSceneObject> parent);

        Result<void> setObjectTransformToMario(RefPtr<PhysicalSceneObject> object,
                                               RefPtr<GroupSceneObject> parent);

        Result<void> setObjectTransformToCamera(RefPtr<PhysicalSceneObject> object,
                                                RefPtr<GroupSceneObject> parent);

        Result<void> setCameraTransformToGameCamera(Transform &camera_transform);

        Result<void> setMarioToCameraTransform(const Transform &camera_transform);

        u32 captureXFBAsTexture(int width, int height);

    protected:
        template <typename _Callable, typename... _Args>
        Result<void, SerialError> submitTask(_Callable task, _Args... args) {
            m_task_queue.push(std::bind(
                [task](Dolphin::DolphinCommunicator &communicator, _Args... _args) {
                    return task(communicator, std::forward<_Args>(_args)...);
                },
                std::placeholders::_1, std::forward<_Args>(args)...));
            return {};
        }

        void run();

    private:
        std::queue<std::function<bool(Dolphin::DolphinCommunicator &)>> m_task_queue;
        std::unordered_map<UUID64, u32> m_actor_address_map;

        bool m_started = false;

        std::mutex m_mutex;
        std::thread m_thread;

        std::atomic<bool> m_hook_flag;

        std::atomic<bool> m_kill_flag;
        std::condition_variable m_kill_condition;
    };

}  // namespace Toolbox::Game