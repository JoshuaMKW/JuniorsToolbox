#include <algorithm>
#include <any>
#include <compare>
#include <set>

#include "dolphin/watch.hpp"
#include "gui/application.hpp"
#include "model/memscanmodel.hpp"

using namespace Toolbox::Object;

namespace Toolbox {

    template <typename T> static T readSingle(const Buffer &mem_buf, u32 address) {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                      "This method requires simple data");

        DolphinHookManager &manager = DolphinHookManager::instance();
        u32 addr_ofs                = manager.getAddressAsOffset(address);

        const char *mem_handle = mem_buf.buf<char>() + addr_ofs;
        uint32_t mem_size      = mem_buf.size();

        // Bounds check
        if (addr_ofs + sizeof(T) >= mem_size) {
            return T();
        }

        T mem_val;
        std::memcpy(&mem_val, mem_handle, sizeof(T));

        if constexpr (std::is_integral_v<T>) {
            mem_val = std::byteswap(mem_val);
        } else if constexpr (std::is_same_v<T, f32>) {
            mem_val = std::bit_cast<T>(std::byteswap(std::bit_cast<u32>(mem_val)));
        } else if constexpr (std::is_same_v<T, f64>) {
            mem_val = std::bit_cast<T>(std::byteswap(std::bit_cast<u64>(mem_val)));
        }

        return mem_val;
    }

    static std::string readString(const Buffer &mem_buf, u32 address, size_t val_size) {

        DolphinHookManager &manager = DolphinHookManager::instance();
        u32 addr_ofs                = manager.getAddressAsOffset(address);

        const char *mem_handle = mem_buf.buf<char>() + addr_ofs;
        uint32_t mem_size      = mem_buf.size();

        // Bounds check
        if (addr_ofs + val_size >= mem_size) {
            return std::string();
        }

        return std::string(mem_handle, val_size);
    }

    static Buffer readBytes(const Buffer &mem_buf, u32 address, size_t val_size) {
        DolphinHookManager &manager = DolphinHookManager::instance();
        u32 addr_ofs                = manager.getAddressAsOffset(address);

        const char *mem_handle = mem_buf.buf<char>() + addr_ofs;
        uint32_t mem_size      = mem_buf.size();

        // Bounds check
        if (addr_ofs + val_size >= mem_size) {
            return Buffer();
        }

        Buffer buf;
        buf.alloc(val_size);
        std::memcpy(buf.buf(), mem_handle, val_size);

        return buf;
    }

    static MetaValue GetMetaValueFromMemCache(const MemScanModel::ScanHistoryEntry &entry,
                                              const MemScanResult &result) {
        u32 address = result.getAddress();
        switch (entry.m_scan_type) {
        case MetaType::BOOL:
            return MetaValue(readSingle<bool>(entry.m_scan_buffer, address));
        case MetaType::S8:
            return MetaValue(readSingle<s8>(entry.m_scan_buffer, address));
        case MetaType::U8:
            return MetaValue(readSingle<u8>(entry.m_scan_buffer, address));
        case MetaType::S16:
            return MetaValue(readSingle<s16>(entry.m_scan_buffer, address));
        case MetaType::U16:
            return MetaValue(readSingle<u16>(entry.m_scan_buffer, address));
        case MetaType::S32:
            return MetaValue(readSingle<s32>(entry.m_scan_buffer, address));
        case MetaType::U32:
            return MetaValue(readSingle<u32>(entry.m_scan_buffer, address));
        case MetaType::F32:
            return MetaValue(readSingle<f32>(entry.m_scan_buffer, address));
        case MetaType::F64:
            return MetaValue(readSingle<f64>(entry.m_scan_buffer, address));
        case MetaType::STRING:
            return MetaValue(readString(entry.m_scan_buffer, address, entry.m_scan_size));
        case MetaType::UNKNOWN:
            return MetaValue(readBytes(entry.m_scan_buffer, address, entry.m_scan_size));
        default:
            return MetaValue(MetaType::UNKNOWN);
        }
    }

    template <typename T>
    static bool compareSingle(const Buffer &mem_buf, u32 address, const T &a, const T &b,
                              const T &last, MemScanModel::ScanOperator op, T *out) {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                      "This method requires simple data");

        DolphinHookManager &manager = DolphinHookManager::instance();
        u32 addr_ofs                = manager.getAddressAsOffset(address);

        const char *mem_handle = mem_buf.buf<char>() + addr_ofs;
        uint32_t mem_size      = mem_buf.size();

        // Bounds check
        if (addr_ofs + sizeof(a) >= mem_size) {
            return false;
        }

        T mem_val;
        std::memcpy(&mem_val, mem_handle, sizeof(a));

        if constexpr (std::is_integral_v<T>) {
            mem_val = std::byteswap(mem_val);
        } else if constexpr (std::is_same_v<T, f32>) {
            mem_val = std::bit_cast<T>(std::byteswap(std::bit_cast<u32>(mem_val)));
        } else if constexpr (std::is_same_v<T, f64>) {
            mem_val = std::bit_cast<T>(std::byteswap(std::bit_cast<u64>(mem_val)));
        }

        if (out) {
            *out = mem_val;
        }

        switch (op) {
        case MemScanModel::ScanOperator::OP_EXACT:
            return a == mem_val;
        case MemScanModel::ScanOperator::OP_INCREASED_BY:
            return mem_val == last + a;
        case MemScanModel::ScanOperator::OP_DECREASED_BY:
            return mem_val == last - a;
        case MemScanModel::ScanOperator::OP_BETWEEN:
            return a <= mem_val && mem_val <= b;
        case MemScanModel::ScanOperator::OP_BIGGER_THAN:
            return mem_val > a;
        case MemScanModel::ScanOperator::OP_SMALLER_THAN:
            return mem_val < a;
        case MemScanModel::ScanOperator::OP_INCREASED:
            return mem_val > last;
        case MemScanModel::ScanOperator::OP_DECREASED:
            return mem_val < last;
        case MemScanModel::ScanOperator::OP_CHANGED:
            return mem_val != last;
        case MemScanModel::ScanOperator::OP_UNCHANGED:
            return mem_val == last;
        case MemScanModel::ScanOperator::OP_UNKNOWN_INITIAL:
            return true;
        }

        return false;
    }

    static bool compareString(const Buffer &mem_buf, u32 address, const std::string &a,
                              MemScanModel::ScanOperator op, std::string *out) {
        if (op != MemScanModel::ScanOperator::OP_EXACT) {
            return false;
        }

        DolphinHookManager &manager = DolphinHookManager::instance();
        u32 addr_ofs                = manager.getAddressAsOffset(address);

        const char *mem_handle = mem_buf.buf<char>() + addr_ofs;
        uint32_t mem_size      = mem_buf.size();

        // Bounds check
        if (addr_ofs + a.size() >= mem_size) {
            return false;
        }

        if (out) {
            out->reserve(a.size());
        }

        const char *a_str = a.c_str();
        for (size_t i = 0; i < a.size(); ++i) {
            if (a_str[i] != mem_handle[i]) {
                return false;
            }
        }

        if (out) {
            *out = std::string(mem_handle, a.size());
        }

        return true;
    }

    static bool compareBytes(const Buffer &mem_buf, u32 address, const Buffer &a,
                             MemScanModel::ScanOperator op, Buffer *out) {
        if (op != MemScanModel::ScanOperator::OP_EXACT) {
            return false;
        }

        DolphinHookManager &manager = DolphinHookManager::instance();
        u32 addr_ofs                = manager.getAddressAsOffset(address);

        const char *mem_handle = mem_buf.buf<char>() + addr_ofs;
        uint32_t mem_size      = mem_buf.size();

        // Bounds check
        if (addr_ofs + a.size() >= mem_size) {
            return false;
        }

        if (out) {
            out->alloc(a.size());
        }

        const char *a_arr = a.buf<const char>();
        for (size_t i = 0; i < a.size(); ++i) {
            if (a_arr[i] != mem_handle[i]) {
                return false;
            }
        }

        if (out) {
            char *buf_out = out->buf<char>();
            std::memcpy(buf_out, mem_handle, out->size());
        }

        return true;
    }

    template <typename T>
    static size_t scanAllSingles(MemScanModel &model, const MemScanModel::MemScanProfile &profile,
                                 std::function<void(double)> prog_setter) {
        const u32 begin_address = profile.m_search_start;
        const u32 end_address   = begin_address + profile.m_search_size;

        if (profile.m_search_size == 0 || begin_address < 0x80000000 ||
            end_address > (0x80000000 | DolphinHookManager::instance().getMemorySize())) {
            return 0;
        }

        const u32 val_width = profile.m_scan_a.computeSize();
        const u32 addr_mask = profile.m_enforce_alignment ? ~(std::min<u32>(val_width, 4) - 1) : ~0;
        const u32 addr_inc  = profile.m_enforce_alignment ? std::min<u32>(val_width, 4) : 1;

        // Reserve an estimate allocation that should be sane.
        size_t O_estimate = profile.m_scan_op == MemScanModel::ScanOperator::OP_UNKNOWN_INITIAL
                                ? profile.m_search_size
                                : profile.m_search_size / 100;
        if (profile.m_enforce_alignment) {
            O_estimate /=
                val_width;  // Since alignment is forced, each entry has to be n bytes apart.
        }

        if (!model.reserveScan(profile.m_scan_type, sizeof(T), O_estimate)) {
            return 0;
        }

        if (!model.captureMemForCache()) {
            return 0;
        }

        const MemScanModel::ScanHistoryEntry &entry = model.getScanHistory();

        T val_a = profile.m_scan_a.get<T>().value_or(T());
        T val_b = profile.m_scan_b.get<T>().value_or(T());

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;
        while (address < end_address) {
            address &= addr_mask;

            T mem_val;
            bool is_match =
                compareSingle<T>(entry.m_scan_buffer, address, val_a, val_b,
                                 T() /*this is the previous scan*/, profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address);
                match_counter += 1;
            }

            address += addr_inc;

            prog_setter((double)(address - begin_address) / (double)profile.m_search_size);

            // if (sleep_counter++ >= profile.m_sleep_granularity) {
            //     sleep_counter = 0;
            //     std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            // }
        }

        return match_counter;
    }

    template <typename T>
    static size_t scanExistingSingles(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile,
                                      std::function<void(double)> prog_setter) {
        const u32 val_width = profile.m_scan_a.computeSize();

        T val_a = profile.m_scan_a.get<T>().value_or(T());
        T val_b = profile.m_scan_b.get<T>().value_or(T());

        size_t match_counter = 0;
        size_t sleep_counter = 0;

        const MemScanModel::ScanHistoryEntry &recent_scan = model.getScanHistory();

        size_t row_count = recent_scan.m_scan_results.size();

        if (!model.reserveScan(profile.m_scan_type, sizeof(T), row_count)) {
            return 0;
        }

        if (!model.captureMemForCache()) {
            return 0;
        }

        const MemScanModel::ScanHistoryEntry &current_scan = model.getScanHistory();

        size_t i = 0;
        while (i < row_count) {
            const MemScanResult &result = recent_scan.m_scan_results[i];
            u32 address                 = result.getAddress();

            MetaValue value = GetMetaValueFromMemCache(recent_scan, result);

            T val = value.get<T>().value_or(T());
            T mem_val;

            bool is_match = compareSingle<T>(current_scan.m_scan_buffer, address, val_a, val_b, val,
                                             profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address);
                match_counter += 1;
            }

            // if (sleep_counter++ >= profile.m_sleep_granularity) {
            //     sleep_counter = 0;
            //     std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            // }

            i += 1;

            prog_setter((double)i / (double)row_count);
        }

        return match_counter;
    }

    static bool _MemScanResultCompareByAddress(const MemScanResult &lhs, const MemScanResult &rhs,
                                               ModelSortOrder order) {
        // Sort by address
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs.getAddress() < rhs.getAddress();
        } else {
            return lhs.getAddress() > rhs.getAddress();
        }
    }

    MemScanModel::~MemScanModel() {
        reset();
        m_listeners.clear();
    }

    void MemScanModel::initialize() {
        reset();
        m_listeners.clear();

        m_scanner = make_scoped<MemoryScanner>(this);
    }

    std::any MemScanModel::getData(const ModelIndex &index, int role) const {
        std::scoped_lock lock(m_mutex);
        return getData_(index, role);
    }

    void MemScanModel::setData(const ModelIndex &index, std::any data, int role) {
        std::scoped_lock lock(m_mutex);
        return setData_(index, data, role);
    }

    ModelIndex MemScanModel::getIndex(const u32 &address) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(address);
    }

    ModelIndex MemScanModel::getIndex(const UUID64 &uuid) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(uuid);
    }

    ModelIndex MemScanModel::getIndex(int64_t row, int64_t column, const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(row, column, parent);
    }

    bool MemScanModel::removeIndex(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return removeIndex_(index);
    }

    ModelIndex MemScanModel::getParent(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getParent_(index);
    }

    ModelIndex MemScanModel::getSibling(int64_t row, int64_t column,
                                        const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getSibling_(row, column, index);
    }

    size_t MemScanModel::getColumnCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumnCount_(index);
    }

    size_t MemScanModel::getRowCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRowCount_(index);
    }

    int64_t MemScanModel::getColumn(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumn_(index);
    }

    int64_t MemScanModel::getRow(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRow_(index);
    }

    bool MemScanModel::hasChildren(const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return hasChildren_(parent);
    }

    ScopePtr<MimeData>
    MemScanModel::createMimeData(const std::unordered_set<ModelIndex> &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
    }

    bool MemScanModel::insertMimeData(const ModelIndex &index, const MimeData &data) {
        std::scoped_lock lock(m_mutex);
        return insertMimeData_(index, data);
    }

    std::vector<std::string> MemScanModel::getSupportedMimeTypes() const {
        return std::vector<std::string>();
    }

    bool MemScanModel::canFetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return canFetchMore_(index);
    }

    void MemScanModel::fetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return fetchMore_(index);
    }

    void MemScanModel::reset() {
        std::scoped_lock lock(m_mutex);

        for (size_t i = 0; i < m_history_size; ++i) {
            m_index_map_history[i].m_scan_type = MetaType::UNKNOWN;
            m_index_map_history[i].m_scan_results.clear();
            m_index_map_history[i].m_scan_results.shrink_to_fit();
            m_index_map_history[i].m_scan_buffer.free();
        }

        m_history_size = 0;
    }

    bool MemScanModel::isScanBusy() const { return m_scanner->tIsAlive(); }

    double MemScanModel::getScanProgress() const {
        return isScanBusy() ? m_scanner->getProgress() : 0.0;
    }

    bool MemScanModel::requestScan(u32 search_start, u32 search_size, MetaType val_type,
                                   ScanOperator scan_op, const std::string &a, const std::string &b,
                                   int desired_radix, bool enforce_alignment, bool new_scan,
                                   size_t sleep_granularity, s64 sleep_duration) {
        MetaValue &&ma = MetaValue(val_type), &&mb = MetaValue(val_type);

        switch (val_type) {
        case MetaType::BOOL: {
            std::string lower_a, lower_b;
            lower_a.reserve(a.length());
            lower_b.reserve(b.length());

            std::transform(a.begin(), a.end(), std::back_inserter(lower_a), [](char c) {
                if (std::isalpha(c)) {
                    return (char)std::tolower(c);
                }
                return c;
            });

            std::transform(b.begin(), b.end(), std::back_inserter(lower_b), [](char c) {
                if (std::isalpha(c)) {
                    return (char)std::tolower(c);
                }
                return c;
            });

            ma.set<bool>(lower_a == "1" || lower_a == "true");
            mb.set<bool>(lower_b == "1" || lower_b == "true");
            break;
        }
        case MetaType::S8:
            ma.set<s8>(std::strtol(a.c_str(), nullptr, desired_radix));
            mb.set<s8>(std::strtol(b.c_str(), nullptr, desired_radix));
            break;
        case MetaType::U8:
            ma.set<u8>(std::strtol(a.c_str(), nullptr, desired_radix));
            mb.set<u8>(std::strtol(b.c_str(), nullptr, desired_radix));
            break;
        case MetaType::S16:
            ma.set<s16>(std::strtol(a.c_str(), nullptr, desired_radix));
            mb.set<s16>(std::strtol(b.c_str(), nullptr, desired_radix));
            break;
        case MetaType::U16:
            ma.set<u16>(std::strtol(a.c_str(), nullptr, desired_radix));
            mb.set<u16>(std::strtol(b.c_str(), nullptr, desired_radix));
            break;
        case MetaType::S32:
            ma.set<s32>(std::strtol(a.c_str(), nullptr, desired_radix));
            mb.set<s32>(std::strtol(b.c_str(), nullptr, desired_radix));
            break;
        case MetaType::U32:
            ma.set<u32>(std::strtol(a.c_str(), nullptr, desired_radix));
            mb.set<u32>(std::strtol(b.c_str(), nullptr, desired_radix));
            break;
        case MetaType::F32:
            ma.set<f32>(std::strtof(a.c_str(), nullptr));
            mb.set<f32>(std::strtof(b.c_str(), nullptr));
            break;
        case MetaType::F64:
            ma.set<f64>(std::strtod(a.c_str(), nullptr));
            mb.set<f64>(std::strtod(b.c_str(), nullptr));
            break;
        case MetaType::STRING:
            ma.set<std::string>(a);
            ma.set<std::string>(b);
            break;
        default:
            return false;
        }

        return requestScan(search_start, search_size, val_type, scan_op, std::move(ma),
                           std::move(mb), enforce_alignment, new_scan, sleep_granularity,
                           sleep_duration);
    }

    bool MemScanModel::requestScan(u32 search_start, u32 search_size, MetaType val_type,
                                   ScanOperator scan_op, MetaValue &&a, MetaValue &&b,
                                   bool enforce_alignment, bool new_scan, size_t sleep_granularity,
                                   s64 sleep_duration) {
        m_scan_type = val_type;
        m_scan_size = search_size;

        m_scan_profile.m_search_start      = search_start;
        m_scan_profile.m_search_size       = search_size;
        m_scan_profile.m_scan_type         = val_type;
        m_scan_profile.m_scan_op           = scan_op;
        m_scan_profile.m_scan_a            = std::move(a);
        m_scan_profile.m_scan_b            = std::move(b);
        m_scan_profile.m_enforce_alignment = enforce_alignment;
        m_scan_profile.m_new_scan          = new_scan;
        m_scan_profile.m_sleep_granularity = sleep_granularity;
        m_scan_profile.m_sleep_duration    = sleep_duration;

        m_wants_scan = true;
        m_scanner->tStart(true, &m_scan_profile);
        return true;
    }

    bool MemScanModel::undoScan() {
        if (!canUndoScan()) {
            return false;
        }

        std::scoped_lock<std::mutex> lock(m_mutex);

        ScanHistoryEntry &entry = m_index_map_history[m_history_size - 1];
        entry.m_scan_results.clear();
        entry.m_scan_results.shrink_to_fit();
        entry.m_scan_buffer.free();
        m_history_size--;
        return true;
    }

    void MemScanModel::addEventListener(UUID64 uuid, event_listener_t listener,
                                        MemScanModelEventFlags flags) {
        m_listeners[uuid] = {listener, flags};
    }

    void MemScanModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    Result<void, SerialError> MemScanModel::serialize(Serializer &out) const {
        std::scoped_lock lock(m_mutex);

        out.write<u32>(static_cast<u32>(m_scan_type));
        out.write<u32>(static_cast<u32>(m_scan_size));

        if (m_history_size == 0) {
            out.write<u32>(0);  // Empty size
            return Result<void, SerialError>();
        }

        ScanHistoryEntry &newest_scan = m_index_map_history[m_history_size - 1];

        out.write<u32>(static_cast<u32>(newest_scan.m_scan_results.size()));
        for (const MemScanResult &result : newest_scan.m_scan_results) {
            out.write<u32>(result.getAddress());
        }

        return Result<void, SerialError>();
    }

    Result<void, SerialError> MemScanModel::deserialize(Deserializer &in) {
        reset();
        {
            std::scoped_lock lock(m_mutex);
            m_scan_type = static_cast<MetaType>(in.read<u32>());
            m_scan_size = in.read<u32>();
        }

        const u32 count = in.read<u32>();
        if (count == 0) {
            return Result<void, SerialError>();
        }

        if (!reserveScan(m_scan_type, m_scan_size, count)) {
            return make_serial_error<void>(in, "Failed to reserve memory for the scan data.");
        }

        if (!captureMemForCache()) {
            reset();
            return make_serial_error<void>(in, "Failed to capture the current memory frame.");
        }

        for (int i = 0; i < count; ++i) {
            u32 address = in.read<u32>();
            makeScanIndex(address);
        }

        return Result<void, SerialError>();
    }

    std::any MemScanModel::getData_(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        if (m_history_size == 0) {
            return {};
        }

        MemScanResult *result = index.data<MemScanResult>();
        if (!result) {
            return {};
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY: {
            std::string disp_name = std::format("{:08X}", result->getAddress());
            return disp_name;
        }
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return {};
        }
        case MemScanRole::MEMSCAN_ROLE_ADDRESS: {
            return static_cast<u32>(result->getAddress());
        }
        case MemScanRole::MEMSCAN_ROLE_SIZE: {
            return m_scan_size;
        }
        case MemScanRole::MEMSCAN_ROLE_TYPE: {
            const ScanHistoryEntry &entry = getScanHistory(result->getHistoryIndex());
            return entry.m_scan_type;
        }
        case MemScanRole::MEMSCAN_ROLE_VALUE: {
            const ScanHistoryEntry &entry = getScanHistory(result->getHistoryIndex());
            return GetMetaValueFromMemCache(entry, *result);
        }
        case MemScanRole::MEMSCAN_ROLE_VALUE_MEM: {
            return getMetaValueFromMemory(index);
        }
        default:
            return {};
        }
    }

    void MemScanModel::setData_(const ModelIndex &index, std::any data, int role) {
        if (!validateIndex(index)) {
            return;
        }

        MemScanResult *result = index.data<MemScanResult>();
        if (!result) {
            return;
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY: {
            return;
        }
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return;
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return;
        }
        case MemScanRole::MEMSCAN_ROLE_ADDRESS: {
            return;
        }
        case MemScanRole::MEMSCAN_ROLE_SIZE: {
            return;
        }
        case MemScanRole::MEMSCAN_ROLE_TYPE: {
            return;
        }
        case MemScanRole::MEMSCAN_ROLE_VALUE: {
            return;
        }
        default:
            return;
        }
    }

    ModelIndex MemScanModel::getIndex_(const u32 &address) const {
        if (m_history_size == 0) {
            return ModelIndex();
        }

        const ScanHistoryEntry &recent_scan = m_index_map_history[m_history_size - 1];

        auto it = std::lower_bound(
            recent_scan.m_scan_results.begin(), recent_scan.m_scan_results.end(), address,
            [](const MemScanResult &it, u32 address) { return it.getAddress() < address; });
        if (it == recent_scan.m_scan_results.end()) {
            return ModelIndex();
        }

        MemScanResult &lhs = *it;
        if (lhs.getAddress() != address) {
            return ModelIndex();
        }

        ModelIndex index(getUUID());
        index.setData(std::addressof(lhs));
        return index;
    }

    ModelIndex MemScanModel::getIndex_(int64_t row, int64_t column,
                                       const ModelIndex &parent) const {
        if (row < 0 || column < 0) {
            return ModelIndex();
        }

        if (validateIndex(parent)) {
            return ModelIndex();
        }

        if (m_history_size == 0) {
            return ModelIndex();
        }

        const ScanHistoryEntry &recent_scan = m_index_map_history[m_history_size - 1];

        if (row >= recent_scan.m_scan_results.size()) {
            return ModelIndex();
        }

        ModelIndex index(getUUID());
        index.setData(&recent_scan.m_scan_results[row]);
        return index;
    }

    bool MemScanModel::removeIndex_(const ModelIndex &index) {
        if (m_history_size == 0) {
            return false;
        }

        MemScanResult *result = index.data<MemScanResult>();
        if (!result) {
            return false;
        }

        ScanHistoryEntry &recent_scan = m_index_map_history[m_history_size - 1];
        return std::erase(recent_scan.m_scan_results, *result) > 0;
    }

    ModelIndex MemScanModel::getParent_(const ModelIndex &index) const { return ModelIndex(); }

    ModelIndex MemScanModel::getSibling_(int64_t row, int64_t column,
                                         const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }
        return getIndex_(row, column, ModelIndex());
    }

    size_t MemScanModel::getColumnCount_(const ModelIndex &index) const { return 1; }

    size_t MemScanModel::getRowCount_(const ModelIndex &index) const {
        if (validateIndex(index)) {
            return 0;
        }

        if (m_history_size == 0) {
            return 0;
        }

        const ScanHistoryEntry &recent_scan = m_index_map_history[m_history_size - 1];

        return recent_scan.m_scan_results.size();
    }

    int64_t MemScanModel::getColumn_(const ModelIndex &index) const {
        return validateIndex(index) ? 0 : -1;
    }

    int64_t MemScanModel::getRow_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return -1;
        }

        ModelIndex parent = getParent_(index);
        if (validateIndex(parent)) {
            return -1;
        }

        if (m_history_size == 0) {
            return -1;
        }

        const ScanHistoryEntry &recent_scan = m_index_map_history[m_history_size - 1];
        const MemScanResult *index_scan     = index.data<MemScanResult>();
        if (!index_scan) {
            return -1;
        }

        auto it =
            std::lower_bound(recent_scan.m_scan_results.begin(), recent_scan.m_scan_results.end(),
                             *index_scan, [](const MemScanResult &it, const MemScanResult &val) {
                                 return it.getAddress() < val.getAddress();
                             });
        if (it == recent_scan.m_scan_results.end()) {
            return -1;
        }

        // Make sure we have a real match, since lower_bound may return the closest element.
        const MemScanResult &lhs = *it;
        const MemScanResult &rhs = *index.data<MemScanResult>();
        if (lhs.getAddress() != rhs.getAddress()) {
            return -1;
        }

        int64_t row = std::distance(recent_scan.m_scan_results.begin(), it);
        return row;
    }

    bool MemScanModel::hasChildren_(const ModelIndex &parent) const { return false; }

    ScopePtr<MimeData>
    MemScanModel::createMimeData_(const std::unordered_set<ModelIndex> &indexes) const {
        TOOLBOX_ERROR("[MemScanModel] Mimedata unimplemented!");
        return ScopePtr<MimeData>();
    }

    bool MemScanModel::insertMimeData_(const ModelIndex &index, const MimeData &data) {
        return false;
    }

    bool MemScanModel::canFetchMore_(const ModelIndex &index) { return false; }

    void MemScanModel::fetchMore_(const ModelIndex &index) { return; }

    MetaValue MemScanModel::getMetaValueFromMemory(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return MetaValue(MetaType::UNKNOWN);
        }

        MemScanResult *result = index.data<MemScanResult>();
        if (!result) {
            return MetaValue(MetaType::UNKNOWN);
        }

        const ScanHistoryEntry &this_scan = getScanHistory(result->getHistoryIndex());

        DolphinHookManager &manager = DolphinHookManager::instance();
        Buffer mem_buf;
        mem_buf.setBuf(manager.getMemoryView(), manager.getMemorySize());

        u32 address = result->getAddress();
        switch (this_scan.m_scan_type) {
        case MetaType::BOOL:
            return MetaValue(readSingle<bool>(mem_buf, address));
        case MetaType::S8:
            return MetaValue(readSingle<s8>(mem_buf, address));
        case MetaType::U8:
            return MetaValue(readSingle<u8>(mem_buf, address));
        case MetaType::S16:
            return MetaValue(readSingle<s16>(mem_buf, address));
        case MetaType::U16:
            return MetaValue(readSingle<u16>(mem_buf, address));
        case MetaType::S32:
            return MetaValue(readSingle<s32>(mem_buf, address));
        case MetaType::U32:
            return MetaValue(readSingle<u32>(mem_buf, address));
        case MetaType::F32:
            return MetaValue(readSingle<f32>(mem_buf, address));
        case MetaType::F64:
            return MetaValue(readSingle<f64>(mem_buf, address));
        case MetaType::STRING:
            return MetaValue(readString(mem_buf, address, this_scan.m_scan_size));
        case MetaType::UNKNOWN:
            return MetaValue(readBytes(mem_buf, address, this_scan.m_scan_size));
        default:
            return MetaValue(MetaType::UNKNOWN);
        }
    }

    void MemScanModel::makeScanIndex(u32 address) {
        if (m_history_size == 0) {
            return;
        }

        ScanHistoryEntry &recent_scan = m_index_map_history[m_history_size - 1];

        {
            std::scoped_lock lock(m_mutex);
            recent_scan.m_scan_results.emplace_back(address, m_history_size - 1);
        }
    }

    bool MemScanModel::captureMemForCache() {
        if (m_history_size == 0) {
            return false;
        }

        DolphinHookManager &manager = DolphinHookManager::instance();

        ScanHistoryEntry &entry = m_index_map_history[m_history_size - 1];

        Buffer the_buf;
        the_buf.setBuf(manager.getMemoryView(), manager.getMemorySize());

        return the_buf.copyTo(entry.m_scan_buffer);
    }

    const MemScanModel::ScanHistoryEntry &MemScanModel::getScanHistory() const {
        static ScanHistoryEntry empty_set = {};
        if (m_history_size == 0) {
            return empty_set;
        }
        return m_index_map_history[m_history_size - 1];
    }

    const MemScanModel::ScanHistoryEntry &MemScanModel::getScanHistory(size_t i) const {
        static ScanHistoryEntry empty_set = {};
        if (i >= m_history_size) {
            return empty_set;
        }
        return m_index_map_history[i];
    }

    size_t MemScanModel::pollChildren(const ModelIndex &index) const { return 0; }

    void MemScanModel::signalEventListeners(const ModelIndex &index, MemScanModelEventFlags flags) {
        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != MemScanModelEventFlags::NONE) {
                listener.first(index, flags);
            }
        }
    }

    size_t MemoryScanner::tRun(void *param) {
        const auto *profile = static_cast<const MemScanModel::MemScanProfile *>(param);
        if (!profile) {
            return 0;
        }

        size_t results = 0;
        while (!tIsSignalKill()) {
            results = searchAddressSpan(*profile);
            break;
        }

        return results;
    }

    size_t MemoryScanner::searchAddressSpan(const MemScanModel::MemScanProfile &profile) {
        setProgress(0.0);

        if (!m_scan_model) {
            return 0;
        }

        if (profile.m_new_scan) {
            m_scan_model->reset();

            switch (profile.m_scan_type) {
            case MetaType::BOOL:
                return scanAllBools(*m_scan_model, profile);
            case MetaType::S8:
                return scanAllS8s(*m_scan_model, profile);
            case MetaType::U8:
                return scanAllU8s(*m_scan_model, profile);
            case MetaType::S16:
                return scanAllS16s(*m_scan_model, profile);
            case MetaType::U16:
                return scanAllU16s(*m_scan_model, profile);
            case MetaType::S32:
                return scanAllS32s(*m_scan_model, profile);
            case MetaType::U32:
                return scanAllU32s(*m_scan_model, profile);
            case MetaType::F32:
                return scanAllF32s(*m_scan_model, profile);
            case MetaType::F64:
                return scanAllF64s(*m_scan_model, profile);
            case MetaType::STRING:
                return scanAllStrings(*m_scan_model, profile);
            case MetaType::UNKNOWN:
                return scanAllByteArrays(*m_scan_model, profile);
            default:
                return 0;
            }
        } else {
            switch (profile.m_scan_type) {
            case MetaType::BOOL:
                return scanExistingBools(*m_scan_model, profile);
            case MetaType::S8:
                return scanExistingS8s(*m_scan_model, profile);
            case MetaType::U8:
                return scanExistingU8s(*m_scan_model, profile);
            case MetaType::S16:
                return scanExistingS16s(*m_scan_model, profile);
            case MetaType::U16:
                return scanExistingU16s(*m_scan_model, profile);
            case MetaType::S32:
                return scanExistingS32s(*m_scan_model, profile);
            case MetaType::U32:
                return scanExistingU32s(*m_scan_model, profile);
            case MetaType::F32:
                return scanExistingF32s(*m_scan_model, profile);
            case MetaType::F64:
                return scanExistingF64s(*m_scan_model, profile);
            case MetaType::STRING:
                return scanExistingStrings(*m_scan_model, profile);
            case MetaType::UNKNOWN:
                return scanExistingByteArrays(*m_scan_model, profile);
            default:
                return 0;
            }
        }
    }

    size_t MemoryScanner::scanAllBools(MemScanModel &model,
                                       const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<bool>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllS8s(MemScanModel &model,
                                     const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<s8>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllU8s(MemScanModel &model,
                                     const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<u8>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllS16s(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<s16>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllU16s(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<u16>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllS32s(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<s32>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllU32s(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<u32>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllF32s(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<f32>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllF64s(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile) {
        return scanAllSingles<f64>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanAllStrings(MemScanModel &model,
                                         const MemScanModel::MemScanProfile &profile) {
        const u32 begin_address = profile.m_search_start;
        const u32 end_address   = begin_address + profile.m_search_size;

        if (profile.m_search_size == 0 || begin_address < 0x80000000 ||
            end_address >= (0x80000000 | DolphinHookManager::instance().getMemorySize())) {
            return 0;
        }

        std::string val_a = profile.m_scan_a.get<std::string>().value_or(std::string());
        if (val_a.empty()) {
            return 0;
        }

        // Reserve an estimate allocation that should be sane.
        size_t O_estimate = profile.m_search_size / 1000;

        if (!model.reserveScan(profile.m_scan_type, val_a.size(), O_estimate)) {
            return 0;
        }

        if (!model.captureMemForCache()) {
            return 0;
        }

        const MemScanModel::ScanHistoryEntry &entry = model.getScanHistory();

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;
        while (address < end_address) {
            std::string mem_val;

            bool is_match =
                compareString(entry.m_scan_buffer, address, val_a, profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address);
                match_counter += 1;
            }

            address += 1;

            setProgress((double)(address - begin_address) / (double)profile.m_search_size);

            // if (sleep_counter++ >= profile.m_sleep_granularity) {
            //     sleep_counter = 0;
            //     std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            // }
        }

        return match_counter;
    }

    size_t MemoryScanner::scanAllByteArrays(MemScanModel &model,
                                            const MemScanModel::MemScanProfile &profile) {
        const u32 begin_address = profile.m_search_start;
        const u32 end_address   = begin_address + profile.m_search_size;

        if (profile.m_search_size == 0 || begin_address < 0x80000000 ||
            end_address >= (0x80000000 | DolphinHookManager::instance().getMemorySize())) {
            return 0;
        }

        auto result = profile.m_scan_a.get<Buffer>();
        if (!result.has_value()) {
            return 0;
        }

        const Buffer &val_a = result.value();

        // Reserve an estimate allocation that should be sane.
        size_t O_estimate = profile.m_search_size / 1000;

        if (!model.reserveScan(profile.m_scan_type, val_a.size(), O_estimate)) {
            return 0;
        }

        if (!model.captureMemForCache()) {
            return 0;
        }

        const MemScanModel::ScanHistoryEntry &entry = model.getScanHistory();

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;
        while (address < end_address) {
            Buffer mem_val;

            bool is_match =
                compareBytes(entry.m_scan_buffer, address, val_a, profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address);
                match_counter += 1;
            }

            address += 1;

            setProgress((double)(address - begin_address) / (double)profile.m_search_size);

            // if (sleep_counter++ >= profile.m_sleep_granularity) {
            //     sleep_counter = 0;
            //     std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            // }
        }

        return match_counter;
    }

    size_t MemoryScanner::scanExistingBools(MemScanModel &model,
                                            const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<bool>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingS8s(MemScanModel &model,
                                          const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<s8>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingU8s(MemScanModel &model,
                                          const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<u8>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingS16s(MemScanModel &model,
                                           const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<s16>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingU16s(MemScanModel &model,
                                           const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<u16>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingS32s(MemScanModel &model,
                                           const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<s32>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingU32s(MemScanModel &model,
                                           const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<u32>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingF32s(MemScanModel &model,
                                           const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<f32>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingF64s(MemScanModel &model,
                                           const MemScanModel::MemScanProfile &profile) {
        return scanExistingSingles<f64>(model, profile, [this](double p) { setProgress(p); });
    }

    size_t MemoryScanner::scanExistingStrings(MemScanModel &model,
                                              const MemScanModel::MemScanProfile &profile) {
        const u32 begin_address = profile.m_search_start;
        const u32 end_address   = begin_address + profile.m_search_size;

        if (profile.m_search_size == 0 || begin_address < 0x80000000 ||
            end_address >= (0x80000000 | DolphinHookManager::instance().getMemorySize())) {
            return {};
        }

        std::string val_a = profile.m_scan_a.get<std::string>().value_or(std::string());
        if (val_a.empty()) {
            return {};
        }

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;

        const MemScanModel::ScanHistoryEntry &recent_scan = model.getScanHistory();

        size_t row_count = recent_scan.m_scan_results.size();

        if (!model.reserveScan(profile.m_scan_type, val_a.size(), row_count)) {
            return 0;
        }

        if (!model.captureMemForCache()) {
            return 0;
        }

        const MemScanModel::ScanHistoryEntry &current_scan = model.getScanHistory();

        size_t i = 0;
        while (i < row_count) {
            const MemScanResult &result = recent_scan.m_scan_results[i];
            u32 address                 = result.getAddress();

            MetaValue value = GetMetaValueFromMemCache(recent_scan, result);

            std::string mem_val;

            bool is_match = compareString(current_scan.m_scan_buffer, address, val_a,
                                          profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address);
                match_counter += 1;
            }

            // if (sleep_counter++ >= profile.m_sleep_granularity) {
            //     sleep_counter = 0;
            //     std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            // }

            i += 1;

            setProgress((double)i / (double)row_count);
        }

        return match_counter;
    }

    size_t MemoryScanner::scanExistingByteArrays(MemScanModel &model,
                                                 const MemScanModel::MemScanProfile &profile) {
        const u32 begin_address = profile.m_search_start;
        const u32 end_address   = begin_address + profile.m_search_size;

        if (profile.m_search_size == 0 || begin_address < 0x80000000 ||
            end_address >= (0x80000000 | DolphinHookManager::instance().getMemorySize())) {
            return {};
        }

        auto result = profile.m_scan_a.get<Buffer>();
        if (!result.has_value()) {
            return 0;
        }

        const Buffer &val_a = result.value();

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;

        const MemScanModel::ScanHistoryEntry &recent_scan = model.getScanHistory();

        size_t row_count = recent_scan.m_scan_results.size();

        if (!model.reserveScan(profile.m_scan_type, val_a.size(), row_count)) {
            return 0;
        }

        if (!model.captureMemForCache()) {
            return 0;
        }

        const MemScanModel::ScanHistoryEntry &current_scan = model.getScanHistory();

        size_t i = 0;
        while (i < row_count) {
            const MemScanResult &result = recent_scan.m_scan_results[i];
            u32 address                 = result.getAddress();

            MetaValue value = GetMetaValueFromMemCache(recent_scan, result);

            Buffer mem_val;

            bool is_match = compareBytes(current_scan.m_scan_buffer, address, val_a,
                                         profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address);
                match_counter += 1;
            }

            if (sleep_counter++ >= profile.m_sleep_granularity) {
                sleep_counter = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            }

            i += 1;

            setProgress((double)i / (double)row_count);
        }

        return match_counter;
    }

}  // namespace Toolbox