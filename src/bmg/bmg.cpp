#include "bmg/bmg.hpp"

#include <algorithm>
#include <format>
#include <iostream>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace Toolbox::BMG {

    static std::unordered_map<std::string, std::string> s_command_to_raw = {
        {"{text:slow}",           "\x1A\x05\x00\x00\x00"    },
        {"{text:end_close}",      "\x1A\x05\x00\x00\x01"    },
        {"{ctx:bananas}",         "\x1A\x06\xFF\x00\x04\x00"},
        {"{ctx:coconuts}",        "\x1A\x06\xFF\x00\x04\x01"},
        {"{ctx:pineapples}",      "\x1A\x06\xFF\x00\x04\x02"},
        {"{ctx:durians}",         "\x1A\x06\xFF\x00\x04\x03"},
        {"{color:white}",         "\x1A\x06\xFF\x00\x00\x00"},
        {"{color:red}",           "\x1A\x06\xFF\x00\x00\x02"},
        {"{color:blue}",          "\x1A\x06\xFF\x00\x00\x03"},
        {"{color:yellow}",        "\x1A\x06\xFF\x00\x00\x04"},
        {"{color:green}",         "\x1A\x06\xFF\x00\x00\x05"},
        {"{record:race_pianta}",  "\x1A\x05\x02\x00\x00"    },
        {"{record:race_gelato}",  "\x1A\x05\x02\x00\x01"    },
        {"{record:crate_time}",   "\x1A\x05\x02\x00\x02"    },
        {"{record:bcoin_shines}", "\x1A\x05\x02\x00\x03"    },
        {"{record:race_noki}",    "\x1A\x05\x02\x00\x06"    },
    };

    static std::unordered_map<std::string, std::string> s_raw_to_command = {
        {"\x1A\x05\x00\x00\x00",     "{text:slow}"          },
        {"\x1A\x05\x00\x00\x01",     "{text:end_close}"     },
        {"\x1A\x06\xFF\x00\x04\x00", "{ctx:bananas}"        },
        {"\x1A\x06\xFF\x00\x04\x01", "{ctx:coconuts}"       },
        {"\x1A\x06\xFF\x00\x04\x02", "{ctx:pineapples}"     },
        {"\x1A\x06\xFF\x00\x04\x03", "{ctx:durians}"        },
        {"\x1A\x06\xFF\x00\x00\x00", "{color:white}"        },
        {"\x1A\x06\xFF\x00\x00\x02", "{color:red}"          },
        {"\x1A\x06\xFF\x00\x00\x03", "{color:blue}"         },
        {"\x1A\x06\xFF\x00\x00\x04", "{color:yellow}"       },
        {"\x1A\x06\xFF\x00\x00\x05", "{color:green}"        },
        {"\x1A\x05\x02\x00\x00",     "{record:race_pianta}" },
        {"\x1A\x05\x02\x00\x01",     "{record:race_gelato}" },
        {"\x1A\x05\x02\x00\x02",     "{record:crate_time}"  },
        {"\x1A\x05\x02\x00\x03",     "{record:bcoin_shines}"},
        {"\x1A\x05\x02\x00\x06",     "{record:race_noki}"   },
    };

    std::size_t CmdMessage::getDataSize() const {
        std::vector<std::string> parts = getParts();
        size_t size                    = 0;
        for (const auto &part : parts) {
            auto result = rawFromCommand(part);
            if (!result) {
                size += part.size();
            } else {
                size += result.value().size();
            }
        }
        return size;
    }

    std::string CmdMessage::getString() const { return m_message_data; }

    std::string CmdMessage::getSimpleString() const {
        std::vector<std::string> parts = getParts();
        std::string txt;
        for (const auto &part : parts) {
            auto result = rawFromCommand(part);
            if (result)
                continue;
            txt += part;
        }
        return txt;
    }

    std::expected<void, SerialError> CmdMessage::serialize(Serializer &out) const {
        std::vector<std::string> parts = getParts();
        for (auto &part : parts) {
            auto result = rawFromCommand(part);
            if (!result) {
                out.writeString(part);
            } else {
                out.writeBytes(result.value());
            }
        }
        out.write<char>('\0');
        return {};
    }

    std::expected<void, SerialError> CmdMessage::deserialize(Deserializer &in) {
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
                cmddata.reserve(cmdsize);
                cmddata.push_back(c);
                cmddata.push_back(cmdsize);

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

    std::vector<std::string> CmdMessage::getParts() const {
        std::vector<std::string> parts;

        size_t bracket_l      = 0;
        size_t bracket_r      = 0;
        size_t next_bracket_l = 0;

        while (bracket_r != std::string::npos) {
            bracket_l = m_message_data.find('{', bracket_r);

            std::string_view text = m_message_data.substr(bracket_r, bracket_l);

            bracket_r      = m_message_data.find('}', bracket_l);
            next_bracket_l = m_message_data.find('{', bracket_l);

            const bool isCommandEnclosed =
                bracket_r < next_bracket_l && bracket_r != std::string::npos;

            if (!text.empty()) {
                parts.push_back(std::string(text));
            }

            if (!isCommandEnclosed) {
                if (bracket_r == std::string::npos) {
                    std::string_view trailing_text =
                        m_message_data.substr(bracket_l);  // to the end
                    if (!trailing_text.empty())
                        parts.push_back(std::string(trailing_text));
                    break;
                }
                std::string_view trailing_text = m_message_data.substr(bracket_l, next_bracket_l);
                if (!trailing_text.empty())
                    parts.push_back(std::string(trailing_text));
                bracket_r = next_bracket_l;  // skip the bracket
                continue;
            }

            std::string_view command = m_message_data.substr(bracket_l, bracket_r);
            if (!command.empty())
                parts.push_back(std::string(command));
        }

        return parts;
    }

    std::optional<std::vector<char>> CmdMessage::rawFromCommand(std::string_view command) {
        if (command.empty() || !command.starts_with('{') || !command.ends_with('}')) {
            return {};
        }

        std::string command_str(command);

        if (s_command_to_raw.contains(command_str)) {
            return std::vector<char>(s_command_to_raw[command_str].begin(),
                                     s_command_to_raw[command_str].end());
        }

        if (command.starts_with("{speed:")) {
            return std::vector<char>{0x1A, 0x06, 0x00, 0x00, 0x00, command.back()};
        }

        if (command.starts_with("{options:")) {
            size_t command_split = command.rfind(':');
            auto message         = command.substr(command_split + 1);

            char option         = std::stoi(command_str.substr(9, command_split - 9));
            size_t command_size = static_cast<char>(5 + message.size());

            std::vector<char> raw(command_size);
            {
                raw[0] = 0x1A;
                raw[1] = command_size;
                raw[2] = 0x01;
                raw[3] = 0x00;
                raw[4] = option;
            }
            std::copy(message.begin(), message.end(), raw.begin() + 5);

            return raw;
        }

        std::vector<char> raw(command.size() + 1);
        {
            raw[0] = 0x1A;
            std::copy(command.begin(), command.end(), raw.end());
        }

        return raw;
    }

    std::optional<std::string> CmdMessage::commandFromRaw(std::span<const char> command) {
        if (command.empty() || command[0] != 0x1A) {
            return {};
        }

        if (s_raw_to_command.contains(std::string(command.begin(), command.end()))) {
            return s_raw_to_command[std::string(command.begin(), command.end())];
        }

        if (command[0] == 0x1A && command[1] == 0x06 && command[2] == 0x00 && command[3] == 0x00 &&
            command[4] == 0x00) {
            char buf[32];
            std::snprintf(buf, 32, "{speed:%d}", command[5]);
            return buf;
        }

        if (command[2] == 0x01 && command[3] == 0x00) {
            char buf[32];
            std::snprintf(buf, 32, "{option:%d:%s}", command[4],
                          std::string(command.begin() + 5, command.end()));
            return buf;
        }

        std::string txt = "{raw:";
        for (size_t i = 1; i < txt.size(); i++) {
            char buf[6];
            std::snprintf(buf, 6, "\\x%02X", txt[i]);
            txt += buf;
        }
        txt += "}";
        return txt;
    }

    const std::optional<MessageData::Entry> MessageData::getEntry(std::string_view name) {
        for (auto &entry : m_entries) {
            if (entry.m_name == name)
                return entry;
        }
        return {};
    }

    const std::optional<MessageData::Entry> MessageData::getEntry(size_t index) {
        if (index >= m_entries.size())
            return {};
        return m_entries[index];
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
        auto section_count = in.read<u32, std::endian::big>();
        bool is_str1_present = section_count >= 3;

        in.popBreakpoint();

        return is_str1_present;
    }

}  // namespace Toolbox::BMG