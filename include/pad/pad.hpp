#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "core/types.hpp"
#include "serial.hpp"

namespace Toolbox {

    enum class PadButtons : u16 {
        BUTTON_NONE  = (0 << 0),
        BUTTON_LEFT  = (1 << 0),
        BUTTON_RIGHT = (1 << 1),
        BUTTON_DOWN  = (1 << 2),
        BUTTON_UP    = (1 << 3),
        BUTTON_Z     = (1 << 4),
        BUTTON_R     = (1 << 5),
        BUTTON_L     = (1 << 6),
        BUTTON_A     = (1 << 8),
        BUTTON_B     = (1 << 9),
        BUTTON_X     = (1 << 10),
        BUTTON_Y     = (1 << 11),
        BUTTON_START = (1 << 12),
    };
    TOOLBOX_BITWISE_ENUM(PadButtons)

    template <typename T> struct PadInputInfo {
        u32 m_frames_active;
        T m_input_state;
    };

    enum class PadTrimCommand {
        TRIM_NONE,
        TRIM_START,
        TRIM_END,
        TRIM_BOTH,
    };

    class PadData : public ISerializable {
    public:
        PadData()                = default;
        PadData(const PadData &) = default;
        PadData(PadData &&)      = default;

        PadData &operator=(const PadData &) = default;
        PadData &operator=(PadData &&)      = default;

        constexpr f32 convertAngleS16ToFloat(s16 angle) const {
            return static_cast<f32>(angle) / 182.04445f;
        }

        constexpr s16 convertAngleFloatToS16(f32 angle) const {
            return static_cast<s16>(angle * 182.04445f);
        }

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        Result<void, SerialError> toText(std::ofstream &out) const;
        Result<void, SerialError> fromText(std::ifstream &in);

        u32 getFrameCount() const noexcept { return m_frame_count; }
        void setFrameCount(u32 frame_count) noexcept { m_frame_count = frame_count; }

        size_t getPadAnalogMagnitudeInfoCount() const noexcept { return m_analog_magnitude.size(); }
        size_t getPadAnalogDirectionInfoCount() const noexcept { return m_analog_direction.size(); }
        size_t getPadButtonInfoCount() const noexcept { return m_buttons.size(); }
        size_t getPadTriggerLInfoCount() const noexcept { return m_trigger_l.size(); }
        size_t getPadTriggerRInfoCount() const noexcept { return m_trigger_r.size(); }

        u32 getPadAnalogMagnitudeStartFrame(size_t index) const;
        size_t getPadAnalogMagnitudeIndex(u32 start_frame) const;

        u32 getPadAnalogDirectionStartFrame(size_t index) const;
        size_t getPadAnalogDirectionIndex(u32 start_frame) const;

        u32 getPadButtonStartFrame(size_t index) const;
        size_t getPadButtonIndex(u32 start_frame) const;

        u32 getPadTriggerLStartFrame(size_t index) const;
        size_t getPadTriggerLIndex(u32 start_frame) const;

        u32 getPadTriggerRStartFrame(size_t index) const;
        size_t getPadTriggerRIndex(u32 start_frame) const;

        // These add a chunk of data to the info list
        size_t addPadAnalogMagnitudeInput(u32 start_frame, u32 frames_held, float magnitude);

        size_t addPadAnalogDirectionInput(u32 start_frame, u32 frames_held, float direction);
        size_t addPadAnalogDirectionInput(u32 start_frame, u32 frames_held, s16 direction);

        size_t addPadButtonInput(u32 start_frame, u32 frames_held, PadButtons buttons);

        size_t addPadTriggerLInput(u32 start_frame, u32 frames_held, float intensity);
        size_t addPadTriggerLInput(u32 start_frame, u32 frames_held, u8 intensity);

        size_t addPadTriggerRInput(u32 start_frame, u32 frames_held, float intensity);
        size_t addPadTriggerRInput(u32 start_frame, u32 frames_held, u8 intensity);

        // These remove a chunk of data from the info list
        Result<void, BaseError> removePadAnalogMagnitudeInput(size_t index);
        Result<void, BaseError> removePadAnalogDirectionInput(size_t index);
        Result<void, BaseError> removePadButtonInput(size_t index);
        Result<void, BaseError> removePadTriggerLInput(size_t index);
        Result<void, BaseError> removePadTriggerRInput(size_t index);

        // These get existing entries
        const PadInputInfo<float> &getPadAnalogMagnitudeInput(size_t index) const {
            return m_analog_magnitude[index];
        }

        // These get existing entries
        const PadInputInfo<s16> &getPadAnalogDirectionInput(size_t index) const {
            return m_analog_direction[index];
        }

        // These get existing entries
        const PadInputInfo<PadButtons> &getPadButtonInput(size_t index) const {
            return m_buttons[index];
        }

        // These get existing entries
        const PadInputInfo<u8> &getPadTriggerLInput(size_t index) const {
            return m_trigger_l[index];
        }

        // These get existing entries
        const PadInputInfo<u8> &getPadTriggerRInput(size_t index) const {
            return m_trigger_r[index];
        }

        // These modify existing entries
        void setPadAnalogMagnitudeInput(size_t index, float new_magnitude);
        void setPadAnalogDirectionInput(size_t index, float new_direction);

        void setPadAnalogDirectionInput(size_t index, s16 new_direction);
        void setPadButtonInput(size_t index, PadButtons new_buttons);

        void setPadTriggerLInput(size_t index, float new_intensity);
        void setPadTriggerLInput(size_t index, u8 new_intensity);

        void setPadTriggerRInput(size_t index, float new_intensity);
        void setPadTriggerRInput(size_t index, u8 new_intensity);

        // These modify existing entries
        size_t retimePadAnalogMagnitudeInput(size_t index, u32 new_start, u32 new_length);
        size_t retimePadAnalogDirectionInput(size_t index, u32 new_start, u32 new_length);
        size_t retimePadButtonInput(size_t index, u32 new_start, u32 new_length);
        size_t retimePadTriggerLInput(size_t index, u32 new_start, u32 new_length);
        size_t retimePadTriggerRInput(size_t index, u32 new_start, u32 new_length);

        void trim(PadTrimCommand command = PadTrimCommand::TRIM_BOTH);

    protected:
        template <typename T>
        std::vector<size_t> _collectIntersectingInputs(const std::vector<PadInputInfo<T>> &infos,
                                                       u32 start_frame, u32 frames_held) {
            std::vector<size_t> intersecting_inputs;
            u32 current_start_frame = 0;
            u32 end_frame           = start_frame + frames_held;
            for (size_t i = 0; i < infos.size(); ++i) {
                // Check for input intersection
                if (current_start_frame < start_frame + frames_held &&
                    start_frame < current_start_frame + infos[i].m_frames_active) {
                    intersecting_inputs.push_back(i);
                }
                current_start_frame += infos[i].m_frames_active;
            }
            return intersecting_inputs;
        }

    private:
        std::string m_metatag = "MARIO RECORDv0.2";

        u32 m_frame_count                                   = 0;
        std::vector<PadInputInfo<float>> m_analog_magnitude = {};
        std::vector<PadInputInfo<s16>> m_analog_direction   = {};
        std::vector<PadInputInfo<PadButtons>> m_buttons     = {};
        std::vector<PadInputInfo<u8>> m_trigger_l           = {};
        std::vector<PadInputInfo<u8>> m_trigger_r           = {};
    };

}  // namespace Toolbox