#include <chrono>

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

        m_analog_magnitude_info = {};
        m_analog_direction_info = {};
        m_button_info           = {};
        m_trigger_l_info        = {};
        m_trigger_r_info        = {};
        m_record_flag.store(true);
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

        u8 trigger_l = communicator.read<u8>(gamepad_ptr + 0x26).value();
        u8 trigger_r = communicator.read<u8>(gamepad_ptr + 0x27).value();

        f32 stick_x = communicator.read<f32>(gamepad_ptr + 0x48).value();
        f32 stick_y = communicator.read<f32>(gamepad_ptr + 0x4C).value();

        f32 stick_mag   = communicator.read<f32>(gamepad_ptr + 0x50).value();
        s16 stick_angle = communicator.read<s16>(gamepad_ptr + 0x54).value();

        // Inverse the input transformation from camera space to world space
        if (m_camera_flag.load()) {
            u32 camera_ptr = communicator.read<u32>(0x8040D0A8).value();
            if (camera_ptr == 0) {
                TOOLBOX_WARN("[PAD RECORD] Attempt to inverse camera transformation failed as "
                             "there is no active player camera.");
            } else {
                stick_angle -=
                    communicator.read<s16>(camera_ptr + 0x258).value();  // Transform to world space
            }
        }

        u32 director_ptr = communicator.read<u32>(application_ptr + 0x4).value();
        u8 director_type = communicator.read<u8>(application_ptr + 0x8).value();

        s32 frame_step = 1;

        // In the stage
        if (director_type == 5) {
            u32 director_qf = communicator.read<u32>(director_ptr + 0x5C).value();
            frame_step  = director_qf - m_last_frame;
            if (frame_step == 0) {
                return;
            }
            if (frame_step < 0) {
                TOOLBOX_WARN_V("[PAD RECORD] Skipping quarter frame {} as it is out of order. (Context broken?)",
                               director_qf);
                return;
            }
            if (frame_step > 1) {
                TOOLBOX_WARN_V(
                    "[PAD RECORD] Missed {} quarter frames ({} <=> {})! This could lead to loss of data in the "
                    "recording. Consider options to increase the performance to avoid this issue.",
                    frame_step - 1, director_qf, m_last_frame);
            }
        }

        u32 current_frame = m_last_frame + frame_step;

        m_button_info.m_info.m_frames_active += frame_step;
        bool is_new_button_info = pressed_buttons != PadButtons::BUTTON_NONE;
        is_new_button_info |= held_buttons != m_button_info.m_info.m_input_state;
        if (is_new_button_info) {
            m_pad_data.addPadButtonInput(m_button_info.m_start_frame,
                                         m_button_info.m_info.m_frames_active,
                                         m_button_info.m_info.m_input_state);
            m_button_info.m_start_frame          = current_frame;
            m_button_info.m_info.m_frames_active = 0;
            m_button_info.m_info.m_input_state   = held_buttons;
        }

        m_trigger_l_info.m_info.m_frames_active += frame_step;
        bool is_new_trigger_l_info = trigger_l != m_trigger_l_info.m_info.m_input_state;
        if (is_new_trigger_l_info) {
            m_pad_data.addPadTriggerLInput(m_trigger_l_info.m_start_frame,
                                           m_trigger_l_info.m_info.m_frames_active,
                                           m_trigger_l_info.m_info.m_input_state);
            m_trigger_l_info.m_start_frame          = current_frame;
            m_trigger_l_info.m_info.m_frames_active = 0;
            m_trigger_l_info.m_info.m_input_state   = trigger_l;
        }

        m_trigger_r_info.m_info.m_frames_active += frame_step;
        bool is_new_trigger_r_info = trigger_r != m_trigger_r_info.m_info.m_input_state;
        if (is_new_trigger_r_info) {
            m_pad_data.addPadTriggerRInput(m_trigger_r_info.m_start_frame,
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
            m_pad_data.addPadAnalogMagnitudeInput(m_analog_magnitude_info.m_start_frame,
                                                  m_analog_magnitude_info.m_info.m_frames_active,
                                                  stick_mag);
            m_analog_magnitude_info.m_start_frame          = current_frame;
            m_analog_magnitude_info.m_info.m_frames_active = 0;
            m_analog_magnitude_info.m_info.m_input_state   = stick_mag;
        }

        m_analog_direction_info.m_info.m_frames_active += frame_step;
        bool is_new_analog_direction_info =
            stick_angle != m_analog_direction_info.m_info.m_input_state;
        if (is_new_analog_direction_info) {
            m_pad_data.addPadAnalogDirectionInput(m_analog_direction_info.m_start_frame,
                                                  m_analog_direction_info.m_info.m_frames_active,
                                                  stick_angle);
            m_analog_direction_info.m_start_frame          = current_frame;
            m_analog_direction_info.m_info.m_frames_active = 0;
            m_analog_direction_info.m_info.m_input_state   = stick_angle;
        }

        m_last_frame = current_frame;
    }

}  // namespace Toolbox