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
#include "objlib/meta/value.hpp"
#include "serial.hpp"
#include "unique.hpp"

#include "watchdog/fswatchdog.hpp"

namespace Toolbox {

    class MemoryScanner;

#define SCAN_IDX_GET_ADDRESS(inline_data) ((u32)(inline_data >> 32))
#define SCAN_IDX_GET_HISTORY_IDX(inline_data)    ((u32)inline_data)
#define SCAN_IDX_MAKE_PAIR(address, history_idx) ((u64)((((u64)address) << 32) | (u64)history_idx))

    class MemScanResult {
        u32 m_bit_data;

        static constexpr u32 addr_mask = 0x03FFFFFF;
        static constexpr u32 idx_mask  = ~addr_mask;
        static constexpr u32 idx_shift = 26;

    public:
        MemScanResult(u32 address, int history_index) {
            m_bit_data = ((history_index << idx_shift) & idx_mask) | (address & addr_mask);
        }

        ~MemScanResult() = default;

        u32 getAddress() const { return 0x80000000 | (m_bit_data & addr_mask); }
        int getHistoryIndex() const { return (m_bit_data & ~addr_mask) >> idx_shift; }

        void setAddress(u32 address) {
            m_bit_data = (m_bit_data & idx_mask) | (address & addr_mask);
        }
        void setHistoryIndex(int index) {
            m_bit_data = (m_bit_data & addr_mask) | ((index << idx_shift) & addr_mask);
        }

        bool operator==(const MemScanResult &other) const {
            return getAddress() == other.getAddress();
        }

        std::strong_ordering operator<=>(const MemScanResult &other) const {
            return getAddress() <=> other.getAddress();
        }
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

        struct ScanHistoryEntry {
            MetaType m_scan_type = MetaType::UNKNOWN;
            u16 m_scan_size;  // UI doesn't allow scans larger than a u16
            mutable std::vector<MemScanResult> m_scan_results = {};

            Buffer m_scan_buffer;
        };

    public:
        MemScanModel() = default;
        ~MemScanModel();

        void initialize();

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool isReadOnly() const { return false; }

        [[nodiscard]] MetaType getScanType(const ModelIndex &index) const {
            return std::any_cast<MetaType>(getData(index, MemScanRole::MEMSCAN_ROLE_TYPE));
        }

        [[nodiscard]] u32 getScanAddress(const ModelIndex &index) const {
            return std::any_cast<u32>(getData(index, MemScanRole::MEMSCAN_ROLE_ADDRESS));
        }

        [[nodiscard]] u16 getScanSize(const ModelIndex &index) const {
            return std::any_cast<u16>(getData(index, MemScanRole::MEMSCAN_ROLE_SIZE));
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

        void loadFromDMEFile(const fs_path &path);

    public:
        [[nodiscard]] ModelIndex getIndex(const u32 &address) const;
        [[nodiscard]] ModelIndex getIndex(const UUID64 &uuid) const override;
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
        createMimeData(const IDataModel::index_container &indexes) const override;
        [[nodiscard]] bool insertMimeData(const ModelIndex &index, const MimeData &data) override;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const override;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index) override;
        void fetchMore(const ModelIndex &index) override;

        void reset() override;

        // -------------------

        bool isScanBusy() const;
        double getScanProgress() const;

        bool requestScan(u32 search_start, u32 search_size, MetaType val_type, ScanOperator scan_op,
                         const std::string &a, const std::string &b, int desired_radix = 10,
                         bool enforce_alignment = true, bool new_scan = true,
                         size_t sleep_granularity = 100000, s64 sleep_duration = 16);
        bool requestScan(u32 search_start, u32 search_size, MetaType val_type, ScanOperator scan_op,
                         MetaValue &&a, MetaValue &&b, bool enforce_alignment = true,
                         bool new_scan = true, size_t sleep_granularity = 100000,
                         s64 sleep_duration = 16);

        bool canUndoScan() const { return m_history_size > 0; }
        bool undoScan();

        // -------------------

        using event_listener_t = std::function<void(const ModelIndex &, MemScanModelEventFlags)>;
        void addEventListener(UUID64 uuid, event_listener_t listener, MemScanModelEventFlags flags);
        void removeEventListener(UUID64 uuid);

        Result<void, SerialError> serialize(Serializer &out) const override;
        Result<void, SerialError> deserialize(Deserializer &in) override;

        void makeScanIndex(u32 address);

        bool reserveScan(MetaType scan_type, size_t scan_size, size_t indexes) {
            if (m_history_size >= m_index_map_history.max_size()) {
                return false;
            }

            ScanHistoryEntry entry;
            entry.m_scan_type = scan_type;
            entry.m_scan_size = scan_size;
            entry.m_scan_results.reserve(indexes);
            m_index_map_history[m_history_size++] = std::move(entry);
            return true;
        }

        bool captureMemForCache();

        const ScanHistoryEntry &getScanHistory() const;
        const ScanHistoryEntry &getScanHistory(size_t i) const;

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
        createMimeData_(const IDataModel::index_container &indexes) const;
        [[nodiscard]] bool insertMimeData_(const ModelIndex &index, const MimeData &data);

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
        mutable std::array<ScanHistoryEntry, 32> m_index_map_history;
        size_t m_history_size = 0;

        MetaType m_scan_type = MetaType::UNKNOWN;
        u32 m_scan_size      = 0;

        std::thread m_scan_thread;
        std::atomic<bool> m_running;

        ScopePtr<MemoryScanner> m_scanner;

        bool m_wants_scan = false;
        MemScanProfile m_scan_profile;
        size_t m_scan_result_num = 0;
    };

    class MemoryScanner : public TaskThread<size_t> {
    public:
        MemoryScanner() = delete;
        MemoryScanner(MemScanModel *model) : m_scan_model(model) {}

        size_t tRun(void *param) override;

    private:
        size_t searchAddressSpan(const MemScanModel::MemScanProfile &profile);

        size_t scanAllBools(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllS8s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllU8s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllS16s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllU16s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllS32s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllU32s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllF32s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllF64s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllStrings(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanAllByteArrays(MemScanModel &model, const MemScanModel::MemScanProfile &profile);

        size_t scanExistingBools(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingS8s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingU8s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingS16s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingU16s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingS32s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingU32s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingF32s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingF64s(MemScanModel &model, const MemScanModel::MemScanProfile &profile);
        size_t scanExistingStrings(MemScanModel &model,
                                   const MemScanModel::MemScanProfile &profile);
        size_t scanExistingByteArrays(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile);

        MemScanModel *m_scan_model;
    };

}  // namespace Toolbox