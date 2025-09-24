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
        communicator.readBytes(value.data(), address, length)
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

        Transform value     = {};
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

        m_watch_address = address;
        m_watch_size    = size;

        if (m_watch_address == 0 || m_watch_size == 0) {
            TOOLBOX_ERROR("Invalid watch address or size.");
            return false;
        }

        m_last_value_buf        = new u8[m_watch_size];
        m_last_value_needs_init = true;
        return true;
    }

    void MemoryWatch::stopWatch() {
        m_watch_address = 0;
        m_watch_size    = 0;

        m_last_value_buf = nullptr;
    }

    void MemoryWatch::processWatch() {
        if (m_watch_address == 0 || m_watch_size == 0) {
            return;
        }

        DolphinHookManager &manager = DolphinHookManager::instance();
        if (!manager.isHooked()) {
            TOOLBOX_ERROR("Dolphin is not hooked.");
            return;
        }

        void *mem_view = manager.getMemoryView();
        u32 mem_size   = manager.getMemorySize();

        u32 true_address = m_watch_address & 0x1FFFFFF;

        if (true_address + m_watch_size > mem_size) {
            TOOLBOX_ERROR("Watch address out of bounds.");
            return;
        }

        void *current_value_buf = (u8 *)mem_view + true_address;
        if (m_last_value_needs_init) {
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

    void MemoryWatch::notify(void *old_value, void *new_value, u32 value_width) {
        if (m_watch_notify_cb) {
            m_watch_notify_cb(old_value, new_value, value_width);
        }
    }

    MetaWatch::MetaWatch(MetaType type) : m_last_value(type) {
        m_meta_type  = type;

        if (type != MetaType::STRING) {
            m_precompute_size = meta_type_size(type);
        } else {
            m_precompute_size = 128;
        }

        m_memory_watch.onWatchNotify([this](void *old_value, void *new_value, u32 value_width) {
            // TODO: Write each method of reading in from DolphinCommunicator
            MetaValue new_meta_value(m_meta_type);
            u32 watch_address = m_memory_watch.getWatchAddress();

            switch (m_meta_type) {
            case MetaType::BOOL: {
                new_meta_value.set(readSingleFromMem<bool>(watch_address));
                break;
            }
            case MetaType::S8: {
                new_meta_value.set(readSingleFromMem<s8>(watch_address));
                break;
            }
            case MetaType::U8: {
                new_meta_value.set(readSingleFromMem<u8>(watch_address));
                break;
            }
            case MetaType::S16: {
                new_meta_value.set(readSingleFromMem<s16>(watch_address));
                break;
            }
            case MetaType::U16: {
                new_meta_value.set(readSingleFromMem<u16>(watch_address));
                break;
            }
            case MetaType::S32: {
                new_meta_value.set(readSingleFromMem<s32>(watch_address));
                break;
            }
            case MetaType::U32: {
                new_meta_value.set(readSingleFromMem<u32>(watch_address));
                break;
            }
            case MetaType::F32: {
                new_meta_value.set(readSingleFromMem<float>(watch_address));
                break;
            }
            case MetaType::F64: {
                new_meta_value.set(readSingleFromMem<double>(watch_address));
                break;
            }
            case MetaType::STRING: {
                if (m_precompute_size == 0) {
                    DolphinHookManager &manager = DolphinHookManager::instance();
                    if (!manager.isHooked()) {
                        break;
                    }
                    u32 rel_addr = watch_address & 0x1FFFFFF;
                    u8 *mem_view = ((u8 *)manager.getMemoryView()) + rel_addr;
                    for (int32_t i = 0; i < std::min<int32_t>(256, 0x1800000 - rel_addr); ++i) {
                        if (mem_view[i] == '\0') {
                            m_precompute_size = i;
                            break;
                        }
                    }
                }
                new_meta_value.set(readStringFromMem(watch_address, m_precompute_size));
                break;
            }
            case MetaType::VEC3: {
                new_meta_value.set<glm::vec3>(readVec3FromMem(watch_address));
                break;
            }
            case MetaType::TRANSFORM: {
                new_meta_value.set<Transform>(readTransformFromMem(watch_address));
                break;
            }
            case MetaType::MTX34: {
                new_meta_value.set<glm::mat3x4>(readMtx34FromMem(watch_address));
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

    u32 MetaWatch::getWatchAddress() const { return m_memory_watch.getWatchAddress(); }

    u32 MetaWatch::getWatchSize() const { return m_memory_watch.getWatchSize(); }

    bool MetaWatch::startWatch(u32 address) {
        return m_memory_watch.startWatch(address, m_precompute_size);
    }

    void MetaWatch::stopWatch() { m_memory_watch.stopWatch(); }

}  // namespace Toolbox