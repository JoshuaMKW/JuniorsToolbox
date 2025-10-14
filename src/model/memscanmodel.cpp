#include <algorithm>
#include <any>
#include <compare>
#include <set>

#include "dolphin/watch.hpp"
#include "gui/application.hpp"
#include "model/memscanmodel.hpp"

using namespace Toolbox::Object;

namespace Toolbox {

    template <typename T> static T readSingle(u32 address) {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                      "This method requires simple data");

        T mem_val;
        DolphinHookManager &manager = DolphinHookManager::instance();
        manager.readBytes(reinterpret_cast<char *>(&mem_val), address, sizeof(T));

        if constexpr (std::is_integral_v<T>) {
            mem_val = std::byteswap(mem_val);
        } else if constexpr (std::is_same_v<T, f32>) {
            mem_val = std::bit_cast<T>(std::byteswap(std::bit_cast<u32>(mem_val)));
        } else if constexpr (std::is_same_v<T, f64>) {
            mem_val = std::bit_cast<T>(std::byteswap(std::bit_cast<u64>(mem_val)));
        }

        return mem_val;
    }

    static std::string readString(u32 address, size_t val_size) {
        DolphinHookManager &manager = DolphinHookManager::instance();
        const char *mem_handle =
            static_cast<const char *>(manager.getMemoryView()) + (address & 0x7FFFFFFF);

        return std::string(mem_handle, val_size);
    }

    static Buffer readBytes(u32 address, size_t val_size) {
        DolphinHookManager &manager = DolphinHookManager::instance();
        const char *mem_handle =
            static_cast<const char *>(manager.getMemoryView()) + (address & 0x7FFFFFFF);

        Buffer buf;
        buf.alloc(val_size);
        std::memcpy(buf.buf(), mem_handle, val_size);

        return buf;
    }

    template <typename T>
    static bool compareSingle(u32 address, const T &a, const T &b, const T &last,
                              MemScanModel::ScanOperator op, T *out) {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>,
                      "This method requires simple data");

        T mem_val;
        DolphinHookManager &manager = DolphinHookManager::instance();
        manager.readBytes(reinterpret_cast<char *>(&mem_val), address, sizeof(T));

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

    static bool compareString(u32 address, const std::string &a, MemScanModel::ScanOperator op,
                              std::string *out) {
        if (op != MemScanModel::ScanOperator::OP_EXACT) {
            return false;
        }

        DolphinHookManager &manager = DolphinHookManager::instance();
        const char *mem_handle =
            static_cast<const char *>(manager.getMemoryView()) + (address & 0x7FFFFFFF);
        size_t mem_size = manager.getMemorySize();

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

    static bool compareBytes(u32 address, const Buffer &a, MemScanModel::ScanOperator op,
                             Buffer *out) {
        if (op != MemScanModel::ScanOperator::OP_EXACT) {
            return false;
        }

        DolphinHookManager &manager = DolphinHookManager::instance();
        const char *mem_handle =
            static_cast<const char *>(manager.getMemoryView()) + (address & 0x7FFFFFFF);
        size_t mem_size = manager.getMemorySize();

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
    static size_t scanAllSingles(MemScanModel &model, const MemScanModel::MemScanProfile &profile) {
        const u32 begin_address = profile.m_search_start;
        const u32 end_address   = begin_address + profile.m_search_size;

        if (profile.m_search_size == 0 || begin_address < 0x80000000 ||
            end_address > (0x80000000 | DolphinHookManager::instance().getMemorySize())) {
            return {};
        }

        const u32 val_width = profile.m_scan_a.computeSize();
        const u32 addr_mask = profile.m_enforce_alignment ? ~(std::min<u32>(val_width, 4) - 1) : ~0;
        const u32 addr_inc  = profile.m_enforce_alignment ? std::min<u32>(val_width, 4) : 1;

        // Reserve an estimate allocation that should be sane.
        size_t O_estimate = profile.m_search_size / 4;
        if (profile.m_enforce_alignment) {
            O_estimate /=
                val_width;  // Since alignment is forced, each entry has to be n bytes apart.
        }

        model.reserveScan(O_estimate);

        T val_a = profile.m_scan_a.get<T>().value_or(T());
        T val_b = profile.m_scan_b.get<T>().value_or(T());

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;
        while (address < end_address) {
            address &= addr_mask;

            T mem_val;
            bool is_match =
                compareSingle<T>(address, val_a, val_b, T() /*this is the previous scan*/,
                                 profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address, MetaValue(mem_val));
                match_counter += 1;
            }

            address += addr_inc;

            if (sleep_counter++ >= profile.m_sleep_granularity) {
                sleep_counter = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            }
        }

        return match_counter;
    }

    static size_t scanAllStrings(MemScanModel &model, const MemScanModel::MemScanProfile &profile) {
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

        // Reserve an estimate allocation that should be sane.
        size_t O_estimate = profile.m_search_size / 1000;
        model.reserveScan(O_estimate);

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;
        while (address < end_address) {
            std::string mem_val;

            bool is_match = compareString(address, val_a, profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address, MetaValue(mem_val));
                match_counter += 1;
            }

            address += 1;

            if (sleep_counter++ >= profile.m_sleep_granularity) {
                sleep_counter = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            }
        }

        return match_counter;
    }

    static size_t scanAllByteArrays(MemScanModel &model,
                                    const MemScanModel::MemScanProfile &profile) {
        const u32 begin_address = profile.m_search_start;
        const u32 end_address   = begin_address + profile.m_search_size;

        if (profile.m_search_size == 0 || begin_address < 0x80000000 ||
            end_address >= (0x80000000 | DolphinHookManager::instance().getMemorySize())) {
            return {};
        }

        // Reserve an estimate allocation that should be sane.
        size_t O_estimate = profile.m_search_size / 1000;
        model.reserveScan(O_estimate);

        Buffer val_a = profile.m_scan_a.get<Buffer>().value_or(Buffer());

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;
        while (address < end_address) {
            Buffer mem_val;

            bool is_match = compareBytes(address, val_a, profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address, MetaValue(mem_val));
                match_counter += 1;
            }

            address += 1;

            if (sleep_counter++ >= profile.m_sleep_granularity) {
                sleep_counter = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            }
        }

        return match_counter;
    }

    template <typename T>
    static size_t scanExistingSingles(MemScanModel &model,
                                      const MemScanModel::MemScanProfile &profile) {
        const u32 val_width = profile.m_scan_a.computeSize();

        T val_a = profile.m_scan_a.get<T>().value_or(T());
        T val_b = profile.m_scan_b.get<T>().value_or(T());

        size_t match_counter = 0;
        size_t sleep_counter = 0;

        const std::vector<ModelIndex> recent_scan = model.getScanHistory();

        size_t row_count = recent_scan.size();
        model.reserveScan(row_count);

        size_t i         = 0;
        while (i < row_count) {
            ModelIndex index = recent_scan[i];
            u32 address      = model.getScanAddress(index);
            MetaValue value  = model.getScanValue(index);

            T val = value.get<T>().value_or(T());
            T mem_val;

            bool is_match =
                compareSingle<T>(address, val_a, val_b, val, profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address, MetaValue(mem_val));
                match_counter += 1;
            }

            if (sleep_counter++ >= profile.m_sleep_granularity) {
                sleep_counter = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            }

            i += 1;
        }

        return match_counter;
    }

    static size_t scanExistingStrings(MemScanModel &model,
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

        size_t row_count = model.getRowCount(ModelIndex());
        model.reserveScan(row_count);

        size_t i         = 0;
        while (i < row_count) {
            ModelIndex index = model.getIndex(i, 0);
            u32 address      = model.getScanAddress(index);
            MetaValue value  = model.getScanValue(index);

            std::string mem_val;

            bool is_match = compareString(address, val_a, profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address, MetaValue(mem_val));
                match_counter += 1;
            }

            if (sleep_counter++ >= profile.m_sleep_granularity) {
                sleep_counter = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            }

            i += 1;
        }

        return match_counter;
    }

    static size_t scanExistingByteArrays(MemScanModel &model,
                                         const MemScanModel::MemScanProfile &profile) {
        const u32 begin_address = profile.m_search_start;
        const u32 end_address   = begin_address + profile.m_search_size;

        if (profile.m_search_size == 0 || begin_address < 0x80000000 ||
            end_address >= (0x80000000 | DolphinHookManager::instance().getMemorySize())) {
            return {};
        }

        Buffer val_a = profile.m_scan_a.get<Buffer>().value_or(Buffer());

        u32 address = begin_address;

        size_t match_counter = 0;
        size_t sleep_counter = 0;

        size_t row_count = model.getRowCount(ModelIndex());
        model.reserveScan(row_count);

        size_t i         = 0;
        while (i < row_count) {
            ModelIndex index = model.getIndex(i, 0);
            u32 address      = model.getScanAddress(index);
            MetaValue value  = model.getScanValue(index);

            Buffer mem_val;

            bool is_match = compareBytes(address, val_a, profile.m_scan_op, &mem_val);
            if (is_match) {
                model.makeScanIndex(address, MetaValue(mem_val));
                match_counter += 1;
            }

            if (sleep_counter++ >= profile.m_sleep_granularity) {
                sleep_counter = 0;
                std::this_thread::sleep_for(std::chrono::milliseconds(profile.m_sleep_duration));
            }

            i += 1;
        }

        return match_counter;
    }

    static bool _MemScanResultCompareByAddress(const MemScanResult &lhs, const MemScanResult &rhs,
                                               ModelSortOrder order) {
        // Sort by address
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs.m_address < rhs.m_address;
        } else {
            return lhs.m_address > rhs.m_address;
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

    ScopePtr<MimeData> MemScanModel::createMimeData(const std::vector<ModelIndex> &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
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

        for (const std::vector<ModelIndex> &index_map : m_index_map_history) {
            for (const ModelIndex &a : index_map) {
                MemScanResult *p = a.data<MemScanResult>();
                if (p) {
                    delete p;
                }
            }
        }

        m_index_map_history.clear();
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
        m_index_map_history.pop_back();
        return true;
    }

    void MemScanModel::addEventListener(UUID64 uuid, event_listener_t listener,
                                        MemScanModelEventFlags flags) {
        m_listeners[uuid] = {listener, flags};
    }

    void MemScanModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    Result<void, SerialError> MemScanModel::serialize(Serializer &out) const {
        std::scoped_lock lock(m_mutex);

        if (m_index_map_history.empty()) {
            return Result<void, SerialError>();
        }

        const std::vector<ModelIndex> &newest_scan = m_index_map_history.back();

        out.write<u32>(static_cast<u32>(m_scan_type));
        out.write<u32>(static_cast<u32>(newest_scan.size()));

        for (const ModelIndex &index : newest_scan) {
            MemScanResult *result = index.data<MemScanResult>();

            out.write<UUID64>(index.getUUID());
            out.write<u32>(result->m_address);
            result->m_scanned_value.serialize(out);
        }

        return Result<void, SerialError>();
    }

    Result<void, SerialError> MemScanModel::deserialize(Deserializer &in) {
        reset();
        {
            std::scoped_lock lock(m_mutex);
            m_scan_type = static_cast<MetaType>(in.read<u32>());
        }

        const u32 count = in.read<u32>();

        std::vector<ModelIndex> the_scan;
        the_scan.reserve(count);

        for (int i = 0; i < count; ++i) {
            UUID64 uuid = in.read<UUID64>();

            ModelIndex index = ModelIndex(getUUID(), uuid);

            u32 address  = in.read<u32>();
            MetaValue mv = MetaValue(MetaType::UNKNOWN);
            mv.deserialize(in);

            MemScanResult *result = new MemScanResult{address, std::move(mv), 0};
            index.setData(result);

            std::scoped_lock lock(m_mutex);
            the_scan.emplace_back(std::move(index));
        }

        m_index_map_history.emplace_back(std::move(the_scan));

        return Result<void, SerialError>();
    }

    std::any MemScanModel::getData_(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        MemScanResult *result = index.data<MemScanResult>();
        if (!result) {
            return {};
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY: {
            std::string disp_name = std::format("{:08X}", result->m_address);
            return disp_name;
        }
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return {};
        }
        case MemScanRole::MEMSCAN_ROLE_ADDRESS: {
            return static_cast<u32>(result->m_address);
        }
        case MemScanRole::MEMSCAN_ROLE_SIZE: {
            return m_scan_size;
        }
        case MemScanRole::MEMSCAN_ROLE_TYPE: {
            return result->m_scanned_value.type();
        }
        case MemScanRole::MEMSCAN_ROLE_VALUE: {
            return result->m_scanned_value;
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
            result->m_address = std::any_cast<u32>(data);
            return;
        }
        case MemScanRole::MEMSCAN_ROLE_SIZE: {
            return;
        }
        case MemScanRole::MEMSCAN_ROLE_TYPE: {
            return;
        }
        case MemScanRole::MEMSCAN_ROLE_VALUE: {
            result->m_scanned_value = std::any_cast<MetaValue>(data);
            return;
        }
        default:
            return;
        }
    }

    ModelIndex MemScanModel::getIndex_(const u32 &address) const {
        if (m_index_map_history.empty()) {
            return ModelIndex();
        }

        const std::vector<ModelIndex> &recent_scan = m_index_map_history.back();

        auto it = std::lower_bound(recent_scan.begin(), recent_scan.end(), address,
                                   [](const ModelIndex &it, u32 address) {
                                       MemScanResult *lhs = it.data<MemScanResult>();
                                       return lhs->m_address < address;
                                   });
        if (it == recent_scan.end()) {
            return ModelIndex();
        }

        MemScanResult *lhs = it->data<MemScanResult>();
        if (lhs->m_address != address) {
            return ModelIndex();
        }

        return *it;
    }

    ModelIndex MemScanModel::getIndex_(const UUID64 &uuid) const {
        if (m_index_map_history.empty()) {
            return ModelIndex();
        }

        const std::vector<ModelIndex> &recent_scan = m_index_map_history.back();

        auto it =
            std::find_if(recent_scan.begin(), recent_scan.end(),
                         [&](const ModelIndex &it) { return it.getUUID() == uuid; });
        if (it == recent_scan.end()) {
            return ModelIndex();
        }

        return *it;
    }

    ModelIndex MemScanModel::getIndex_(int64_t row, int64_t column,
                                       const ModelIndex &parent) const {
        if (row < 0 || column < 0) {
            return ModelIndex();
        }

        if (validateIndex(parent)) {
            return ModelIndex();
        }

        if (m_index_map_history.empty()) {
            return ModelIndex();
        }

        const std::vector<ModelIndex> &recent_scan = m_index_map_history.back();

        if (row >= recent_scan.size()) {
            return ModelIndex();
        }

        return recent_scan[row];
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

        if (m_index_map_history.empty()) {
            return 0;
        }

        const std::vector<ModelIndex> &recent_scan = m_index_map_history.back();

        return recent_scan.size();
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

        if (m_index_map_history.empty()) {
            return -1;
        }

        const std::vector<ModelIndex> &recent_scan = m_index_map_history.back();

        auto it = std::lower_bound(recent_scan.begin(), recent_scan.end(), index,
                                   [](const ModelIndex &it, const ModelIndex &val) {
                                       MemScanResult *lhs = it.data<MemScanResult>();
                                       MemScanResult *rhs = val.data<MemScanResult>();
                                       return lhs->m_address < rhs->m_address;
                                   });
        if (it == recent_scan.end()) {
            return -1;
        }

        // Make sure we have a real match, since lower_bound may return the closest element.
        MemScanResult *lhs = it->data<MemScanResult>();
        MemScanResult *rhs = index.data<MemScanResult>();
        if (lhs->m_address != rhs->m_address) {
            return -1;
        }

        int64_t row = std::distance(recent_scan.begin(), it);
        return row;
    }

    bool MemScanModel::hasChildren_(const ModelIndex &parent) const { return false; }

    ScopePtr<MimeData> MemScanModel::createMimeData_(const std::vector<ModelIndex> &indexes) const {
        TOOLBOX_ERROR("[MemScanModel] Mimedata unimplemented!");
        return ScopePtr<MimeData>();
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

        u32 address = result->m_address;
        switch (result->m_scanned_value.type()) {
        case MetaType::BOOL:
            return MetaValue(readSingle<bool>(address));
        case MetaType::S8:
            return MetaValue(readSingle<s8>(address));
        case MetaType::U8:
            return MetaValue(readSingle<u8>(address));
        case MetaType::S16:
            return MetaValue(readSingle<s16>(address));
        case MetaType::U16:
            return MetaValue(readSingle<u16>(address));
        case MetaType::S32:
            return MetaValue(readSingle<s32>(address));
        case MetaType::U32:
            return MetaValue(readSingle<u32>(address));
        case MetaType::F32:
            return MetaValue(readSingle<f32>(address));
        case MetaType::F64:
            return MetaValue(readSingle<f64>(address));
        case MetaType::STRING:
            return MetaValue(readString(address, result->m_scanned_value.computeSize()));
        case MetaType::UNKNOWN:
            return MetaValue(readBytes(address, result->m_scanned_value.computeSize()));
        default:
            return MetaValue(MetaType::UNKNOWN);
        }
    }

    void MemScanModel::makeScanIndex(u32 address, MetaValue &&value) {
        if (m_index_map_history.empty()) {
            return;
        }

        std::vector<ModelIndex> &recent_scan = m_index_map_history.back();

        ModelIndex new_index(getUUID());

        MemScanResult *result = new MemScanResult{address, std::move(value)};
        new_index.setData(result);

        {
            std::scoped_lock lock(m_mutex);
            recent_scan.emplace_back(std::move(new_index));
        }
    }

    const std::vector<ModelIndex> &MemScanModel::getScanHistory() const {
        // TODO: insert return statement here
        static std::vector<ModelIndex> empty_set = {};
        if (m_index_map_history.empty()) {
            return empty_set;
        }
        return m_index_map_history.back();
    }

    const std::vector<ModelIndex> &MemScanModel::getScanHistory(size_t i) const {
        // TODO: insert return statement here
        static std::vector<ModelIndex> empty_set = {};
        if (i >= m_index_map_history.size()) {
            return empty_set;
        }
        return m_index_map_history.at(i);
    }

    size_t MemScanModel::pollChildren(const ModelIndex &index) const { return 0; }

    void MemScanModel::signalEventListeners(const ModelIndex &index, MemScanModelEventFlags flags) {
        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != MemScanModelEventFlags::NONE) {
                listener.first(index, flags);
            }
        }
    }

    RefPtr<MemScanModel> MemScanModelSortFilterProxy::getSourceModel() const {
        return m_source_model;
    }

    void MemScanModelSortFilterProxy::setSourceModel(RefPtr<MemScanModel> model) {
        if (m_source_model == model) {
            return;
        }

        if (m_source_model) {
            m_source_model->removeEventListener(getUUID());
        }

        m_source_model = model;

        if (m_source_model) {
            m_source_model->addEventListener(getUUID(), TOOLBOX_BIND_EVENT_FN(watchDataUpdateEvent),
                                             MemScanModelEventFlags::EVENT_SCAN_ANY);
        }
    }

    ModelSortOrder MemScanModelSortFilterProxy::getSortOrder() const { return m_sort_order; }
    void MemScanModelSortFilterProxy::setSortOrder(ModelSortOrder order) { m_sort_order = order; }

    MemScanModelSortRole MemScanModelSortFilterProxy::getSortRole() const { return m_sort_role; }
    void MemScanModelSortFilterProxy::setSortRole(MemScanModelSortRole role) { m_sort_role = role; }

    const std::string &MemScanModelSortFilterProxy::getFilter() const & { return m_filter; }
    void MemScanModelSortFilterProxy::setFilter(const std::string &filter) { m_filter = filter; }

    std::any MemScanModelSortFilterProxy::getData(const ModelIndex &index, int role) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getData(std::move(source_index), role);
    }

    void MemScanModelSortFilterProxy::setData(const ModelIndex &index, std::any data, int role) {
        ModelIndex &&source_index = toSourceIndex(index);
        m_source_model->setData(std::move(source_index), data, role);
    }

    ModelIndex MemScanModelSortFilterProxy::getIndex(const UUID64 &uuid) const {
        ModelIndex index = m_source_model->getIndex(uuid);
        return toProxyIndex(index);
    }

    ModelIndex MemScanModelSortFilterProxy::getIndex(int64_t row, int64_t column,
                                                     const ModelIndex &parent) const {
        ModelIndex parent_src = toSourceIndex(parent);
        return toProxyIndex(row, column, parent_src);
    }

    ModelIndex MemScanModelSortFilterProxy::getParent(const ModelIndex &index) const {
        ModelIndex source_index = toSourceIndex(index);
        return toProxyIndex(m_source_model->getParent(std::move(source_index)));
    }

    ModelIndex MemScanModelSortFilterProxy::getSibling(int64_t row, int64_t column,
                                                       const ModelIndex &index) const {
        std::scoped_lock lock(m_cache_mutex);

        ModelIndex source_index       = toSourceIndex(index);
        ModelIndex src_parent         = m_source_model->getParent(source_index);
        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) == m_row_map.end()) {
            cacheIndex_(src_parent);
        }

        if (row < m_row_map[map_key].size()) {
            int64_t the_row = m_row_map[map_key][row];
            return toProxyIndex(m_source_model->getIndex(the_row, column, src_parent));
        }

        return ModelIndex();
    }

    size_t MemScanModelSortFilterProxy::getColumnCount(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getColumnCount(source_index);
    }

    size_t MemScanModelSortFilterProxy::getRowCount(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getRowCount(source_index);
    }

    int64_t MemScanModelSortFilterProxy::getColumn(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getColumn(source_index);
    }

    int64_t MemScanModelSortFilterProxy::getRow(const ModelIndex &index) const {

        ModelIndex &&source_index     = toSourceIndex(index);
        ModelIndex src_parent         = m_source_model->getParent(source_index);
        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) == m_row_map.end()) {
            cacheIndex_(src_parent);
        }

        int64_t src_row = m_source_model->getRow(source_index);
        if (src_row == -1) {
            return src_row;
        }

        const std::vector<int64_t> &row_map = m_row_map[map_key];
        for (size_t i = 0; i < row_map.size(); ++i) {
            if (src_row == row_map[i]) {
                return i;
            }
        }

        return -1;
    }

    bool MemScanModelSortFilterProxy::hasChildren(const ModelIndex &parent) const {
        ModelIndex &&source_index = toSourceIndex(parent);
        return m_source_model->hasChildren(source_index);
    }

    ScopePtr<MimeData>
    MemScanModelSortFilterProxy::createMimeData(const std::vector<ModelIndex> &indexes) const {
        std::vector<ModelIndex> indexes_copy = indexes;
        std::transform(indexes.begin(), indexes.end(), indexes_copy.begin(),
                       [&](const ModelIndex &index) { return toSourceIndex(index); });

        return m_source_model->createMimeData(indexes);
    }

    std::vector<std::string> MemScanModelSortFilterProxy::getSupportedMimeTypes() const {
        return m_source_model->getSupportedMimeTypes();
    }

    bool MemScanModelSortFilterProxy::canFetchMore(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->canFetchMore(std::move(source_index));
    }

    void MemScanModelSortFilterProxy::fetchMore(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        m_source_model->fetchMore(std::move(source_index));
    }

    void MemScanModelSortFilterProxy::reset() {
        std::scoped_lock lock(m_cache_mutex);
        m_source_model->reset();
        m_filter_map.clear();
        m_row_map.clear();
    }

    ModelIndex MemScanModelSortFilterProxy::toSourceIndex(const ModelIndex &index) const {
        if (m_source_model->validateIndex(index)) {
            return index;
        }

        if (!validateIndex(index)) {
            return ModelIndex();
        }

        return m_source_model->getIndex(index.data<MemScanResult>()->m_address);
    }

    ModelIndex MemScanModelSortFilterProxy::toProxyIndex(const ModelIndex &index) const {
        if (!m_source_model->validateIndex(index)) {
            return ModelIndex();
        }

        ModelIndex proxy_index = ModelIndex(getUUID());
        proxy_index.setData(index.data<MemScanResult>());

        IDataModel::setIndexUUID(proxy_index, index.getUUID());

        return proxy_index;
    }

    ModelIndex MemScanModelSortFilterProxy::toProxyIndex(int64_t row, int64_t column,
                                                         const ModelIndex &src_parent) const {
        std::scoped_lock lock(m_cache_mutex);

        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) == m_row_map.end()) {
            cacheIndex_(src_parent);
        }

        if (row < m_row_map[map_key].size()) {
            int64_t the_row = m_row_map[map_key][row];
            return toProxyIndex(m_source_model->getIndex(the_row, column, src_parent));
        }

        return ModelIndex();
    }

    bool MemScanModelSortFilterProxy::isFiltered(const UUID64 &uuid) const { return false; }

    void MemScanModelSortFilterProxy::cacheIndex(const ModelIndex &dir_index) const {
        std::scoped_lock lock(m_cache_mutex);
        cacheIndex_(dir_index);
    }

    void MemScanModelSortFilterProxy::cacheIndex_(const ModelIndex &dir_index) const {
        std::vector<UUID64> orig_children  = {};
        std::vector<UUID64> proxy_children = {};

        ModelIndex src_parent         = toSourceIndex(dir_index);
        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        size_t i          = 0;
        ModelIndex root_s = m_source_model->getIndex(i++, 0, src_parent);
        while (m_source_model->validateIndex(root_s)) {
            orig_children.push_back(root_s.getUUID());
            if (isFiltered(root_s.getUUID())) {
                root_s = m_source_model->getIndex(i++, 0, src_parent);
                continue;
            }
            proxy_children.push_back(root_s.getUUID());
            root_s = m_source_model->getIndex(i++, 0, src_parent);
        }

        if (proxy_children.empty()) {
            return;
        }

        switch (m_sort_role) {
        case MemScanModelSortRole::SORT_ROLE_ADDRESS: {
            std::sort(proxy_children.begin(), proxy_children.end(),
                      [&](const UUID64 &lhs, const UUID64 &rhs) {
                          return _MemScanResultCompareByAddress(
                              *m_source_model->getIndex(lhs).data<MemScanResult>(),
                              *m_source_model->getIndex(rhs).data<MemScanResult>(), m_sort_order);
                      });
            break;
        }
        default:
            break;
        }

        // Build the row map
        m_row_map[map_key] = {};
        m_row_map[map_key].resize(proxy_children.size());
        for (size_t i = 0; i < proxy_children.size(); i++) {
            for (size_t j = 0; j < orig_children.size(); j++) {
                if (proxy_children[i] == orig_children[j]) {
                    m_row_map[map_key][i] = j;
                }
            }
        }
    }

    void MemScanModelSortFilterProxy::watchDataUpdateEvent(const ModelIndex &index,
                                                           MemScanModelEventFlags flags) {
        if ((flags & MemScanModelEventFlags::EVENT_SCAN_ANY) == MemScanModelEventFlags::NONE) {
            return;
        }

        ModelIndex proxy_index = toProxyIndex(index);
        if (!validateIndex(proxy_index)) {
            return;
        }

        if ((flags & MemScanModelEventFlags::EVENT_SCAN_ADDED) != MemScanModelEventFlags::NONE) {
            cacheIndex(getParent(proxy_index));
            return;
        }

        {
            std::scoped_lock lock(m_cache_mutex);

            m_filter_map.clear();
            m_row_map.clear();
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
        if (!m_scan_model) {
            return 0;
        }

        if (profile.m_new_scan) {
            m_scan_model->reset();

            switch (profile.m_scan_type) {
            case MetaType::BOOL:
                return scanAllSingles<bool>(*m_scan_model, profile);
            case MetaType::S8:
                return scanAllSingles<s8>(*m_scan_model, profile);
            case MetaType::U8:
                return scanAllSingles<u8>(*m_scan_model, profile);
            case MetaType::S16:
                return scanAllSingles<s16>(*m_scan_model, profile);
            case MetaType::U16:
                return scanAllSingles<u16>(*m_scan_model, profile);
            case MetaType::S32:
                return scanAllSingles<s32>(*m_scan_model, profile);
            case MetaType::U32:
                return scanAllSingles<u32>(*m_scan_model, profile);
            case MetaType::F32:
                return scanAllSingles<f32>(*m_scan_model, profile);
            case MetaType::F64:
                return scanAllSingles<f64>(*m_scan_model, profile);
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
                return scanExistingSingles<bool>(*m_scan_model, profile);
            case MetaType::S8:
                return scanExistingSingles<s8>(*m_scan_model, profile);
            case MetaType::U8:
                return scanExistingSingles<u8>(*m_scan_model, profile);
            case MetaType::S16:
                return scanExistingSingles<s16>(*m_scan_model, profile);
            case MetaType::U16:
                return scanExistingSingles<u16>(*m_scan_model, profile);
            case MetaType::S32:
                return scanExistingSingles<s32>(*m_scan_model, profile);
            case MetaType::U32:
                return scanExistingSingles<u32>(*m_scan_model, profile);
            case MetaType::F32:
                return scanExistingSingles<f32>(*m_scan_model, profile);
            case MetaType::F64:
                return scanExistingSingles<f64>(*m_scan_model, profile);
            case MetaType::STRING:
                return scanExistingStrings(*m_scan_model, profile);
            case MetaType::UNKNOWN:
                return scanExistingByteArrays(*m_scan_model, profile);
            default:
                return 0;
            }
        }
    }

}  // namespace Toolbox