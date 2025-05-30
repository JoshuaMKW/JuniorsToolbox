#pragma once

#include <functional>

#include "dolphin/hook.hpp"
#include "dolphin/process.hpp"
#include "objlib/meta/struct.hpp"

namespace Toolbox {

    class MemoryWatch {
    public:
        using watch_notify_cb = std::function<void(void *, void *, u32)>;

    public:
        MemoryWatch()                    = default;
        MemoryWatch(const MemoryWatch &) = default;
        MemoryWatch(MemoryWatch &&)      = default;
        ~MemoryWatch() {
            stopWatch();
            if (m_last_value_buf) {
                delete[] static_cast<char *>(m_last_value_buf);
                m_last_value_buf = nullptr;
            }
        }

        MemoryWatch &operator=(const MemoryWatch &) = default;
        MemoryWatch &operator=(MemoryWatch &&)      = default;

    public:
        const std::string &getWatchName() const { return m_watch_name; }
        void setWatchName(const std::string &name) { m_watch_name = name; }

        bool isLocked() const { return m_is_locked; }
        void setLocked(bool locked) { m_is_locked = locked; }

        [[nodiscard]] u32 getWatchAddress() const { return m_watch_address; }
        [[nodiscard]] u32 getWatchSize() const { return m_watch_size; }

        [[nodiscard]] bool startWatch(u32 address, u32 size);
        void stopWatch();

        void onWatchNotify(watch_notify_cb cb) { m_watch_notify_cb = std::move(cb); }

        // This is the method that should be called
        // to update the watch state and potentially
        // notify the listener of changes
        void processWatch();

    protected:
        void notify(void *old_value, void *new_value, u32 value_width);

    private:
        std::string m_watch_name;
        u32 m_watch_address;
        u32 m_watch_size;
        watch_notify_cb m_watch_notify_cb;
        void *m_last_value_buf       = nullptr;
        bool m_last_value_needs_init = true;
        bool m_is_locked = false;
    };

    class MetaWatch {
    public:
        // MetaWatch is a specialized MemoryWatch that uses
        // a MetaStruct to determine the watch address and size.
        using meta_watch_notify_cb =
            std::function<void(const Object::MetaStruct &, const Object::MetaStruct &)>;

    public:
        MetaWatch(const Object::MetaStruct &meta_struct);
        MetaWatch(Object::MetaStruct &&meta_struct);

        MetaWatch()                  = delete;
        MetaWatch(const MetaWatch &) = default;
        MetaWatch(MetaWatch &&)      = default;

        MetaWatch &operator=(const MetaWatch &) = default;
        MetaWatch &operator=(MetaWatch &&)      = default;

    public:
        const std::string &getWatchName() const { return m_memory_watch.getWatchName(); }
        void setWatchName(const std::string &name) { m_memory_watch.setWatchName(name); }

        std::string_view getWatchType() const { return m_meta_struct->name(); }

        bool isLocked() const { return m_memory_watch.isLocked(); }
        void setLocked(bool locked) { m_memory_watch.setLocked(locked); }

        [[nodiscard]] u32 getWatchAddress() const;
        [[nodiscard]] u32 getWatchSize() const;

        [[nodiscard]] bool startWatch(u32 address);
        void stopWatch();

        void onWatchNotify(meta_watch_notify_cb cb) { m_watch_notify_cb = std::move(cb); }

        // This is the method that should be called
        // to update the watch state and potentially
        // notify the listener of changes
        void processWatch() { m_memory_watch.processWatch(); }

    private:
        MemoryWatch m_memory_watch;
        ScopePtr<Object::MetaStruct> m_last_struct;
        ScopePtr<Object::MetaStruct> m_meta_struct;
        size_t m_precompute_size = 0;
        meta_watch_notify_cb m_watch_notify_cb;
    };

}  // namespace Toolbox