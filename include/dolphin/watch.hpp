#pragma once

#include <functional>

#include "dolphin/hook.hpp"
#include "dolphin/process.hpp"
#include "objlib/meta/struct.hpp"
#include "objlib/meta/value.hpp"

using namespace Toolbox::Object;

#define WATCH_MAX_BUFFER_SIZE 65536

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
        [[nodiscard]] const std::string &getWatchName() const { return m_watch_name; }
        void setWatchName(const std::string &name) { m_watch_name = name; }

        [[nodiscard]] bool isLocked() const { return m_is_locked; }
        void setLocked(bool locked) { m_is_locked = locked; }

        [[nodiscard]] bool isWatchPointer() const { return m_watch_is_pointer; }
        [[nodiscard]] std::vector<u32> getPointerChain() const { return m_pointer_chain; }
        [[nodiscard]] u32 getWatchAddress() const { return m_watch_address; }
        [[nodiscard]] u32 getWatchSize() const { return m_watch_size; }

        [[nodiscard]] bool startWatch(u32 address, u32 size);
        [[nodiscard]] bool startWatch(const std::vector<u32> &pointer_chain, u32 size);
        void stopWatch();

        void onWatchNotify(watch_notify_cb cb) { m_watch_notify_cb = std::move(cb); }

        // This is the method that should be called
        // to update the watch state and potentially
        // notify the listener of changes
        void processWatch();

        static u32 TracePointerChainToAddress(const std::vector<u32> &pointer_chain);
        static std::vector<u32> ResolvePointerChainAsAddress(const std::vector<u32> &pointer_chain);

    protected:
        void notify(void *old_value, void *new_value, u32 value_width);

        u32 traceAddressFromPointerChain() const;

    private:
        std::string m_watch_name;

        std::vector<u32> m_pointer_chain;
        u32 m_watch_address;
        bool m_watch_is_pointer = false;

        u32 m_watch_size;

        watch_notify_cb m_watch_notify_cb;
        void *m_last_value_buf       = nullptr;
        u32 m_buf_size               = 0;
        bool m_last_value_needs_init = true;
        bool m_is_locked             = false;
    };

    class MetaWatch : public IUnique {
    public:
        // MetaWatch is a specialized MemoryWatch that uses
        // a MetaStruct to determine the watch address and size.
        using meta_watch_notify_cb =
            std::function<void(const Object::MetaValue &, const Object::MetaValue &)>;

    public:
        MetaWatch(MetaType type);

        MetaWatch(const MetaWatch &) = default;
        MetaWatch(MetaWatch &&)      = default;

        MetaWatch &operator=(const MetaWatch &) = default;
        MetaWatch &operator=(MetaWatch &&)      = default;

    protected:
        MetaWatch() = default;

    public:
        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        const std::string &getWatchName() const { return m_memory_watch.getWatchName(); }
        void setWatchName(const std::string &name) { m_memory_watch.setWatchName(name); }

        MetaType getWatchType() const;
        const MetaValue &getMetaValue() const { return m_last_value; }

        bool isLocked() const { return m_memory_watch.isLocked(); }
        void setLocked(bool locked) { m_memory_watch.setLocked(locked); }

        [[nodiscard]] bool isWatchPointer() const { return m_memory_watch.isWatchPointer(); }
        [[nodiscard]] std::vector<u32> getPointerChain() const { return m_memory_watch.getPointerChain(); }
        [[nodiscard]] u32 getWatchAddress() const { return m_memory_watch.getWatchAddress(); }
        [[nodiscard]] u32 getWatchSize() const { return m_memory_watch.getWatchSize(); }

        [[nodiscard]] bool startWatch(u32 address, u32 size = 0);
        [[nodiscard]] bool startWatch(const std::vector<u32> &pointer_chain, u32 size = 0);
        void stopWatch();

        void onWatchNotify(meta_watch_notify_cb cb) { m_watch_notify_cb = std::move(cb); }

        // This is the method that should be called
        // to update the watch state and potentially
        // notify the listener of changes
        void processWatch() { m_memory_watch.processWatch(); }

    private:
        UUID64 m_uuid;
        MemoryWatch m_memory_watch;
        MetaType m_meta_type;
        MetaValue m_last_value;
        size_t m_precompute_size = 0;
        meta_watch_notify_cb m_watch_notify_cb;
    };

}  // namespace Toolbox