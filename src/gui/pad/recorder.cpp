#include <chrono>
#include <fstream>

#include "gui/application.hpp"
#include "gui/pad/recorder.hpp"

namespace Toolbox {

    void PadRecorder::tRun(void *param) {
        while (!tIsSignalKill()) {
            if (!m_record_flag.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            {
                std::unique_lock<std::mutex> lk(m_record_mutex);
                recordPadData();
            }
        }
    }

    void PadRecorder::startRecording() {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("[PAD RECORD] Dolphin is not running or the memory is not hooked.");
            return;
        }

        u32 application_ptr = 0x803E9700;
        u32 director_ptr    = communicator.read<u32>(application_ptr + 0x4).value();
        u8 director_type    = communicator.read<u8>(application_ptr + 0x8).value();

        if (director_type == 5) {
            m_last_frame = communicator.read<u32>(director_ptr + 0x5C).value();
        } else if (m_camera_flag.load()) {
            TOOLBOX_ERROR(
                "[PAD RECORD] Director type is not 5. Please ensure that the game is in a "
                "stage before recording.");
            return;
        } else {
            m_last_frame = 0;
        }

        m_link_data.clearLinkNodes();
        initNewLinkData();
        resetRecordState();
        initNextInputData();
        m_record_flag.store(true);
    }

    void PadRecorder::stopRecording() {
        m_record_flag.store(false);
        initNewLinkData();
        resetRecordState();
    }

    bool PadRecorder::loadFromFolder(const std::filesystem::path &folder_path) {
        if (!std::filesystem::exists(folder_path)) {
            TOOLBOX_ERROR_V("[PAD RECORD] Folder '{}' does not exist.", folder_path.string());
            return false;
        }

        m_pad_datas.clear();

        {
            std::filesystem::path link_path = folder_path / "linkdata.bin";
            std::ifstream link_file(link_path, std::ios::binary);
            if (!link_file.is_open()) {
                TOOLBOX_ERROR_V("[PAD RECORD] Failed to open link data file at '{}'.",
                                link_path.string());
                return false;
            }

            Deserializer link_deserializer(link_file.rdbuf());
            m_link_data.deserialize(link_deserializer);
        }

        for (const ReplayLinkNode &node : m_link_data.linkNodes()) {
            for (size_t i = 0; i < 3; ++i) {
                if (node.m_infos[i].m_next_link == '*') {
                    continue;
                }
                std::filesystem::path pad_path =
                    folder_path / std::format("tutorial{}{}.bin",
                                              std::tolower(node.m_node_name.name().back()),
                                              std::tolower(node.m_infos[i].m_next_link));
                std::ifstream pad_file(pad_path, std::ios::binary);
                if (!pad_file.is_open()) {
                    TOOLBOX_ERROR_V("[PAD RECORD] Failed to open pad data file at '{}'.",
                                    pad_path.string());
                    return false;
                }

                Deserializer pad_deserializer(pad_file.rdbuf());
                PadData pad_data;
                pad_data.deserialize(pad_deserializer);

                PadDataLinkInfo link_info;
                link_info.m_data      = std::move(pad_data);
                link_info.m_from_link = node.m_node_name.name().back();
                link_info.m_to_link   = node.m_infos[1].m_next_link;
                m_pad_datas.push_back(std::move(link_info));
            }
        }

        return true;
    }

    bool PadRecorder::saveToFolder(const std::filesystem::path &folder_path) {
        if (m_pad_datas.empty()) {
            TOOLBOX_ERROR("[PAD RECORD] No data to save.");
            return false;
        }

        Toolbox::create_directories(folder_path);

        {
            std::filesystem::path link_path = folder_path / "linkdata.bin";
            std::ofstream link_file(link_path, std::ios::binary);
            if (!link_file.is_open()) {
                TOOLBOX_ERROR_V("[PAD RECORD] Failed to open link data file at '{}'.",
                                link_path.string());
                return false;
            }

            Serializer link_serializer(link_file.rdbuf());
            m_link_data.serialize(link_serializer);
        }

        for (size_t i = 0; i < m_pad_datas.size(); ++i) {
            const PadDataLinkInfo &pad_data = m_pad_datas[i];
            std::filesystem::path pad_path =
                folder_path / std::format("tutorial{}{}.bin", std::tolower(pad_data.m_from_link),
                                          std::tolower(pad_data.m_to_link));
            std::ofstream pad_file(pad_path, std::ios::binary);
            if (!pad_file.is_open()) {
                TOOLBOX_ERROR_V("[PAD RECORD] Failed to open pad data file at '{}'.",
                                pad_path.string());
                return false;
            }

            Serializer pad_serializer(pad_file.rdbuf());
            pad_data.m_data.serialize(pad_serializer);
        }

        return true;
    }

    void PadRecorder::recordPadData() {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();

        u32 application_ptr = 0x803E9700;
        u32 gamepad_ptr = communicator.read<u32>(application_ptr + 0x20 + (m_port << 2)).value();

        bool is_connected = communicator.read<u8>(gamepad_ptr + 0x7A).value() != 0xFF;
        if (!is_connected) {
            TOOLBOX_ERROR("[PAD RECORD] Controller is not connected. Please ensure that "
                          "the controller is "
                          "connected and the input is being read by Dolphin.");
            return;
        }

        PadButtons held_buttons =
            static_cast<PadButtons>(communicator.read<u32>(gamepad_ptr + 0x18).value());
        PadButtons pressed_buttons =
            static_cast<PadButtons>(communicator.read<u32>(gamepad_ptr + 0x1C).value());

        bool is_new_link_button_pressed =
            (pressed_buttons & PadButtons::BUTTON_UP) != PadButtons::BUTTON_NONE;

        held_buttons &= ~PadButtons::BUTTON_UP;
        pressed_buttons &= ~PadButtons::BUTTON_UP;

        u8 trigger_l = communicator.read<u8>(gamepad_ptr + 0x26).value();
        u8 trigger_r = communicator.read<u8>(gamepad_ptr + 0x27).value();

        f32 stick_x = communicator.read<f32>(gamepad_ptr + 0x48).value();
        f32 stick_y = communicator.read<f32>(gamepad_ptr + 0x4C).value();

        f32 stick_mag   = communicator.read<f32>(gamepad_ptr + 0x50).value() * 32.0f;
        s16 stick_angle = communicator.read<s16>(gamepad_ptr + 0x54).value();

        // Inverse the input transformation from camera space to world space
        if (m_camera_flag.load()) {
            u32 camera_ptr = communicator.read<u32>(0x8040D0A8).value();
            if (camera_ptr == 0) {
                TOOLBOX_WARN("[PAD RECORD] Attempt to inverse camera transformation failed as "
                             "there is no active player camera.");
            } else {
                stick_angle +=
                    communicator.read<s16>(camera_ptr + 0x258).value();  // Transform to world space
            }
        }

        u32 director_ptr = communicator.read<u32>(application_ptr + 0x4).value();
        u8 director_type = communicator.read<u8>(application_ptr + 0x8).value();

        s32 frame_step = 1;

        // In the stage
        if (director_type == 5) {
            u32 director_qf = communicator.read<u32>(director_ptr + 0x5C).value();
            frame_step      = director_qf - m_last_frame;
            if (frame_step == 0) {
                return;
            }
            if (frame_step < 0) {
                TOOLBOX_ERROR_V("[PAD RECORD] Stopping recording at quarter frame {} as it is out "
                                "of order. (Context broken?)",
                                director_qf);
                stopRecording();
                return;
            }
            if (frame_step > 1) {
                TOOLBOX_WARN_V(
                    "[PAD RECORD] Missed {} quarter frames ({} <=> {})! This could lead to loss of "
                    "data in the "
                    "recording. Consider options to increase the performance to avoid this issue.",
                    frame_step - 1, director_qf, m_last_frame);
            }
        }

        u32 current_frame = m_last_frame + frame_step;

        PadDataLinkInfo &pad_data = m_pad_datas.back();

        m_button_info.m_info.m_frames_active += frame_step;
        bool is_new_button_info = pressed_buttons != PadButtons::BUTTON_NONE;
        is_new_button_info |= held_buttons != m_button_info.m_info.m_input_state;
        if (is_new_button_info) {
            pad_data.m_data.addPadButtonInput(m_button_info.m_start_frame,
                                              m_button_info.m_info.m_frames_active,
                                              m_button_info.m_info.m_input_state);
            m_button_info.m_start_frame          = current_frame;
            m_button_info.m_info.m_frames_active = 0;
            m_button_info.m_info.m_input_state   = held_buttons;
        }

        m_trigger_l_info.m_info.m_frames_active += frame_step;
        bool is_new_trigger_l_info = trigger_l != m_trigger_l_info.m_info.m_input_state;
        if (is_new_trigger_l_info) {
            pad_data.m_data.addPadTriggerLInput(m_trigger_l_info.m_start_frame,
                                                m_trigger_l_info.m_info.m_frames_active,
                                                m_trigger_l_info.m_info.m_input_state);
            m_trigger_l_info.m_start_frame          = current_frame;
            m_trigger_l_info.m_info.m_frames_active = 0;
            m_trigger_l_info.m_info.m_input_state   = trigger_l;
        }

        m_trigger_r_info.m_info.m_frames_active += frame_step;
        bool is_new_trigger_r_info = trigger_r != m_trigger_r_info.m_info.m_input_state;
        if (is_new_trigger_r_info) {
            pad_data.m_data.addPadTriggerRInput(m_trigger_r_info.m_start_frame,
                                                m_trigger_r_info.m_info.m_frames_active,
                                                m_trigger_r_info.m_info.m_input_state);
            m_trigger_r_info.m_start_frame          = current_frame;
            m_trigger_r_info.m_info.m_frames_active = 0;
            m_trigger_r_info.m_info.m_input_state   = trigger_r;
        }

        m_analog_magnitude_info.m_info.m_frames_active += frame_step;
        bool is_new_analog_magnitude_info =
            stick_mag != m_analog_magnitude_info.m_info.m_input_state;
        if (is_new_analog_magnitude_info) {
            pad_data.m_data.addPadAnalogMagnitudeInput(
                m_analog_magnitude_info.m_start_frame,
                m_analog_magnitude_info.m_info.m_frames_active,
                m_analog_magnitude_info.m_info.m_input_state);
            m_analog_magnitude_info.m_start_frame          = current_frame;
            m_analog_magnitude_info.m_info.m_frames_active = 0;
            m_analog_magnitude_info.m_info.m_input_state   = stick_mag;
        }

        m_analog_direction_info.m_info.m_frames_active += frame_step;
        bool is_new_analog_direction_info =
            stick_angle != m_analog_direction_info.m_info.m_input_state;
        if (is_new_analog_direction_info) {
            pad_data.m_data.addPadAnalogDirectionInput(
                m_analog_direction_info.m_start_frame,
                m_analog_direction_info.m_info.m_frames_active,
                m_analog_direction_info.m_info.m_input_state);
            m_analog_direction_info.m_start_frame          = current_frame;
            m_analog_direction_info.m_info.m_frames_active = 0;
            m_analog_direction_info.m_info.m_input_state   = stick_angle;
        }

        m_last_frame = current_frame;

        if (is_new_link_button_pressed) {
            applyInputChunk();
            initNextInputData();
            initNewLinkData();
            resetRecordState();
        }
    }

    void PadRecorder::resetRecordState() {
        m_start_frame           = m_last_frame;
        m_analog_magnitude_info = {};
        m_analog_direction_info = {};
        m_button_info           = {};
        m_trigger_l_info        = {};
        m_trigger_r_info        = {};
    }

    void PadRecorder::initNextInputData() {
        char new_link_chr = static_cast<char>('A' + m_pad_datas.size() + 1);
        char prev_link_chr =
            m_pad_datas.size() > 0 ? static_cast<char>('A' + m_pad_datas.size()) : '*';

        PadDataLinkInfo pad_data = {};
        pad_data.m_from_link = prev_link_chr;
        pad_data.m_to_link   = new_link_chr;
        m_pad_datas.push_back(std::move(pad_data));
    }

    void PadRecorder::applyInputChunk() {
        PadDataLinkInfo &pad_data = m_pad_datas.back();
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
        pad_data.m_data.setFrameCount(m_last_frame - m_start_frame);
    }

    void PadRecorder::initNewLinkData() {
        char new_link_chr  = static_cast<char>('A' + m_link_data.linkNodes().size());
        char prev_link_chr = m_link_data.linkNodes().size() > 0
                                 ? static_cast<char>('A' + m_link_data.linkNodes().size() - 1)
                                 : '*';

        if (prev_link_chr != '*') {
            m_link_data.modifyLinkNode(m_link_data.linkNodes().size() - 1, nullptr, &new_link_chr,
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
        m_link_data.addLinkNode(node);

        if (m_on_create_link) {
            m_on_create_link(node);
        }
    }

}  // namespace Toolbox