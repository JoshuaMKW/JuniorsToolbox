#pragma once

#include "core/memory.hpp"
#include "core/threaded.hpp"
#include "core/time/timestep.hpp"
#include "dolphin/process.hpp"
#include "fsystem.hpp"

#include "pad/linkdata.hpp"
#include "pad/pad.hpp"

#include <glm/glm.hpp>
#include <numbers>

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

        enum class PadSourceType {
            SOURCE_PLAYER,
            SOURCE_EMARIO,
            SOURCE_PIANTISSIMO,
        };

        struct PadFrameData {
            PadButtons m_held_buttons;
            PadButtons m_pressed_buttons;
            u8 m_trigger_l;
            u8 m_trigger_r;
            f32 m_stick_x;
            f32 m_stick_y;
            f32 m_stick_mag;
            s16 m_stick_angle;
            f32 m_c_stick_x;
            f32 m_c_stick_y;
            f32 m_c_stick_mag;
            s16 m_c_stick_angle;
            f32 m_rumble_x;
            f32 m_rumble_y;
        };

        using create_link_cb = std::function<void(const ReplayLinkNode &)>;
        using playback_frame_cb = std::function<void(const PadFrameData &)>;

        PadRecorder();
        PadRecorder(const PadRecorder &)     = delete;
        PadRecorder(PadRecorder &&) noexcept = default;
        ~PadRecorder()                       = default;

        PadRecorder &operator=(const PadRecorder &)     = delete;
        PadRecorder &operator=(PadRecorder &&) noexcept = default;

        static inline float convertAngleS16ToRadians(s16 angle) {
            return PadData::convertAngleS16ToFloat(angle) * (std::numbers::pi / 180.0f);
        }

        const std::vector<PadDataLinkInfo> &padData() const noexcept { return m_pad_datas; }
        RefPtr<ReplayLinkData> linkData() const noexcept { return m_link_data; }

        [[nodiscard]] bool hasRecordData(char from_link, char to_link) const;

        [[nodiscard]] u8 getPort() const { return m_port; }
        void setPort(u8 port) { m_port = port; }

        [[nodiscard]] PadTrimCommand getTrimState() const { return m_trim_state; }
        void setTrimState(PadTrimCommand state) { m_trim_state = state; }

        [[nodiscard]] bool isCameraInversed() const { return m_world_space.load(); }
        void setCameraInversed(bool inversed) { m_world_space.store(inversed); }

        [[nodiscard]] bool isRecordComplete() const;

        [[nodiscard]] bool isPlaying() const { return m_play_flag.load(); }
        [[nodiscard]] bool isPlaying(char from_link, char to_link) const {
            return isPlaying() && m_current_link == from_link && m_next_link == to_link;
        }

        [[nodiscard]] bool isRecording() const { return m_record_flag.load(); }
        [[nodiscard]] bool isRecording(char from_link, char to_link) const {
            return isRecording() && m_current_link == from_link && m_next_link == to_link &&
                   from_link != '*' && to_link != '*';
        }

        void resetRecording() {
            m_record_flag.store(false);
            m_link_data->clearLinkNodes();
            m_pad_datas.clear();
        }
        void startRecording();
        void startRecording(char from_link, char to_link);
        void stopRecording();

        bool loadFromFolder(const std::filesystem::path &folder_path);
        bool saveToFolder(const std::filesystem::path &folder_path);

        bool loadPadRecording(char from_link, char to_link, const std::filesystem::path &file_path);
        bool savePadRecording(char from_link, char to_link, const std::filesystem::path &file_path);

        bool playPadRecording(char from_link, char to_link, playback_frame_cb on_frame_cb);
        void stopPadPlayback();

        void clearLink(char from_link, char to_link);

        void onCreateLink(create_link_cb callback) { m_on_create_link = callback; }

        Result<PadFrameData> readPadFrameData(PadSourceType source);
        PadFrameData getPadFrameData(char from_link, char to_link, u32 frame) const;
        u32 getPadFrameCount(char from_link, char to_link);

    protected:
        void tRun(void *param) override;
        void sleep();

        Result<void> processCurrentFrame(PadFrameData &&frame_data);

        void initNextInputData();
        void initNextInputData(char from_link, char to_link);
        void applyInputChunk();

        void playPadData(TimeStep delta_time);
        void recordPadData();
        void resetRecordState();
        void resetRecordState(char from_link, char to_link);
        void initNewLinkData();

        bool setPlayerTransRot(const glm::vec3 &pos, f32 rotY);

        Result<bool, FSError> loadFromFolder_(const std::filesystem::path &folder_path);
        Result<bool, FSError> saveToFolder_(const std::filesystem::path &folder_path);

        s32 getFrameStep() const;

        Result<PadFrameData> readPadFrameDataPlayer();
        Result<PadFrameData> readPadFrameDataEMario();
        Result<PadFrameData> readPadFrameDataPiantissimo();

    private:
        RefPtr<ReplayLinkData> m_link_data;
        std::vector<PadDataLinkInfo> m_pad_datas = {};

        u8 m_port                   = 0;
        PadTrimCommand m_trim_state = PadTrimCommand::TRIM_NONE;

        u8 m_scene_id = 0, m_episode_id = 0;
        bool m_is_viewing_shadow_mario = false;
        bool m_is_viewing_piantissimo  = false;
        u32 m_shadow_mario_ptr         = 0;
        u32 m_piantissimo_ptr          = 0;

        TimePoint m_last_frame_time;
        bool m_is_replaying_pad = false;
        playback_frame_cb m_playback_frame_cb = nullptr;

        bool m_first_input_found          = false;
        PadButtons m_last_pressed_buttons = PadButtons::BUTTON_NONE;
        u32 m_start_frame                 = 0;
        u32 m_last_frame                  = 0;
        float m_playback_frame            = 0.0f;

        char m_current_link = '*';
        char m_next_link    = '*';

        PadRecordInfo<float> m_analog_magnitude_info = {};
        PadRecordInfo<s16> m_analog_direction_info   = {};
        PadRecordInfo<PadButtons> m_button_info      = {};
        PadRecordInfo<u8> m_trigger_l_info           = {};
        PadRecordInfo<u8> m_trigger_r_info           = {};

        std::mutex m_mutex;
        std::atomic<bool> m_play_flag   = false;
        std::atomic<bool> m_record_flag = false;
        std::atomic<bool> m_world_space = true;

        std::atomic<bool> m_kill_flag = false;
        std::condition_variable m_kill_condition;

        create_link_cb m_on_create_link = nullptr;
    };

}  // namespace Toolbox