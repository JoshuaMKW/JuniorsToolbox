#include "dolphin/watch.hpp"
#include "gui/application.hpp"
#include "objlib/meta/member.hpp"

using namespace Toolbox::Dolphin;
using namespace Toolbox::Object;

namespace Toolbox {

    template <typename T> static T readSingleFromMem(u32 address) {
        static_assert(std::is_default_constructible_v<T>, "T must be default constructible");

        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("Dolphin is not hooked.");
            return T();
        }

        u32 mem_size = communicator.manager().getMemorySize();

        u32 true_address = address & 0x1FFFFFF;
        if (true_address + sizeof(T) > mem_size) {
            TOOLBOX_ERROR("Watch address out of bounds.");
            return T();
        }

        return communicator.read<T>(address).value_or(T());
    }

    static std::string readStringFromMem(u32 address, u32 length) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("Dolphin is not hooked.");
            return std::string();
        }

        u32 mem_size = communicator.manager().getMemorySize();

        u32 true_address = address & 0x1FFFFFF;
        if (true_address + length > mem_size) {
            TOOLBOX_ERROR("Watch address out of bounds.");
            return std::string();
        }

        std::string value = std::string(length, '\0');
        communicator.readCString(value.data(), length, address)
            .and_then([&]() { return Result<void>{}; })
            .or_else([&](const BaseError &err) {
                TOOLBOX_ERROR("Failed to read string from memory: %s", err.m_message);
                return Result<void>{};
            });

        return value;
    }

    static glm::vec3 readVec3FromMem(u32 address) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("Dolphin is not hooked.");
            return glm::vec3();
        }

        u32 mem_size = communicator.manager().getMemorySize();

        u32 true_address = address & 0x1FFFFFF;
        if (true_address + sizeof(glm::vec3) > mem_size) {
            TOOLBOX_ERROR("Watch address out of bounds.");
            return glm::vec3();
        }

        glm::vec3 value = {};
        value.x         = communicator.read<f32>(address + 0).value_or(0.0f);
        value.y         = communicator.read<f32>(address + 4).value_or(0.0f);
        value.z         = communicator.read<f32>(address + 8).value_or(0.0f);

        return value;
    }

    static Transform readTransformFromMem(u32 address) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("Dolphin is not hooked.");
            return Transform();
        }

        u32 mem_size = communicator.manager().getMemorySize();

        u32 true_address = address & 0x1FFFFFF;
        if (true_address + sizeof(Transform) > mem_size) {
            TOOLBOX_ERROR("Watch address out of bounds.");
            return Transform();
        }

        Transform value       = {};
        value.m_translation.x = communicator.read<f32>(address).value_or(0.0f);
        value.m_translation.y = communicator.read<f32>(address + 0x04).value_or(0.0f);
        value.m_translation.z = communicator.read<f32>(address + 0x08).value_or(0.0f);
        value.m_rotation.x    = communicator.read<f32>(address + 0x14).value_or(0.0f);
        value.m_rotation.y    = communicator.read<f32>(address + 0x18).value_or(0.0f);
        value.m_rotation.z    = communicator.read<f32>(address + 0x1C).value_or(0.0f);
        value.m_scale.x       = communicator.read<f32>(address + 0x20).value_or(0.0f);
        value.m_scale.y       = communicator.read<f32>(address + 0x24).value_or(0.0f);
        value.m_scale.z       = communicator.read<f32>(address + 0x28).value_or(0.0f);

        return value;
    }

    static glm::mat3x4 readMtx34FromMem(u32 address) {
        DolphinCommunicator &communicator = GUIApplication::instance().getDolphinCommunicator();
        if (!communicator.manager().isHooked()) {
            TOOLBOX_ERROR("Dolphin is not hooked.");
            return glm::mat3x4();
        }

        u32 mem_size = communicator.manager().getMemorySize();

        u32 true_address = address & 0x1FFFFFF;
        if (true_address + sizeof(glm::mat3x4) > mem_size) {
            TOOLBOX_ERROR("Watch address out of bounds.");
            return glm::mat3x4();
        }

        glm::mat3x4 value = {};
        value[0][0]       = communicator.read<f32>(address + 0).value_or(0.0f);
        value[0][1]       = communicator.read<f32>(address + 4).value_or(0.0f);
        value[0][2]       = communicator.read<f32>(address + 8).value_or(0.0f);
        value[0][3]       = communicator.read<f32>(address + 12).value_or(0.0f);
        value[1][0]       = communicator.read<f32>(address + 16).value_or(0.0f);
        value[1][1]       = communicator.read<f32>(address + 20).value_or(0.0f);
        value[1][2]       = communicator.read<f32>(address + 24).value_or(0.0f);
        value[1][3]       = communicator.read<f32>(address + 28).value_or(0.0f);
        value[2][0]       = communicator.read<f32>(address + 32).value_or(0.0f);
        value[2][1]       = communicator.read<f32>(address + 36).value_or(0.0f);
        value[2][2]       = communicator.read<f32>(address + 40).value_or(0.0f);
        value[2][3]       = communicator.read<f32>(address + 44).value_or(0.0f);

        return value;
    }

    bool MemoryWatch::startWatch(u32 address, u32 size) {
        if (m_last_value_buf) {
            return false;
        }

        m_pointer_chain    = {address};
        m_watch_address    = address;
        m_watch_is_pointer = false;
        m_watch_size       = std::min<u32>(size, WATCH_MAX_BUFFER_SIZE);

        if (m_watch_address == 0 || m_watch_size == 0) {
            TOOLBOX_ERROR("Invalid watch address or size.");
            return false;
        }

        m_buf_size              = m_watch_size;
        m_last_value_buf        = new u8[m_buf_size];
        m_last_value_needs_init = true;
        return true;
    }

    bool MemoryWatch::startWatch(const std::vector<u32> &pointer_chain, u32 size) {
        if (m_last_value_buf) {
            return false;
        }

        m_pointer_chain    = pointer_chain;
        m_watch_address    = 0;
        m_watch_is_pointer = true;
        m_watch_size       = std::min<u32>(size, WATCH_MAX_BUFFER_SIZE);

        if (m_pointer_chain.empty() || m_watch_size == 0) {
            TOOLBOX_ERROR("Invalid watch pointer chain or size.");
            return false;
        }

        m_buf_size              = m_watch_size;
        m_last_value_buf        = new u8[m_buf_size];
        m_last_value_needs_init = true;
        return true;
    }

    void MemoryWatch::stopWatch() {
        m_watch_address = 0;
        m_watch_size    = 0;

        m_last_value_buf = nullptr;
    }

    void MemoryWatch::processWatch() {
        if (m_watch_size == 0) {
            return;
        }

        DolphinHookManager &manager = DolphinHookManager::instance();
        if (!manager.isHooked()) {
            TOOLBOX_ERROR("Dolphin is not hooked.");
            return;
        }

        void *mem_view = manager.getMemoryView();
        u32 mem_size   = manager.getMemorySize();

        m_watch_address  = traceAddressFromPointerChain();
        u32 true_address = m_watch_address & 0x1FFFFFF;

        if (true_address + m_watch_size > mem_size) {
            TOOLBOX_ERROR("Watch address out of bounds.");
            return;
        }

        void *current_value_buf = (u8 *)mem_view + true_address;
        if (m_last_value_needs_init) {
            notify(m_last_value_buf, current_value_buf, m_watch_size);
            memcpy(m_last_value_buf, current_value_buf, m_watch_size);
            m_last_value_needs_init = false;
            return;
        }

        // Check if the value has changed and signal a notification if it has
        if (memcmp(m_last_value_buf, current_value_buf, m_watch_size) != 0) {
            notify(m_last_value_buf, current_value_buf, m_watch_size);
            if (!isLocked()) {
                memcpy(m_last_value_buf, current_value_buf, m_watch_size);
            } else {
                memcpy(current_value_buf, m_last_value_buf, m_watch_size);
            }
        }
    }

    u32 MemoryWatch::TracePointerChainToAddress(const std::vector<u32> &pointer_chain) {
        if (pointer_chain.empty()) {
            return 0;
        }

        u32 address = pointer_chain[0];
        if (address == 0) {
            return address;
        }

        for (size_t i = 1; i < pointer_chain.size(); ++i) {
            address = readSingleFromMem<u32>(address) + pointer_chain[i];
            if (address == 0) {
                return address;
            }
        }

        return address;
    }

    std::vector<u32>
    MemoryWatch::ResolvePointerChainAsAddress(const std::vector<u32> &pointer_chain) {
        if (pointer_chain.empty()) {
            return {};
        }

        std::vector<u32> result;
        result.resize(pointer_chain.size());

        result[0] = pointer_chain[0];
        if (result[0] == 0) {
            return result;
        }

        for (size_t i = 1; i < pointer_chain.size(); ++i) {
            result[i] = readSingleFromMem<u32>(result[i - 1]) + pointer_chain[i];
            if (result[i] == 0) {
                return result;
            }
        }

        return result;
    }

    void MemoryWatch::notify(void *old_value, void *new_value, u32 value_width) {
        if (m_watch_notify_cb) {
            m_watch_notify_cb(old_value, new_value, value_width);
        }
    }

    u32 MemoryWatch::traceAddressFromPointerChain() const {
        if (m_pointer_chain.empty()) {
            return 0;
        }

        u32 address = m_pointer_chain[0];
        if (!m_watch_is_pointer) {
            return address;
        }

        return TracePointerChainToAddress(m_pointer_chain);
    }

    MetaWatch::MetaWatch(MetaType type) : m_last_value(type) {
        m_meta_type = type;

        if (type != MetaType::STRING) {
            m_precompute_size = meta_type_size(type);
        } else {
            m_precompute_size = 0;
        }

        m_memory_watch.onWatchNotify([this](void *old_value, void *new_value, u32 value_width) {
            MetaValue new_meta_value(m_meta_type);
            u32 watch_address = m_memory_watch.getWatchAddress();

            if (meta_type_size(m_meta_type) > value_width) {
                TOOLBOX_ERROR_V("[META_WATCH] Watch notified with width {} but needs width {}",
                                value_width, meta_type_size(m_meta_type));
                return;
            }

            Buffer value_buf;
            value_buf.setBuf(new_value, value_width);

            switch (m_meta_type) {
            case MetaType::BOOL: {
                new_meta_value.set(*static_cast<bool *>(new_value));
                break;
            }
            case MetaType::S8: {
                new_meta_value.set(*static_cast<s8 *>(new_value));
                break;
            }
            case MetaType::U8: {
                new_meta_value.set(*static_cast<u8 *>(new_value));
                break;
            }
            case MetaType::S16: {
                new_meta_value.set(bs16(*static_cast<s16 *>(new_value)));
                break;
            }
            case MetaType::U16: {
                new_meta_value.set(bu16(*static_cast<u16 *>(new_value)));
                break;
            }
            case MetaType::S32: {
                new_meta_value.set(bs32(*static_cast<s32 *>(new_value)));
                break;
            }
            case MetaType::U32: {
                new_meta_value.set(bu32(*static_cast<u32 *>(new_value)));
                break;
            }
            case MetaType::F32: {
                new_meta_value.set(bf32(*static_cast<f32 *>(new_value)));
                break;
            }
            case MetaType::F64: {
                new_meta_value.set(bf64(*static_cast<f64 *>(new_value)));
                break;
            }
            case MetaType::STRING: {
                const char *raw_str = static_cast<const char *>(new_value);
                size_t len          = strnlen(raw_str, value_width);
                std::string str     = std::string(raw_str, len);
                new_meta_value.set(str);
                break;
            }
            case MetaType::VEC3: {
                glm::vec3 new_vec = {
                    bf32(static_cast<f32 *>(new_value)[0]),
                    bf32(static_cast<f32 *>(new_value)[1]),
                    bf32(static_cast<f32 *>(new_value)[2]),
                };
                new_meta_value.set<glm::vec3>(new_vec);
                break;
            }
            case MetaType::TRANSFORM: {
                Transform new_transform = {
                    {
                     bf32(static_cast<f32 *>(new_value)[0]),
                     bf32(static_cast<f32 *>(new_value)[1]),
                     bf32(static_cast<f32 *>(new_value)[2]),
                     },
                    {
                     bf32(static_cast<f32 *>(new_value)[3]),
                     bf32(static_cast<f32 *>(new_value)[4]),
                     bf32(static_cast<f32 *>(new_value)[5]),
                     },
                    {
                     bf32(static_cast<f32 *>(new_value)[6]),
                     bf32(static_cast<f32 *>(new_value)[7]),
                     bf32(static_cast<f32 *>(new_value)[8]),
                     },
                };
                new_meta_value.set<Transform>(new_transform);
                break;
            }
            case MetaType::MTX34: {
                glm::mat3x4 new_mat = {
                    {
                     bf32(static_cast<f32 *>(new_value)[0]),
                     bf32(static_cast<f32 *>(new_value)[1]),
                     bf32(static_cast<f32 *>(new_value)[2]),
                     bf32(static_cast<f32 *>(new_value)[3]),
                     },
                    {
                     bf32(static_cast<f32 *>(new_value)[4]),
                     bf32(static_cast<f32 *>(new_value)[5]),
                     bf32(static_cast<f32 *>(new_value)[6]),
                     bf32(static_cast<f32 *>(new_value)[7]),
                     },
                    {
                     bf32(static_cast<f32 *>(new_value)[8]),
                     bf32(static_cast<f32 *>(new_value)[9]),
                     bf32(static_cast<f32 *>(new_value)[10]),
                     bf32(static_cast<f32 *>(new_value)[11]),
                     },
                };
                new_meta_value.set<glm::mat3x4>(new_mat);
                break;
            }
            case MetaType::RGB: {
                Color::RGB24 new_color;
                Deserializer::BytesToObject(value_buf, new_color);
                new_meta_value.set<Color::RGB24>(new_color);
                break;
            }
            case MetaType::RGBA: {
                Color::RGBA32 new_color;
                Deserializer::BytesToObject(value_buf, new_color);
                new_meta_value.set<Color::RGBA32>(new_color);
                break;
            }
            case MetaType::UNKNOWN: {
                new_meta_value.set<Buffer>(value_buf);
                break;
            }
            }

            if (m_watch_notify_cb) {
                m_watch_notify_cb(m_last_value, new_meta_value);
            }

            if (!isLocked()) {
                m_last_value = std::move(new_meta_value);
            }
        });
    }

    MetaType MetaWatch::getWatchType() const { return m_meta_type; }

    bool MetaWatch::startWatch(u32 address, u32 size) {
        if (m_precompute_size == 0) {
            m_precompute_size = size;
        }

        return m_memory_watch.startWatch(address, m_precompute_size);
    }

    bool MetaWatch::startWatch(const std::vector<u32> &pointer_chain, u32 size) {
        if (m_precompute_size == 0) {
            m_precompute_size = size;
        }

        return m_memory_watch.startWatch(pointer_chain, m_precompute_size);
    }

    void MetaWatch::stopWatch() { m_memory_watch.stopWatch(); }

}  // namespace Toolbox