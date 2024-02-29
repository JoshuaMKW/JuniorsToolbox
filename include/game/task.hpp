#pragma once

#include <expected>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

#include "core/error.hpp"
#include "core/memory.hpp"
#include "core/types.hpp"
#include "dolphin/interpreter/system.hpp"
#include "dolphin/process.hpp"
#include "gui/scene/camera.hpp"
#include "objlib/object.hpp"

using namespace Toolbox;

namespace Toolbox::Game {

    class TaskCommunicator : public Threaded<void> {
    public:
        TaskCommunicator() = default;

        using transact_complete_cb = std::function<void(u32)>;

        // API

        bool isSceneLoaded();
        bool isSceneLoaded(u8 stage, u8 scenario);

        u32 getActorPtr(RefPtr<ISceneObject> actor);

        Result<void> taskLoadScene(u8 stage, u8 scenario,
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
                                                RefPtr<ISceneObject> object);

        Result<void> setObjectTransformToMario(RefPtr<PhysicalSceneObject> object);
        Result<void> setObjectTranslationToMario(RefPtr<PhysicalSceneObject> object);

        Result<void> setCameraTransformToGameCamera(Camera &camera);

        Result<void> setMarioToCameraTransform(const Transform &camera_transform);

        Result<void> setObjectTransform(RefPtr<PhysicalSceneObject> object,
                                        const Transform &transform);

        u32 captureXFBAsTexture(int width, int height);

        ScopePtr<Interpreter::SystemDolphin> createInterpreter();
        ScopePtr<Interpreter::SystemDolphin> createInterpreterUnchecked();

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

        void tRun(void *param) override;

        constexpr f32 convertAngleS16ToFloat(s16 angle) {
            return static_cast<f32>(angle) / 182.04445f;
        }

        constexpr s16 convertAngleFloatToS16(f32 angle) {
            return static_cast<s16>(angle * 182.04445f);
        }

    private:
        Interpreter::SystemDolphin m_game_interpreter;

        std::queue<std::function<bool(Dolphin::DolphinCommunicator &)>> m_task_queue;
        std::unordered_map<UUID64, u32> m_actor_address_map;

        bool m_started = false;

        std::mutex m_mutex;
        std::thread m_thread;

        std::mutex m_interpreter_mutex;

        std::atomic<bool> m_hook_flag;

        std::atomic<bool> m_kill_flag;
        std::condition_variable m_kill_condition;
    };

}  // namespace Toolbox::Game