#pragma once

#include "serial.hpp"
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Toolbox::BMG {

    class CmdMessage : public ISerializable {
    public:
        CmdMessage() = default;
        CmdMessage(const std::string &message) : m_message_data(message) {}
        CmdMessage(const CmdMessage &) = default;
        CmdMessage(CmdMessage &&)      = default;
        ~CmdMessage()                  = default;

        [[nodiscard]] std::size_t getDataSize() const;
        [[nodiscard]] std::string getString() const;
        [[nodiscard]] std::string getSimpleString() const;

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

    protected:
        std::vector<std::string> getParts() const;
        static std::optional<std::vector<char>> rawFromCommand(std::string_view command);
        static std::optional<std::string> commandFromRaw(std::span<const char> command);

    private:
        std::string m_message_data;
    };

    enum class MessageSound : u8 {
        NOTHING                        = 69,
        PEACH_NORMAL                   = 0,
        PEACH_SURPRISE                 = 1,
        PEACH_WORRY                    = 2,
        PEACH_ANGER_L                  = 3,
        PEACH_APPEAL                   = 4,
        PEACH_DOUBT                    = 5,
        TOADSWORTH_NORMAL              = 6,
        TOADSWORTH_EXCITED             = 7,
        TOADSWORTH_ADVICE              = 8,
        TOADSWORTH_CONFUSED            = 9,
        TOADSWORTH_ASK                 = 10,
        TOAD_NORMAL                    = 11,
        TOAD_CRY_S                     = 12,
        TOAD_CRY_L                     = 13,
        TOAD_PEACE                     = 14,
        TOAD_SAD_S                     = 15,
        TOAD_ASK                       = 16,
        TOAD_CONFUSED                  = 17,
        TOAD_RECOVER                   = 18,
        TOAD_SAD_L                     = 19,
        MALE_PIANTA_NORMAL             = 20,
        MALE_PIANTA_LAUGH_D            = 21,
        MALE_PIANTA_DISGUSTED          = 22,
        MALE_PIANTA_ANGER_S            = 23,
        MALE_PIANTA_SURPRISE           = 24,
        MALE_PIANTA_DOUBT              = 25,
        MALE_PIANTA_DISPLEASED         = 26,
        MALE_PIANTA_CONFUSED           = 27,
        MALE_PIANTA_REGRET             = 28,
        MALE_PIANTA_PROUD              = 29,
        MALE_PIANTA_RECOVER            = 30,
        MALE_PIANTA_INVITING           = 31,
        MALE_PIANTA_QUESTION           = 32,
        MALE_PIANTA_LAUGH              = 33,
        MALE_PIANTA_THANKS             = 34,
        FEMALE_PIANTA_NORMAL           = 35,
        FEMALE_PIANTA_REGRET           = 36,
        FEMALE_PIANTA_INVITING         = 37,
        FEMALE_PIANTA_LAUGH            = 38,
        FEMALE_PIANTA_LAUGH_D          = 70,
        FEMALE_PIANTA_DISGUSTED        = 71,
        FEMALE_PIANTA_ANGER_S          = 72,
        FEMALE_PIANTA_SURPRISE         = 73,
        FEMALE_PIANTA_DOUBT            = 74,
        FEMALE_PIANTA_DISPLEASED       = 75,
        FEMALE_PIANTA_CONFUSED         = 76,
        FEMALE_PIANTA_PROUD            = 77,
        FEMALE_PIANTA_RECOVER          = 78,
        FEMALE_PIANTA_QUESTION         = 79,
        FEMALE_PIANTA_THANKS           = 80,
        CHILD_MALE_PIANTA_NORMAL       = 81,
        CHILD_MALE_PIANTA_LAUGH_D      = 82,
        CHILD_MALE_PIANTA_DISGUSTED    = 83,
        CHILD_MALE_PIANTA_ANGER_S      = 84,
        CHILD_MALE_PIANTA_SURPRISE     = 85,
        CHILD_MALE_PIANTA_DOUBT        = 86,
        CHILD_MALE_PIANTA_DISPLEASED   = 87,
        CHILD_MALE_PIANTA_CONFUSED     = 88,
        CHILD_MALE_PIANTA_REGRET       = 89,
        CHILD_MALE_PIANTA_PROUD        = 90,
        CHILD_MALE_PIANTA_RECOVER      = 91,
        CHILD_MALE_PIANTA_INVITING     = 92,
        CHILD_MALE_PIANTA_QUESTION     = 93,
        CHILD_MALE_PIANTA_LAUGH        = 94,
        CHILD_MALE_PIANTA_THANKS       = 95,
        CHILD_FEMALE_PIANTA_NORMAL     = 96,
        CHILD_FEMALE_PIANTA_LAUGH_D    = 97,
        CHILD_FEMALE_PIANTA_DISGUSTED  = 98,
        CHILD_FEMALE_PIANTA_ANGER_S    = 99,
        CHILD_FEMALE_PIANTA_SURPRISE   = 100,
        CHILD_FEMALE_PIANTA_DOUBT      = 101,
        CHILD_FEMALE_PIANTA_DISPLEASED = 102,
        CHILD_FEMALE_PIANTA_CONFUSED   = 103,
        CHILD_FEMALE_PIANTA_REGRET     = 104,
        CHILD_FEMALE_PIANTA_PROUD      = 105,
        CHILD_FEMALE_PIANTA_RECOVER    = 106,
        CHILD_FEMALE_PIANTA_INVITING   = 107,
        CHILD_FEMALE_PIANTA_QUESTION   = 108,
        CHILD_FEMALE_PIANTA_LAUGH      = 109,
        CHILD_FEMALE_PIANTA_THANKS     = 110,
        MALE_NOKI_NORMAL               = 39,
        MALE_NOKI_REGRET               = 40,
        MALE_NOKI_LAUGH                = 41,
        MALE_NOKI_APPEAL               = 42,
        MALE_NOKI_SUPRISE              = 43,
        MALE_NOKI_SAD                  = 44,
        MALE_NOKI_ASK                  = 45,
        MALE_NOKI_PROMPT               = 46,
        MALE_NOKI_THANKS               = 47,
        FEMALE_NOKI_NORMAL             = 48,
        FEMALE_NOKI_REGRET             = 49,
        FEMALE_NOKI_LAUGH              = 50,
        FEMALE_NOKI_APPEAL             = 51,
        FEMALE_NOKI_SURPRISE           = 52,
        FEMALE_NOKI_SAD                = 53,
        FEMALE_NOKI_ASK                = 54,
        FEMALE_NOKI_PROMPT             = 55,
        FEMALE_NOKI_THANKS             = 56,
        ELDER_NOKI_NORMAL              = 57,
        ELDER_NOKI_REGRET              = 58,
        ELDER_NOKI_LAUGH               = 59,
        ELDER_NOKI_APPEAL              = 60,
        ELDER_NOKI_SURPRISE            = 61,
        ELDER_NOKI_SAD                 = 62,
        ELDER_NOKI_ASK                 = 63,
        ELDER_NOKI_PROMPT              = 64,
        ELDER_NOKI_THANKS              = 65,
        CHILD_MALE_NOKI_NORMAL         = 111,
        CHILD_MALE_NOKI_REGRET         = 112,
        CHILD_MALE_NOKI_LAUGH          = 113,
        CHILD_MALE_NOKI_APPEAL         = 114,
        CHILD_MALE_NOKI_SURPRISE       = 115,
        CHILD_MALE_NOKI_SAD            = 116,
        CHILD_MALE_NOKI_ASK            = 117,
        CHILD_MALE_NOKI_PROMPT         = 118,
        CHILD_MALE_NOKI_THANKS         = 119,
        CHILD_FEMALE_NOKI_NORMAL       = 120,
        CHILD_FEMALE_NOKI_REGRET       = 121,
        CHILD_FEMALE_NOKI_LAUGH        = 122,
        CHILD_FEMALE_NOKI_APPEAL       = 123,
        CHILD_FEMALE_NOKI_SURPRISE     = 124,
        CHILD_FEMALE_NOKI_SAD          = 125,
        CHILD_FEMALE_NOKI_ASK          = 126,
        CHILD_FEMALE_NOKI_PROMPT       = 127,
        CHILD_FEMALE_NOKI_THANKS       = 128,
        SUNFLOWER_PARENT_JOY           = 67,
        SUNFLOWER_PARENT_SAD           = 68,
        TANUKI_NORMAL                  = 66,
        SHADOW_MARIO_PSHOT             = 132,
        IL_PIANTISSIMO_NORMAL          = 133,
        IL_PIANTISSIMO_LOST            = 134,
        ITEM_COLLECT_DELIGHT           = 129,
        ITEM_NOT_COLLECT               = 131,
        BGM_FANFARE                    = 130,
    };

    class MessageData : public ISerializable {
    public:
        struct Entry {
            std::string m_name = "";
            CmdMessage m_message = CmdMessage();
            MessageSound m_sound = MessageSound::NOTHING;
            u16 m_start_frame = 0;
            u16 m_end_frame = 0;
            std::vector<char> m_unk_flags = {};
        };

        MessageData() = default;
        MessageData(std::vector<Entry> entries) : m_entries(std::move(entries)) {}
        MessageData(std::vector<Entry> entries, u8 flag_size)
            : m_entries(std::move(entries)), m_flag_size(flag_size) {}
        MessageData(std::vector<Entry> entries, bool has_str1)
            : m_entries(std::move(entries)), m_has_str1(has_str1) {}
        MessageData(std::vector<Entry> entries, u8 flag_size, bool has_str1)
            : m_entries(std::move(entries)), m_flag_size(m_flag_size), m_has_str1(has_str1) {}
        MessageData(const MessageData &) = default;
        MessageData(MessageData &&)      = default;
        ~MessageData()                   = default;

        const std::vector<Entry> &entries() const { return m_entries; }

        const std::optional<Entry> getEntry(std::string_view name);
        const std::optional<Entry> getEntry(size_t index);

        size_t getDataSize() const;
        size_t getINF1Size() const;
        size_t getDAT1Size() const;
        size_t getSTR1Size() const;

        void addEntry(const Entry &entry);
        bool insertEntry(size_t index, const Entry &entry);
        void removeEntry(const Entry &entry);

        MessageData &operator=(const MessageData &) = default;

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

    protected:
        static bool isMagicValid(Deserializer &in);
        static bool isSTR1InData(Deserializer &in);

    private:
        std::vector<Entry> m_entries = {};

        u8 m_flag_size = 12;

        // Message names, PAL only
        bool m_has_str1 = false;
    };

}  // namespace Toolbox::BMG