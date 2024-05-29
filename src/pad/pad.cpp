#include "pad/pad.hpp"

#include <algorithm>
#include <fstream>
#include <magic_enum.hpp>
#include <numeric>

namespace Toolbox {

    template <typename T>
    static std::vector<PadInputInfo<T>> deserializeInputAnalog(Deserializer &in, u32 frames_ofs,
                                                               u32 data_ofs, u32 frames_count) {

        in.seek(frames_ofs, std::ios::beg);

        std::vector<u32> pad_frame_data;
        for (u32 i = 0; i < frames_count; i++) {
            pad_frame_data.push_back(in.read<u32, std::endian::big>());
        }

        in.seek(data_ofs, std::ios::beg);

        std::vector<T> pad_input_data;
        for (u32 i = 0; i < frames_count; i++) {
            pad_input_data.push_back(in.read<T, std::endian::big>());
        }

        std::vector<PadInputInfo<T>> pad_info;
        for (u32 i = 0; i < frames_count; i++) {
            pad_info.emplace_back(pad_frame_data[i], pad_input_data[i]);
        }

        return pad_info;
    }

    static std::vector<PadInputInfo<PadButtons>>
    deserializeInputButtons(Deserializer &in, u32 frames_ofs, u32 data_ofs, u32 frames_count) {

        in.seek(frames_ofs, std::ios::beg);

        std::vector<u32> pad_frame_data;
        for (u32 i = 0; i < frames_count; i++) {
            pad_frame_data.push_back(in.read<u32, std::endian::big>());
        }

        in.seek(data_ofs, std::ios::beg);

        std::vector<u16> pad_input_data;
        for (u32 i = 0; i < frames_count; i++) {
            pad_input_data.push_back(in.read<u16, std::endian::big>());
        }

        std::vector<PadInputInfo<PadButtons>> pad_info;
        for (u32 i = 0; i < frames_count; i++) {
            pad_info.emplace_back(pad_frame_data[i], PadButtons(pad_input_data[i]));
        }

        return pad_info;
    }

    Result<void, SerialError> PadData::serialize(Serializer &out) const {
        out.writeBytes({m_metatag.c_str(), 0x10});
        out.write<u32, std::endian::big>(m_frame_count);

        u32 analog_magnitude_frames = 0x40;
        u32 analog_magnitude_data =
            static_cast<u32>(analog_magnitude_frames + (m_analog_magnitude.size() << 2));

        u32 analog_direction_frames =
            static_cast<u32>(analog_magnitude_data + (m_analog_magnitude.size() << 2));
        u32 analog_direction_data =
            static_cast<u32>(analog_direction_frames + (m_analog_direction.size() << 2));

        // TODO: Nintendo files have some form of padding/alignment data that seems unimportant

        u32 button_input_frames =
            static_cast<u32>(analog_direction_data + (m_analog_direction.size() << 1));
        u32 button_input_data = static_cast<u32>(button_input_frames + (m_buttons.size() << 2));

        u32 trigger_l_frames = static_cast<u32>(button_input_data + (m_buttons.size() << 1));
        u32 trigger_l_data   = static_cast<u32>(trigger_l_frames + (m_trigger_l.size() << 2));

        u32 trigger_r_frames = static_cast<u32>(trigger_l_data + m_trigger_l.size());
        u32 trigger_r_data   = static_cast<u32>(trigger_r_frames + (m_trigger_r.size() << 2));

        out.write<u32, std::endian::big>(analog_magnitude_frames);
        out.write<u32, std::endian::big>(analog_magnitude_data);
        out.write<u32, std::endian::big>(analog_direction_frames);
        out.write<u32, std::endian::big>(analog_direction_data);
        out.write<u32, std::endian::big>(button_input_frames);
        out.write<u32, std::endian::big>(button_input_data);
        out.write<u32, std::endian::big>(trigger_l_frames);
        out.write<u32, std::endian::big>(trigger_l_data);
        out.write<u32, std::endian::big>(trigger_r_frames);
        out.write<u32, std::endian::big>(trigger_r_data);

        out.padTo(32);

        auto serializeInfo = [&out](const auto &info_vec) {
            for (const auto &info : info_vec) {
                out.write<u32, std::endian::big>(info.m_frames_active);
            }
            for (const auto &info : info_vec) {
                out.write<decltype(info.m_input_state), std::endian::big>(info.m_input_state);
            }
        };

        auto serializeButtons = [&out](const std::vector<PadInputInfo<PadButtons>> &info_vec) {
            for (const PadInputInfo<PadButtons> &info : info_vec) {
                out.write<u32, std::endian::big>(info.m_frames_active);
            }
            for (const PadInputInfo<PadButtons> &info : info_vec) {
                out.write<u16, std::endian::big>(static_cast<u16>(info.m_input_state));
            }
        };

        serializeInfo(m_analog_magnitude);
        serializeInfo(m_analog_direction);
        serializeButtons(m_buttons);
        serializeInfo(m_trigger_l);
        serializeInfo(m_trigger_r);

        return {};
    }

    Result<void, SerialError> PadData::deserialize(Deserializer &in) {
        m_metatag = in.readCString(0x10);

        TOOLBOX_ASSERT(m_metatag == "MARIO RECORDv0.2",
                       "[PAD] Invalid metatag (expected \"MARIO RECORDv0.2\")");

        m_frame_count = in.read<u32, std::endian::big>();

        u32 analog_magnitude_frames = in.read<u32, std::endian::big>();
        u32 analog_magnitude_data   = in.read<u32, std::endian::big>();

        u32 analog_direction_frames = in.read<u32, std::endian::big>();
        u32 analog_direction_data   = in.read<u32, std::endian::big>();

        u32 button_input_frames = in.read<u32, std::endian::big>();
        u32 button_input_data   = in.read<u32, std::endian::big>();

        u32 trigger_l_frames = in.read<u32, std::endian::big>();
        u32 trigger_l_data   = in.read<u32, std::endian::big>();

        u32 trigger_r_frames = in.read<u32, std::endian::big>();
        u32 trigger_r_data   = in.read<u32, std::endian::big>();

        u32 analog_magnitude_frames_size = (analog_magnitude_data - analog_magnitude_frames) >> 2;
        u32 analog_direction_frames_size = (analog_direction_data - analog_direction_frames) >> 2;
        u32 button_input_frames_size     = (button_input_data - button_input_frames) >> 2;
        u32 trigger_l_frames_size        = (trigger_l_data - trigger_l_frames) >> 2;
        u32 trigger_r_frames_size        = (trigger_r_data - trigger_r_frames) >> 2;

        m_analog_magnitude = deserializeInputAnalog<float>(
            in, analog_magnitude_frames, analog_magnitude_data, analog_magnitude_frames_size);

        m_analog_direction = deserializeInputAnalog<s16>(
            in, analog_direction_frames, analog_direction_data, analog_direction_frames_size);

        m_buttons = deserializeInputButtons(in, button_input_frames, button_input_data,
                                            button_input_frames_size);

        m_trigger_l =
            deserializeInputAnalog<u8>(in, trigger_l_frames, trigger_l_data, trigger_l_frames_size);

        m_trigger_r =
            deserializeInputAnalog<u8>(in, trigger_r_frames, trigger_r_data, trigger_r_frames_size);

        return {};
    }

    Result<void, SerialError> PadData::toText(std::ofstream &out) const {
        out << "# Analog Magnitude: ([How long in frames], [How far the stick is pushed from 0 to "
               "32])"
            << std::endl;
        for (const auto &info : m_analog_magnitude) {
            out << std::format("analog_magnitude({}, {})", info.m_frames_active, info.m_input_state)
                << std::endl;
        }
        out << "# Actions for " << m_frame_count << " frames" << std::endl;

        out << std::endl;

        out << "# Analog Direction: ([How long in frames], [Stick Direction from -180 to 180)"
            << std::endl;
        for (const auto &info : m_analog_direction) {
            out << std::format("analog_direction({}, {})", info.m_frames_active,
                               convertAngleS16ToFloat(info.m_input_state))
                << std::endl;
        }
        out << "# Actions for " << m_frame_count << " frames" << std::endl;

        out << std::endl;

        constexpr std::array<std::string_view, 16> c_button_names = {
            "LEFT", "RIGHT", "DOWN", "UP", "Z", "R", "L", "0x0080", "A", "B", "X", "Y", "START"};

        out << "# Buttons pressed: ([How long in frames], [Which buttons pressed])" << std::endl;

        for (const auto &info : m_buttons) {
            std::string str_input = "";
            for (size_t i = 0; i < c_button_names.size(); i++) {
                if ((info.m_input_state & PadButtons((1 << i))) != PadButtons::BUTTON_NONE) {
                    if (!str_input.empty()) {
                        str_input += "|";
                    }
                    str_input += c_button_names[i];
                }
            }
            out << std::format("buttons_pressed({}, {})", info.m_frames_active, str_input)
                << std::endl;
        }
        out << "# Actions for " << m_frame_count << " frames" << std::endl;

        out << std::endl;

        out << "# Trigger 1 held: ([How long in frames], [How far it is pushed from 0 to 150])"
            << std::endl;
        for (const auto &info : m_trigger_l) {
            out << std::format("trigger_1_held({}, {})", info.m_frames_active, info.m_input_state)
                << std::endl;
        }
        out << "# Actions for " << m_frame_count << " frames" << std::endl;

        out << std::endl;

        out << "# Trigger 2 held: ([How long in frames], [How far it is pushed from 0 to 150])"
            << std::endl;
        for (const auto &info : m_trigger_r) {
            out << std::format("trigger_2_held({}, {})", info.m_frames_active, info.m_input_state)
                << std::endl;
        }
        out << "# Actions for " << m_frame_count << " frames" << std::endl;

        return {};
    }

    Result<void, SerialError> PadData::fromText(std::ifstream &in) { return {}; }

    bool PadData::calcFrameCount(u32 &frame_count) {
        frame_count = 0;

        u32 buttons_frame_count = std::accumulate(
            m_buttons.begin(), m_buttons.end(), 0,
            [](u32 val, const PadInputInfo<PadButtons> &info) { return val + info.m_frames_active; });

        u32 analog_magnitude_frame_count = std::accumulate(
            m_analog_magnitude.begin(), m_analog_magnitude.end(), 0,
            [](u32 val, const PadInputInfo<float> &info) { return val + info.m_frames_active; });

        if (buttons_frame_count != analog_magnitude_frame_count) {
            return false;
        }

        u32 analog_direction_frame_count = std::accumulate(
            m_analog_direction.begin(), m_analog_direction.end(), 0,
            [](u32 val, const PadInputInfo<s16> &info) { return val + info.m_frames_active; });

        if (analog_magnitude_frame_count != analog_direction_frame_count) {
            return false;
        }

        u32 trigger_l_frame_count = std::accumulate(
            m_trigger_l.begin(), m_trigger_l.end(), 0,
            [](u32 val, const PadInputInfo<u8> &info) { return val + info.m_frames_active; });

        if (analog_direction_frame_count != trigger_l_frame_count) {
            return false;
        }

        u32 trigger_r_frame_count = std::accumulate(
            m_trigger_r.begin(), m_trigger_r.end(), 0,
            [](u32 val, const PadInputInfo<u8> &info) { return val + info.m_frames_active; });

        if (trigger_l_frame_count != trigger_r_frame_count) {
            return false;
        }

        m_frame_count = buttons_frame_count;
        frame_count = buttons_frame_count;
        return true;
    }

    u32 PadData::getPadAnalogMagnitudeStartFrame(size_t index) const {
        u32 start_frame = 0;
        for (size_t i = 0; i < index; ++i) {
            start_frame += m_analog_magnitude[i].m_frames_active;
        }
        return start_frame;
    }

    size_t PadData::getPadAnalogMagnitudeIndex(u32 start_frame) const {
        u32 frame = 0;
        for (size_t i = 0; i < m_analog_magnitude.size(); ++i) {
            frame += m_analog_magnitude[i].m_frames_active;
            if (frame >= start_frame) {
                return i;
            }
        }
        return npos;
    }

    u32 PadData::getPadAnalogDirectionStartFrame(size_t index) const {
        u32 start_frame = 0;
        for (size_t i = 0; i < index; ++i) {
            start_frame += m_analog_direction[i].m_frames_active;
        }
        return start_frame;
    }

    size_t PadData::getPadAnalogDirectionIndex(u32 start_frame) const {
        u32 frame = 0;
        for (size_t i = 0; i < m_analog_direction.size(); ++i) {
            frame += m_analog_direction[i].m_frames_active;
            if (frame >= start_frame) {
                return i;
            }
        }
        return npos;
    }

    u32 PadData::getPadButtonStartFrame(size_t index) const {
        u32 start_frame = 0;
        for (size_t i = 0; i < index; ++i) {
            start_frame += m_buttons[i].m_frames_active;
        }
        return start_frame;
    }

    size_t PadData::getPadButtonIndex(u32 start_frame) const {
        u32 frame = 0;
        for (size_t i = 0; i < m_buttons.size(); ++i) {
            frame += m_buttons[i].m_frames_active;
            if (frame >= start_frame) {
                return i;
            }
        }
        return npos;
    }

    u32 PadData::getPadTriggerLStartFrame(size_t index) const {
        u32 start_frame = 0;
        for (size_t i = 0; i < index; ++i) {
            start_frame += m_trigger_l[i].m_frames_active;
        }
        return start_frame;
    }

    size_t PadData::getPadTriggerLIndex(u32 start_frame) const {
        u32 frame = 0;
        for (size_t i = 0; i < m_trigger_l.size(); ++i) {
            frame += m_trigger_l[i].m_frames_active;
            if (frame >= start_frame) {
                return i;
            }
        }
        return npos;
    }

    u32 PadData::getPadTriggerRStartFrame(size_t index) const {
        u32 start_frame = 0;
        for (size_t i = 0; i < index; ++i) {
            start_frame += m_trigger_r[i].m_frames_active;
        }
        return start_frame;
    }

    size_t PadData::getPadTriggerRIndex(u32 start_frame) const {
        u32 frame = 0;
        for (size_t i = 0; i < m_trigger_r.size(); ++i) {
            frame += m_trigger_r[i].m_frames_active;
            if (frame >= start_frame) {
                return i;
            }
        }
        return npos;
    }

    size_t PadData::addPadAnalogMagnitudeInput(u32 start_frame, u32 frames_held, float magnitude) {
#if 0
        size_t index = getPadAnalogMagnitudeIndex(start_frame);
        std::vector<size_t> intersecting_indicies =
            _collectIntersectingInputs(m_analog_magnitude, start_frame, frames_held);
        // TODO: Fragment existing inputs to constructively insert new input
        size_t shift = 0;
        for (size_t i : intersecting_indicies) {
            PadInputInfo<float> &info = m_analog_magnitude[i];
            u32 info_start_frame      = getPadAnalogMagnitudeStartFrame(i);
            u32 info_end_frame        = info_start_frame + info.m_frames_active;
            if (info_start_frame < start_frame) {
                // Fragment the input at the start
                u32 new_frames_active = start_frame - info_start_frame;
                m_analog_magnitude.insert(
                    m_analog_magnitude.begin() + i,
                    PadInputInfo<float>(new_frames_active, info.m_input_state));
                shift += 1;  // Shift the index point
            }
            if (info_end_frame > start_frame + frames_held) {
                // Fragment the input at the end
                u32 new_frames_active = info_end_frame - (start_frame + frames_held);
                m_analog_magnitude.insert(
                    m_analog_magnitude.begin() + i + 1,
                    PadInputInfo<float>(new_frames_active, info.m_input_state));
                shift += 1;  // Shift the index point
            }
        }
        // TODO: Insert new input
        return index;
#else
        m_analog_magnitude.emplace_back(frames_held, magnitude);
        return m_analog_magnitude.size() - 1;
#endif
    }

    size_t PadData::addPadAnalogDirectionInput(u32 start_frame, u32 frames_held, float direction) {
        m_analog_direction.emplace_back(frames_held, convertAngleFloatToS16(direction));
        return m_analog_direction.size() - 1;
    }

    size_t PadData::addPadAnalogDirectionInput(u32 start_frame, u32 frames_held, s16 direction) {
        m_analog_direction.emplace_back(frames_held, direction);
        return m_analog_direction.size() - 1;
    }

    size_t PadData::addPadButtonInput(u32 start_frame, u32 frames_held, PadButtons buttons) {
        m_buttons.emplace_back(frames_held, buttons);
        return m_buttons.size() - 1;
    }

    size_t PadData::addPadTriggerLInput(u32 start_frame, u32 frames_held, float intensity) {
        m_trigger_l.emplace_back(frames_held, static_cast<u8>(intensity * 150.0f));
        return m_trigger_l.size() - 1;
    }

    size_t PadData::addPadTriggerLInput(u32 start_frame, u32 frames_held, u8 intensity) {
        m_trigger_l.emplace_back(frames_held, intensity);
        return m_trigger_l.size() - 1;
    }

    size_t PadData::addPadTriggerRInput(u32 start_frame, u32 frames_held, float intensity) {
        m_trigger_r.emplace_back(frames_held, static_cast<u8>(intensity * 150.0f));
        return m_trigger_r.size() - 1;
    }

    size_t PadData::addPadTriggerRInput(u32 start_frame, u32 frames_held, u8 intensity) {
        m_trigger_r.emplace_back(frames_held, intensity);
        return m_trigger_r.size() - 1;
    }

    Result<void, BaseError> PadData::removePadAnalogMagnitudeInput(size_t index) {
        if (index >= m_analog_magnitude.size()) {
            return make_error<void>("PAD", std::format("Index out of bounds (index {} exceeds {})",
                                                       index, m_analog_magnitude.size()));
        }
        if (index == m_analog_magnitude.size() - 1) {
            m_analog_magnitude.pop_back();
        } else {
            m_analog_magnitude[0].m_input_state = 0.0f;
        }
        return {};
    }

    Result<void, BaseError> PadData::removePadAnalogDirectionInput(size_t index) {
        if (index >= m_analog_direction.size()) {
            return make_error<void>("PAD", std::format("Index out of bounds (index {} exceeds {})",
                                                       index, m_analog_direction.size()));
        }
        if (index == m_analog_direction.size() - 1) {
            m_analog_direction.pop_back();
        } else {
            m_analog_direction[0].m_input_state = 0;
        }
        return {};
    }

    Result<void, BaseError> PadData::removePadButtonInput(size_t index) {
        if (index >= m_buttons.size()) {
            return make_error<void>("PAD", std::format("Index out of bounds (index {} exceeds {})",
                                                       index, m_buttons.size()));
        }
        if (index == m_buttons.size() - 1) {
            m_buttons.pop_back();
        } else {
            m_buttons[0].m_input_state = PadButtons::BUTTON_NONE;
        }
        return {};
    }

    Result<void, BaseError> PadData::removePadTriggerLInput(size_t index) {
        if (index >= m_trigger_l.size()) {
            return make_error<void>("PAD", std::format("Index out of bounds (index {} exceeds {})",
                                                       index, m_trigger_l.size()));
        }
        if (index == m_trigger_l.size() - 1) {
            m_trigger_l.pop_back();
        } else {
            m_trigger_l[0].m_input_state = 0;
        }
        return {};
    }

    Result<void, BaseError> PadData::removePadTriggerRInput(size_t index) {
        if (index >= m_trigger_r.size()) {
            return make_error<void>("PAD", std::format("Index out of bounds (index {} exceeds {})",
                                                       index, m_trigger_r.size()));
        }
        if (index == m_trigger_r.size() - 1) {
            m_trigger_r.pop_back();
        } else {
            m_trigger_r[0].m_input_state = 0;
        }
        return {};
    }

    void PadData::setPadAnalogMagnitudeInput(size_t index, float new_magnitude) {}

    void PadData::setPadAnalogDirectionInput(size_t index, float new_direction) {}

    void PadData::setPadAnalogDirectionInput(size_t index, s16 new_direction) {}

    void PadData::setPadButtonInput(size_t index, PadButtons new_buttons) {}

    void PadData::setPadTriggerLInput(size_t index, float new_intensity) {}

    void PadData::setPadTriggerLInput(size_t index, u8 new_intensity) {}

    void PadData::setPadTriggerRInput(size_t index, float new_intensity) {}

    void PadData::setPadTriggerRInput(size_t index, u8 new_intensity) {}

    size_t PadData::retimePadAnalogMagnitudeInput(size_t index, u32 new_start, u32 new_length) {
        return size_t();
    }

    size_t PadData::retimePadAnalogDirectionInput(size_t index, u32 new_start, u32 new_length) {
        return size_t();
    }

    size_t PadData::retimePadButtonInput(size_t index, u32 new_start, u32 new_length) {
        return size_t();
    }

    size_t PadData::retimePadTriggerLInput(size_t index, u32 new_start, u32 new_length) {
        return size_t();
    }

    size_t PadData::retimePadTriggerRInput(size_t index, u32 new_start, u32 new_length) {
        return size_t();
    }

    void PadData::trim(PadTrimCommand command) {
        if (command == PadTrimCommand::TRIM_START || command == PadTrimCommand::TRIM_BOTH) {
            if (m_analog_magnitude.size() > 1) {
                if (m_analog_magnitude[0].m_input_state == 0.0f) {
                    m_analog_magnitude.erase(m_analog_magnitude.begin());
                }
            }

            if (m_analog_direction.size() > 1) {
                if (m_analog_direction[0].m_input_state == 0) {
                    m_analog_direction.erase(m_analog_direction.begin());
                }
            }

            if (m_buttons.size() > 1) {
                if (m_buttons[0].m_input_state == PadButtons::BUTTON_NONE) {
                    m_buttons.erase(m_buttons.begin());
                }
            }

            if (m_trigger_l.size() > 1) {
                if (m_trigger_l[0].m_input_state == 0) {
                    m_trigger_l.erase(m_trigger_l.begin());
                }
            }

            if (m_trigger_r.size() > 1) {
                if (m_trigger_r[0].m_input_state == 0) {
                    m_trigger_r.erase(m_trigger_r.begin());
                }
            }
        }

        if (command == PadTrimCommand::TRIM_END || command == PadTrimCommand::TRIM_BOTH) {
            if (m_analog_magnitude.size() > 1) {
                if (m_analog_magnitude.back().m_input_state == 0.0f) {
                    m_analog_magnitude.pop_back();
                }
            }

            if (m_analog_direction.size() > 1) {
                if (m_analog_direction.back().m_input_state == 0) {
                    m_analog_direction.pop_back();
                }
            }

            if (m_buttons.size() > 1) {
                if (m_buttons.back().m_input_state == PadButtons::BUTTON_NONE) {
                    m_buttons.pop_back();
                }
            }

            if (m_trigger_l.size() > 1) {
                if (m_trigger_l.back().m_input_state == 0) {
                    m_trigger_l.pop_back();
                }
            }

            if (m_trigger_r.size() > 1) {
                if (m_trigger_r.back().m_input_state == 0) {
                    m_trigger_r.pop_back();
                }
            }
        }
    }

}  // namespace Toolbox
