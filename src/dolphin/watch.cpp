#include "dolphin/watch.hpp"
#include "objlib/meta/member.hpp"

using namespace Toolbox::Dolphin;
using namespace Toolbox::Object;

namespace Toolbox {

    bool MemoryWatch::startWatch(u32 address, u32 size) {
        m_watch_address = address;
        m_watch_size    = size;

        if (m_watch_address == 0 || m_watch_size == 0) {
            TOOLBOX_ERROR("Invalid watch address or size.");
            return false;
        }

        m_last_value_buf        = new u8[m_watch_size];
        m_last_value_needs_init = true;
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
            memcpy(m_last_value_buf, current_value_buf, m_watch_size);
        }
    }

    void MemoryWatch::notify(void *old_value, void *new_value, u32 value_width) {
        if (m_watch_notify_cb) {
            m_watch_notify_cb(old_value, new_value, value_width);
        }
    }

    MetaWatch::MetaWatch(const Object::MetaStruct &meta_struct) {
        m_meta_struct = make_deep_clone<MetaStruct>(meta_struct);

        m_precompute_size = m_meta_struct->computeSize();
        if (m_precompute_size == 0) {
            TOOLBOX_ERROR("MetaStruct has no size, cannot create MetaWatch.");
            return;
        }

        m_memory_watch.onWatchNotify([this](void *old_value, void *new_value, u32 value_width) {
            std::stringstream str_in(std::string((char *)new_value, value_width));
            Deserializer in(str_in.rdbuf());

            m_last_struct.swap(m_meta_struct);

            bool result = true;
            m_meta_struct->deserialize(in).or_else([&result](const BaseError &err) {
                result = false;
                TOOLBOX_ERROR("Failed to deserialize MetaStruct: {}", err.what());
                return Result<void, SerialError>();
            });

            if (m_watch_notify_cb) {
                m_watch_notify_cb(*m_last_struct, *m_meta_struct);
            }
        });
    }

    MetaWatch::MetaWatch(Object::MetaStruct &&meta_struct) {
        m_precompute_size = m_meta_struct->computeSize();
        if (m_precompute_size == 0) {
            TOOLBOX_ERROR("MetaStruct has no size, cannot create MetaWatch.");
            return;
        }

        m_memory_watch.onWatchNotify([this](void *old_value, void *new_value, u32 value_width) {
            std::stringstream str_in(std::string((char *)new_value, value_width));
            Deserializer in(str_in.rdbuf());

            m_last_struct.swap(m_meta_struct);

            bool result = true;
            m_meta_struct->deserialize(in).or_else([&result](const BaseError &err) {
                result = false;
                TOOLBOX_ERROR("Failed to deserialize MetaStruct: {}", err.what());
                return Result<void, SerialError>();
            });

            if (m_watch_notify_cb) {
                m_watch_notify_cb(*m_last_struct, *m_meta_struct);
            }
        });
    }

    u32 MetaWatch::getWatchAddress() const { return m_memory_watch.getWatchAddress(); }

    u32 MetaWatch::getWatchSize() const { return m_memory_watch.getWatchSize(); }

    bool MetaWatch::startWatch(u32 address) {
        return m_memory_watch.startWatch(address, m_precompute_size);
    }

    void MetaWatch::stopWatch() { m_memory_watch.stopWatch(); }

}  // namespace Toolbox