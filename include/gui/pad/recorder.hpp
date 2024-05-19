#pragma once

#include "core/memory.hpp"
#include "core/threaded.hpp"
#include "core/time/timestep.hpp"
#include "dolphin/process.hpp"
#include "fsystem.hpp"

#include "pad/pad.hpp"

namespace Toolbox {

    class PadRecorder : public Threaded<void> {
    public:
        PadRecorder()                        = default;
        PadRecorder(const PadRecorder &)     = delete;
        PadRecorder(PadRecorder &&) noexcept = default;
        ~PadRecorder()                       = default;

        PadRecorder &operator=(const PadRecorder &)     = delete;
        PadRecorder &operator=(PadRecorder &&) noexcept = default;

        u8 getPort() const { return m_port; }
        void setPort(u8 port) { m_port = port; }

        PadTrimCommand getTrimState() const { return m_trim_state; }
        void setTrimState(PadTrimCommand state) { m_trim_state = state; }

        bool isCameraInversed() const { return m_camera_flag.load(); }
        void setCameraInversed(bool inversed) { m_camera_flag.store(inversed); }

        bool isRecording() const { return m_record_flag.load(); }
        void resetRecording() {
            m_pad_data = PadData();
            stopRecording();
        }
        void startRecording();
        void stopRecording() {
            m_record_flag.store(false);
            m_pad_data.trim(m_trim_state);
        }

        const PadData &data() const noexcept { return m_pad_data; }

    protected:
        void tRun(void *param) override;

        void recordPadData();

        template <typename T> struct PadRecordInfo {
            u32 m_start_frame      = 0;
            PadInputInfo<T> m_info = {};
        };

    private:
        PadData m_pad_data;

        u8 m_port = 0;
        PadTrimCommand m_trim_state = PadTrimCommand::TRIM_NONE;

        u32 m_last_frame = 0;

        PadRecordInfo<float> m_analog_magnitude_info = {};
        PadRecordInfo<s16> m_analog_direction_info   = {};
        PadRecordInfo<PadButtons> m_button_info      = {};
        PadRecordInfo<u8> m_trigger_l_info           = {};
        PadRecordInfo<u8> m_trigger_r_info           = {};

        std::mutex m_record_mutex;
        std::atomic<bool> m_record_flag = false;
        std::atomic<bool> m_camera_flag = true;

        std::atomic<bool> m_kill_flag = false;
        std::condition_variable m_kill_condition;
    };

}  // namespace Toolbox