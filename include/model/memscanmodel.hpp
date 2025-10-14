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
#include "model/model.hpp"
#include "objlib/meta/value.hpp "
#include "serial.hpp"
#include "unique.hpp"

#include "watchdog/fswatchdog.hpp"

namespace Toolbox {

    class MemoryScanner;

    struct MemScanResult {
        u32 m_address;
        MetaValue m_scanned_value;
        int m_scan_history_index;
    };

    enum class MemScanModelSortRole {
        SORT_ROLE_NONE,
        SORT_ROLE_ADDRESS,
    };

    enum class MemScanModelEventFlags {
        NONE                = 0,
        EVENT_SCAN_ADDED    = BIT(0),
        EVENT_SCAN_MODIFIED = BIT(1),
        EVENT_SCAN_REMOVED  = BIT(2),
        EVENT_SCAN_ANY      = EVENT_SCAN_ADDED | EVENT_SCAN_MODIFIED | EVENT_SCAN_REMOVED,
    };
    TOOLBOX_BITWISE_ENUM(MemScanModelEventFlags)

    enum MemScanRole {
        MEMSCAN_ROLE_ADDRESS = ModelDataRole::DATA_ROLE_USER,
        MEMSCAN_ROLE_TYPE,
        MEMSCAN_ROLE_SIZE,
        MEMSCAN_ROLE_VALUE,
        MEMSCAN_ROLE_VALUE_MEM,
    };

    class MemScanModel : public IDataModel, public ISerializable {
    public:
        enum class ScanOperator {
            OP_EXACT,
            OP_INCREASED_BY,
            OP_DECREASED_BY,
            OP_BETWEEN,
            OP_BIGGER_THAN,
            OP_SMALLER_THAN,
            OP_INCREASED,
            OP_DECREASED,
            OP_CHANGED,
            OP_UNCHANGED,
            OP_UNKNOWN_INITIAL,
        };

    public:
        MemScanModel() = default;
        ~MemScanModel();

        void initialize();

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] MetaType getScanType(const ModelIndex &index) const {
            return std::any_cast<MetaType>(getData(index, MemScanRole::MEMSCAN_ROLE_TYPE));
        }

        [[nodiscard]] u32 getScanAddress(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, MemScanRole::MEMSCAN_ROLE_ADDRESS));
        }

        [[nodiscard]] u32 getScanSize(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, MemScanRole::MEMSCAN_ROLE_SIZE));
        }

        [[nodiscard]] MetaValue getScanValue(const ModelIndex &index) const {
            return std::any_cast<MetaValue>(getData(index, MemScanRole::MEMSCAN_ROLE_VALUE));
        }

        void setScanValue(const ModelIndex &index, MetaValue &&value) {
            setData(index, std::move(value), MemScanRole::MEMSCAN_ROLE_VALUE);
        }

        [[nodiscard]] MetaValue getCurrentValue(const ModelIndex &index) const {
            return std::any_cast<MetaValue>(getData(index, MemScanRole::MEMSCAN_ROLE_VALUE_MEM));
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;
        void setData(const ModelIndex &index, std::any data, int role) override;

    public:
        [[nodiscard]] ModelIndex getIndex(const u32 &address) const;
        [[nodiscard]] ModelIndex getIndex(const UUID64 &uuid) const override;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] ModelIndex getParent(const ModelIndex &index) const override;
        [[nodiscard]] ModelIndex getSibling(int64_t row, int64_t column,
                                            const ModelIndex &index) const override;

        [[nodiscard]] size_t getColumnCount(const ModelIndex &index) const override;
        [[nodiscard]] size_t getRowCount(const ModelIndex &index) const override;

        [[nodiscard]] int64_t getColumn(const ModelIndex &index) const override;
        [[nodiscard]] int64_t getRow(const ModelIndex &index) const override;

        [[nodiscard]] bool hasChildren(const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData(const std::vector<ModelIndex> &indexes) const override;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const override;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index) override;
        void fetchMore(const ModelIndex &index) override;

        void reset() override;

        bool requestScan(u32 search_start, u32 search_size, MetaType val_type, ScanOperator scan_op,
                         const std::string &a, const std::string &b, int desired_radix = 10,
                         bool enforce_alignment = true, bool new_scan = true,
                         size_t sleep_granularity = 100000, s64 sleep_duration = 16);
        bool requestScan(u32 search_start, u32 search_size, MetaType val_type, ScanOperator scan_op,
                         MetaValue &&a, MetaValue &&b, bool enforce_alignment = true,
                         bool new_scan = true, size_t sleep_granularity = 100000,
                         s64 sleep_duration = 16);

        bool canUndoScan() const { return !m_index_map_history.empty(); }
        bool undoScan();

        using event_listener_t = std::function<void(const ModelIndex &, MemScanModelEventFlags)>;
        void addEventListener(UUID64 uuid, event_listener_t listener, MemScanModelEventFlags flags);
        void removeEventListener(UUID64 uuid);

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        void makeScanIndex(u32 address, MetaValue &&value);

        void reserveScan(size_t indexes) {
            std::vector<ModelIndex> index_map;
            index_map.reserve(indexes);
            m_index_map_history.emplace_back(std::move(index_map));
        }

        const std::vector<ModelIndex> &getScanHistory() const;
        const std::vector<ModelIndex> &getScanHistory(size_t i) const;

        struct MemScanProfile {
            u32 m_search_start;
            u32 m_search_size;
            MetaType m_scan_type;
            ScanOperator m_scan_op;
            MetaValue m_scan_a;
            MetaValue m_scan_b;
            bool m_enforce_alignment;
            bool m_new_scan;
            size_t m_sleep_granularity;
            s64 m_sleep_duration;
        };

    protected:
        // Implementation of public API for mutex locking reasons
        [[nodiscard]] std::any getData_(const ModelIndex &index, int role) const;
        void setData_(const ModelIndex &index, std::any data, int role);

        [[nodiscard]] ModelIndex getIndex_(const u32 &address) const;
        [[nodiscard]] ModelIndex getIndex_(const UUID64 &path) const;
        [[nodiscard]] ModelIndex getIndex_(int64_t row, int64_t column,
                                           const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] ModelIndex getParent_(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex getSibling_(int64_t row, int64_t column,
                                             const ModelIndex &index) const;

        [[nodiscard]] size_t getColumnCount_(const ModelIndex &index) const;
        [[nodiscard]] size_t getRowCount_(const ModelIndex &index) const;

        [[nodiscard]] int64_t getColumn_(const ModelIndex &index) const;
        [[nodiscard]] int64_t getRow_(const ModelIndex &index) const;

        [[nodiscard]] bool hasChildren_(const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData_(const std::vector<ModelIndex> &indexes) const;

        [[nodiscard]] bool canFetchMore_(const ModelIndex &index);
        void fetchMore_(const ModelIndex &index);
        // -- END -- //

        size_t pollChildren(const ModelIndex &index) const;

        void signalEventListeners(const ModelIndex &index, MemScanModelEventFlags flags);

        MetaValue getMetaValueFromMemory(const ModelIndex &index) const;

    private:
        UUID64 m_uuid;

        mutable std::mutex m_mutex;

        std::unordered_map<UUID64, std::pair<event_listener_t, MemScanModelEventFlags>> m_listeners;

        // This will necessarily always be sorted
        // by means of linear construction
        mutable std::vector<std::vector<ModelIndex>> m_index_map_history;
        MetaType m_scan_type;
        u32 m_scan_size;

        std::thread m_scan_thread;
        std::atomic<bool> m_running;

        ScopePtr<MemoryScanner> m_scanner;

        bool m_wants_scan = false;
        MemScanProfile m_scan_profile;
        size_t m_scan_result_num = 0;
    };

    class MemScanModelSortFilterProxy : public IDataModel {
    public:
        MemScanModelSortFilterProxy()  = default;
        ~MemScanModelSortFilterProxy() = default;

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] RefPtr<MemScanModel> getSourceModel() const;
        void setSourceModel(RefPtr<MemScanModel> model);

        [[nodiscard]] ModelSortOrder getSortOrder() const;
        void setSortOrder(ModelSortOrder order);

        [[nodiscard]] MemScanModelSortRole getSortRole() const;
        void setSortRole(MemScanModelSortRole role);

        [[nodiscard]] const std::string &getFilter() const &;
        void setFilter(const std::string &filter);

        [[nodiscard]] MetaType getScanType(const ModelIndex &index) const {
            return std::any_cast<MetaType>(getData(index, MemScanRole::MEMSCAN_ROLE_TYPE));
        }

        [[nodiscard]] u32 getScanAddress(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, MemScanRole::MEMSCAN_ROLE_ADDRESS));
        }

        [[nodiscard]] u32 getScanSize(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, MemScanRole::MEMSCAN_ROLE_SIZE));
        }

        [[nodiscard]] MetaValue getScanValue(const ModelIndex &index) const {
            return std::any_cast<MetaValue>(getData(index, MemScanRole::MEMSCAN_ROLE_VALUE));
        }

        void setScanValue(const ModelIndex &index, MetaValue &&value) {
            setData(index, std::move(value), MemScanRole::MEMSCAN_ROLE_VALUE);
        }

        [[nodiscard]] MetaValue getCurrentValue(const ModelIndex &index) const {
            return std::any_cast<MetaValue>(getData(index, MemScanRole::MEMSCAN_ROLE_VALUE_MEM));
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;
        void setData(const ModelIndex &index, std::any data, int role) override;

        [[nodiscard]] ModelIndex getIndex(const UUID64 &path) const override;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] ModelIndex getParent(const ModelIndex &index) const override;
        [[nodiscard]] ModelIndex getSibling(int64_t row, int64_t column,
                                            const ModelIndex &index) const override;

        [[nodiscard]] size_t getColumnCount(const ModelIndex &index) const override;
        [[nodiscard]] size_t getRowCount(const ModelIndex &index) const override;

        [[nodiscard]] int64_t getColumn(const ModelIndex &index) const override;
        [[nodiscard]] int64_t getRow(const ModelIndex &index) const override;

        [[nodiscard]] bool hasChildren(const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData(const std::vector<ModelIndex> &indexes) const override;
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

        void watchDataUpdateEvent(const ModelIndex &index, MemScanModelEventFlags flags);

    private:
        UUID64 m_uuid;

        RefPtr<MemScanModel> m_source_model = nullptr;
        ModelSortOrder m_sort_order         = ModelSortOrder::SORT_ASCENDING;
        MemScanModelSortRole m_sort_role    = MemScanModelSortRole::SORT_ROLE_NONE;
        std::string m_filter                = "";

        bool m_dirs_only = false;

        mutable std::mutex m_cache_mutex;
        mutable std::unordered_map<UUID64, bool> m_filter_map;
        mutable std::unordered_map<UUID64, std::vector<int64_t>> m_row_map;
    };

    class MemoryScanner : public TaskThread<size_t> {
    public:
        MemoryScanner() = delete;
        MemoryScanner(MemScanModel *model) : m_scan_model(model) {}

        size_t tRun(void *param) override;

    private:
        size_t searchAddressSpan(const MemScanModel::MemScanProfile &profile);

        MemScanModel *m_scan_model;
    };

}  // namespace Toolbox