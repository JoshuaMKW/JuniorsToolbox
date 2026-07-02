#include "bmg/bmg.hpp"
#include "magic_enum.hpp"
#include "strutil.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace std::string_literals;

namespace Toolbox::BMG {

    static std::unordered_map<std::string_view, std::string> s_command_to_raw = {
        {"{{text:slow}}",           "\x1A\x05\x00\x00\x00"s    },
        {"{{text:end_close}}",      "\x1A\x05\x00\x00\x01"s    },
        {"{{ctx:bananas}}",         "\x1A\x06\xFF\x00\x04\x00"s},
        {"{{ctx:coconuts}}",        "\x1A\x06\xFF\x00\x04\x01"s},
        {"{{ctx:pineapples}}",      "\x1A\x06\xFF\x00\x04\x02"s},
        {"{{ctx:durians}}",         "\x1A\x06\xFF\x00\x04\x03"s},
        {"{{color:white}}",         "\x1A\x06\xFF\x00\x00\x00"s},
        {"{{color:red}}",           "\x1A\x06\xFF\x00\x00\x02"s},
        {"{{color:blue}}",          "\x1A\x06\xFF\x00\x00\x03"s},
        {"{{color:yellow}}",        "\x1A\x06\xFF\x00\x00\x04"s},
        {"{{color:green}}",         "\x1A\x06\xFF\x00\x00\x05"s},
        {"{{record:race_pianta}}",  "\x1A\x05\x02\x00\x00"s    },
        {"{{record:race_gelato}}",  "\x1A\x05\x02\x00\x01"s    },
        {"{{record:crate_time}}",   "\x1A\x05\x02\x00\x02"s    },
        {"{{record:bcoin_shines}}", "\x1A\x05\x02\x00\x03"s    },
        {"{{record:race_noki}}",    "\x1A\x05\x02\x00\x06"s    },
    };

    static std::unordered_map<std::string_view, std::string> s_raw_to_command = {
        {"\x1A\x05\x00\x00\x00"s,     "{{text:slow}}"          },
        {"\x1A\x05\x00\x00\x01"s,     "{{text:end_close}}"     },
        {"\x1A\x06\xFF\x00\x04\x00"s, "{{ctx:bananas}}"        },
        {"\x1A\x06\xFF\x00\x04\x01"s, "{{ctx:coconuts}}"       },
        {"\x1A\x06\xFF\x00\x04\x02"s, "{{ctx:pineapples}}"     },
        {"\x1A\x06\xFF\x00\x04\x03"s, "{{ctx:durians}}"        },
        {"\x1A\x06\xFF\x00\x00\x00"s, "{{color:white}}"        },
        {"\x1A\x06\xFF\x00\x00\x02"s, "{{color:red}}"          },
        {"\x1A\x06\xFF\x00\x00\x03"s, "{{color:blue}}"         },
        {"\x1A\x06\xFF\x00\x00\x04"s, "{{color:yellow}}"       },
        {"\x1A\x06\xFF\x00\x00\x05"s, "{{color:green}}"        },
        {"\x1A\x05\x02\x00\x00"s,     "{{record:race_pianta}}" },
        {"\x1A\x05\x02\x00\x01"s,     "{{record:race_gelato}}" },
        {"\x1A\x05\x02\x00\x02"s,     "{{record:crate_time}}"  },
        {"\x1A\x05\x02\x00\x03"s,     "{{record:bcoin_shines}}"},
        {"\x1A\x05\x02\x00\x06"s,     "{{record:race_noki}}"   },
    };

    static u16 GetValueFromMessageFlagSize(MessageFlagSize size) {
        switch (size) {
        case MessageFlagSize::FLAG_SIZE_SYSTEM:
            return 4;
        case MessageFlagSize::FLAG_SIZE_UNK8:
            return 8;
        case MessageFlagSize::FLAG_SIZE_NPC:
            return 12;
        }
        return 0;
    }

    static MessageFlagSize GetMessageFlagSizeFromValue(u16 value) {
        switch (value) {
        case 4:
            return MessageFlagSize::FLAG_SIZE_SYSTEM;
        case 8:
            return MessageFlagSize::FLAG_SIZE_UNK8;
        case 12:
            return MessageFlagSize::FLAG_SIZE_NPC;
        }
        return MessageFlagSize::FLAG_SIZE_NPC;
    }

    std::size_t CmdMessage::getDataSize() const {
        std::vector<std::string_view> parts = getParts();
        size_t size                    = 0;
        for (const auto &part : parts) {
            auto result = rawFromCommand(part);
            if (!result) {
                size += part.size();
            } else {
                size += result.value().size();
            }
        }
        return size + 1;
    }

    const std::string &CmdMessage::getString() const { return m_message_data; }

    std::string CmdMessage::getSimpleString() const {
        std::vector<std::string_view> parts = getParts();
        std::string txt;
        for (const auto &part : parts) {
            auto result = rawFromCommand(part);
            if (result)
                continue;
            txt += part;
        }
        return txt;
    }

    Result<void, SerialError> CmdMessage::serialize(Serializer &out) const {
        out.write<u64>(m_uuid);
        return gameSerialize(out);
    }

    Result<void, SerialError> CmdMessage::deserialize(Deserializer &in) {
        m_uuid = in.read<u64>();
        return gameDeserialize(in);
    }

    Result<void, SerialError> CmdMessage::gameSerialize(Serializer &out) const {
        std::vector<std::string_view> parts = getParts();
        for (size_t i = 0; i < parts.size(); ++i) {
            std::string_view part = parts[i];
            
            auto result = rawFromCommand(part);
            if (!result) {
                out.writeCString(part);
                if (i < parts.size() - 1) {
                    out.seek(-1);  // Overwrite the null char
                }
            } else {
                out.writeBytes(result.value());
            }
        }

        return {};
    }

    Result<void, SerialError> CmdMessage::gameDeserialize(Deserializer &in) {
        m_message_data.clear();

        while (true) {
            auto c = in.read<char>();

            if (c == '\0') {
                break;
            }

            if (c != '\x1A') {
                m_message_data += c;
                continue;
            }

            auto cmdsize = in.read<char>();
            if (cmdsize == '\0') {
                return make_serial_error<void>(in, "Unexpected command size of 0", -1);
            }

            std::vector<char> cmddata;
            {
                cmddata.push_back(c);
                cmddata.push_back(cmdsize);
                cmddata.resize(cmdsize);

                // Append the rest of the command
                in.readBytes(std::span<char>(cmddata.begin() + 2, cmddata.end()));
            }

            auto cmd = commandFromRaw(cmddata);
            if (!cmd) {
                return make_serial_error<void>(in, "Unknown command", -1);
            }

            m_message_data += cmd.value();
        }
        return {};
    }

    void CmdMessage::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        std::vector<std::string> lines;
        size_t line_start = 0, line_end = 0;
        while (line_end != std::string::npos) {
            line_end = m_message_data.find('\n', line_start);
            lines.push_back(m_message_data.substr(line_start, line_end - line_start));
            line_start = line_end + 1;
        }

        auto indention_str = std::string(std::max(size_t(1), indention * indention_width), ' ');
        auto start_indention_str = indention_str.substr(0, indention_str.size() - 1);

        out << start_indention_str << "\"";
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i == 0) {
                out << lines[i] << std::endl;
            } else if (i == lines.size() - 1) {
                out << indention_str << lines[i] << "\"" << std::endl;
            } else {
                out << indention_str << lines[i] << std::endl;
            }
        }
    }

    std::vector<std::string_view> CmdMessage::getParts() const {
        std::vector<std::string_view> parts;

        size_t current_pos = 0;
        size_t end_pos     = 0;

        std::string_view message = m_message_data;

        while (true) {
            if (current_pos >= message.size()) {
                break;
            }

            const bool is_command_start = current_pos < message.size() - 1 &&
                                          message.at(current_pos) == '{' &&
                                          message.at(current_pos + 1) == '{';
            if (is_command_start) {
                size_t r_pos = message.find("}}", current_pos);
                if (r_pos != std::string_view::npos) {
                    std::string_view command = message.substr(current_pos, r_pos - current_pos + 2);
                    parts.push_back(command);

                    current_pos              = r_pos + 2;
                    continue;
                }

                std::string_view trailing = message.substr(current_pos);
                parts.push_back(trailing);
                break;
            }

            end_pos = message.find("{{", current_pos);
            if (end_pos == std::string_view::npos) {
                std::string_view trailing = message.substr(current_pos);
                parts.push_back(trailing);
                break;
            }

            std::string_view slice = message.substr(current_pos, end_pos - current_pos);
            parts.push_back(slice);

            current_pos = end_pos;
        }

        return parts;
    }

    std::optional<std::vector<char>> CmdMessage::rawFromCommand(std::string_view command) {
        if (command.empty() || !command.starts_with("{{") || !command.ends_with("}}")) {
            return {};
        }

        command = command.substr(1, command.size() - 2);

        if (s_command_to_raw.contains(command)) {
            const std::string &raw_str = s_command_to_raw[command];
            return std::vector<char>(raw_str.begin(), raw_str.end());
        }

        if (command.starts_with("{speed:")) {
            return std::vector<char>{0x1A, 0x06, 0x00, 0x00, 0x00, command[command.size() - 2]};
        }

        if (command.starts_with("{option:")) {
            size_t command_split = command.rfind(':');
            std::string_view message         = command.substr(command_split + 1, command.size() - (command_split + 1) - 1);

            u8 option         = String::StringToTypedIntegral<u8>(command.substr(9, command_split - 9 - 1)).value_or(0);
            size_t command_size = static_cast<char>(5 + message.size());

            std::vector<char> raw(command_size);
            {
                raw[0] = 0x1A;
                raw[1] = static_cast<char>(command_size);
                raw[2] = 0x01;
                raw[3] = 0x00;
                raw[4] = static_cast<char>(option);
            }
            std::copy(message.begin(), message.end(), raw.begin() + 5);

            return raw;
        }

        if (command.starts_with("{raw:")) {
            std::string_view message = command.substr(5, command.size() - 1);
            std::vector<char> raw(message.size() + 2);
            {
                raw[0] = 0x1A;
                raw[1] = static_cast<char>(command.size());
                std::copy(message.begin(), message.end(), raw.end());
            }
            return raw;
        }

        return {};
    }

    std::optional<std::string> CmdMessage::commandFromRaw(std::span<const char> command) {
        if (command.size() < 2 || command[0] != 0x1A) {
            return {};
        }

        if (s_raw_to_command.contains(std::string(command.begin(), command.end()))) {
            return s_raw_to_command[std::string(command.begin(), command.end())];
        }

        if (command.size() > 2) {
            if (command[0] == 0x1A && command[1] == 0x06 && command[2] == 0x00 &&
                command[3] == 0x00 && command[4] == 0x00) {
                char buf[32];
                std::snprintf(buf, 32, "{{speed:%d}}", command[5]);
                return buf;
            }

            if (command[2] == 0x01 && command[3] == 0x00) {
                char buf[32];
                std::snprintf(buf, 32, "{{option:%d:%s}}", command[4],
                              std::string(command.begin() + 5, command.end()).c_str());
                return buf;
            }
        }

        std::string txt = "{{raw:";
        for (size_t i = 2; i < command.size(); i++) {
            char buf[6];
            std::snprintf(buf, 6, "\\x%02X", command[i]);
            txt += buf;
        }
        txt += "}}";
        return txt;
    }

    const std::optional<MessageData::Entry> MessageData::getEntry(std::string_view name) {
        for (auto &entry : m_entries) {
            if (entry.getName() == name)
                return entry;
        }
        return {};
    }

    const std::optional<MessageData::Entry> MessageData::getEntry(size_t index) {
        if (index >= m_entries.size())
            return {};
        return m_entries[index];
    }

    size_t MessageData::getDataSize() const {
        return 32 + getINF1Size() + getSTR1Size() + getDAT1Size();
    }

    size_t MessageData::getINF1Size() const {
        return (0x10 + (m_entries.size() * GetValueFromMessageFlagSize(m_flag_size)) + 0x1F) & ~0x1F;
    }

    size_t MessageData::getDAT1Size() const {
        size_t size = std::accumulate(
            m_entries.begin(), m_entries.end(), size_t(9),
            [](size_t sum, const Entry &entry) { return sum + entry.getMessage().getDataSize(); });
        return (size + 0x1F) & ~0x1F;
    }

    size_t MessageData::getSTR1Size() const {
        if (!m_has_str1)
            return 0;

        size_t size = std::accumulate(
            m_entries.begin(), m_entries.end(), size_t(9),
            [](size_t sum, const Entry &entry) { return sum + entry.getName().size() + 1; });
        return (size + 0x1F) & ~0x1F;
    }

    bool MessageData::isMagicValid(Deserializer &in) {
        in.pushBreakpoint();

        in.seek(0, std::ios::beg);

        auto magic_u        = in.read<u32, std::endian::big>();
        auto magic_l        = in.read<u32, std::endian::big>();
        bool is_magic_valid = (magic_u == 'MESG' && magic_l == 'bmg1');

        in.popBreakpoint();

        return is_magic_valid;
    }

    bool MessageData::isSTR1InData(Deserializer &in) {
        in.pushBreakpoint();

        in.seek(12, std::ios::beg);
        auto section_count   = in.read<u32, std::endian::big>();
        bool is_str1_present = section_count >= 3;

        in.popBreakpoint();

        return is_str1_present;
    }

    void MessageData::addEntry(const Entry &entry) { m_entries.push_back(entry); }

    bool MessageData::insertEntry(size_t index, const Entry &entry) {
        if (index > m_entries.size())
            return false;
        m_entries.insert(m_entries.begin() + index, entry);
        return true;
    }

    void MessageData::removeEntry(const Entry &entry) {
        auto it = std::find(m_entries.begin(), m_entries.end(), entry);
        if (it == m_entries.end())
            return;
        m_entries.erase(it);
    }

    void MessageData::dump(std::ostream &out, size_t indention, size_t indention_width) const {
        auto indention_str       = std::string(indention * indention_width, ' ');
        auto value_indention_str = std::string((indention + 1) * indention_width, ' ');

        out << indention_str << "BMG Data (Flag Size: " << static_cast<size_t>(m_flag_size) << ") {"
            << std::endl;
        for (auto &entry : m_entries) {
            std::string name = entry.getName().empty() ? "(Unknown)" : entry.getName();
            out << value_indention_str << name << " (" << entry.getStartFrame() << ", "
                << entry.getEndFrame() << ", " << magic_enum::enum_name(entry.getSoundID()) << ") {"
                << std::endl;
            entry.getMessage().dump(out, indention + 2, indention_width);
            out << value_indention_str << "}" << std::endl;
        }
        out << indention_str << "}" << std::endl;
    }

    Result<void, SerialError> MessageData::serialize(Serializer &out) const {
        return gameSerialize(out);
    }

    Result<void, SerialError> MessageData::deserialize(Deserializer &in) {
        return gameDeserialize(in);
    }

    Result<void, SerialError> MessageData::gameSerialize(Serializer &out) const {
        // Header
        out.write<u32, std::endian::big>('MESG');
        out.write<u32, std::endian::big>('bmg1');
        out.write<u32, std::endian::big>(static_cast<u32>(getDataSize()) / 32);
        out.write<u32, std::endian::big>(m_has_str1 ? 3 : 2);
        out.padTo(32);

        // INF1
        out.write<u32, std::endian::big>('INF1');
        out.write<u32, std::endian::big>(static_cast<u32>(getINF1Size()));
        out.write<u16, std::endian::big>(static_cast<u16>(m_entries.size()));
        out.write<u16, std::endian::big>(GetValueFromMessageFlagSize(m_flag_size));
        out.write<u32, std::endian::big>(0x80000);  // Unknown

        size_t data_offset = 1;  // Padding
        size_t name_offset = 1;  // Padding

        for (auto &entry : m_entries) {
            out.write<u32, std::endian::big>(static_cast<u32>(data_offset));
            switch (m_flag_size) {
            case MessageFlagSize::FLAG_SIZE_SYSTEM:
                break;
            case MessageFlagSize::FLAG_SIZE_UNK8:
                out.writeBytes(entry.getUnknownFlags());
                out.seek(std::max(size_t(0), 4 - entry.getUnknownFlags().size()));
                break;
            case MessageFlagSize::FLAG_SIZE_NPC:
                out.write<u16, std::endian::big>(entry.getStartFrame());
                out.write<u16, std::endian::big>(entry.getEndFrame());
                if (m_has_str1) {
                    out.write<u16, std::endian::big>(static_cast<u32>(name_offset));
                    out.write<u8>(static_cast<u8>(entry.getSoundID()));
                    out.seek(1);
                } else {
                    out.write<u8>(static_cast<u8>(entry.getSoundID()));
                    out.seek(3);
                }
                break;
            default:
                return make_serial_error<void>(out, "Invalid flag size");
            }
            data_offset += entry.getMessage().getDataSize();
            name_offset += entry.getName().size() + 1;
        }

        // Align stream
        out.padTo(32);

        // DAT1
        out.write<u32, std::endian::big>('DAT1');
        out.write<u32, std::endian::big>(static_cast<u32>(getDAT1Size()));
        out.write<char>(0);  // Padding

        for (auto &entry : m_entries) {
            auto result = entry.getMessage().gameSerialize(out);
            if (!result)
                return std::unexpected(result.error());
        }

        // Align stream
        out.padTo(32);

        // STR1
        if (m_has_str1) {
            out.write<u32, std::endian::big>('STR1');
            out.write<u32, std::endian::big>(static_cast<u32>(getSTR1Size()));
            out.write<char>(0);  // Padding

            for (auto &entry : m_entries) {
                out.writeCString(entry.getName());
            }
        }

        // Align stream
        out.padTo(32);

        return {};
    }

    Result<void, SerialError> MessageData::gameDeserialize(Deserializer &in) {
        if (!isMagicValid(in)) {
            return make_serial_error<void>(in, "Magic of BMG is invalid! (Expected MESGbmg1)");
        }

        in.seek(8);  // Skip magic

        auto bmg_size = in.read<u32, std::endian::big>() * 32;
        {
            in.pushBreakpoint();
            in.seek(0, std::ios::end);
            if (bmg_size != static_cast<size_t>(in.tell())) {
                return make_serial_error<void>(in, "BMG size marker doesn't match stream size");
            }
            in.popBreakpoint();
        }

        m_entries.clear();

        auto section_count = in.read<u32, std::endian::big>();
        m_has_str1         = section_count >= 3;

        in.seek(0x10);  // Padding

        size_t message_count = 0;
        m_flag_size          = MessageFlagSize::FLAG_SIZE_NPC;

        std::vector<size_t> data_offsets;
        std::vector<size_t> name_offsets;

        for (size_t i = 0; i < section_count; ++i) {
            auto section_start = static_cast<size_t>(in.tell());
            auto section_magic = in.read<u32, std::endian::big>();
            auto section_size  = in.read<u32, std::endian::big>();
            if (section_magic == 'INF1') {
                if (i != 0) {
                    return make_serial_error<void>(
                        in,
                        std::format(
                            "INF1 section marker found at incorrect index (Expected 0, got {})", 1),
                        -8);
                }
                message_count = in.read<u16, std::endian::big>();
                m_flag_size   = GetMessageFlagSizeFromValue(in.read<u16, std::endian::big>());
                in.seek(4);  // Unknown values

                for (size_t j = 0; j < message_count; ++j) {
                    Entry entry;
                    data_offsets.push_back(in.read<u32, std::endian::big>());
                    switch (m_flag_size) {
                    case MessageFlagSize::FLAG_SIZE_SYSTEM:
                        break;
                    case MessageFlagSize::FLAG_SIZE_UNK8:
                        entry.m_unk_flags.resize(4);
                        in.readBytes(entry.m_unk_flags);
                        break;
                    case MessageFlagSize::FLAG_SIZE_NPC:
                        entry.setStartFrame(in.read<u16, std::endian::big>());
                        entry.setEndFrame(in.read<u16, std::endian::big>());
                        if (m_has_str1) {
                            name_offsets.push_back(in.read<u16, std::endian::big>());
                        }
                        entry.setSoundID(magic_enum::enum_cast<MessageSound>(in.read<u8>())
                                             .value_or(MessageSound::NOTHING));
                        in.seek(m_has_str1 ? 1 : 3);
                        break;
                    default:
                        return make_serial_error<void>(in, "Invalid flag size");
                    }
                    m_entries.push_back(entry);
                }
            } else if (section_magic == 'DAT1') {
                if (i == 0) {
                    return make_serial_error<void>(
                        in, "DAT1 section marker found at incorrect index (Expected > 0, got 0)",
                        -8);
                }
                in.seek(1);  // Skip padding
                for (size_t i = 0; i < m_entries.size(); ++i) {
                    in.seek(section_start + data_offsets[i] + 8, std::ios::beg);
                    auto result = m_entries[i].getMessage().gameDeserialize(in);
                    if (!result)
                        return std::unexpected(result.error());

                    /*size_t data_size;
                    if (i == m_entries.size() - 1) {
                        data_size = section_size - (static_cast<size_t>(in.tell()) - section_start);
                    } else {
                        data_size = data_offsets[i + 1] - data_offsets[i];
                    }

                    std::vector<char> message_data(data_size);
                    in.readBytes(message_data);*/
                }
            } else if (section_magic == 'STR1') {
                if (i == 0) {
                    return make_serial_error<void>(
                        in, "INF1 section marker found at incorrect index (Expected > 0, got 0)",
                        -8);
                }

                if (!m_has_str1) {
                    return make_serial_error<void>(
                        in, "INF1 found when not expected! Likely missing DAT1.", -8);
                }

                in.seek(1);  // Skip padding
                for (size_t i = 0; i < m_entries.size(); ++i) {
                    in.seek(section_start + name_offsets[i] + 8, std::ios::beg);
                    m_entries[i].setName(in.readCString(
                        section_size - (static_cast<size_t>(in.tell()) - section_start)));
                }
            }
            in.alignTo(32);
        }

        return {};
    }

    Result<void, SerialError> MessageData::Entry::serialize(Serializer &out) const {
        out.writeString(m_name);

        m_message.serialize(out);
        out.write<u8>(static_cast<u8>(m_sound));
        out.write<u16>(m_start_frame);
        out.write<u16>(m_end_frame);
        out.writeBytes(m_unk_flags);

        return {};
    }

    Result<void, SerialError> MessageData::Entry::deserialize(Deserializer &in) {
        m_name = in.readString();

        m_message.deserialize(in);
        m_sound       = static_cast<MessageSound>(in.read<u8>());
        m_start_frame = in.read<u16>();
        m_end_frame   = in.read<u16>();
        in.readBytes(m_unk_flags);

        return {};
    }

}  // namespace Toolbox::BMG