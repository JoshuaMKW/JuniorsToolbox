#include "bmg/bmg.hpp"

#include <format>
#include <iostream>
#include <string>
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

    RichMessage RichMessage::fromRawString(std::string_view message) { return RichMessage(); }

    std::vector<char> RichMessage::rawFromCommand(std::string_view command) {
        std::string command_str(command);

        if (s_command_to_raw.contains(command_str)) {
            return std::vector<char>(s_command_to_raw[command_str].begin(),
                                     s_command_to_raw[command_str].end());
        }

        if (command.starts_with("{speed:")) {
            return std::vector<char>{0x1A, 0x06, 0x00, 0x00, 0x00, command.back()};
        }

        if (command.starts_with("{options:")) {
            auto command_split = command.rfind(':');
            auto message       = command.substr(command_split + 1);
            auto option        = std::stoi(command_str.substr(9, command_split - 9));
            auto command_size  = static_cast<char>(5 + message.size());

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

    std::string RichMessage::commandFromRaw(std::span<const char> command) {
        if (command.empty()) {
            return "";
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

}  // namespace Toolbox::BMG