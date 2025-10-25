#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/mimedata/mimedata.hpp"
#include "core/types.hpp"
#include "fsystem.hpp"
#include "image/imagehandle.hpp"
#include "jsonlib.hpp"
#include "model/model.hpp"
#include "serial.hpp"
#include "unique.hpp"

#include "watchdog/fswatchdog.hpp"

namespace Toolbox {

    enum class WatchValueBase : u8 {
        BASE_BINARY,
        BASE_OCTAL,
        BASE_DECIMAL,
        BASE_HEXADECIMAL,
    };

    class WatchGroup : public IUnique {
    public:
        WatchGroup()                       = default;
        WatchGroup(const WatchGroup &)     = default;
        WatchGroup(WatchGroup &&) noexcept = default;

        WatchGroup &operator=(const WatchGroup &)     = default;
        WatchGroup &operator=(WatchGroup &&) noexcept = default;

    public:
        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] const std::string &getName() const { return m_name; }
        void setName(const std::string &name) { m_name = name; }

        [[nodiscard]] bool isEmpty() const { return m_children.empty(); }

        [[nodiscard]] const std::vector<UUID64> &getChildren() const { return m_children; }
        [[nodiscard]] size_t getChildCount() const { return m_children.size(); }

        [[nodiscard]] bool isLocked() const { return m_locked; }
        void setLocked(bool locked) { m_locked = locked; }

        [[nodiscard]] bool hasChild(UUID64 uuid) const {
            return std::any_of(m_children.begin(), m_children.end(),
                               [&uuid](const UUID64 &it) { return it == uuid; });
        }

        bool addChild(UUID64 uuid) {
            m_children.emplace_back(uuid);
            return true;
        }
        bool insertChild(int64_t row, UUID64 uuid) {
            if (row < 0 || row > static_cast<int64_t>(m_children.size())) {
                return false;
            }
            auto it = m_children.begin();
            std::advance(it, row);
            m_children.insert(it, uuid);
            return true;
        }
        bool removeChild(UUID64 uuid) { return std::erase(m_children, uuid) > 0; }

        [[nodiscard]] UUID64 getChildUUID(size_t index) const {
            if (index >= m_children.size()) {
                return 0;
            }
            auto it = std::next(m_children.begin(), index);
            return *it;
        }

    private:
        UUID64 m_uuid;
        std::string m_name;
        std::vector<UUID64> m_children;
        bool m_locked = false;
    };

    enum class WatchModelSortRole {
        SORT_ROLE_NONE,
        SORT_ROLE_NAME,
        SORT_ROLE_TYPE,
    };

    enum class WatchModelEventFlags {
        NONE                 = 0,
        EVENT_RESET          = BIT(0),
        EVENT_WATCH_ADDED    = BIT(1),
        EVENT_WATCH_MODIFIED = BIT(2),
        EVENT_WATCH_REMOVED  = BIT(3),
        EVENT_GROUP_ADDED    = BIT(4),
        EVENT_GROUP_MODIFIED = BIT(5),
        EVENT_GROUP_REMOVED  = BIT(6),
        EVENT_WATCH_ANY      = EVENT_WATCH_ADDED | EVENT_WATCH_MODIFIED | EVENT_WATCH_REMOVED,
        EVENT_GROUP_ANY      = EVENT_GROUP_ADDED | EVENT_GROUP_MODIFIED | EVENT_GROUP_REMOVED,
        EVENT_ANY            = EVENT_RESET | EVENT_WATCH_ANY | EVENT_GROUP_ANY,
    };
    TOOLBOX_BITWISE_ENUM(WatchModelEventFlags)

    enum WatchDataRole {
        WATCH_DATA_ROLE_TYPE = ModelDataRole::DATA_ROLE_USER,
        WATCH_DATA_ROLE_VALUE_META,
        WATCH_DATA_ROLE_ADDRESS,
        WATCH_DATA_ROLE_LOCK,
        WATCH_DATA_ROLE_SIZE,
        WATCH_DATA_ROLE_VIEW_BASE,
    };

    class WatchDataModel : public IDataModel, public ISerializable {
    public:
        using json_t = nlohmann::ordered_json;

    public:
        WatchDataModel() = default;
        ~WatchDataModel();

        void initialize();

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool isReadOnly() const override { return false; }

        [[nodiscard]] bool isIndexGroup(const ModelIndex &index) const;

        [[nodiscard]] std::string getWatchType(const ModelIndex &index) const {
            return std::any_cast<std::string>(getData(index, WatchDataRole::WATCH_DATA_ROLE_TYPE));
        }

        [[nodiscard]] MetaValue getWatchValueMeta(const ModelIndex &index) const {
            return std::any_cast<MetaValue>(
                getData(index, WatchDataRole::WATCH_DATA_ROLE_VALUE_META));
        }

        [[nodiscard]] u32 getWatchAddress(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, WatchDataRole::WATCH_DATA_ROLE_ADDRESS));
        }

        void setWatchAddress(const ModelIndex &index, u32 address) {
            setData(index, address, WatchDataRole::WATCH_DATA_ROLE_ADDRESS);
        }

        [[nodiscard]] bool getWatchLock(const ModelIndex &index) const {
            return std::any_cast<bool>(getData(index, WatchDataRole::WATCH_DATA_ROLE_LOCK));
        }

        void setWatchLock(const ModelIndex &index, bool locked) {
            setData(index, locked, WatchDataRole::WATCH_DATA_ROLE_LOCK);
        }

        [[nodiscard]] u32 getWatchSize(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, WatchDataRole::WATCH_DATA_ROLE_SIZE));
        }

        void setWatchSize(const ModelIndex &index, u32 size) {
            setData(index, size, WatchDataRole::WATCH_DATA_ROLE_SIZE);
        }

        [[nodiscard]] WatchValueBase getWatchViewBase(const ModelIndex &index) const {
            return std::any_cast<WatchValueBase>(getData(index, WatchDataRole::WATCH_DATA_ROLE_VIEW_BASE));
        }

        void setWatchViewBase(const ModelIndex &index, WatchValueBase size) {
            setData(index, size, WatchDataRole::WATCH_DATA_ROLE_VIEW_BASE);
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;
        void setData(const ModelIndex &index, std::any data, int role) override;

        void setRefreshRate(s64 milliseconds) { m_refresh_rate = milliseconds; }

        [[nodiscard]] std::string findUniqueName(const ModelIndex &index,
                                                 const std::string &name) const;

        Result<void, JSONError> loadFromDMEFile(const fs_path &path);

    public:
        [[nodiscard]] ModelIndex getIndex(const UUID64 &path) const override;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const override;
        [[nodiscard]] bool removeIndex(const ModelIndex &index) override;

        [[nodiscard]] ModelIndex getParent(const ModelIndex &index) const override;
        [[nodiscard]] ModelIndex getSibling(int64_t row, int64_t column,
                                            const ModelIndex &index) const override;

        [[nodiscard]] size_t getColumnCount(const ModelIndex &index) const override;
        [[nodiscard]] size_t getRowCount(const ModelIndex &index) const override;

        [[nodiscard]] int64_t getColumn(const ModelIndex &index) const override;
        [[nodiscard]] int64_t getRow(const ModelIndex &index) const override;

        [[nodiscard]] bool hasChildren(const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData(const std::unordered_set<ModelIndex> &indexes) const override;
        [[nodiscard]] bool insertMimeData(const ModelIndex &index, const MimeData &data) override;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const override;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index) override;
        void fetchMore(const ModelIndex &index) override;

        void reset() override;

        ModelIndex makeWatchIndex(const std::string &name, MetaType type,
                                  const std::vector<u32> &pointer_chain, u32 size, bool is_pointer,
                                  WatchValueBase value_base, int64_t row, const ModelIndex &parent,
                                  bool find_unique_name = true);
        ModelIndex makeGroupIndex(const std::string &name, int64_t row, const ModelIndex &parent,
                                  bool find_unique_name = true);

        using event_listener_t = std::function<void(const ModelIndex &, WatchModelEventFlags)>;
        void addEventListener(UUID64 uuid, event_listener_t listener, WatchModelEventFlags flags);
        void removeEventListener(UUID64 uuid);

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        void signalEventListeners(const ModelIndex &index, WatchModelEventFlags flags);

    protected:
        // Implementation of public API for mutex locking reasons
        [[nodiscard]] std::any getData_(const ModelIndex &index, int role) const;
        void setData_(const ModelIndex &index, std::any data, int role);

        [[nodiscard]] std::string findUniqueName_(const ModelIndex &index,
                                                  const std::string &name) const;

        [[nodiscard]] ModelIndex getIndex_(const UUID64 &path) const;
        [[nodiscard]] ModelIndex getIndex_(int64_t row, int64_t column,
                                           const ModelIndex &parent = ModelIndex()) const;
        [[nodiscard]] bool removeIndex_(const ModelIndex &index);

        [[nodiscard]] ModelIndex getParent_(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex getSibling_(int64_t row, int64_t column,
                                             const ModelIndex &index) const;

        [[nodiscard]] size_t getColumnCount_(const ModelIndex &index) const;
        [[nodiscard]] size_t getRowCount_(const ModelIndex &index) const;

        [[nodiscard]] int64_t getColumn_(const ModelIndex &index) const;
        [[nodiscard]] int64_t getRow_(const ModelIndex &index) const;

        [[nodiscard]] bool hasChildren_(const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData_(const std::unordered_set<ModelIndex> &indexes) const;
        [[nodiscard]] bool insertMimeData_(const ModelIndex &index, const MimeData &data);

        [[nodiscard]] bool canFetchMore_(const ModelIndex &index);
        void fetchMore_(const ModelIndex &index);
        // -- END -- //

        size_t pollChildren(const ModelIndex &index) const;

        void processWatches();

    protected:
        struct _WatchIndexData {
            enum class Type { GROUP, WATCH };

            UUID64 m_parent    = 0;
            UUID64 m_self_uuid = 0;

            Type m_type = Type::GROUP;
            union {
                WatchGroup *m_group;
                MetaWatch *m_watch;
            };

            WatchValueBase m_value_base;

            bool hasChild(UUID64 uuid) const {
                if (m_type != Type::GROUP || !m_group) {
                    return false;
                }

                return m_group->hasChild(uuid);
            }

            std::strong_ordering operator<=>(const _WatchIndexData &rhs) const {
                return m_self_uuid <=> rhs.m_self_uuid;
            }
        };

        static bool _WatchIndexDataIsGroup(const _WatchIndexData &data);

        _WatchIndexData &getIndexData_(const ModelIndex &index);
        const _WatchIndexData &getIndexData_(const ModelIndex &index) const;

    private:
        UUID64 m_uuid;

        mutable std::mutex m_mutex;

        std::unordered_map<UUID64, std::pair<event_listener_t, WatchModelEventFlags>> m_listeners;

        mutable std::vector<_WatchIndexData> m_index_map;

        std::thread m_watch_thread;
        std::atomic<bool> m_running;

        s64 m_refresh_rate;
    };

    class WatchDataModelSortFilterProxy : public IDataModel {
    public:
        WatchDataModelSortFilterProxy()  = default;
        ~WatchDataModelSortFilterProxy() = default;

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool isReadOnly() const override { return m_source_model->isReadOnly(); }

        [[nodiscard]] RefPtr<WatchDataModel> getSourceModel() const;
        void setSourceModel(RefPtr<WatchDataModel> model);

        [[nodiscard]] ModelSortOrder getSortOrder() const;
        void setSortOrder(ModelSortOrder order);

        [[nodiscard]] WatchModelSortRole getSortRole() const;
        void setSortRole(WatchModelSortRole role);

        [[nodiscard]] const std::string &getFilter() const &;
        void setFilter(const std::string &filter);

        [[nodiscard]] bool isIndexGroup(const ModelIndex &index) const;

        [[nodiscard]] std::string getWatchType(const ModelIndex &index) const {
            return std::any_cast<std::string>(getData(index, WatchDataRole::WATCH_DATA_ROLE_TYPE));
        }

        [[nodiscard]] MetaValue getWatchValueMeta(const ModelIndex &index) const {
            return std::any_cast<MetaValue>(
                getData(index, WatchDataRole::WATCH_DATA_ROLE_VALUE_META));
        }

        [[nodiscard]] u32 getWatchAddress(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, WatchDataRole::WATCH_DATA_ROLE_ADDRESS));
        }

        void setWatchAddress(const ModelIndex &index, u32 address) {
            setData(index, address, WatchDataRole::WATCH_DATA_ROLE_ADDRESS);
        }

        [[nodiscard]] bool getWatchLock(const ModelIndex &index) const {
            return std::any_cast<bool>(getData(index, WatchDataRole::WATCH_DATA_ROLE_LOCK));
        }

        void setWatchLock(const ModelIndex &index, bool locked) {
            setData(index, locked, WatchDataRole::WATCH_DATA_ROLE_LOCK);
        }

        [[nodiscard]] u32 getWatchSize(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, WatchDataRole::WATCH_DATA_ROLE_SIZE));
        }

        void setWatchSize(const ModelIndex &index, u32 size) {
            setData(index, size, WatchDataRole::WATCH_DATA_ROLE_SIZE);
        }

        [[nodiscard]] WatchValueBase getWatchViewBase(const ModelIndex &index) const {
            return std::any_cast<WatchValueBase>(
                getData(index, WatchDataRole::WATCH_DATA_ROLE_VIEW_BASE));
        }

        void setWatchViewBase(const ModelIndex &index, WatchValueBase size) {
            setData(index, size, WatchDataRole::WATCH_DATA_ROLE_VIEW_BASE);
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;
        void setData(const ModelIndex &index, std::any data, int role) override;

        [[nodiscard]] ModelIndex getIndex(const UUID64 &path) const override;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const override;
        [[nodiscard]] bool removeIndex(const ModelIndex &index) override;

        [[nodiscard]] ModelIndex getParent(const ModelIndex &index) const override;
        [[nodiscard]] ModelIndex getSibling(int64_t row, int64_t column,
                                            const ModelIndex &index) const override;

        [[nodiscard]] size_t getColumnCount(const ModelIndex &index) const override;
        [[nodiscard]] size_t getRowCount(const ModelIndex &index) const override;

        [[nodiscard]] int64_t getColumn(const ModelIndex &index) const override;
        [[nodiscard]] int64_t getRow(const ModelIndex &index) const override;

        [[nodiscard]] bool hasChildren(const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData(const std::unordered_set<ModelIndex> &indexes) const override;
        [[nodiscard]] bool insertMimeData(const ModelIndex &index, const MimeData &data) override;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const override;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index) override;
        void fetchMore(const ModelIndex &index) override;

        void reset() override;

        [[nodiscard]] ModelIndex toSourceIndex(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex toProxyIndex(const ModelIndex &index) const;

    protected:
        [[nodiscard]] ModelIndex toProxyIndex(int64_t row, int64_t column,
                                              const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] bool isFiltered(const UUID64 &uuid) const;

        void cacheIndex(const ModelIndex &index) const;
        void cacheIndex_(const ModelIndex &index) const;

        void watchDataUpdateEvent(const ModelIndex &index, WatchModelEventFlags flags);

    private:
        UUID64 m_uuid;

        RefPtr<WatchDataModel> m_source_model = nullptr;
        ModelSortOrder m_sort_order           = ModelSortOrder::SORT_ASCENDING;
        WatchModelSortRole m_sort_role        = WatchModelSortRole::SORT_ROLE_NONE;
        std::string m_filter                  = "";

        bool m_dirs_only = false;

        mutable std::mutex m_cache_mutex;
        mutable std::unordered_map<UUID64, bool> m_filter_map;
        mutable std::unordered_map<UUID64, std::vector<int64_t>> m_row_map;
    };

}  // namespace Toolbox