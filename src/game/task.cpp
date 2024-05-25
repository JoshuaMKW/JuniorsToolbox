#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "core/core.hpp"
#include "core/types.hpp"

#include "game/task.hpp"

#include "gui/application.hpp"
#include "gui/settings.hpp"

using namespace Toolbox;

namespace Toolbox::Game {

    namespace {
        std::string StringifyFPR(const Interpreter::Register::FPR &fpr) {
            double fpr_val        = fpr.ps0AsDouble();
            std::string formatter = fpr_val < 0.0 ? "{:9.9g}" : "{:10.10g}";
            return std::format("{:10.09g}", fpr_val);
        }
    }  // namespace

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
            result.push_back(std::format("| f{:<2} = {}, f{:<2} = {}, f{:<2} = "
                                         "{}, f{:<2} = {} |",
                                         i, StringifyFPR(snapshot.m_fpr[i]), i + 8,
                                         StringifyFPR(snapshot.m_fpr[i + 8]), i + 16,
                                         StringifyFPR(snapshot.m_fpr[i + 16]), i + 24,
                                         StringifyFPR(snapshot.m_fpr[i + 24])));
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
        m_game_interpreter = Interpreter::SystemDolphin();

        m_game_interpreter.onException([](u32 bad_instr_ptr, Interpreter::ExceptionCause cause,
                                          const Interpreter::Register::RegisterSnapshot &snapshot) {
            TOOLBOX_ERROR_V("[Interpreter] {} at PC = 0x{:08X}, Last PC = 0x{:08X}:",
                            magic_enum::enum_name(cause), bad_instr_ptr, snapshot.m_last_pc);
            std::vector<std::string> exception_message = StringifySnapshot(snapshot);
            for (auto &line : exception_message) {
                TOOLBOX_ERROR_V("[Interpreter] {}", line);
            }
        });

        m_game_interpreter.onInvalid([](u32 bad_instr_ptr, const std::string &cause,
                                        const Interpreter::Register::RegisterSnapshot &snapshot) {
            TOOLBOX_ERROR_V("[Interpreter] Invalid instruction at PC = 0x{:08X}, Last PC = "
                            "0x{:08X} (Reason: {}):",
                            bad_instr_ptr, snapshot.m_last_pc, cause);
            std::vector<std::string> exception_message = StringifySnapshot(snapshot);
            for (auto &line : exception_message) {
                TOOLBOX_ERROR_V("[Interpreter] {}", line);
            }
        });

        m_game_interpreter.setGlobalsPointerR(0x80416BA0);
        m_game_interpreter.setGlobalsPointerRW(0x804141C0);

        while (!tIsSignalKill()) {
            AppSettings &settings = SettingsManager::instance().getCurrentProfile();

            DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

            m_game_interpreter.setMemoryBuffer(communicator.manager().getMemoryView(),
                                               communicator.manager().getMemorySize());
            checkForAcquiredStackFrameAndBuffer();

            // Dismiss tasks if disconnected to avoid errors
            while (!m_task_queue.empty()) {
                std::unique_lock<std::mutex> lk(m_mutex);
                std::function<bool(DolphinCommunicator &)> task = m_task_queue.front();
                if (task(communicator)) {
                    m_task_queue.pop();
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
        }
    }

    u32 TaskCommunicator::allocGameMemory(u32 heap_ptr, u32 size, u32 alignment) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 alloc_fn_ptr = 0;

        u32 vtable_ptr = communicator.read<u32>(heap_ptr).value();
        if (vtable_ptr == 0x803E0070) /* Solid Heap (USA) */ {
            alloc_fn_ptr = 0x802C48C4;
        } else if (vtable_ptr == 0x803DFF38) /* Exp Heap (USA) */ {
            alloc_fn_ptr = 0x802C14D0;
        }

        if (alloc_fn_ptr == 0) {
            return 0;
        }

        // u32 spoof_thread_id = static_cast<u32>(UUID64());
        // u32 spoof_thread_id = communicator.read<u32>(0x800000E4).value();
        u32 spoof_thread_id = 0x80002B00;

        // constructThread(spoof_thread_id, 0, 0, 0x80000A00, 0x1F0, 18, 0);

        // We must respect the mutex
        // lockMutex(spoof_thread_id, heap_ptr + 0x18);

        waitMutex(heap_ptr + 0x18);

        u32 args[3]   = {heap_ptr, size, alignment};
        auto snapshot = m_game_interpreter.evaluateFunction(alloc_fn_ptr, 3, args, 0, nullptr);

        // unlockMutex(spoof_thread_id, heap_ptr + 0x18);

        return static_cast<u32>(snapshot.m_gpr[3]);
    }

    bool TaskCommunicator::constructThread(u32 thread_ptr, u32 func, u32 parameter, u32 stack,
                                           u32 stackSize, u32 priority, u16 attributes) {
        u32 args[7] = {thread_ptr, func, parameter, stack, stackSize, priority, attributes};
        Interpreter::Register::RegisterSnapshot snapshot =
            m_game_interpreter.evaluateFunction(0x80348948, 7, args, 0, nullptr);
        return static_cast<bool>(snapshot.m_gpr[3]);
    }

    void TaskCommunicator::waitMutex(u32 mutex_ptr) {
        while (true) {
            DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
            AppSettings &settings             = SettingsManager::instance().getCurrentProfile();
            u32 mutex_lock_count              = communicator.read<u32>(mutex_ptr + 0xC).value();
            if (mutex_lock_count == 0) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
        }
    }

    void TaskCommunicator::lockMutex(u32 thread_ptr, u32 mutex_ptr) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        AppSettings &settings             = SettingsManager::instance().getCurrentProfile();

        while (true) {
            u32 mutex_thread_ptr = communicator.read<u32>(mutex_ptr + 0x8).value();
            u32 mutex_lock_count = communicator.read<u32>(mutex_ptr + 0xC).value();
            if (mutex_thread_ptr == 0) {
                communicator.write<u32>(mutex_ptr + 0x8, thread_ptr);
                communicator.write<u32>(mutex_ptr + 0xC, mutex_lock_count + 1);
                u32 thread_mutex_ptr = communicator.read<u32>(thread_ptr + 0x2F8).value();
                if (thread_mutex_ptr == 0) {
                    communicator.write<u32>(thread_ptr + 0x2F4, mutex_ptr);
                } else {
                    communicator.write<u32>(thread_ptr + 0x10, mutex_ptr);
                }
                communicator.write<u32>(mutex_ptr + 0x14, thread_mutex_ptr);
                communicator.write<u32>(mutex_ptr + 0x10, 0);
                communicator.write<u32>(thread_ptr + 0x2F8, mutex_ptr);
                return;
            } else if (mutex_thread_ptr == thread_ptr) {
                communicator.write<u32>(mutex_ptr + 0xC, mutex_lock_count + 1);
                return;
            }
            communicator.write<u32>(thread_ptr + 0x2F0, mutex_ptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(settings.m_dolphin_refresh_rate));
            communicator.write<u32>(thread_ptr + 0x2F0, 0);
        }
    }

    void TaskCommunicator::unlockMutex(u32 thread_ptr, u32 mutex_ptr) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 mutex_thread_ptr = communicator.read<u32>(mutex_ptr + 0x8).value();
        u32 mutex_lock_count = communicator.read<u32>(mutex_ptr + 0xC).value();
        if (mutex_thread_ptr == thread_ptr) {
            if (mutex_lock_count - 1 == 0) {
                communicator.write<u32>(mutex_ptr + 0x8, 0);
                communicator.write<u32>(mutex_ptr + 0xC, 0);
            } else {
                communicator.write<u32>(mutex_ptr + 0xC, mutex_lock_count - 1);
            }
            return;
        }
    }

    u32 TaskCommunicator::listInsert(u32 list_ptr, u32 iter_at, u32 item_ptr) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 prev_it = communicator.read<u32>(iter_at + 0x4).value();

        // Allocate the new bytes

        u32 current_heap_ptr = communicator.read<u32>(0x8040E294).value();
        if (current_heap_ptr == 0) {
            return 0;
        }

        // Evaluate for TNode_ type
        u32 node_ptr = allocGameMemory(current_heap_ptr, 0xC, 4);
        if (node_ptr == 0) {
            return 0;
        }

        // Initialize the new node
        communicator.write<u32>(node_ptr, iter_at);
        communicator.write<u32>(node_ptr + 0x4, prev_it);
        communicator.write<u32>(node_ptr + 0x8, item_ptr);

        // Update links
        communicator.write<u32>(iter_at + 0x4, node_ptr);
        communicator.write<u32>(prev_it, node_ptr);

        u32 list_size = communicator.read<u32>(list_ptr + 0x4).value();
        communicator.write<u32>(list_ptr + 0x4, list_size + 1);

        return node_ptr;
    }

    u32 TaskCommunicator::listBegin(u32 list_ptr) const {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        return communicator.read<u32>(list_ptr + 0x8).value();
    }

    u32 TaskCommunicator::listEnd(u32 list_ptr) const { return list_ptr + 0x8; }

    u32 TaskCommunicator::listNext(u32 iter) const {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        return communicator.read<u32>(iter).value();
    }

    u32 TaskCommunicator::listItem(u32 iter) const {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        return communicator.read<u32>(iter + 0x8).value();
    }

    void TaskCommunicator::listForEach(u32 list_ptr,
                                       std::function<void(DolphinCommunicator &, u32, u32)> fn) {
        TOOLBOX_ASSERT(fn, "Must provide a callback to listForEach!");
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        u32 list_it                       = listBegin(list_ptr);
        u32 list_end                      = listEnd(list_ptr);
        while (list_it && list_it != list_end) {
            fn(communicator, list_it, listItem(list_it));
            list_it = listNext(list_it);
        }
    }

    u32 TaskCommunicator::vectorInsert(u32 vector_ptr, u32 iter_at, u32 item_ptr) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        return 0;
    }

    u32 TaskCommunicator::vectorBegin(u32 vector_ptr) const {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        return communicator.read<u32>(vector_ptr + 0x4).value();
    }

    u32 TaskCommunicator::vectorEnd(u32 vector_ptr) const {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        return communicator.read<u32>(vector_ptr + 0x8).value();
    }

    u32 TaskCommunicator::vectorNext(u32 iter, u32 item_size) const { return iter + item_size; }

    u32 TaskCommunicator::vectorItem(u32 iter) const { return iter; }

    void TaskCommunicator::vectorForEach(u32 vector_ptr, u32 item_size,
                                         std::function<void(DolphinCommunicator &, u32)> fn) {
        TOOLBOX_ASSERT(fn, "Must provide a callback to vectorForEach!");
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        u32 vector_it                     = vectorBegin(vector_ptr);
        u32 vector_end                    = vectorEnd(vector_ptr);
        while (vector_it != vector_end) {
            fn(communicator, vectorItem(vector_it));
            vector_it = vectorNext(vector_it, item_size);
        }
    }

    bool TaskCommunicator::checkForAcquiredStackFrameAndBuffer() {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            m_game_interpreter.setStackPointer(0);
            return false;
        }

        // Here we use an unsafe buffer just to acquire
        // the true buffer through a call to JKRHeap::alloc
        if (!m_game_interpreter.isStackPointerValid()) {
            // Under this circumstance, the heap hasn't been initialized yet
            u32 system_heap_ptr = communicator.read<u32>(0x8040E290).value();
            if (system_heap_ptr == 0) {
                m_game_interpreter.setStackPointer(0);
                return false;
            }

            u32 cached_stack_ptr = communicator.read<u32>(0x800001C0).value();
            if (cached_stack_ptr != 0) {
                m_game_interpreter.setStackPointer(cached_stack_ptr);
            } else {
                // Memory below fst_ptr is the end of the heap, which we
                // can assume is unused (if it isn't, holy shit lol)
                u32 fst_ptr = communicator.read<u32>(0x80000034).value();
                m_game_interpreter.setStackPointer(fst_ptr - 0x100);

                {
                    // Get stack pointer
                    u32 alloc_ptr = allocGameMemory(system_heap_ptr, 0x2000, 0x4);
                    if (alloc_ptr == 0) {
                        TOOLBOX_ERROR("(Task) Failed to request memory for stack frame!");
                        return false;
                    }

                    m_game_interpreter.setStackPointer(alloc_ptr + 0x4000);
                    communicator.write<u32>(0x800001C0, alloc_ptr);
                }
            }

            u32 cached_buffer_ptr = communicator.read<u32>(0x800001C4).value();
            if (cached_buffer_ptr == 0) {
                // Get data buffer
                u32 alloc_ptr = allocGameMemory(system_heap_ptr, 0x8000, 0x4);
                if (alloc_ptr == 0) {
                    TOOLBOX_ERROR("(Task) Failed to request memory for stack frame!");
                    return false;
                }

                communicator.write<u32>(0x800001C4, alloc_ptr);
            }
        }

        return true;
    }

    u32 TaskCommunicator::getActorPtr(RefPtr<ISceneObject> actor) {
        if (u32 actor_ptr = actor->getGamePtr())
            return actor_ptr;

        if (!isSceneLoaded()) {
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

        std::memset(interpreter_buf.buf<u8>() + (request_buffer_address - 0x80000000), '\0', 0x200);

        std::string actor_name = result.value();
        std::strncpy(interpreter_buf.buf<char>() + (request_buffer_address - 0x80000000),
                     actor_name.c_str(), actor_name.size());

        u32 namerefgen_addr = dolphin_interpreter->read<u32>(0x8040E408);
        u32 rootref_addr    = dolphin_interpreter->read<u32>(namerefgen_addr + 0x4);

        u32 argv[2]   = {rootref_addr, request_buffer_address};
        auto snapshot = dolphin_interpreter->evaluateFunction(0x80198d0c, 2, argv, 0, nullptr);

        return static_cast<u32>(snapshot.m_gpr[3]);
    }

    u32 TaskCommunicator::getActorPtr(const std::string &name) {
        if (!isSceneLoaded()) {
            return 0;
        }

        auto dolphin_interpreter = createInterpreterUnchecked();
        if (!dolphin_interpreter) {
            return 0;
        }

        constexpr u32 request_buffer_address = 0x80000FA0;

        Buffer &interpreter_buf = dolphin_interpreter->getMemoryBuffer();

        std::memset(interpreter_buf.buf<u8>() + (request_buffer_address - 0x80000000), '\0', 0x200);

        std::strncpy(interpreter_buf.buf<char>() + (request_buffer_address - 0x80000000),
                     name.c_str(), name.size());

        u32 namerefgen_addr = dolphin_interpreter->read<u32>(0x8040E408);
        u32 rootref_addr    = dolphin_interpreter->read<u32>(namerefgen_addr + 0x4);

        u32 argv[2]   = {rootref_addr, request_buffer_address};
        auto snapshot = dolphin_interpreter->evaluateFunction(0x80198D0C, 2, argv, 0, nullptr);

        return static_cast<u32>(snapshot.m_gpr[3]);
    }

    bool TaskCommunicator::isSceneLoaded() {
        constexpr u8 c_mar_director_id = 5;
        constexpr u32 application_addr = 0x803E9700;

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        // Early exit to avoid errors
        if (!communicator.manager().isHooked()) {
            return false;
        }

        // The mar director ain't directing
        if (communicator.read<u8>(application_addr + 0x8).value() != c_mar_director_id) {
            return false;
        }

        u32 mar_director_address = communicator.read<u32>(application_addr + 0x4).value();

        u16 mar_director_frames = communicator.read<u32>(mar_director_address + 0x5C).value();
        if (mar_director_frames == 0) {
            return false;
        }

        return true;
    }

    bool TaskCommunicator::isSceneLoaded(u8 stage, u8 scenario) {
        constexpr u8 c_mar_director_id = 5;
        constexpr u32 application_addr = 0x803E9700;

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        // Early exit to avoid errors
        if (!communicator.manager().isHooked()) {
            return false;
        }

        auto game_stage_result = communicator.read<u8>(application_addr + 0xE);
        if (!game_stage_result) {
            LogError(game_stage_result.error());
            return false;
        }

        auto game_scenario_result = communicator.read<u8>(application_addr + 0xF);
        if (!game_scenario_result) {
            LogError(game_scenario_result.error());
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

        u16 mar_director_frames = communicator.read<u32>(mar_director_address + 0x5C).value();
        if (mar_director_frames == 0) {
            return false;
        }

        return true;
    }

    bool TaskCommunicator::getLoadedScene(u8 &stage, u8 &scenario) {
        constexpr u32 application_addr = 0x803E9700;

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        // Early exit to avoid errors
        if (!communicator.manager().isHooked()) {
            return false;
        }

        auto game_stage_result = communicator.read<u8>(application_addr + 0xE);
        if (!game_stage_result) {
            LogError(game_stage_result.error());
            return false;
        }

        auto game_scenario_result = communicator.read<u8>(application_addr + 0xF);
        if (!game_scenario_result) {
            LogError(game_scenario_result.error());
            return false;
        }

        stage    = game_stage_result.value();
        scenario = game_scenario_result.value();
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
                    LogError(game_stage_result.error());
                    return true;
                }

                auto game_scenario_result = communicator.read<u8>(application_addr + 0xF);
                if (!game_scenario_result) {
                    LogError(game_scenario_result.error());
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
        u32 parent_ptr = getActorPtr(parent);
        if (parent_ptr == 0) {
            return make_error<void>(
                "Task", "Failed to add object to game scene since parent isn't in game scene!");
        }

        if (parent->type() != "IdxGroup") {
            return make_error<void>(
                "Task", "Failed to add object to game scene since parent isn't IdxGroup!");
        }

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        // Get cached buffer ptr
        u32 buffer_ptr = communicator.read<u32>(0x800001C4).value();
        if (buffer_ptr == 0) {
            return make_error<void>(
                "Task", "Failed to add object to game scene (Claimed buffer doesn't exist)!");
        }

        std::span<u8> obj_data = object->getData();
        if (obj_data.size() > (0x10000 - 0x100)) {
            return make_error<void>(
                "Task", "Failed to add object to game scene (Obj data is too large for buffer)!");
        }

        communicator.writeBytes(reinterpret_cast<const char *>(obj_data.data()), buffer_ptr + 0x100,
                                obj_data.size());

        // Create this JSUMemoryInputStream
        // 0  - VTable
        // 4  - unknown (state?)
        // 8  - buffer
        // c  - length
        // 10 - position
        communicator.write<u32>(buffer_ptr, 0x803E01C8);
        communicator.write<u32>(buffer_ptr + 0x4, 0);
        communicator.write<u32>(buffer_ptr + 0x8, buffer_ptr + 0x100);
        communicator.write<u32>(buffer_ptr + 0xC, static_cast<u32>(obj_data.size()));
        communicator.write<u32>(buffer_ptr + 0x10, 0);

        // Create reference out JSUMemoryInputStream
        // 0  - VTable
        // 4  - unknown (state?)
        // 8  - buffer
        // c  - length
        // 10 - position
        communicator.write<u32>(buffer_ptr + 0x20, 0x803E01C8);
        communicator.write<u32>(buffer_ptr + 0x24, 0);
        communicator.write<u32>(buffer_ptr + 0x28, 0);
        communicator.write<u32>(buffer_ptr + 0x2C, 0);
        communicator.write<u32>(buffer_ptr + 0x30, 0);

        // genObject -> (in, ref_out)
        u32 args[2]       = {buffer_ptr, buffer_ptr + 0x20};
        auto gen_snapshot = m_game_interpreter.evaluateFunction(0x802FA598, 2, args, 0, nullptr);

        u32 nameref_ptr = static_cast<u32>(gen_snapshot.m_gpr[3]);
        if (nameref_ptr == 0) {
            return make_error<void>(
                "Task", "Failed to add object to game scene (Call to genObject returned nullptr)!");
        }

        // Here we insert our generated object pointer into the perform list
        // of our parent object...
        u32 parent_perform_list = parent_ptr + 0x10;
        u32 new_it = listInsert(parent_perform_list, listEnd(parent_perform_list), nameref_ptr);
        if (new_it == 0) {
            return make_error<void>(
                "Task", "Failed to add object to game scene (Parent insertion failed)!");
        }

        u32 vtable_ptr = communicator.read<u32>(nameref_ptr).value();

        {
            // Call virtual function load(JSUMemoryInputStream &in)
            u32 args[2] = {nameref_ptr, buffer_ptr + 0x20};

            u32 func_load_ptr = communicator.read<u32>(vtable_ptr + 0x10).value();

            m_game_interpreter.evaluateFunction(func_load_ptr, 2, args, 0, nullptr);
        }

        {
            // Call virtual function loadAfter()
            u32 args[1] = {nameref_ptr};

            u32 func_load_after_ptr = communicator.read<u32>(vtable_ptr + 0x18).value();

            m_game_interpreter.evaluateFunction(func_load_after_ptr, 1, args, 0, nullptr);
        }

        // Object is initialized successfully
        return {};
    }

    Result<void> TaskCommunicator::taskRemoveSceneObject(RefPtr<ISceneObject> object,
                                                         RefPtr<GroupSceneObject> parent,
                                                         transact_complete_cb complete_cb) {
        if (!isSceneLoaded())
            return make_error<void>(
                "Task", "Failed to remove object from game scene (Scene isn't loaded)!");

        u32 obj_ptr = getActorPtr(object);
        if (obj_ptr == 0) {
            return {};
        }

        u32 parent_ptr = getActorPtr(parent);
        if (parent_ptr == 0) {
            return make_error<void>(
                "Task", "Failed to remove object from game scene (Parent doesn't exist)!");
        }

        if (parent->type() != "IdxGroup") {
            return make_error<void>(
                "Task", "Failed to remove object from game scene (Parent isn't IdxGroup)!");
        }

        auto obj_game_key_result = String::toGameEncoding(object->getNameRef().name());
        if (!obj_game_key_result) {
            return make_error<void>(
                "Task", "Failed to remove object from game scene (Failed to encode object name)!");
        }

        std::string obj_game_key = obj_game_key_result.value();

        // Remove the object from the parent list
        listForEach(parent_ptr + 0x10,
                    [&](DolphinCommunicator &communicator, u32 iter_ptr, u32 item_ptr) {
                        u32 item_key_ptr = communicator.read<u32>(item_ptr + 4).value();
                        if (item_key_ptr == 0) {
                            return;
                        }

                        const char *item_key =
                            static_cast<const char *>(communicator.manager().getMemoryView()) +
                            item_key_ptr - 0x80000000;
                        if (obj_game_key == item_key) {
                            u32 next_ptr = communicator.read<u32>(iter_ptr).value();
                            u32 prev_ptr = communicator.read<u32>(iter_ptr + 0x4).value();

                            communicator.write<u32>(prev_ptr, next_ptr);
                            communicator.write<u32>(next_ptr + 0x4, prev_ptr);

                            u32 list_size = communicator.read<u32>(parent_ptr + 0x14).value();
                            communicator.write<u32>(parent_ptr + 0x14, list_size - 1);
                        }
                    });

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 conductor_ptr = communicator.read<u32>(0x8040D110).value();

        // Remove the object from the conducting behavior list
        listForEach(conductor_ptr + 0x10, [&](DolphinCommunicator &communicator, u32 iter_ptr,
                                              u32 item_ptr) {
            u32 obj_count     = communicator.read<u32>(item_ptr + 0x14).value();
            u32 obj_array_ptr = communicator.read<u32>(item_ptr + 0x18).value();
            for (u32 i = 0; i < obj_count; ++i) {
                u32 elem_array_at = obj_array_ptr + i * sizeof(u32);
                u32 elem_ptr      = communicator.read<u32>(elem_array_at).value();
                if (elem_ptr == obj_ptr) {
                    // This moves the future elements on top of the deleted one
                    char *buf = static_cast<char *>(communicator.manager().getMemoryView());
                    std::memmove(buf + elem_array_at - 0x80000000,
                                 buf + elem_array_at + 4 - 0x80000000, (obj_count - i) * 4);
                    communicator.write<u32>(item_ptr + 0x14, --obj_count);
                    break;
                }
            }
        });

        // TODO: Call delete on object pointer using game interpreter

        return {};
    }

    Result<void> TaskCommunicator::taskPlayCameraDemo(std::string_view demo_name,
                                                      transact_complete_cb complete_cb) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        if (!isSceneLoaded()) {
            return make_error<void>("Task",
                                    std::format("Failed to play camera demo \"{}\"!", demo_name));
        }

        u32 mar_director_address = communicator.read<u32>(0x803E9704).value();

        u8 cam_index = communicator.read<u8>(mar_director_address + 0x24C).value();
        u8 cam_max   = communicator.read<u8>(mar_director_address + 0x24D).value();
        if (((cam_index - cam_max) & 7) >= 7) {
            return make_error<void>("Task",
                                    std::format("Failed to play camera demo \"{}\"!", demo_name));
        }

        {
            u16 mar_director_state = communicator.read<u16>(mar_director_address + 0x4C).value();
            mar_director_state |= 0x40;
            communicator.write<u16>(mar_director_address + 0x4C, mar_director_state);
        }

        {
            constexpr u32 request_buffer_address = 0x80000FA0;
            constexpr u32 name_address           = request_buffer_address - 0x80000000;

            std::memset(static_cast<char *>(communicator.manager().getMemoryView()) + name_address,
                        '\0', 0x200);

            std::strncpy(static_cast<char *>(communicator.manager().getMemoryView()) + name_address,
                         demo_name.data(), demo_name.size());

            s32 offset = static_cast<s32>(cam_index * 0x24);
            communicator.write<u32>(mar_director_address + offset + 0x12C, name_address);
            communicator.write<u32>(mar_director_address + offset + 0x130, 0);
            communicator.write<s32>(mar_director_address + offset + 0x134, -1);
            communicator.write<f32>(mar_director_address + offset + 0x138, 0.0f);
            communicator.write<bool>(mar_director_address + offset + 0x13C, true);
            communicator.write<u32>(mar_director_address + offset + 0x140, 0);
            communicator.write<u32>(mar_director_address + offset + 0x144, 0);
            communicator.write<u32>(mar_director_address + offset + 0x148, 0);
            communicator.write<u16>(mar_director_address + offset + 0x14C, 0);
        }

        communicator.write<u8>(mar_director_address + 0x24C, (cam_index + 1) & 7);

        return {};
    }

    Result<void> TaskCommunicator::updateSceneObjectParameter(const QualifiedName &member_name,
                                                              size_t member_game_offset,
                                                              RefPtr<ISceneObject> object) {
        return Result<void>();
    }

    Result<void> TaskCommunicator::setObjectTransformToMario(RefPtr<PhysicalSceneObject> object) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        auto transform_result = object->getMember("Transform");
        if (!transform_result) {
            return make_error<void>("Task", transform_result.error().m_message);
        }

        // This also checks for connected Dolphin
        if (!isSceneLoaded()) {
            return make_error<void>("Task",
                                    "Failed to set object transform in scene (Scene not loaded)!");
        }

        u32 mario_ptr = communicator.read<u32>(0x8040E108).value();

        glm::vec3 translation = {
            communicator.read<f32>(mario_ptr + 0x10).value(),
            communicator.read<f32>(mario_ptr + 0x14).value(),
            communicator.read<f32>(mario_ptr + 0x18).value(),
        };

        glm::vec3 rotation = {
            communicator.read<f32>(mario_ptr + 0x30).value(),
            communicator.read<f32>(mario_ptr + 0x34).value(),
            communicator.read<f32>(mario_ptr + 0x38).value(),
        };

        RefPtr<MetaMember> transform_member = transform_result.value();
        Transform transform                 = getMetaValue<Transform>(transform_member, 0).value();
        transform.m_translation             = translation;
        transform.m_rotation                = rotation;
        auto result                         = setMetaValue(transform_member, 0, transform);
        if (!result) {
            return make_error<void>("Task",
                                    "Failed to set object transform in scene (Member not found)!");
        }

        return setObjectTransform(object, transform);
    }

    Result<void> TaskCommunicator::setObjectTranslationToMario(RefPtr<PhysicalSceneObject> object) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        auto transform_result = object->getMember("Transform");
        if (!transform_result) {
            return make_error<void>("Task", transform_result.error().m_message);
        }

        // This also checks for connected Dolphin
        if (!isSceneLoaded()) {
            return make_error<void>("Task", "Failed to set object transform in scene!");
        }

        u32 mario_ptr = communicator.read<u32>(0x8040E108).value();

        glm::vec3 translation = {
            communicator.read<f32>(mario_ptr + 0x10).value(),
            communicator.read<f32>(mario_ptr + 0x14).value(),
            communicator.read<f32>(mario_ptr + 0x18).value(),
        };

        RefPtr<MetaMember> transform_member = transform_result.value();
        Transform transform                 = getMetaValue<Transform>(transform_member, 0).value();
        transform.m_translation             = translation;
        auto result                         = setMetaValue(transform_member, 0, transform);
        if (!result) {
            return make_error<void>("Task",
                                    "Failed to set object transform in scene (Member not found)!");
        }

        return setObjectTransform(object, transform);
    }

    Result<void> TaskCommunicator::setCameraTransformToGameCamera(Camera &camera) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        // This also checks for connected Dolphin
        if (!isSceneLoaded()) {
            return make_error<void>("Task", "Failed to set object transform in scene!");
        }

        u32 camera_ptr = communicator.read<u32>(0x8040D0A8).value();

        glm::vec3 translation = {
            communicator.read<f32>(camera_ptr + 0x10).value(),
            communicator.read<f32>(camera_ptr + 0x14).value(),
            communicator.read<f32>(camera_ptr + 0x18).value(),
        };

        glm::vec3 up_vec = {
            communicator.read<f32>(camera_ptr + 0x30).value(),
            communicator.read<f32>(camera_ptr + 0x34).value(),
            communicator.read<f32>(camera_ptr + 0x38).value(),
        };

        glm::vec3 target_pos = {
            communicator.read<f32>(camera_ptr + 0x3C).value(),
            communicator.read<f32>(camera_ptr + 0x40).value(),
            communicator.read<f32>(camera_ptr + 0x44).value(),
        };

        f32 fovy   = communicator.read<f32>(camera_ptr + 0x48).value();
        f32 aspect = communicator.read<f32>(camera_ptr + 0x4C).value();

        camera.setOrientAndPosition(up_vec, target_pos, translation);
        return {};
    }

    Result<void> TaskCommunicator::getMarioTransform(Transform &transform) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        // This also checks for connected Dolphin
        if (!isSceneLoaded()) {
            return make_error<void>("Task", "Failed to set object transform in scene!");
        }

        u32 mario_ptr = communicator.read<u32>(0x8040E108).value();

        transform.m_translation.x = communicator.read<f32>(mario_ptr + 0x10).value();
        transform.m_translation.y = communicator.read<f32>(mario_ptr + 0x14).value();
        transform.m_translation.z = communicator.read<f32>(mario_ptr + 0x18).value();
        s16 raw_angle             = communicator.read<s16>(mario_ptr + 0x96).value();

        transform.m_rotation = {0.0f, convertAngleS16ToFloat(raw_angle), 0.0f};
        transform.m_scale    = {1.0f, 1.0f, 1.0f};
        return Result<void>();
    }

    Result<void> TaskCommunicator::setMarioTransform(const Transform &transform) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        // This also checks for connected Dolphin
        if (!isSceneLoaded()) {
            return make_error<void>("Task", "Failed to set mario transform in scene (not loaded)!");
        }

        u32 mario_ptr = communicator.read<u32>(0x8040E108).value();
        if (mario_ptr == 0) {
            return make_error<void>("Task", "Failed to set mario transform in scene (nullptr)!");
        }

        communicator.write<f32>(mario_ptr + 0x10, transform.m_translation.x);
        communicator.write<f32>(mario_ptr + 0x14, transform.m_translation.y);
        communicator.write<f32>(mario_ptr + 0x18, transform.m_translation.z);
        communicator.write<f32>(mario_ptr + 0x34, transform.m_rotation.y);

        s16 signed_angle = convertAngleFloatToS16(transform.m_rotation.y);
        communicator.write<s16>(mario_ptr + 0x96, signed_angle);

        return {};
    }

    Result<void> TaskCommunicator::setObjectTransform(RefPtr<PhysicalSceneObject> object,
                                                      const Transform &transform) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        // This also checks for connected Dolphin
        if (!isSceneLoaded()) {
            return make_error<void>("Task", "Failed to set object transform in scene!");
        }

        u32 ptr = object->getGamePtr();
        if (ptr == 0) {
            TOOLBOX_INFO_V(
                "(Task) Pointer for object \"{}\" was null, attempting to find pointer...",
                object->getNameRef().name());
            u32 new_ptr = getActorPtr(object);
            object->setGamePtr(new_ptr);
            if (new_ptr == 0) {
                return make_error<void>("Task", "Failed to object ptr in scene!");
            }
            TOOLBOX_INFO_V("(Task) Pointer for object \"{}\" was found successfully!",
                           object->getNameRef().name());
        }

        communicator.write<f32>(ptr + 0x10, transform.m_translation.x);
        communicator.write<f32>(ptr + 0x14, transform.m_translation.y);
        communicator.write<f32>(ptr + 0x18, transform.m_translation.z);
        communicator.write<f32>(ptr + 0x24, transform.m_scale.x);
        communicator.write<f32>(ptr + 0x28, transform.m_scale.y);
        communicator.write<f32>(ptr + 0x2C, transform.m_scale.z);
        communicator.write<f32>(ptr + 0x30, transform.m_rotation.x);
        communicator.write<f32>(ptr + 0x34, transform.m_rotation.y);
        communicator.write<f32>(ptr + 0x38, transform.m_rotation.z);

        std::string type = object->type();
        if (type.starts_with("NPC")) {
            communicator.write<f32>(ptr + 0xAC, 0.0f);
            communicator.write<f32>(ptr + 0xB0, 0.0f);
            communicator.write<f32>(ptr + 0xB4, 0.0f);
        }

        if (type.ends_with("Fruit")) {
            communicator.write<f32>(ptr + 0xAC, 0.0f);
            communicator.write<f32>(ptr + 0xB0, 0.0f);
            communicator.write<f32>(ptr + 0xB4, 0.0f);
        }

        return {};
    }

    ImageHandle TaskCommunicator::captureXFBAsTexture(int width, int height) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            return ImageHandle();
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
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("(TASK) Dolphin is not hooked!");
            return nullptr;
        }

        return createInterpreterUnchecked();
    }

    ScopePtr<Interpreter::SystemDolphin> TaskCommunicator::createInterpreterUnchecked() {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        auto dolphin_interpreter = Toolbox::make_scoped<Interpreter::SystemDolphin>();

        dolphin_interpreter->onException(
            [](u32 bad_instr_ptr, Interpreter::ExceptionCause cause,
               const Interpreter::Register::RegisterSnapshot &snapshot) {
                TOOLBOX_ERROR_V("[Interpreter] {} at PC = 0x{:08X}:", magic_enum::enum_name(cause),
                                bad_instr_ptr);
                std::vector<std::string> exception_message = StringifySnapshot(snapshot);
                for (auto &line : exception_message) {
                    TOOLBOX_ERROR_V("[Interpreter] {}", line);
                }
            });
        dolphin_interpreter->onInvalid([](u32 bad_instr_ptr, const std::string &cause,
                                          const Interpreter::Register::RegisterSnapshot &snapshot) {
            TOOLBOX_ERROR_V("[Interpreter] Invalid instruction at PC = 0x{:08X} (Reason: {}):",
                            bad_instr_ptr, cause);
            std::vector<std::string> exception_message = StringifySnapshot(snapshot);
            for (auto &line : exception_message) {
                TOOLBOX_ERROR_V("[Interpreter] {}", line);
            }
        });

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