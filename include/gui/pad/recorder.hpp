#pragma once

#include "core/memory.hpp"
#include "core/threaded.hpp"
#include "core/time/timestep.hpp"
#include "dolphin/process.hpp"
#include "fsystem.hpp"

#include "pad/linkdata.hpp"
#include "pad/pad.hpp"

namespace Toolbox {

    class PadRecorder : public Threaded<void> {
    protected:
        template <typename T> struct PadRecordInfo {
            u32 m_start_frame      = 0;
            PadInputInfo<T> m_info = {};
        };

    public:
        struct PadDataLinkInfo {
            char m_from_link = '*';
            char m_to_link   = '*';
            PadData m_data   = {};
        };

        using create_link_cb = std::function<void(const ReplayLinkNode &)>;

        PadRecorder()                        = default;
        PadRecorder(const PadRecorder &)     = delete;
        PadRecorder(PadRecorder &&) noexcept = default;
        ~PadRecorder()                       = default;

        PadRecorder &operator=(const PadRecorder &)     = delete;
        PadRecorder &operator=(PadRecorder &&) noexcept = default;

        const std::vector<PadDataLinkInfo> &padData() const noexcept { return m_pad_datas; }
        const ReplayLinkData &linkData() const noexcept { return m_link_data; }

        [[nodiscard]] bool hasRecordData(char from_link, char to_link) const;

        [[nodiscard]] u8 getPort() const { return m_port; }
        void setPort(u8 port) { m_port = port; }

        [[nodiscard]] PadTrimCommand getTrimState() const { return m_trim_state; }
        void setTrimState(PadTrimCommand state) { m_trim_state = state; }

        [[nodiscard]] bool isCameraInversed() const { return m_camera_flag.load(); }
        void setCameraInversed(bool inversed) { m_camera_flag.store(inversed); }

        [[nodiscard]] bool isRecordComplete() const;
        [[nodiscard]] bool isRecording() const { return m_record_flag.load(); }
        [[nodiscard]] bool isRecording(char from_link, char to_link) const {
            return isRecording() && m_current_link == from_link && m_next_link == to_link;
        }
        void resetRecording() {
            m_record_flag.store(false);
            m_link_data.clearLinkNodes();
            m_pad_datas.clear();
        }
        void startRecording();
        void startRecording(char from_link, char to_link);
        void stopRecording();

        bool loadFromFolder(const std::filesystem::path &folder_path);
        bool saveToFolder(const std::filesystem::path &folder_path);

        void onCreateLink(create_link_cb callback) { m_on_create_link = callback; }

    protected:
        void tRun(void *param) override;

        void initNextInputData();
        void initNextInputData(char from_link, char to_link);
        void applyInputChunk();

        void recordPadData();
        void resetRecordState();
        void initNewLinkData();

    private:
        ReplayLinkData m_link_data;
        std::vector<PadDataLinkInfo> m_pad_datas;

        u8 m_port                   = 0;
        PadTrimCommand m_trim_state = PadTrimCommand::TRIM_NONE;

        bool m_first_input_found = false;
        u32 m_start_frame        = 0;
        u32 m_last_frame         = 0;

        char m_current_link = '*';
        char m_next_link    = '*';

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

        create_link_cb m_on_create_link = nullptr;
    };

}  // namespace Toolbox