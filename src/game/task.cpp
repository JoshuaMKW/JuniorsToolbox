#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "core/application.hpp"
#include "core/core.hpp"
#include "core/types.hpp"

#include "game/task.hpp"

#include "gui/settings.hpp"

using namespace Toolbox;

namespace Toolbox::Game {

    static std::vector<std::string>
    StringifySnapshot(const Interpreter::Register::RegisterSnapshot &snapshot) {
        std::vector<std::string> result;
        result.push_back(
            std::format("-- PC: 0x{:08X} --------------------------------------------------------",
                        snapshot.m_pc));
        for (size_t i = 0; i < 8; ++i) {
            result.push_back(std::format(
                "| r{:<2} = 0x{:08X}, r{:<2} = 0x{:08X}, r{:<2} = 0x{:08X}, r{:<2} = 0x{:08X} |", i,
                snapshot.m_gpr[i], i + 8, snapshot.m_gpr[i + 8], i + 16, snapshot.m_gpr[i + 16],
                i + 24, snapshot.m_gpr[i + 24]));
        }
        for (size_t i = 0; i < 8; ++i) {
            result.push_back(std::format(
                "| f{:<2} = {:.08f}, f{:<2} = {:.08f}, f{:<2} = {:.08f}, f{:<2} = {:.08f} |", i,
                snapshot.m_fpr[i].ps0AsDouble(), i + 8, snapshot.m_fpr[i + 8].ps0AsDouble(), i + 16,
                snapshot.m_fpr[i + 16].ps0AsDouble(), i + 24,
                snapshot.m_fpr[i + 24].ps0AsDouble()));
        }
        result.push_back(
            "--------------------------------------------------------------------------");
        return result;
    }

    enum class ETask {
        NONE,
        GET_NAMEREF_PTR,
        CREATE_NAMEREF,
        DELETE_NAMEREF,
        PLAY_CAMERA_DEMO,
        SET_NAMEREF_PARAMETER
    };

    void TaskCommunicator::tRun(void *param) {
        m_game_interpreter.setGlobalsPointerR(0x80416BA0);
        m_game_interpreter.setGlobalsPointerRW(0x804141C0);
        m_game_interpreter.onException([](u32 bad_instr_ptr, Interpreter::ExceptionCause cause,
                                          const Interpreter::Register::RegisterSnapshot &snapshot) {
            TOOLBOX_ERROR_V("[Interpreter] {} at PC = 0x{:08X}:", magic_enum::enum_name(cause),
                            bad_instr_ptr);
            std::vector<std::string> exception_message = StringifySnapshot(snapshot);
            for (auto &line : exception_message) {
                TOOLBOX_ERROR_V("[Interpreter] {}", line);
            }
        });
        m_game_interpreter.onInvalid([](u32 bad_instr_ptr, const std::string &cause,
                                        const Interpreter::Register::RegisterSnapshot &snapshot) {
            TOOLBOX_ERROR_V("[Interpreter] Invalid instruction at PC = {:08X} (Reason: {}):",
                            bad_instr_ptr, cause);
            std::vector<std::string> exception_message = StringifySnapshot(snapshot);
            for (auto &line : exception_message) {
                TOOLBOX_ERROR_V("[Interpreter] {}", line);
            }
        });
        m_game_interpreter.tStart(false, param);

        while (!m_kill_flag.load()) {
            AppSettings &settings = SettingsManager::instance().getCurrentProfile();

            DolphinCommunicator &communicator =
                MainApplication::instance().getDolphinCommunicator();

            /*m_game_interpreter.setMemoryBuffer(communicator.manager().getMemoryView(),
                                               communicator.manager().getMemorySize());*/

            // Dismiss tasks if disconnected to avoid errors
            if (communicator.manager().isHooked()) {
                while (!m_task_queue.empty()) {
                    std::unique_lock<std::mutex> lk(m_mutex);
                    std::function<bool(DolphinCommunicator &)> task = m_task_queue.front();
                    if (task(communicator)) {
                        m_task_queue.pop();
                    }
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
        }

        m_game_interpreter.tJoin();
        m_kill_condition.notify_all();
    }

    u32 TaskCommunicator::getActorPtr(RefPtr<ISceneObject> actor) {
        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();

        if (!communicator.manager().isHooked()) {
            return 0;
        }

        constexpr u32 application_addr = 0x803E9700;
        u8 game_stage                  = communicator.read<u8>(application_addr + 0xE).value();
        u8 game_scenario               = communicator.read<u8>(application_addr + 0xF).value();

        if (!isSceneLoaded(game_stage, game_scenario)) {
            return 0;
        }

        auto dolphin_interpreter = createInterpreterUnchecked();
        if (!dolphin_interpreter) {
            return 0;
        }

        auto result = String::toGameEncoding(actor->getNameRef().name());
        if (!result) {
            return 0;
        }

        constexpr u32 request_buffer_address = 0x80000FA0;

        Buffer &interpreter_buf = dolphin_interpreter->getMemoryBuffer();

        std::memset(interpreter_buf.buf<u8>() + (request_buffer_address - 0x80000000), '\0',
                    0x200);

        std::string actor_name = result.value();
        std::strncpy(interpreter_buf.buf<char>() + (request_buffer_address - 0x80000000),
                     actor_name.c_str(), actor_name.size());

        u32 namerefgen_addr = dolphin_interpreter->read<u32>(0x8040E408);
        u32 rootref_addr    = dolphin_interpreter->read<u32>(namerefgen_addr + 0x4);

        u32 argv[2]   = {rootref_addr, request_buffer_address};
        auto snapshot = dolphin_interpreter->evalFunction(0x80198d0c, 2, argv, 0, nullptr);

        return snapshot.m_gpr[3];
    }

    bool TaskCommunicator::isSceneLoaded(u8 stage, u8 scenario) {
        constexpr u8 c_mar_director_id = 5;
        constexpr u32 application_addr = 0x803E9700;

        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();

        // Early exit to avoid errors
        if (!communicator.manager().isHooked()) {
            return false;
        }

        auto game_stage_result = communicator.read<u8>(application_addr + 0xE);
        if (!game_stage_result) {
            logError(game_stage_result.error());
            return false;
        }

        auto game_scenario_result = communicator.read<u8>(application_addr + 0xF);
        if (!game_scenario_result) {
            logError(game_scenario_result.error());
            return false;
        }

        u8 game_stage    = game_stage_result.value();
        u8 game_scenario = game_scenario_result.value();

        // The game stage is not what we want, tell the game to reload is possible
        if (game_stage != stage || game_scenario != scenario) {
            return false;
        }

        // The mar director ain't directing
        if (communicator.read<u8>(application_addr + 0x8).value() != c_mar_director_id) {
            return false;
        }

        u32 mar_director_address = communicator.read<u32>(application_addr + 0x4).value();

        u8 mar_game_stage_result    = communicator.read<u8>(mar_director_address + 0x7C).value();
        u8 mar_game_scenario_result = communicator.read<u8>(mar_director_address + 0x7D).value();

        // The game stage is not what we want, tell the game to reload is possible
        if (mar_game_stage_result != stage || mar_game_scenario_result != game_scenario) {
            return false;
        }

        u16 mar_director_state = communicator.read<u32>(mar_director_address + 0x5C).value();
        if (mar_director_state == 0) {
            return false;
        }

        return true;
    }

    Result<void> TaskCommunicator::taskLoadScene(u8 stage, u8 scenario,
                                                 transact_complete_cb complete_cb) {
        constexpr u8 c_mar_director_id = 5;

        return submitTask(
            [&](Dolphin::DolphinCommunicator &communicator, u8 stage, u8 scenario,
                transact_complete_cb cb) {
                constexpr u32 application_addr = 0x803E9700;

                // Early exit to avoid errors
                if (!communicator.manager().isHooked()) {
                    return false;
                }

                u32 comm_state = communicator.read<u32>(0x80000298).value();

                auto game_stage_result = communicator.read<u8>(application_addr + 0xE);
                if (!game_stage_result) {
                    logError(game_stage_result.error());
                    return true;
                }

                auto game_scenario_result = communicator.read<u8>(application_addr + 0xF);
                if (!game_scenario_result) {
                    logError(game_scenario_result.error());
                    return true;
                }

                u8 game_stage    = game_stage_result.value();
                u8 game_scenario = game_scenario_result.value();

                // The game stage is not what we want, tell the game to reload is possible
                if (game_stage != stage || game_scenario != game_scenario) {
                    communicator.write<u8>(application_addr + 0x12, stage);
                    communicator.write<u8>(application_addr + 0x13, scenario);

                    // In this case TMarDirector is running and we should
                    // also tell it about the changes
                    if (communicator.read<u8>(application_addr + 0x8).value() ==
                        c_mar_director_id) {
                        // Check if already communicated
                        if (!(comm_state & BIT(31))) {
                            communicator.write<u32>(0x80000298, comm_state | BIT(31));

                            u32 mar_director_address =
                                communicator.read<u32>(application_addr + 0x4).value();

                            u16 mar_director_state =
                                communicator.read<u16>(mar_director_address + 0x4C).value();

                            u16 new_flags = 0;
                            if ((game_stage == 1 && stage == 5) ||
                                (game_stage == 1 && stage == 6) ||
                                (game_stage == 1 && stage == 11)) {
                                new_flags |= 0x8;
                            } else {
                                new_flags |= 0x2;
                            }
                            if (stage == 7) {
                                new_flags |= 0x100;
                            }

                            communicator.write<u16>(mar_director_address + 0x4C,
                                                    mar_director_state | new_flags);
                        }

                        return false;
                    } else {
                        // ... TODO: allow for direct boot out of other directors?

                        return false;
                    }
                }

                // Stage is loaded and the task was performed
                if (isSceneLoaded(stage, scenario)) {
                    if (cb)
                        cb(communicator.read<u32>(0x800002E8).value());
                    communicator.write<u32>(0x80000298, comm_state & ~BIT(31));
                    return true;
                }

                return false;
            },
            stage, scenario, complete_cb);
    }

    Result<void> TaskCommunicator::taskAddSceneObject(RefPtr<ISceneObject> object,
                                                      RefPtr<GroupSceneObject> parent,
                                                      transact_complete_cb complete_cb) {
        return submitTask(
            [&](Dolphin::DolphinCommunicator &communicator, RefPtr<ISceneObject> object,
                RefPtr<GroupSceneObject> parent, transact_complete_cb cb) {
                // Early exit to avoid errors
                if (!communicator.manager().isHooked()) {
                    return false;
                }

                u32 comm_state = communicator.read<u32>(0x80000298).value();

                // Check if already communicated
                if ((comm_state & BIT(30))) {
                    // If so, check for response
                    ETask task = (ETask)communicator.read<u32>(0x800002E0).value();
                    if (task == ETask::NONE) {
                        communicator.write<u32>(0x80000298, comm_state & ~BIT(30));

                        u32 response_buffer_address = communicator.read<u32>(0x800002E8).value();
                        if (response_buffer_address != 0) {
                            u32 obj_addr = communicator.read<u32>(response_buffer_address).value();
                            m_actor_address_map[object->getUUID()] = obj_addr;
                            if (cb)
                                cb(obj_addr);
                        } else {
                            if (cb)
                                cb(0);
                        }

                        return true;
                    } else {
                        return false;
                    }
                }

                u32 request_buffer_address = communicator.read<u32>(0x800002E4).value();
                if (request_buffer_address == 0) {
                    return true;
                }

                // Lock access
                communicator.write<u32>(0x80000298, comm_state | BIT(30));

                // Write cmd type
                communicator.write<u32>(0x800002E0, (u32)ETask::CREATE_NAMEREF);

                Buffer request_buffer;
                request_buffer.alloc(0x10000);
                request_buffer.initTo(0);

                {
                    char *internal_buf = request_buffer.buf<char>();

                    std::string _type = parent->type();
                    strncpy_s(internal_buf, _type.size() + 1, _type.c_str(), _type.size() + 1);

                    NameRef nameref           = parent->getNameRef();
                    std::string shift_jis_ref = String::toGameEncoding(nameref.name()).value();

                    strncpy_s(internal_buf + 0x80, shift_jis_ref.size() + 1, shift_jis_ref.data(),
                              shift_jis_ref.size() + 1);

                    std::span<u8> obj_data = object->getData();
                    memcpy_s(internal_buf + 0x200, obj_data.size() + 1, obj_data.data(),
                             obj_data.size() + 1);
                }

                communicator.writeBytes(request_buffer.buf<char>(), request_buffer_address,
                                        request_buffer.size());

                return false;
            },
            object, parent, complete_cb);
    }

    Result<void> TaskCommunicator::taskRemoveSceneObject(RefPtr<ISceneObject> object,
                                                         RefPtr<GroupSceneObject> parent,
                                                         transact_complete_cb complete_cb) {
        return submitTask(
            [&](Dolphin::DolphinCommunicator &communicator, RefPtr<ISceneObject> object,
                RefPtr<GroupSceneObject> parent, transact_complete_cb cb) {
                // Early exit to avoid errors
                if (!communicator.manager().isHooked()) {
                    return false;
                }

                u32 comm_state = communicator.read<u32>(0x80000298).value();

                // Check if already communicated
                if ((comm_state & BIT(29))) {
                    // If so, check for response
                    ETask task = (ETask)communicator.read<u32>(0x800002E0).value();
                    if (task == ETask::NONE) {
                        m_actor_address_map.erase(object->getUUID());
                        communicator.write<u32>(0x80000298, comm_state & ~BIT(29));
                        if (cb)
                            cb(communicator.read<u32>(0x800002E8).value());
                        return true;
                    } else {
                        return false;
                    }
                }

                u32 request_buffer_address = communicator.read<u32>(0x800002E4).value();
                if (request_buffer_address == 0) {
                    return true;
                }

                // Lock access
                communicator.write<u32>(0x80000298, comm_state | BIT(29));

                // Write cmd type
                communicator.write<u32>(0x800002E0, (u32)ETask::DELETE_NAMEREF);

                Buffer request_buffer;
                request_buffer.alloc(0x10000);
                request_buffer.initTo(0);

                {
                    char *internal_buf = request_buffer.buf<char>();

                    std::string _type = parent->type();
                    strncpy_s(internal_buf, _type.size() + 1, _type.c_str(), _type.size() + 1);

                    {
                        NameRef parent_nameref = parent->getNameRef();
                        std::string shift_jis_ref =
                            String::toGameEncoding(parent_nameref.name()).value();

                        strncpy_s(internal_buf + 0x80, shift_jis_ref.size() + 1,
                                  shift_jis_ref.data(), shift_jis_ref.size() + 1);
                    }

                    {
                        NameRef this_nameref = object->getNameRef();
                        std::string shift_jis_ref =
                            String::toGameEncoding(this_nameref.name()).value();

                        strncpy_s(internal_buf + 0x200, shift_jis_ref.size() + 1,
                                  shift_jis_ref.data(), shift_jis_ref.size() + 1);
                    }
                }

                communicator.writeBytes(request_buffer.buf<char>(), request_buffer_address,
                                        request_buffer.size());

                return false;
            },
            object, parent, complete_cb);
    }

    Result<void> TaskCommunicator::taskPlayCameraDemo(std::string_view demo_name,
                                                      transact_complete_cb complete_cb) {
        return Result<void>();
    }

    Result<void> TaskCommunicator::updateSceneObjectParameter(const QualifiedName &member_name,
                                                              size_t member_game_offset,
                                                              RefPtr<ISceneObject> object,
                                                              RefPtr<GroupSceneObject> parent) {
        return Result<void>();
    }

    Result<void> TaskCommunicator::setObjectTransformToMario(RefPtr<PhysicalSceneObject> object,
                                                             RefPtr<GroupSceneObject> parent) {
        return Result<void>();
    }

    Result<void> TaskCommunicator::setObjectTransformToCamera(RefPtr<PhysicalSceneObject> object,
                                                              RefPtr<GroupSceneObject> parent) {
        return Result<void>();
    }

    Result<void> TaskCommunicator::setCameraTransformToGameCamera(Transform &camera_transform) {
        return Result<void>();
    }

    Result<void> TaskCommunicator::setMarioToCameraTransform(const Transform &camera_transform) {
        return Result<void>();
    }

    u32 TaskCommunicator::captureXFBAsTexture(int width, int height) {
        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            return 0xFFFFFFFF;
        }

        u32 application_address = 0x803E9700;
        u32 display_address     = communicator.read<u32>(application_address + 0x1C).value();
        u32 xfb_address         = communicator.read<u32>(display_address + 0x8).value();
        u16 xfb_width           = communicator.read<u16>(display_address + 0x14).value();
        u16 xfb_height          = communicator.read<u16>(display_address + 0x18).value();

        return communicator.manager().captureXFBAsTexture(width, height, xfb_address, xfb_width,
                                                          xfb_height);
    }

    ScopePtr<Interpreter::SystemDolphin> TaskCommunicator::createInterpreter() {
        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();

        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("(TASK) Dolphin is not hooked!");
            return nullptr;
        }

        return createInterpreterUnchecked();
    }

    ScopePtr<Interpreter::SystemDolphin> TaskCommunicator::createInterpreterUnchecked() {
        DolphinCommunicator &communicator = MainApplication::instance().getDolphinCommunicator();

        auto dolphin_interpreter = Toolbox::make_scoped<Interpreter::SystemDolphin>();

        // Arbitrary based on BSMS allocation
        // TODO: Region unlock using game magic
        dolphin_interpreter->setStackPointer(0x804277E8);
        dolphin_interpreter->setGlobalsPointerR(0x80416BA0);
        dolphin_interpreter->setGlobalsPointerRW(0x804141C0);

        // Copy bytes so we only have to check hook status once
        // m_storage.resize(communicator.manager().getMemorySize());
        dolphin_interpreter->applyMemory(communicator.manager().getMemoryView(),
                                         communicator.manager().getMemorySize());

        return dolphin_interpreter;
    }

}  // namespace Toolbox::Game