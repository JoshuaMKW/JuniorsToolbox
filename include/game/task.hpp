#pragma once

#include <expected>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "core/memory.hpp"
#include "core/types.hpp"
#include "dolphin/process.hpp"
#include "error.hpp"
#include "objlib/object.hpp"

using namespace Toolbox;

namespace Toolbox::Game {

    class TaskCommunicator {
    public:
        TaskCommunicator() = default;

        std::expected<void, BaseError> loadScene(u8 stage, u8 scenario);

        std::expected<void, BaseError> addSceneObject(RefPtr<ISceneObject> object,
                                                      RefPtr<GroupSceneObject> parent);

        std::expected<void, BaseError> removeSceneObject(RefPtr<ISceneObject> object,
                                                         RefPtr<GroupSceneObject> parent);

        std::expected<void, BaseError> updateSceneObjectParameter(const QualifiedName &member_name,
                                                                  RefPtr<ISceneObject> object,
                                                                  RefPtr<GroupSceneObject> parent);

        std::expected<void, BaseError> setObjectTransformToMario(RefPtr<ISceneObject> object,
                                                                 RefPtr<GroupSceneObject> parent);

        std::expected<void, BaseError> setObjectTransformToCamera(RefPtr<ISceneObject> object,
                                                                  RefPtr<GroupSceneObject> parent);
        
        std::expected<void, BaseError> playCameraDemo(RefPtr<ISceneObject> object,
                                                      RefPtr<GroupSceneObject> parent);

        std::expected<void, BaseError> setCameraTransformToGameCamera(Transform &camera_transform);

        std::expected<void, BaseError> setMarioToCameraTransform(const Transform &camera_transform);


    protected:
        template <typename _Callable, typename... _Args>
        std::expected<void, BaseError> submitTask(_Callable task, _Args... args) {
            m_task_queue.push(
                [&](Dolphin::DolphinCommunicator &) { return task(std::forward<Args>(args)...); });
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