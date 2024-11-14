#include "bmg/bmg.hpp"
#include "magic_enum.hpp"

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

    static std::unordered_map<std::string, std::string> s_command_to_raw = {
        {"{text:slow}",           "\x1A\x05\x00\x00\x00"s    },
        {"{text:end_close}",      "\x1A\x05\x00\x00\x01"s    },
        {"{ctx:bananas}",         "\x1A\x06\xFF\x00\x04\x00"s},
        {"{ctx:coconuts}",        "\x1A\x06\xFF\x00\x04\x01"s},
        {"{ctx:pineapples}",      "\x1A\x06\xFF\x00\x04\x02"s},
        {"{ctx:durians}",         "\x1A\x06\xFF\x00\x04\x03"s},
        {"{color:white}",         "\x1A\x06\xFF\x00\x00\x00"s},
        {"{color:red}",           "\x1A\x06\xFF\x00\x00\x02"s},
        {"{color:blue}",          "\x1A\x06\xFF\x00\x00\x03"s},
        {"{color:yellow}",        "\x1A\x06\xFF\x00\x00\x04"s},
        {"{color:green}",         "\x1A\x06\xFF\x00\x00\x05"s},
        {"{record:race_pianta}",  "\x1A\x05\x02\x00\x00"s    },
        {"{record:race_gelato}",  "\x1A\x05\x02\x00\x01"s    },
        {"{record:crate_time}",   "\x1A\x05\x02\x00\x02"s    },
        {"{record:bcoin_shines}", "\x1A\x05\x02\x00\x03"s    },
        {"{record:race_noki}",    "\x1A\x05\x02\x00\x06"s    },
    };

    static std::unordered_map<std::string, std::string> s_raw_to_command = {
        {"\x1A\x05\x00\x00\x00"s,     "{text:slow}"          },
        {"\x1A\x05\x00\x00\x01"s,     "{text:end_close}"     },
        {"\x1A\x06\xFF\x00\x04\x00"s, "{ctx:bananas}"        },
        {"\x1A\x06\xFF\x00\x04\x01"s, "{ctx:coconuts}"       },
        {"\x1A\x06\xFF\x00\x04\x02"s, "{ctx:pineapples}"     },
        {"\x1A\x06\xFF\x00\x04\x03"s, "{ctx:durians}"        },
        {"\x1A\x06\xFF\x00\x00\x00"s, "{color:white}"        },
        {"\x1A\x06\xFF\x00\x00\x02"s, "{color:red}"          },
        {"\x1A\x06\xFF\x00\x00\x03"s, "{color:blue}"         },
        {"\x1A\x06\xFF\x00\x00\x04"s, "{color:yellow}"       },
        {"\x1A\x06\xFF\x00\x00\x05"s, "{color:green}"        },
        {"\x1A\x05\x02\x00\x00"s,     "{record:race_pianta}" },
        {"\x1A\x05\x02\x00\x01"s,     "{record:race_gelato}" },
        {"\x1A\x05\x02\x00\x02"s,     "{record:crate_time}"  },
        {"\x1A\x05\x02\x00\x03"s,     "{record:bcoin_shines}"},
        {"\x1A\x05\x02\x00\x06"s,     "{record:race_noki}"   },
    };

    std::size_t CmdMessage::getDataSize() const {
        std::vector<std::string> parts = getParts();
        size_t size                    = 0;
        for (const auto &part : parts) {
            auto result = rawFromCommand(part);
            if (!result) {
                size += part.size() + 1;
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

    Result<void, SerialError> CmdMessage::serialize(Serializer &out) const {
        std::vector<std::string> parts = getParts();
        for (auto &part : parts) {
            auto result = rawFromCommand(part);
            if (!result) {
                out.writeCString(part);
            } else {
                out.writeBytes(result.value());
            }
        }
        return {};
    }

    Result<void, SerialError> CmdMessage::deserialize(Deserializer &in) {
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

    std::vector<std::string> CmdMessage::getParts() const {
        std::vector<std::string> parts;

        size_t bracket_l      = 0;
        size_t bracket_r      = 0;
        size_t next_bracket_l = 0;

        if (m_message_data.find('{') == std::string::npos) {
            parts.push_back(m_message_data);
            return parts;
        }

        while (bracket_r != std::string::npos) {
            bracket_l = m_message_data.find('{', bracket_r);

            // If no more commands, return the end
            if (bracket_l == std::string::npos) {
                if ((bracket_r + 1) < m_message_data.size()) {
                    parts.push_back(m_message_data.substr(bracket_r + 1));
                }
                break;
            }

            std::string text = m_message_data.substr(bracket_r, bracket_l);

            bracket_r      = m_message_data.find('}', bracket_l);
            next_bracket_l = m_message_data.find('{', bracket_l + 1);

            const bool isCommandEnclosed =
                bracket_r < next_bracket_l && bracket_r != std::string::npos;

            if (!text.empty()) {
                parts.push_back(std::string(text));
            }

            if (!isCommandEnclosed) {
                if (bracket_r == std::string::npos) {
                    std::string trailing_text = m_message_data.substr(bracket_l);  // to the end
                    if (!trailing_text.empty())
                        parts.push_back(std::string(trailing_text));
                    break;
                }
                std::string trailing_text = m_message_data.substr(bracket_l, next_bracket_l);
                if (!trailing_text.empty())
                    parts.push_back(trailing_text);
                bracket_r = next_bracket_l;  // skip the bracket
                continue;
            }

            std::string command = m_message_data.substr(bracket_l, bracket_r + 1);
            if (!command.empty())
                parts.push_back(command);
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
                raw[1] = static_cast<char>(command_size);
                raw[2] = 0x01;
                raw[3] = 0x00;
                raw[4] = option;
            }
            std::copy(message.begin(), message.end(), raw.begin() + 5);

            return raw;
        }

        std::vector<char> raw(command.size() + 2);
        {
            raw[0] = 0x1A;
            raw[1] = static_cast<char>(command.size());
            std::copy(command.begin(), command.end(), raw.end());
        }

        return raw;
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
                std::snprintf(buf, 32, "{speed:%d}", command[5]);
                return buf;
            }

            if (command[2] == 0x01 && command[3] == 0x00) {
                char buf[32];
                std::snprintf(buf, 32, "{option:%d:%s}", command[4],
                              std::string(command.begin() + 5, command.end()).c_str());
                return buf;
            }
        }

        std::string txt = "{raw:";
        for (size_t i = 2; i < command.size(); i++) {
            char buf[6];
            std::snprintf(buf, 6, "\\x%02X", command[i]);
            txt += buf;
        }
        txt += "}";
        return txt;
    }

    const char* ppMessageSound(MessageSound snd) {
        switch (snd) {
        case MessageSound::NOTHING:
            return "NOTHING";
        case MessageSound::PEACH_NORMAL:
            return "PEACH_NORMAL";
        case MessageSound::PEACH_SURPRISE:
            return "PEACH_SURPRISE";
        case MessageSound::PEACH_WORRY:
            return "PEACH_WORRY";
        case MessageSound::PEACH_ANGER_L:
            return "PEACH_ANGER_L";
        case MessageSound::PEACH_APPEAL:
            return "PEACH_APPEAL";
        case MessageSound::PEACH_DOUBT:
            return "PEACH_DOUBT";
        case MessageSound::TOADSWORTH_NORMAL:
            return "TOADSWORTH_NORMAL";
        case MessageSound::TOADSWORTH_EXCITED:
            return "TOADSWORTH_EXCITED";
        case MessageSound::TOADSWORTH_ADVICE:
            return "TOADSWORTH_ADVICE";
        case MessageSound::TOADSWORTH_CONFUSED:
            return "TOADSWORTH_CONFUSED";
        case MessageSound::TOADSWORTH_ASK:
            return "TOADSWORTH_ASK";
        case MessageSound::TOAD_NORMAL:
            return "TOAD_NORMAL";
        case MessageSound::TOAD_CRY_S:
            return "TOAD_CRY_S";
        case MessageSound::TOAD_CRY_L:
            return "TOAD_CRY_L";
        case MessageSound::TOAD_PEACE:
            return "TOAD_PEACE";
        case MessageSound::TOAD_SAD_S:
            return "TOAD_SAD_S";
        case MessageSound::TOAD_ASK:
            return "TOAD_ASK";
        case MessageSound::TOAD_CONFUSED:
            return "TOAD_CONFUSED";
        case MessageSound::TOAD_RECOVER:
            return "TOAD_RECOVER";
        case MessageSound::TOAD_SAD_L:
            return "TOAD_SAD_L";
        case MessageSound::MALE_PIANTA_NORMAL:
            return "MALE_PIANTA_NORMAL";
        case MessageSound::MALE_PIANTA_LAUGH_D:
            return "MALE_PIANTA_LAUGH_D";
        case MessageSound::MALE_PIANTA_DISGUSTED:
            return "MALE_PIANTA_DISGUSTED";
        case MessageSound::MALE_PIANTA_ANGER_S:
            return "MALE_PIANTA_ANGER_S";
        case MessageSound::MALE_PIANTA_SURPRISE:
            return "MALE_PIANTA_SURPRISE";
        case MessageSound::MALE_PIANTA_DOUBT:
            return "MALE_PIANTA_DOUBT";
        case MessageSound::MALE_PIANTA_DISPLEASED:
            return "MALE_PIANTA_DISPLEASED";
        case MessageSound::MALE_PIANTA_CONFUSED:
            return "MALE_PIANTA_CONFUSED";
        case MessageSound::MALE_PIANTA_REGRET:
            return "MALE_PIANTA_REGRET";
        case MessageSound::MALE_PIANTA_PROUD:
            return "MALE_PIANTA_PROUD";
        case MessageSound::MALE_PIANTA_RECOVER:
            return "MALE_PIANTA_RECOVER";
        case MessageSound::MALE_PIANTA_INVITING:
            return "MALE_PIANTA_INVITING";
        case MessageSound::MALE_PIANTA_QUESTION:
            return "MALE_PIANTA_QUESTION";
        case MessageSound::MALE_PIANTA_LAUGH:
            return "MALE_PIANTA_LAUGH";
        case MessageSound::MALE_PIANTA_THANKS:
            return "MALE_PIANTA_THANKS";
        case MessageSound::FEMALE_PIANTA_NORMAL:
            return "FEMALE_PIANTA_NORMAL";
        case MessageSound::FEMALE_PIANTA_REGRET:
            return "FEMALE_PIANTA_REGRET";
        case MessageSound::FEMALE_PIANTA_INVITING:
            return "FEMALE_PIANTA_INVITING";
        case MessageSound::FEMALE_PIANTA_LAUGH:
            return "FEMALE_PIANTA_LAUGH";
        case MessageSound::FEMALE_PIANTA_LAUGH_D:
            return "FEMALE_PIANTA_LAUGH_D";
        case MessageSound::FEMALE_PIANTA_DISGUSTED:
            return "FEMALE_PIANTA_DISGUSTED";
        case MessageSound::FEMALE_PIANTA_ANGER_S:
            return "FEMALE_PIANTA_ANGER_S";
        case MessageSound::FEMALE_PIANTA_SURPRISE:
            return "FEMALE_PIANTA_SURPRISE";
        case MessageSound::FEMALE_PIANTA_DOUBT:
            return "FEMALE_PIANTA_DOUBT";
        case MessageSound::FEMALE_PIANTA_DISPLEASED:
            return "FEMALE_PIANTA_DISPLEASED";
        case MessageSound::FEMALE_PIANTA_CONFUSED:
            return "FEMALE_PIANTA_CONFUSED";
        case MessageSound::FEMALE_PIANTA_PROUD:
            return "FEMALE_PIANTA_PROUD";
        case MessageSound::FEMALE_PIANTA_RECOVER:
            return "FEMALE_PIANTA_RECOVER";
        case MessageSound::FEMALE_PIANTA_QUESTION:
            return "FEMALE_PIANTA_QUESTION";
        case MessageSound::FEMALE_PIANTA_THANKS:
            return "FEMALE_PIANTA_THANKS";
        case MessageSound::CHILD_MALE_PIANTA_NORMAL:
            return "CHILD_MALE_PIANTA_NORMAL";
        case MessageSound::CHILD_MALE_PIANTA_LAUGH_D:
            return "CHILD_MALE_PIANTA_LAUGH_D";
        case MessageSound::CHILD_MALE_PIANTA_DISGUSTED:
            return "CHILD_MALE_PIANTA_DISGUSTED";
        case MessageSound::CHILD_MALE_PIANTA_ANGER_S:
            return "CHILD_MALE_PIANTA_ANGER_S";
        case MessageSound::CHILD_MALE_PIANTA_SURPRISE:
            return "CHILD_MALE_PIANTA_SURPRISE";
        case MessageSound::CHILD_MALE_PIANTA_DOUBT:
            return "CHILD_MALE_PIANTA_DOUBT";
        case MessageSound::CHILD_MALE_PIANTA_DISPLEASED:
            return "CHILD_MALE_PIANTA_DISPLEASED";
        case MessageSound::CHILD_MALE_PIANTA_CONFUSED:
            return "CHILD_MALE_PIANTA_CONFUSED";
        case MessageSound::CHILD_MALE_PIANTA_REGRET:
            return "CHILD_MALE_PIANTA_REGRET";
        case MessageSound::CHILD_MALE_PIANTA_PROUD:
            return "CHILD_MALE_PIANTA_PROUD";
        case MessageSound::CHILD_MALE_PIANTA_RECOVER:
            return "CHILD_MALE_PIANTA_RECOVER";
        case MessageSound::CHILD_MALE_PIANTA_INVITING:
            return "CHILD_MALE_PIANTA_INVITING";
        case MessageSound::CHILD_MALE_PIANTA_QUESTION:
            return "CHILD_MALE_PIANTA_QUESTION";
        case MessageSound::CHILD_MALE_PIANTA_LAUGH:
            return "CHILD_MALE_PIANTA_LAUGH";
        case MessageSound::CHILD_MALE_PIANTA_THANKS:
            return "CHILD_MALE_PIANTA_THANKS";
        case MessageSound::CHILD_FEMALE_PIANTA_NORMAL:
            return "CHILD_FEMALE_PIANTA_NORMAL";
        case MessageSound::CHILD_FEMALE_PIANTA_LAUGH_D:
            return "CHILD_FEMALE_PIANTA_LAUGH_D";
        case MessageSound::CHILD_FEMALE_PIANTA_DISGUSTED:
            return "CHILD_FEMALE_PIANTA_DISGUSTED";
        case MessageSound::CHILD_FEMALE_PIANTA_ANGER_S:
            return "CHILD_FEMALE_PIANTA_ANGER_S";
        case MessageSound::CHILD_FEMALE_PIANTA_SURPRISE:
            return "CHILD_FEMALE_PIANTA_SURPRISE";
        case MessageSound::CHILD_FEMALE_PIANTA_DOUBT:
            return "CHILD_FEMALE_PIANTA_DOUBT";
        case MessageSound::CHILD_FEMALE_PIANTA_DISPLEASED:
            return "CHILD_FEMALE_PIANTA_DISPLEASED";
        case MessageSound::CHILD_FEMALE_PIANTA_CONFUSED:
            return "CHILD_FEMALE_PIANTA_CONFUSED";
        case MessageSound::CHILD_FEMALE_PIANTA_REGRET:
            return "CHILD_FEMALE_PIANTA_REGRET";
        case MessageSound::CHILD_FEMALE_PIANTA_PROUD:
            return "CHILD_FEMALE_PIANTA_PROUD";
        case MessageSound::CHILD_FEMALE_PIANTA_RECOVER:
            return "CHILD_FEMALE_PIANTA_RECOVER";
        case MessageSound::CHILD_FEMALE_PIANTA_INVITING:
            return "CHILD_FEMALE_PIANTA_INVITING";
        case MessageSound::CHILD_FEMALE_PIANTA_QUESTION:
            return "CHILD_FEMALE_PIANTA_QUESTION";
        case MessageSound::CHILD_FEMALE_PIANTA_LAUGH:
            return "CHILD_FEMALE_PIANTA_LAUGH";
        case MessageSound::CHILD_FEMALE_PIANTA_THANKS:
            return "CHILD_FEMALE_PIANTA_THANKS";
        case MessageSound::MALE_NOKI_NORMAL:
            return "MALE_NOKI_NORMAL";
        case MessageSound::MALE_NOKI_REGRET:
            return "MALE_NOKI_REGRET";
        case MessageSound::MALE_NOKI_LAUGH:
            return "MALE_NOKI_LAUGH";
        case MessageSound::MALE_NOKI_APPEAL:
            return "MALE_NOKI_APPEAL";
        case MessageSound::MALE_NOKI_SUPRISE:
            return "MALE_NOKI_SUPRISE";
        case MessageSound::MALE_NOKI_SAD:
            return "MALE_NOKI_SAD";
        case MessageSound::MALE_NOKI_ASK:
            return "MALE_NOKI_ASK";
        case MessageSound::MALE_NOKI_PROMPT:
            return "MALE_NOKI_PROMPT";
        case MessageSound::MALE_NOKI_THANKS:
            return "MALE_NOKI_THANKS";
        case MessageSound::FEMALE_NOKI_NORMAL:
            return "FEMALE_NOKI_NORMAL";
        case MessageSound::FEMALE_NOKI_REGRET:
            return "FEMALE_NOKI_REGRET";
        case MessageSound::FEMALE_NOKI_LAUGH:
            return "FEMALE_NOKI_LAUGH";
        case MessageSound::FEMALE_NOKI_APPEAL:
            return "FEMALE_NOKI_APPEAL";
        case MessageSound::FEMALE_NOKI_SURPRISE:
            return "FEMALE_NOKI_SURPRISE";
        case MessageSound::FEMALE_NOKI_SAD:
            return "FEMALE_NOKI_SAD";
        case MessageSound::FEMALE_NOKI_ASK:
            return "FEMALE_NOKI_ASK";
        case MessageSound::FEMALE_NOKI_PROMPT:
            return "FEMALE_NOKI_PROMPT";
        case MessageSound::FEMALE_NOKI_THANKS:
            return "FEMALE_NOKI_THANKS";
        case MessageSound::ELDER_NOKI_NORMAL:
            return "ELDER_NOKI_NORMAL";
        case MessageSound::ELDER_NOKI_REGRET:
            return "ELDER_NOKI_REGRET";
        case MessageSound::ELDER_NOKI_LAUGH:
            return "ELDER_NOKI_LAUGH";
        case MessageSound::ELDER_NOKI_APPEAL:
            return "ELDER_NOKI_APPEAL";
        case MessageSound::ELDER_NOKI_SURPRISE:
            return "ELDER_NOKI_SURPRISE";
        case MessageSound::ELDER_NOKI_SAD:
            return "ELDER_NOKI_SAD";
        case MessageSound::ELDER_NOKI_ASK:
            return "ELDER_NOKI_ASK";
        case MessageSound::ELDER_NOKI_PROMPT:
            return "ELDER_NOKI_PROMPT";
        case MessageSound::ELDER_NOKI_THANKS:
            return "ELDER_NOKI_THANKS";
        case MessageSound::CHILD_MALE_NOKI_NORMAL:
            return "CHILD_MALE_NOKI_NORMAL";
        case MessageSound::CHILD_MALE_NOKI_REGRET:
            return "CHILD_MALE_NOKI_REGRET";
        case MessageSound::CHILD_MALE_NOKI_LAUGH:
            return "CHILD_MALE_NOKI_LAUGH";
        case MessageSound::CHILD_MALE_NOKI_APPEAL:
            return "CHILD_MALE_NOKI_APPEAL";
        case MessageSound::CHILD_MALE_NOKI_SURPRISE:
            return "CHILD_MALE_NOKI_SURPRISE";
        case MessageSound::CHILD_MALE_NOKI_SAD:
            return "CHILD_MALE_NOKI_SAD";
        case MessageSound::CHILD_MALE_NOKI_ASK:
            return "CHILD_MALE_NOKI_ASK";
        case MessageSound::CHILD_MALE_NOKI_PROMPT:
            return "CHILD_MALE_NOKI_PROMPT";
        case MessageSound::CHILD_MALE_NOKI_THANKS:
            return "CHILD_MALE_NOKI_THANKS";
        case MessageSound::CHILD_FEMALE_NOKI_NORMAL:
            return "CHILD_FEMALE_NOKI_NORMAL";
        case MessageSound::CHILD_FEMALE_NOKI_REGRET:
            return "CHILD_FEMALE_NOKI_REGRET";
        case MessageSound::CHILD_FEMALE_NOKI_LAUGH:
            return "CHILD_FEMALE_NOKI_LAUGH";
        case MessageSound::CHILD_FEMALE_NOKI_APPEAL:
            return "CHILD_FEMALE_NOKI_APPEAL";
        case MessageSound::CHILD_FEMALE_NOKI_SURPRISE:
            return "CHILD_FEMALE_NOKI_SURPRISE";
        case MessageSound::CHILD_FEMALE_NOKI_SAD:
            return "CHILD_FEMALE_NOKI_SAD";
        case MessageSound::CHILD_FEMALE_NOKI_ASK:
            return "CHILD_FEMALE_NOKI_ASK";
        case MessageSound::CHILD_FEMALE_NOKI_PROMPT:
            return "CHILD_FEMALE_NOKI_PROMPT";
        case MessageSound::CHILD_FEMALE_NOKI_THANKS:
            return "CHILD_FEMALE_NOKI_THANKS";
        case MessageSound::SUNFLOWER_PARENT_JOY:
            return "SUNFLOWER_PARENT_JOY";
        case MessageSound::SUNFLOWER_PARENT_SAD:
            return "SUNFLOWER_PARENT_SAD";
        case MessageSound::TANUKI_NORMAL:
            return "TANUKI_NORMAL";
        case MessageSound::SHADOW_MARIO_PSHOT:
            return "SHADOW_MARIO_PSHOT";
        case MessageSound::IL_PIANTISSIMO_NORMAL:
            return "IL_PIANTISSIMO_NORMAL";
        case MessageSound::IL_PIANTISSIMO_LOST:
            return "IL_PIANTISSIMO_LOST";
        case MessageSound::ITEM_COLLECT_DELIGHT:
            return "ITEM_COLLECT_DELIGHT";
        case MessageSound::ITEM_NOT_COLLECT:
            return "ITEM_NOT_COLLECT";
        case MessageSound::BGM_FANFARE:
            return "BGM_FANFARE";
        default:
            return "UNKNOWN";
        }
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

    size_t MessageData::getDataSize() const {
        return 32 + getINF1Size() + getSTR1Size() + getDAT1Size();
    }

    size_t MessageData::getINF1Size() const {
        return (0x10 + (m_entries.size() * m_flag_size) + 0x1F) & ~0x1F;
    }

    size_t MessageData::getDAT1Size() const {
        size_t size = std::accumulate(
            m_entries.begin(), m_entries.end(), size_t(9),
            [](size_t sum, const Entry &entry) { return sum + entry.m_message.getDataSize(); });
        return (size + 0x1F) & ~0x1F;
    }

    size_t MessageData::getSTR1Size() const {
        if (!m_has_str1)
            return 0;

        size_t size = std::accumulate(
            m_entries.begin(), m_entries.end(), size_t(9),
            [](size_t sum, const Entry &entry) { return sum + entry.m_name.size() + 1; });
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
            std::string name = entry.m_name.empty() ? "(Unknown)" : entry.m_name;
            out << value_indention_str << name << " (" << entry.m_start_frame << ", "
                << entry.m_end_frame << ", " << magic_enum::enum_name(entry.m_sound) << ") {"
                << std::endl;
            entry.m_message.dump(out, indention + 2, indention_width);
            out << value_indention_str << "}" << std::endl;
        }
        out << indention_str << "}" << std::endl;
    }

    Result<void, SerialError> MessageData::serialize(Serializer &out) const {
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
        out.write<u16, std::endian::big>(m_flag_size);
        out.write<u32, std::endian::big>(0x80000);  // Unknown

        size_t data_offset = 1;  // Padding
        size_t name_offset = 1;  // Padding

        for (auto &entry : m_entries) {
            out.write<u32, std::endian::big>(static_cast<u32>(data_offset));
            if (m_flag_size == 12) {
                out.write<u16, std::endian::big>(entry.m_start_frame);
                out.write<u16, std::endian::big>(entry.m_end_frame);
                if (m_has_str1) {
                    out.write<u16, std::endian::big>(static_cast<u32>(name_offset));
                    out.write<u8>(static_cast<u8>(entry.m_sound));
                    out.seek(1);
                } else {
                    out.write<u8>(static_cast<u8>(entry.m_sound));
                    out.seek(3);
                }
            } else if (m_flag_size == 8) {
                out.writeBytes(entry.m_unk_flags);
                out.seek(std::max(size_t(0), 4 - entry.m_unk_flags.size()));
            } else if (m_flag_size == 4) {
                // Nothing to do
            } else {
                return make_serial_error<void>(out, "Invalid flag size");
            }
            data_offset += entry.m_message.getDataSize();
            name_offset += entry.m_name.size() + 1;
        }

        // Align stream
        out.padTo(32);

        // DAT1
        out.write<u32, std::endian::big>('DAT1');
        out.write<u32, std::endian::big>(static_cast<u32>(getDAT1Size()));
        out.write<char>(0);  // Padding

        for (auto &entry : m_entries) {
            auto result = entry.m_message.serialize(out);
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
                out.writeCString(entry.m_name);
            }
        }

        // Align stream
        out.padTo(32);

        return {};
    }

    Result<void, SerialError> MessageData::deserialize(Deserializer &in) {
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
        m_flag_size          = 0;

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
                m_flag_size   = in.read<u16, std::endian::big>();
                in.seek(4);  // Unknown values

                for (size_t j = 0; j < message_count; ++j) {
                    Entry entry;
                    data_offsets.push_back(in.read<u32, std::endian::big>());
                    if (m_flag_size == 12) {
                        entry.m_start_frame = in.read<u16, std::endian::big>();
                        entry.m_end_frame   = in.read<u16, std::endian::big>();
                        if (m_has_str1) {
                            name_offsets.push_back(in.read<u16, std::endian::big>());
                        }
                        entry.m_sound = magic_enum::enum_cast<MessageSound>(in.read<u8>())
                                            .value_or(MessageSound::NOTHING);
                        in.seek(m_has_str1 ? 1 : 3);
                    } else if (m_flag_size == 8) {
                        entry.m_unk_flags.resize(4);
                        in.readBytes(entry.m_unk_flags);
                    } else if (m_flag_size == 4) {
                        // Nothing to do
                    } else {
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
                    auto result = m_entries[i].m_message.deserialize(in);
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
                    m_entries[i].m_name = in.readCString(
                        section_size - (static_cast<size_t>(in.tell()) - section_start));
                }
            }
            in.alignTo(32);
        }

        return {};
    }

}  // namespace Toolbox::BMG
