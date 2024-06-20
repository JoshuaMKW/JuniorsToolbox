#include <chrono>
#include <fstream>

#include "gui/application.hpp"
#include "gui/pad/recorder.hpp"

namespace Toolbox {

    PadRecorder::PadRecorder() : Threaded<void>() {
        m_link_data = make_referable<ReplayLinkData>();
    }

    void PadRecorder::tRun(void *param) {
        while (!tIsSignalKill()) {
            TimePoint current_time = std::chrono::high_resolution_clock::now();
            TimeStep delta_time    = TimeStep(m_last_frame_time, current_time);
            m_last_frame_time      = current_time;

            if (m_record_flag.load()) {
                std::scoped_lock lock(m_mutex);
                recordPadData();
                continue;
            }

            if (m_play_flag.load()) {
                std::scoped_lock lock(m_mutex);
                playPadData(delta_time);

                Game::TaskCommunicator &task_communicator =
                    GUIApplication::instance().getTaskCommunicator();
                if (!task_communicator.isSceneLoaded()) {
                    sleep();
                }
                continue;
            }

            sleep();
        }
    }

    void PadRecorder::sleep() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    Result<void> PadRecorder::processCurrentFrame(PadFrameData &&frame_data) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        frame_data.m_stick_mag *= 32.0f;

        bool is_new_link_button_pressed =
            (frame_data.m_pressed_buttons & PadButtons::BUTTON_UP) != PadButtons::BUTTON_NONE;
        is_new_link_button_pressed &=
            (m_last_pressed_buttons & PadButtons::BUTTON_UP) == PadButtons::BUTTON_NONE;

        bool is_isolated_link_recording = m_current_link != '*' && m_next_link != '*';

        // Inverse the input transformation from camera space to world space
        if (m_world_space.load()) {
            u32 mario_ptr = communicator.read<u32>(0x8040E108).value();
            if (mario_ptr == 0) {
                TOOLBOX_WARN("[PAD RECORD] Attempt to transform to world space failed, enter a "
                             "scene first.");
            } else {
                u32 mario_ptr            = communicator.read<u32>(0x8040E108).value();
                frame_data.m_stick_angle = communicator.read<s16>(mario_ptr + 0x90).value();
            }
        }

        u32 application_ptr = 0x803E9700;
        u32 director_ptr    = communicator.read<u32>(application_ptr + 0x4).value();
        u8 director_type    = communicator.read<u8>(application_ptr + 0x8).value();

        s32 frame_step = 1;

        // In the stage
        if (director_type == 5) {
            u32 director_qf = communicator.read<u32>(director_ptr + 0x5C).value();
            frame_step      = director_qf - m_last_frame;

            // Wait for the next frame
            if (frame_step == 0) {
                return {};
            }

            // Check if the frame is out of order
            if (frame_step < 0) {
                stopRecording();
                return make_error<void>(
                    "PAD RECORD", std::format("Stopping recording at quarter frame {} as it is out "
                                              "of order. (Context broken?)",
                                              director_qf));
            }

            // Check if the frame is missed
            if (frame_step > 1) {
                stopRecording();
                return make_error<void>(
                    "PAD RECORD",
                    std::format("Missed {} quarter frames ({} <=> {})! This could lead to loss of "
                                "data in the "
                                "recording. Consider options to increase the "
                                "performance to avoid this issue.",
                                frame_step - 1, director_qf, m_last_frame));
            }
        }

        m_last_pressed_buttons = frame_data.m_pressed_buttons;
        frame_data.m_held_buttons &= ~PadButtons::BUTTON_UP;
        frame_data.m_pressed_buttons &= ~PadButtons::BUTTON_UP;

        u32 current_frame = m_last_frame + frame_step;

        PadDataLinkInfo &pad_data = m_pad_datas.back();

        m_button_info.m_info.m_frames_active += frame_step;
        bool is_new_button_info = frame_data.m_pressed_buttons != PadButtons::BUTTON_NONE;
        is_new_button_info |= frame_data.m_held_buttons != m_button_info.m_info.m_input_state;
        if (is_new_button_info) {
            pad_data.m_data.addPadButtonInput(m_button_info.m_start_frame,
                                              m_button_info.m_info.m_frames_active,
                                              m_button_info.m_info.m_input_state);
            m_button_info.m_start_frame          = current_frame;
            m_button_info.m_info.m_frames_active = 0;
            m_button_info.m_info.m_input_state   = frame_data.m_held_buttons;
        }

        m_trigger_l_info.m_info.m_frames_active += frame_step;
        bool is_new_trigger_l_info =
            frame_data.m_trigger_l != m_trigger_l_info.m_info.m_input_state;
        if (is_new_trigger_l_info) {
            pad_data.m_data.addPadTriggerLInput(m_trigger_l_info.m_start_frame,
                                                m_trigger_l_info.m_info.m_frames_active,
                                                m_trigger_l_info.m_info.m_input_state);
            m_trigger_l_info.m_start_frame          = current_frame;
            m_trigger_l_info.m_info.m_frames_active = 0;
            m_trigger_l_info.m_info.m_input_state   = frame_data.m_trigger_l;
        }

        m_trigger_r_info.m_info.m_frames_active += frame_step;
        bool is_new_trigger_r_info =
            frame_data.m_trigger_r != m_trigger_r_info.m_info.m_input_state;
        if (is_new_trigger_r_info) {
            pad_data.m_data.addPadTriggerRInput(m_trigger_r_info.m_start_frame,
                                                m_trigger_r_info.m_info.m_frames_active,
                                                m_trigger_r_info.m_info.m_input_state);
            m_trigger_r_info.m_start_frame          = current_frame;
            m_trigger_r_info.m_info.m_frames_active = 0;
            m_trigger_r_info.m_info.m_input_state   = frame_data.m_trigger_r;
        }

        m_analog_magnitude_info.m_info.m_frames_active += frame_step;
        bool is_new_analog_magnitude_info =
            frame_data.m_stick_mag != m_analog_magnitude_info.m_info.m_input_state;
        if (is_new_analog_magnitude_info) {
            pad_data.m_data.addPadAnalogMagnitudeInput(
                m_analog_magnitude_info.m_start_frame,
                m_analog_magnitude_info.m_info.m_frames_active,
                m_analog_magnitude_info.m_info.m_input_state);
            m_analog_magnitude_info.m_start_frame          = current_frame;
            m_analog_magnitude_info.m_info.m_frames_active = 0;
            m_analog_magnitude_info.m_info.m_input_state   = frame_data.m_stick_mag;
        }

        m_analog_direction_info.m_info.m_frames_active += frame_step;
        bool is_new_analog_direction_info =
            frame_data.m_stick_angle != m_analog_direction_info.m_info.m_input_state;
        if (is_new_analog_direction_info) {
            pad_data.m_data.addPadAnalogDirectionInput(
                m_analog_direction_info.m_start_frame,
                m_analog_direction_info.m_info.m_frames_active,
                m_analog_direction_info.m_info.m_input_state);
            m_analog_direction_info.m_start_frame          = current_frame;
            m_analog_direction_info.m_info.m_frames_active = 0;
            m_analog_direction_info.m_info.m_input_state   = frame_data.m_stick_angle;
        }

        m_last_frame = current_frame;

        if (is_new_link_button_pressed) {
            if (is_isolated_link_recording) {
                stopRecording();
            } else {
                applyInputChunk();
                initNextInputData();
                initNewLinkData();
                resetRecordState();
            }
        }

        return {};
    }

    bool PadRecorder::hasRecordData(char from_link, char to_link) const {
        return std::any_of(m_pad_datas.begin(), m_pad_datas.end(),
                           [&](const PadDataLinkInfo &info) {
                               return info.m_from_link == from_link && info.m_to_link == to_link;
                           });
    }

    bool PadRecorder::isRecordComplete() const {
        const std::vector<ReplayLinkNode> &link_nodes = m_link_data->linkNodes();
        for (size_t i = 0; i < link_nodes.size(); ++i) {
            for (size_t j = 0; j < 3; ++j) {
                char from_link = 'A' + static_cast<char>(i);
                char to_link   = link_nodes[i].m_infos[j].m_next_link;
                if (to_link == '*') {
                    continue;
                }
                if (!hasRecordData(from_link, to_link)) {
                    return false;
                }
            }
        }
        return true;
    }

    void PadRecorder::startRecording() {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("[PAD RECORD] Dolphin is not running or the memory is not hooked.");
            return;
        }

        std::scoped_lock lock(m_mutex);

        u32 application_ptr = 0x803E9700;
        u32 director_ptr    = communicator.read<u32>(application_ptr + 0x4).value();
        u8 director_type    = communicator.read<u8>(application_ptr + 0x8).value();

        if (director_type == 5) {
            m_last_frame = communicator.read<u32>(director_ptr + 0x5C).value();
        } else if (m_world_space.load()) {
            TOOLBOX_ERROR(
                "[PAD RECORD] Director type is not 5. Please ensure that the game is in a "
                "stage before recording.");
            return;
        } else {
            m_last_frame = 0;
        }

        initNewLinkData();
        resetRecordState();
        initNextInputData();
        m_play_flag.store(false);
        m_record_flag.store(true);
    }

    void PadRecorder::startRecording(char from_link, char to_link) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("[PAD RECORD] Dolphin is not running or the memory is not hooked.");
            return;
        }

        if (from_link == to_link) {
            TOOLBOX_ERROR("[PAD RECORD] Cannot record to the same link.");
            return;
        }

        if (from_link == '*' || to_link == '*') {
            TOOLBOX_ERROR("[PAD RECORD] Cannot record to or from the sentinel link.");
            return;
        }

        // std::scoped_lock lock(m_mutex);

        u32 application_ptr = 0x803E9700;
        u32 director_ptr    = communicator.read<u32>(application_ptr + 0x4).value();
        u8 director_type    = communicator.read<u8>(application_ptr + 0x8).value();

        if (director_type == 5) {
            m_last_frame = communicator.read<u32>(director_ptr + 0x5C).value();
        } else if (m_world_space.load()) {
            TOOLBOX_ERROR(
                "[PAD RECORD] Director type is not 5. Please ensure that the game is in a "
                "stage before recording.");
            return;
        } else {
            m_last_frame = 0;
        }

        m_current_link = from_link;
        m_next_link    = to_link;

        resetRecordState(from_link, to_link);
        initNextInputData(from_link, to_link);
        m_record_flag.store(true);
    }

    void PadRecorder::stopRecording() {
        m_record_flag.store(false);

        // std::scoped_lock lock(m_mutex);

        applyInputChunk();
        if (m_current_link == '*' || m_next_link == '*') {
            initNewLinkData();
        }
        resetRecordState();
        m_current_link = '*';
        m_next_link    = '*';
    }

    bool PadRecorder::loadFromFolder(const std::filesystem::path &folder_path) {
        bool result = false;

        Toolbox::Filesystem::exists(folder_path)
            .and_then([&](bool exists) {
                if (!exists) {
                    return make_fs_error<bool>(
                        std::error_code(), {std::format("[PAD RECORD] Folder `{}' does not exist.",
                                                        folder_path.string())});
                }
                return loadFromFolder_(folder_path);
            })
            .and_then([&](bool success) {
                result = success;
                return Result<bool, FSError>();
            })
            .or_else([](const FSError &error) {
                LogError(error);
                return Result<bool, FSError>();
            });

        return result;
    }

    bool PadRecorder::saveToFolder(const std::filesystem::path &folder_path) {
        if (m_pad_datas.empty()) {
            TOOLBOX_ERROR("[PAD RECORD] No data to save.");
            return false;
        }

        bool result = false;

        Toolbox::Filesystem::create_directories(folder_path)
            .and_then([&](bool created) {
                if (!created) {
                    return make_fs_error<bool>(std::error_code(),
                                               {"[PAD RECORD] Failed to create folder."});
                }
                return saveToFolder_(folder_path);
            })
            .and_then([&](bool success) {
                result = success;
                return Result<bool, FSError>();
            })
            .or_else([](const FSError &error) {
                LogError(error);
                return Result<bool, FSError>();
            });

        return result;
    }

    bool PadRecorder::loadPadRecording(char from_link, char to_link,
                                       const std::filesystem::path &file_path) {
        if (from_link == '*' || to_link == '*') {
            TOOLBOX_ERROR("[PAD RECORD] Cannot load data to or from the sentinel link.");
            return false;
        }

        if (from_link == to_link) {
            TOOLBOX_ERROR("[PAD RECORD] Cannot load data to the same link.");
            return false;
        }

        if (!m_link_data->hasLinkNode(from_link, to_link)) {
            TOOLBOX_ERROR_V("[PAD RECORD] Link node from '{}' to '{}' does not exist.", from_link,
                            to_link);
            return false;
        }

        if (!std::filesystem::is_regular_file(file_path)) {
            TOOLBOX_ERROR_V("[PAD RECORD] File '{}' does not exist.", file_path.string());
            return false;
        }

        PadDataLinkInfo link_info;
        link_info.m_from_link = from_link;
        link_info.m_to_link   = to_link;

        if (file_path.extension() == ".pad") {
            std::ifstream pad_file(file_path, std::ios::binary);
            if (!pad_file.is_open()) {
                TOOLBOX_ERROR_V("[PAD RECORD] Failed to open pad data file at '{}'.",
                                file_path.string());
                return false;
            }
            Deserializer in(pad_file.rdbuf());

            PadData pad_data;
            pad_data.deserialize(in);

            link_info.m_data = std::move(pad_data);
        } else if (file_path.extension() == ".txt") {
            PadDataLinkInfo link_info;
            link_info.m_from_link = from_link;
            link_info.m_to_link   = to_link;

            std::ifstream pad_file(file_path);
            if (!pad_file.is_open()) {
                TOOLBOX_ERROR_V("[PAD RECORD] Failed to open pad data file at '{}'.",
                                file_path.string());
                return false;
            }

            PadData pad_data;
            pad_data.fromText(pad_file);

            link_info.m_data = std::move(pad_data);
        } else {
            TOOLBOX_ERROR("[PAD RECORD] Unsupported file extension. Only .pad and .txt files are "
                          "supported.");
            return false;
        }

        auto pad_it =
            std::find_if(m_pad_datas.begin(), m_pad_datas.end(), [&](const PadDataLinkInfo &info) {
                return info.m_from_link == from_link && info.m_to_link == to_link;
            });
        if (pad_it != m_pad_datas.end()) {
            *pad_it = std::move(link_info);
        } else {
            m_pad_datas.push_back(std::move(link_info));
        }

        return true;
    }

    bool PadRecorder::savePadRecording(char from_link, char to_link,
                                       const std::filesystem::path &file_path) {
        if (from_link == '*' || to_link == '*') {
            TOOLBOX_ERROR("[PAD RECORD] Cannot load data to or from the sentinel link.");
            return false;
        }

        if (from_link == to_link) {
            TOOLBOX_ERROR("[PAD RECORD] Cannot load data to the same link.");
            return false;
        }

        if (!m_link_data->hasLinkNode(from_link, to_link)) {
            TOOLBOX_ERROR_V("[PAD RECORD] Link node from '{}' to '{}' does not exist.", from_link,
                            to_link);
            return false;
        }

        auto pad_it =
            std::find_if(m_pad_datas.begin(), m_pad_datas.end(), [&](const PadDataLinkInfo &info) {
                return info.m_from_link == from_link && info.m_to_link == to_link;
            });
        if (pad_it == m_pad_datas.end()) {
            TOOLBOX_ERROR_V("[PAD RECORD] Pad data from '{}' to '{}' does not exist.", from_link,
                            to_link);
            return false;
        }

        if (file_path.extension() == ".pad") {
            std::ofstream pad_file(file_path, std::ios::binary);
            if (!pad_file.is_open()) {
                TOOLBOX_ERROR_V("[PAD RECORD] Failed to open pad data file at '{}'.",
                                file_path.string());
                return false;
            }

            Serializer out(pad_file.rdbuf());

            auto result = pad_it->m_data.serialize(out);
            if (!result) {
                LogError(result.error());
                return false;
            }
        } else if (file_path.extension() == ".txt") {
            std::ofstream pad_file(file_path);
            if (!pad_file.is_open()) {
                TOOLBOX_ERROR_V("[PAD RECORD] Failed to open pad data file at '{}'.",
                                file_path.string());
                return false;
            }
            pad_it->m_data.toText(pad_file);
        } else {
            TOOLBOX_ERROR("[PAD RECORD] Unsupported file extension. Only .pad and .txt files are "
                          "supported.");
            return false;
        }

        return true;
    }

    bool PadRecorder::playPadRecording(char from_link, char to_link,
                                       playback_frame_cb on_frame_cb) {
        if (isRecording()) {
            TOOLBOX_ERROR("[PAD RECORD] Cannot play data while recording.");
            return false;
        }

        if (from_link == '*' || to_link == '*') {
            TOOLBOX_ERROR("[PAD RECORD] Cannot play data to or from the sentinel link.");
            return false;
        }

        if (from_link == to_link) {
            TOOLBOX_ERROR("[PAD RECORD] Cannot play data to the same link.");
            return false;
        }

        if (!m_link_data->hasLinkNode(from_link, to_link)) {
            TOOLBOX_ERROR_V("[PAD RECORD] Link node from '{}' to '{}' does not exist.", from_link,
                            to_link);
            return false;
        }

        auto pad_it =
            std::find_if(m_pad_datas.begin(), m_pad_datas.end(), [&](const PadDataLinkInfo &info) {
                return info.m_from_link == from_link && info.m_to_link == to_link;
            });

        if (pad_it == m_pad_datas.end()) {
            TOOLBOX_ERROR("[PAD RECORD] Pad data does not exist.");
            return false;
        }

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 application_ptr = 0x803E9700;
        u32 director_ptr    = communicator.read<u32>(application_ptr + 0x4).value();
        u8 director_type    = communicator.read<u8>(application_ptr + 0x8).value();

        if (director_type == 5) {
            m_last_frame = communicator.read<u32>(director_ptr + 0x5C).value();
        } else if (m_world_space.load()) {
            TOOLBOX_ERROR(
                "[PAD RECORD] Director type is not 5. Please ensure that the game is in a "
                "stage before recording.");
            return false;
        } else {
            m_last_frame = 0;
        }

        m_playback_frame_cb = on_frame_cb;
        m_current_link      = from_link;
        m_next_link         = to_link;
        m_start_frame       = m_last_frame;
        m_play_flag.store(true);
        return true;
    }

    void PadRecorder::stopPadPlayback() {
        m_play_flag.store(false);
        m_playback_frame = 0.0f;
        m_last_frame     = 0;
        m_current_link   = '*';
        m_next_link      = '*';
    }

    void PadRecorder::clearLink(char from_link, char to_link) {
        m_mutex.lock();
        {
            auto remove_it =
                std::find_if(m_pad_datas.begin(), m_pad_datas.end(),
                             [from_link, to_link](const PadDataLinkInfo &info) {
                                 return info.m_from_link == from_link && info.m_to_link == to_link;
                             });
            if (remove_it != m_pad_datas.end()) {
                m_pad_datas.erase(remove_it);
            }
            m_link_data->removeLinkNode(from_link, to_link);
        }
        m_mutex.unlock();
    }

    Result<PadRecorder::PadFrameData> PadRecorder::readPadFrameData(PadSourceType source) {
        switch (source) {
        case PadSourceType::SOURCE_PLAYER:
            return readPadFrameDataPlayer();
        case PadSourceType::SOURCE_EMARIO:
            return readPadFrameDataEMario();
        case PadSourceType::SOURCE_PIANTISSIMO:
            return readPadFrameDataPiantissimo();
        default:
            return make_error<PadFrameData>("PAD RECORD", "Invalid source type.");
        }
    }

    PadRecorder::PadFrameData PadRecorder::getPadFrameData(char from_link, char to_link,
                                                           u32 frame) const {
        auto pad_it =
            std::find_if(m_pad_datas.begin(), m_pad_datas.end(), [&](const PadDataLinkInfo &info) {
                return info.m_from_link == from_link && info.m_to_link == to_link;
            });

        if (pad_it == m_pad_datas.end()) {
            return {};
        }

        PadFrameData frame_data = {};

        size_t analog_chunk_idx = pad_it->m_data.getPadAnalogMagnitudeIndex(frame);
        if (analog_chunk_idx != PadData::npos) {
            const PadInputInfo<float> &analog_chunk =
                pad_it->m_data.getPadAnalogMagnitudeInput(analog_chunk_idx);
            frame_data.m_stick_mag = analog_chunk.m_input_state / 32.0f;
        }

        size_t direction_chunk_idx = pad_it->m_data.getPadAnalogDirectionIndex(frame);
        if (direction_chunk_idx != PadData::npos) {
            const PadInputInfo<s16> &direction_chunk =
                pad_it->m_data.getPadAnalogDirectionInput(direction_chunk_idx);
            frame_data.m_stick_angle = direction_chunk.m_input_state;
        }

        frame_data.m_stick_x =
            std::cos(convertAngleS16ToRadians(frame_data.m_stick_angle)) * frame_data.m_stick_mag;
        frame_data.m_stick_y =
            std::sin(convertAngleS16ToRadians(frame_data.m_stick_angle)) * frame_data.m_stick_mag;

        size_t button_chunk_idx = pad_it->m_data.getPadButtonIndex(frame);
        if (button_chunk_idx != PadData::npos) {
            const PadInputInfo<PadButtons> &button_chunk =
                pad_it->m_data.getPadButtonInput(button_chunk_idx);
            frame_data.m_held_buttons = button_chunk.m_input_state;
        }

        size_t trigger_l_chunk_idx = pad_it->m_data.getPadTriggerLIndex(frame);
        if (trigger_l_chunk_idx != PadData::npos) {
            const PadInputInfo<u8> &trigger_l_chunk =
                pad_it->m_data.getPadTriggerLInput(trigger_l_chunk_idx);
            frame_data.m_trigger_l = trigger_l_chunk.m_input_state;
        }

        size_t trigger_r_chunk_idx = pad_it->m_data.getPadTriggerRIndex(frame);
        if (trigger_r_chunk_idx != PadData::npos) {
            const PadInputInfo<u8> &trigger_r_chunk =
                pad_it->m_data.getPadTriggerRInput(trigger_r_chunk_idx);
            frame_data.m_trigger_r = trigger_r_chunk.m_input_state;
        }

        frame_data.m_c_stick_x     = 0.0f;
        frame_data.m_c_stick_y     = 0.0f;
        frame_data.m_c_stick_mag   = 0.0f;
        frame_data.m_c_stick_angle = 0;

        return frame_data;
    }

    u32 PadRecorder::getPadFrameCount(char from_link, char to_link) {
        auto pad_it =
            std::find_if(m_pad_datas.begin(), m_pad_datas.end(), [&](const PadDataLinkInfo &info) {
                return info.m_from_link == from_link && info.m_to_link == to_link;
            });

        if (pad_it == m_pad_datas.end()) {
            return 0;
        }

        u32 frame_count = 0;
        if (!pad_it->m_data.calcFrameCount(frame_count)) {
            TOOLBOX_ERROR("[PAD RECORD] Failed to calculate frame count.");
            return 0;
        }

        return frame_count;
    }

    void PadRecorder::playPadData(TimeStep delta_time) {
        if (!m_playback_frame_cb) {
            stopPadPlayback();
            return;
        }

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        m_playback_frame += 4 * delta_time * 30.0f;
        if (m_playback_frame >= getPadFrameCount(m_current_link, m_next_link)) {
            stopPadPlayback();
            return;
        }

        PadFrameData frame_data =
            getPadFrameData(m_current_link, m_next_link, static_cast<u32>(m_playback_frame));
        m_playback_frame_cb(frame_data);
    }

    void PadRecorder::recordPadData() {
        if (m_is_replaying_pad) {
            TOOLBOX_ERROR("[PAD RECORD] Cannot record data while replaying.");
            return;
        }

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("[PAD RECORD] Dolphin is not running or the memory is not hooked.");
            return;
        }

        readPadFrameData(PadSourceType::SOURCE_PLAYER)
            .and_then([&](PadFrameData &&frame_data) {
                return processCurrentFrame(std::move(frame_data));
            })
            .or_else([](const BaseError &error) {
                LogError(error);
                return Result<void, BaseError>();
            });
    }

    void PadRecorder::resetRecordState() {
        m_start_frame           = m_last_frame;
        m_analog_magnitude_info = {};
        m_analog_direction_info = {};
        m_button_info           = {};
        m_trigger_l_info        = {};
        m_trigger_r_info        = {};
    }

    void PadRecorder::resetRecordState(char from_link, char to_link) {
        resetRecordState();

        auto pad_it =
            std::find_if(m_pad_datas.begin(), m_pad_datas.end(), [&](const PadDataLinkInfo &info) {
                return info.m_from_link == from_link && info.m_to_link == to_link;
            });

        if (pad_it != m_pad_datas.end()) {
            pad_it->m_data = PadData();
        }
    }

    void PadRecorder::initNextInputData() {
        char new_link_chr  = static_cast<char>('A' + m_pad_datas.size() + 1);
        char prev_link_chr = static_cast<char>('A' + m_pad_datas.size());

        PadDataLinkInfo pad_data = {};
        pad_data.m_from_link     = prev_link_chr;
        pad_data.m_to_link       = new_link_chr;
        m_pad_datas.push_back(std::move(pad_data));
    }

    void PadRecorder::initNextInputData(char from_link, char to_link) {
        PadDataLinkInfo pad_data = {};
        pad_data.m_from_link     = from_link;
        pad_data.m_to_link       = to_link;
        auto pad_it =
            std::find_if(m_pad_datas.begin(), m_pad_datas.end(), [&](const PadDataLinkInfo &info) {
                return info.m_from_link == from_link && info.m_to_link == to_link;
            });
        if (pad_it == m_pad_datas.end()) {
            m_pad_datas.push_back(std::move(pad_data));
        }
    }

    void PadRecorder::applyInputChunk() {
        if (m_pad_datas.empty()) {
            return;
        }
        auto pad_it =
            std::find_if(m_pad_datas.begin(), m_pad_datas.end(), [&](const PadDataLinkInfo &info) {
                return info.m_from_link == m_current_link && info.m_to_link == m_next_link;
            });
        bool link_exists =
            pad_it != m_pad_datas.end() && (m_current_link != '*' && m_next_link != '*');

        PadDataLinkInfo &pad_data = link_exists ? *pad_it : m_pad_datas.back();

        pad_data.m_data.addPadButtonInput(m_button_info.m_start_frame,
                                          m_button_info.m_info.m_frames_active,
                                          m_button_info.m_info.m_input_state);
        pad_data.m_data.addPadTriggerLInput(m_trigger_l_info.m_start_frame,
                                            m_trigger_l_info.m_info.m_frames_active,
                                            m_trigger_l_info.m_info.m_input_state);
        pad_data.m_data.addPadTriggerRInput(m_trigger_r_info.m_start_frame,
                                            m_trigger_r_info.m_info.m_frames_active,
                                            m_trigger_r_info.m_info.m_input_state);
        pad_data.m_data.addPadAnalogMagnitudeInput(m_analog_magnitude_info.m_start_frame,
                                                   m_analog_magnitude_info.m_info.m_frames_active,
                                                   m_analog_magnitude_info.m_info.m_input_state);
        pad_data.m_data.addPadAnalogDirectionInput(m_analog_direction_info.m_start_frame,
                                                   m_analog_direction_info.m_info.m_frames_active,
                                                   m_analog_direction_info.m_info.m_input_state);

        u32 total_frames = 0;
        if (!pad_data.m_data.calcFrameCount(total_frames)) {
            TOOLBOX_ERROR("[PAD RECORD] Frame validation failed. Cannot apply input chunk.");
            return;
        }
    }

    void PadRecorder::initNewLinkData() {
        char new_link_chr  = static_cast<char>('A' + m_link_data->linkNodes().size());
        char prev_link_chr = m_link_data->linkNodes().size() > 0
                                 ? static_cast<char>('A' + m_link_data->linkNodes().size() - 1)
                                 : '*';

        if (prev_link_chr != '*') {
            m_link_data->modifyLinkNode(m_link_data->linkNodes().size() - 1, nullptr, &new_link_chr,
                                        nullptr);
        }

        ReplayLinkNode node;
        node.m_link_name.setName("Link");
        node.m_node_name.setName(std::format("Node{}", new_link_chr));
        node.m_infos[0].m_unk_0     = 1;
        node.m_infos[0].m_next_link = prev_link_chr;
        node.m_infos[1].m_unk_0     = 1;
        node.m_infos[1].m_next_link = '*';
        node.m_infos[2].m_unk_0     = 1;
        node.m_infos[2].m_next_link = '*';
        m_link_data->addLinkNode(node);

        if (m_on_create_link) {
            m_on_create_link(node);
        }
    }

    bool PadRecorder::setPlayerTransRot(const glm::vec3 &pos, f32 rotY) {
        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();
        Transform player_transform;
        task_communicator.getMarioTransform(player_transform);
        player_transform.m_translation = pos;
        player_transform.m_rotation.y  = rotY;
        task_communicator.setMarioTransform(player_transform, true);
        return true;
    }

    Result<bool, FSError> PadRecorder::loadFromFolder_(const std::filesystem::path &folder_path) {
        {
            std::filesystem::path link_path = folder_path / "linkdata.bin";
            std::ifstream link_file(link_path, std::ios::binary);
            if (!link_file.is_open()) {
                return make_fs_error<bool>(
                    std::error_code(),
                    {std::format("[PAD RECORD] Failed to open link data file at '{}'.",
                                 link_path.string())});
            }

            Deserializer link_deserializer(link_file.rdbuf());
            m_link_data->deserialize(link_deserializer);
        }

        std::vector<PadDataLinkInfo> pad_datas;

        for (const ReplayLinkNode &node : m_link_data->linkNodes()) {
            for (size_t i = 0; i < 3; ++i) {
                if (node.m_infos[i].m_next_link == '*') {
                    continue;
                }
                std::filesystem::path pad_path =
                    folder_path /
                    std::format("tutorial{}{}.bin",
                                static_cast<char>(std::tolower(node.m_node_name.name().back())),
                                static_cast<char>(std::tolower(node.m_infos[i].m_next_link)));
                std::ifstream pad_file(pad_path, std::ios::binary);
                if (!pad_file.is_open()) {
                    return make_fs_error<bool>(
                        std::error_code(),
                        {std::format("[PAD RECORD] Failed to open pad data file at '{}'.",
                                     pad_path.string())});
                }

                Deserializer pad_deserializer(pad_file.rdbuf());
                PadData pad_data;
                pad_data.deserialize(pad_deserializer);

                PadDataLinkInfo link_info;
                link_info.m_data      = std::move(pad_data);
                link_info.m_from_link = node.m_node_name.name().back();
                link_info.m_to_link   = node.m_infos[1].m_next_link;
                pad_datas.push_back(std::move(link_info));
            }
        }

        m_pad_datas.clear();
        m_pad_datas = std::move(pad_datas);

        return true;
    }

    Result<bool, FSError> PadRecorder::saveToFolder_(const std::filesystem::path &folder_path) {
        {
            std::filesystem::path link_path = folder_path / "linkdata.bin";
            std::ofstream link_file(link_path, std::ios::binary);
            if (!link_file.is_open()) {
                return make_fs_error<bool>(
                    std::error_code(),
                    {std::format("[PAD RECORD] Failed to open link data file at '{}'.",
                                 link_path.string())});
            }

            Serializer link_serializer(link_file.rdbuf());
            m_link_data->serialize(link_serializer);
        }

        for (size_t i = 0; i < m_pad_datas.size(); ++i) {
            const PadDataLinkInfo &pad_data = m_pad_datas[i];
            std::filesystem::path pad_path =
                folder_path / std::format("tutorial{}{}.pad",
                                          static_cast<char>(std::tolower(pad_data.m_from_link)),
                                          static_cast<char>(std::tolower(pad_data.m_to_link)));
            std::ofstream pad_file(pad_path, std::ios::binary);
            if (!pad_file.is_open()) {
                return make_fs_error<bool>(
                    std::error_code(),
                    {std::format("[PAD RECORD] Failed to open pad data file at '{}'.",
                                 pad_path.string())});
            }

            Serializer pad_serializer(pad_file.rdbuf());
            pad_data.m_data.serialize(pad_serializer);
        }

        return true;
    }

    s32 PadRecorder::getFrameStep() const {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            return -1;
        }

        u32 application_ptr = 0x803E9700;
        u32 director_ptr    = communicator.read<u32>(application_ptr + 0x4).value();
        u8 director_type    = communicator.read<u8>(application_ptr + 0x8).value();

        // In the stage
        if (director_type == 5) {
            u32 director_qf = communicator.read<u32>(director_ptr + 0x5C).value();
            return director_qf - m_last_frame;
        } else {
            return 1;
        }
    }

    Result<PadRecorder::PadFrameData> PadRecorder::readPadFrameDataPlayer() {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 application_ptr = 0x803E9700;
        u32 gamepad_ptr = communicator.read<u32>(application_ptr + 0x20 + (m_port << 2)).value();

        bool is_connected = communicator.read<u8>(gamepad_ptr + 0x7A).value() != 0xFF;
        if (!is_connected) {
            TOOLBOX_ERROR("[PAD RECORD] Controller is not connected. Please ensure that "
                          "the controller is "
                          "connected and the input is being read by Dolphin.");
            return make_error<PadFrameData>("PAD RECORD",
                                            "Controller is not connected. Please ensure that "
                                            "the controller is "
                                            "connected and the input is being read by Dolphin.");
        }

        PadFrameData frame_data{};
        {
            frame_data.m_held_buttons =
                static_cast<PadButtons>(communicator.read<u32>(gamepad_ptr + 0x18).value());
            frame_data.m_pressed_buttons =
                static_cast<PadButtons>(communicator.read<u32>(gamepad_ptr + 0x1C).value());

            frame_data.m_trigger_l = communicator.read<u8>(gamepad_ptr + 0x26).value();
            frame_data.m_trigger_r = communicator.read<u8>(gamepad_ptr + 0x27).value();

            frame_data.m_stick_x = communicator.read<f32>(gamepad_ptr + 0x48).value();
            frame_data.m_stick_y = communicator.read<f32>(gamepad_ptr + 0x4C).value();

            frame_data.m_stick_mag   = communicator.read<f32>(gamepad_ptr + 0x50).value();
            frame_data.m_stick_angle = communicator.read<s16>(gamepad_ptr + 0x54).value();

            frame_data.m_c_stick_x = communicator.read<f32>(gamepad_ptr + 0x58).value();
            frame_data.m_c_stick_y = communicator.read<f32>(gamepad_ptr + 0x5C).value();

            frame_data.m_c_stick_mag   = communicator.read<f32>(gamepad_ptr + 0x60).value();
            frame_data.m_c_stick_angle = communicator.read<s16>(gamepad_ptr + 0x64).value();

            frame_data.m_rumble_x = 0.0f;
            frame_data.m_rumble_y = 0.0f;
            u32 rumble_ptr        = communicator.read<u32>(0x804141C0 - 0x60F0).value();
            if (rumble_ptr) {
                u32 data_ptr = communicator.read<u32>(rumble_ptr + 0xC + (m_port << 2)).value();
                frame_data.m_rumble_x = communicator.read<f32>(data_ptr + 0x0).value();
                frame_data.m_rumble_y = communicator.read<f32>(data_ptr + 0x4).value();
            }
        }
        return frame_data;
    }

    Result<PadRecorder::PadFrameData> PadRecorder::readPadFrameDataEMario() {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();
        if (!task_communicator.isSceneLoaded(m_scene_id, m_episode_id) || m_shadow_mario_ptr == 0) {
            if (!task_communicator.getLoadedScene(m_scene_id, m_episode_id)) {
                m_shadow_mario_ptr        = 0;
                m_is_viewing_shadow_mario = false;
                return make_error<PadRecorder::PadFrameData>("PAD RECORD", "Scene is not loaded.");
            } else {
                std::string shadow_mario_name =
                    "\x83\x7D\x83\x8A\x83\x49\x83\x82\x83\x68\x83\x4C\x5F\x30";
                m_shadow_mario_ptr = task_communicator.getActorPtr(shadow_mario_name);
            }
        }

        if (m_shadow_mario_ptr == 0) {
            m_is_viewing_shadow_mario = false;
            return make_error<PadRecorder::PadFrameData>("PAD RECORD",
                                                         "Shadow Mario not found in scene.");
        } else {
            PadFrameData frame_data{};

            u32 enemy_mario_ptr    = communicator.read<u32>(m_shadow_mario_ptr + 0x150).value();
            frame_data.m_stick_mag = communicator.read<f32>(enemy_mario_ptr + 0x8C).value() / 32.0f;
            frame_data.m_stick_angle = communicator.read<s16>(enemy_mario_ptr + 0x90).value();
            frame_data.m_stick_x =
                frame_data.m_stick_mag *
                std::cos(convertAngleS16ToRadians(frame_data.m_stick_angle) - IM_PI / 2);
            frame_data.m_stick_y =
                frame_data.m_stick_mag *
                std::sin(convertAngleS16ToRadians(frame_data.m_stick_angle) - IM_PI / 2);

            frame_data.m_c_stick_x     = 0.0f;
            frame_data.m_c_stick_y     = 0.0f;
            frame_data.m_c_stick_mag   = 0.0f;
            frame_data.m_c_stick_angle = 0;

            u32 controller_meaning_ptr   = communicator.read<u32>(enemy_mario_ptr + 0x108).value();
            frame_data.m_pressed_buttons = static_cast<PadButtons>(
                communicator.read<u32>(controller_meaning_ptr + 0x8).value());
            frame_data.m_held_buttons = static_cast<PadButtons>(
                communicator.read<u32>(controller_meaning_ptr + 0x4).value());

            frame_data.m_rumble_x = 0.0f;
            frame_data.m_rumble_y = 0.0f;
            return frame_data;
        }
    }

    Result<PadRecorder::PadFrameData> PadRecorder::readPadFrameDataPiantissimo() {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        Game::TaskCommunicator &task_communicator =
            GUIApplication::instance().getTaskCommunicator();
        if (!task_communicator.isSceneLoaded(m_scene_id, m_episode_id) || m_piantissimo_ptr == 0) {
            if (!task_communicator.getLoadedScene(m_scene_id, m_episode_id)) {
                m_piantissimo_ptr        = 0;
                m_is_viewing_piantissimo = false;
                return make_error<PadRecorder::PadFrameData>("PAD RECORD", "Scene is not loaded.");
            } else {
                std::string piantissimo_name = "\x83\x82\x83\x93\x83\x65\x83\x7D\x83\x93";
                m_piantissimo_ptr            = task_communicator.getActorPtr(piantissimo_name);
            }
        }

        if (m_piantissimo_ptr == 0) {
            m_is_viewing_piantissimo = false;
            return make_error<PadRecorder::PadFrameData>("PAD RECORD",
                                                         "Piantissimo not found in scene.");
        } else {
            PadFrameData frame_data{};

            u32 enemy_mario_ptr    = communicator.read<u32>(m_piantissimo_ptr + 0x150).value();
            frame_data.m_stick_mag = communicator.read<f32>(enemy_mario_ptr + 0x8C).value() / 32.0f;
            frame_data.m_stick_angle = communicator.read<s16>(enemy_mario_ptr + 0x90).value();
            frame_data.m_stick_x =
                frame_data.m_stick_mag *
                std::cos(convertAngleS16ToRadians(frame_data.m_stick_angle) - IM_PI / 2);
            frame_data.m_stick_y =
                frame_data.m_stick_mag *
                std::sin(convertAngleS16ToRadians(frame_data.m_stick_angle) - IM_PI / 2);

            frame_data.m_c_stick_x     = 0.0f;
            frame_data.m_c_stick_y     = 0.0f;
            frame_data.m_c_stick_mag   = 0.0f;
            frame_data.m_c_stick_angle = 0;

            u32 controller_meaning_ptr   = communicator.read<u32>(enemy_mario_ptr + 0x108).value();
            frame_data.m_pressed_buttons = static_cast<PadButtons>(
                communicator.read<u32>(controller_meaning_ptr + 0x8).value());
            frame_data.m_held_buttons = static_cast<PadButtons>(
                communicator.read<u32>(controller_meaning_ptr + 0x4).value());
            frame_data.m_rumble_x = 0.0f;
            frame_data.m_rumble_y = 0.0f;

            return frame_data;
        }
    }

}  // namespace Toolbox