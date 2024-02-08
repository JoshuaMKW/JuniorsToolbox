#pragma once

#include <expected>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "core/memory.hpp"
#include "core/types.hpp"
#include "dolphin/process.hpp"
#include "core/error.hpp"
#include "objlib/object.hpp"

using namespace Toolbox;

namespace Toolbox::Game {

    class TaskCommunicator {
    public:
        TaskCommunicator() = default;

        Result<void> loadScene(u8 stage, u8 scenario);

        Result<void> addSceneObject(RefPtr<ISceneObject> object,
                                                      RefPtr<GroupSceneObject> parent);

        Result<void> removeSceneObject(RefPtr<ISceneObject> object,
                                                         RefPtr<GroupSceneObject> parent);

        Result<void> updateSceneObjectParameter(const QualifiedName &member_name,
                                                                  RefPtr<ISceneObject> object,
                                                                  RefPtr<GroupSceneObject> parent);

        Result<void> setObjectTransformToMario(RefPtr<ISceneObject> object,
                                                                 RefPtr<GroupSceneObject> parent);

        Result<void> setObjectTransformToCamera(RefPtr<ISceneObject> object,
                                                                  RefPtr<GroupSceneObject> parent);

        Result<void> playCameraDemo(std::string_view demo_name);

        Result<void> setCameraTransformToGameCamera(Transform &camera_transform);

        Result<void> setMarioToCameraTransform(const Transform &camera_transform);

    protected:
        template <typename _Callable, typename... _Args>
        Result<void, SerialError> submitTask(_Callable task, _Args... args) {
            m_task_queue.push([&](Dolphin::DolphinCommunicator &communicator) {
                return task(communicator, std::forward<_Args>(args)...);
            });
            return {};
        }

        void run();

    private:
        std::queue<std::function<bool(Dolphin::DolphinCommunicator &)>> m_task_queue;

        bool m_started = false;

        std::mutex m_mutex;
        std::thread m_thread;

        std::atomic<bool> m_hook_flag;

        std::atomic<bool> m_kill_flag;
        std::condition_variable m_kill_condition;
    };

}  // namespace Toolbox::Game