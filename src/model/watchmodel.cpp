#include <algorithm>
#include <any>
#include <compare>
#include <set>

#include "dolphin/watch.hpp"
#include "gui/application.hpp"
#include "model/watchmodel.hpp"

using namespace Toolbox::Object;

namespace Toolbox {

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
#if 0
            UUID64 lhs_uuid, rhs_uuid;

            if (m_type == Type::GROUP) {
                lhs_uuid = m_group->getUUID();
            } else {
                lhs_uuid = m_watch->getUUID();
            }

            if (rhs.m_type == Type::GROUP) {
                rhs_uuid = rhs.m_group->getUUID();
            } else {
                rhs_uuid = rhs.m_watch->getUUID();
            }

            return lhs_uuid <=> rhs_uuid;
#else
            return m_self_uuid <=> rhs.m_self_uuid;
#endif
        }
    };

    static bool _WatchIndexDataIsGroup(const _WatchIndexData &data) {
        return data.m_type == _WatchIndexData::Type::GROUP;
    }

    static bool _WatchIndexDataCompareByName(const _WatchIndexData &lhs, const _WatchIndexData &rhs,
                                             ModelSortOrder order) {
        const bool is_lhs_group = _WatchIndexDataIsGroup(lhs);
        const bool is_rhs_group = _WatchIndexDataIsGroup(rhs);

        // Folders come first
        if (is_lhs_group && !is_rhs_group) {
            return true;
        }

        if (!is_lhs_group && is_rhs_group) {
            return false;
        }

        // Sort by name
        if (order == ModelSortOrder::SORT_ASCENDING) {
            if (is_lhs_group) {
                return lhs.m_group->getName() < rhs.m_group->getName();
            }
            return lhs.m_watch->getWatchName() < rhs.m_watch->getWatchName();
        } else {
            if (is_lhs_group) {
                return lhs.m_group->getName() > rhs.m_group->getName();
            }
            return lhs.m_watch->getWatchName() > rhs.m_watch->getWatchName();
        }
    }

    static bool _WatchIndexDataCompareByType(const _WatchIndexData &lhs, const _WatchIndexData &rhs,
                                             ModelSortOrder order) {
        const bool is_lhs_group = _WatchIndexDataIsGroup(lhs);
        const bool is_rhs_group = _WatchIndexDataIsGroup(rhs);

        // Folders come first
        if (is_lhs_group && !is_rhs_group) {
            return true;
        }

        if (!is_lhs_group && is_rhs_group) {
            return false;
        }

        if (is_lhs_group) {
            // Sort by name
            if (order == ModelSortOrder::SORT_ASCENDING) {
                return lhs.m_group->getName() < rhs.m_group->getName();
            } else {
                return lhs.m_group->getName() > rhs.m_group->getName();
            }
        }

        // Sort by type
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs.m_watch->getWatchType() < rhs.m_watch->getWatchType();
        } else {
            return lhs.m_watch->getWatchType() > rhs.m_watch->getWatchType();
        }
    }

    WatchDataModel::~WatchDataModel() {
        m_index_map.clear();
        m_listeners.clear();
    }

    void WatchDataModel::initialize() {
        m_index_map.clear();
        m_listeners.clear();

        m_running      = true;
        m_watch_thread = std::thread([&]() {
            while (m_running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(m_refresh_rate));
                processWatches();
            }
        });
        m_watch_thread.detach();
    }

    bool WatchDataModel::isIndexGroup(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);

        if (!validateIndex(index)) {
            return false;  // Invalid index
        }

        _WatchIndexData *data = index.data<_WatchIndexData>();
        if (!data) {
            return false;
        }
        return _WatchIndexDataIsGroup(*data);
    }

    std::any WatchDataModel::getData(const ModelIndex &index, int role) const {
        std::scoped_lock lock(m_mutex);
        return getData_(index, role);
    }

    void WatchDataModel::setData(const ModelIndex &index, std::any data, int role) {
        std::scoped_lock lock(m_mutex);
        return setData_(index, data, role);
    }

    std::string WatchDataModel::findUniqueName(const ModelIndex &index,
                                               const std::string &name) const {
        std::scoped_lock lock(m_mutex);
        return findUniqueName_(index, name);
    }

    static ModelIndex LoadWatchFromJSON(WatchDataModel &model,
                                        const WatchDataModel::json_t &mem_watch, int row,
                                        const ModelIndex &parent) {
        std::vector<u32> pointer_chain;
        pointer_chain.reserve(8);

        MetaType watch_type;
        u32 watch_size;

        int base_index              = mem_watch["baseIndex"];
        const std::string &name_str = mem_watch["label"];

        const std::string &address_str = mem_watch["address"];
        u32 address                    = std::strtoul(address_str.c_str(), nullptr, 16);
        pointer_chain.emplace_back(address);

        if (mem_watch.contains("pointerOffsets")) {
            const WatchDataModel::json_t &pointer_offsets = mem_watch["pointerOffsets"];
            for (const std::string &offset_str : pointer_offsets) {
                u32 offset = std::strtoul(offset_str.c_str(), nullptr, 16);
                pointer_chain.emplace_back(offset);
            }
        }

        int type_index = mem_watch["typeIndex"];
        bool unsgned   = mem_watch["unsigned"];
        switch (type_index) {
        case 0:
            watch_type = unsgned ? MetaType::U8 : MetaType::S8;
            break;
        case 1:
            watch_type = unsgned ? MetaType::U16 : MetaType::S16;
            break;
        case 2:
            watch_type = unsgned ? MetaType::U32 : MetaType::S32;
            break;
        case 3:
            watch_type = MetaType::F32;
            break;
        case 4:
            watch_type = MetaType::F64;
            break;
        case 5:
            watch_type = MetaType::STRING;
            break;
        case 6:
            watch_type = MetaType::UNKNOWN;
            break;
        default:
            return ModelIndex();
        }

        WatchValueBase value_base;
        switch (base_index) {
        case 0:
            value_base = WatchValueBase::BASE_DECIMAL;
            break;
        case 1:
            value_base = WatchValueBase::BASE_HEXADECIMAL;
            break;
        case 2:
            value_base = WatchValueBase::BASE_OCTAL;
            break;
        case 3:
            value_base = WatchValueBase::BASE_BINARY;
            break;
        }

        if (mem_watch.contains("length")) {
            watch_size = mem_watch["length"];
        } else {
            watch_size = meta_type_size(watch_type);
        }

        return model.makeWatchIndex(name_str, watch_type, pointer_chain, watch_size,
                                    pointer_chain.size() > 1, value_base, row, parent, true);
    }

    Result<void, JSONError> WatchDataModel::loadFromDMEFile(const fs_path &path) {
        std::ifstream in_stream = std::ifstream(path, std::ios::in);

        json_t profile_json;
        return tryJSONWithResult(profile_json, [&](json_t &j) {
            in_stream >> j;
            if (!j.contains("watchList")) {
                return make_json_error<void>("MEMSCANMODEL", "DME file has no `watchList' field",
                                             0);
            }

            const json_t &watch_list = j["watchList"];
            if (!watch_list.is_array()) {
                return make_json_error<void>("MEMSCANMODEL",
                                             "DME file `watchList' field should be an array", 0);
            }

            // Reset the model before loading all the data
            reset();

            size_t root_row = 0;

            for (const json_t &watch_entry : watch_list) {
                if (watch_entry.contains("groupEntries")) {
                    const json_t &group_list = watch_entry["groupEntries"];
                    if (!group_list.is_array()) {
                        return make_json_error<void>(
                            "MEMSCANMODEL", "DME file `groupEntries' field should be an array", 0);
                    }

                    const std::string &group_name = watch_entry["groupName"];
                    ModelIndex group_idx = makeGroupIndex(group_name, root_row, ModelIndex(), true);
                    if (!validateIndex(group_idx)) {
                        continue;
                    }

                    root_row += 1;

                    size_t child_row = 0;
                    for (const json_t &child_watch : group_list) {
                        ModelIndex new_idx =
                            LoadWatchFromJSON(*this, child_watch, child_row, group_idx);
                        if (validateIndex(new_idx)) {
                            child_row += 1;
                        }
                    }
                } else {
                    ModelIndex new_idx =
                        LoadWatchFromJSON(*this, watch_entry, root_row, ModelIndex());
                    if (validateIndex(new_idx)) {
                        root_row += 1;
                    }
                }
            }
        });
    }

    ModelIndex WatchDataModel::getIndex(const UUID64 &uuid) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(uuid);
    }

    ModelIndex WatchDataModel::getIndex(int64_t row, int64_t column,
                                        const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(row, column, parent);
    }

    bool WatchDataModel::removeIndex(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return removeIndex_(index);
    }

    ModelIndex WatchDataModel::getParent(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getParent_(index);
    }

    ModelIndex WatchDataModel::getSibling(int64_t row, int64_t column,
                                          const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getSibling_(row, column, index);
    }

    size_t WatchDataModel::getColumnCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumnCount_(index);
    }

    size_t WatchDataModel::getRowCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRowCount_(index);
    }

    int64_t WatchDataModel::getColumn(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumn_(index);
    }

    int64_t WatchDataModel::getRow(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRow_(index);
    }

    bool WatchDataModel::hasChildren(const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return hasChildren_(parent);
    }

    ScopePtr<MimeData>
    WatchDataModel::createMimeData(const std::unordered_set<ModelIndex> &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
    }

    bool WatchDataModel::insertMimeData(const ModelIndex &index, const MimeData &data) {
        std::scoped_lock lock(m_mutex);
        return insertMimeData_(index, data);
    }

    std::vector<std::string> WatchDataModel::getSupportedMimeTypes() const {
        return std::vector<std::string>();
    }

    bool WatchDataModel::canFetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return canFetchMore_(index);
    }

    void WatchDataModel::fetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return fetchMore_(index);
    }

    void WatchDataModel::reset() {
        std::scoped_lock lock(m_mutex);

        for (auto &[key, val] : m_index_map) {
            _WatchIndexData *p = val.data<_WatchIndexData>();
            if (p) {
                delete p;
            }
        }

        m_index_map.clear();

        signalEventListeners(ModelIndex(), WatchModelEventFlags::EVENT_RESET);
    }

    void WatchDataModel::addEventListener(UUID64 uuid, event_listener_t listener,
                                          WatchModelEventFlags flags) {
        m_listeners[uuid] = {listener, flags};
    }

    void WatchDataModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    Result<void, SerialError> WatchDataModel::serialize(Serializer &out) const {
        std::scoped_lock lock(m_mutex);

        for (const auto &[uuid, index] : m_index_map) {
            _WatchIndexData *data = index.data<_WatchIndexData>();
            if (!data) {
                TOOLBOX_DEBUG_LOG("[WatchDataModel] Serialize: Index with no data.");
                continue;
            }

            // Header stub
            // ---
            out.write<UUID64>(uuid);
            out.write<UUID64>(data->m_parent);
            // ---

            if (_WatchIndexDataIsGroup(*data)) {
                const std::string &name             = data->m_group->getName();
                const std::vector<UUID64> &children = data->m_group->getChildren();

                out.write<u8>((u8)_WatchIndexData::Type::GROUP);
                out.writeString(name);
                out.write<bool>(data->m_group->isLocked());
                out.write<u32>(children.size());
                for (const UUID64 &child_uuid : children) {
                    out.write<UUID64>(child_uuid);
                }
            } else {
                const std::string &name = data->m_watch->getWatchName();
                const u32 address       = data->m_watch->getWatchAddress();
                const u32 size          = data->m_watch->getWatchSize();
                const MetaType type     = data->m_watch->getWatchType();
                const bool locked       = data->m_watch->isLocked();

                out.write<u8>((u8)_WatchIndexData::Type::WATCH);
                out.writeString(name);
                out.write<u32>(address);
                out.write<u32>(size);
                out.write<u8>(static_cast<u8>(type));
                out.write<bool>(locked);
            }
        }

        return Result<void, SerialError>();
    }

    Result<void, SerialError> WatchDataModel::deserialize(Deserializer &in) {
        std::scoped_lock lock(m_mutex);

        while (true) {
            size_t remaining = in.remaining();
            if (remaining < sizeof(UUID64) * 2 + sizeof(u8)) {
                break;  // Not enough data to read a header
            }

            remaining -= sizeof(UUID64) * 2 + sizeof(u8);

            UUID64 uuid                = in.read<UUID64>();
            UUID64 parent              = in.read<UUID64>();
            _WatchIndexData::Type type = (_WatchIndexData::Type)in.read<u8>();

            if (type == _WatchIndexData::Type::GROUP) {
                const size_t group_size = sizeof(u16) + sizeof(bool) + sizeof(u32);
                if (remaining < group_size) {
                    return make_serial_error<void>(in, "Not enough data to read group header");
                }

                std::string name = in.readString();
                bool locked      = in.read<bool>();
                u32 child_count  = in.read<u32>();

                WatchGroup *group = new WatchGroup();
                group->setName(name);
                group->setLocked(locked);

                for (u32 i = 0; i < child_count; ++i) {
                    UUID64 child_uuid = in.read<UUID64>();
                    group->addChild(child_uuid);
                }

                _WatchIndexData *data = new _WatchIndexData;
                data->m_parent        = parent;
                data->m_self_uuid     = uuid;
                data->m_type          = type;
                data->m_group         = group;

                ModelIndex index = ModelIndex(getUUID(), uuid);
                index.setData(data);

                m_index_map[index.getUUID()] = index;
            } else if (type == _WatchIndexData::Type::WATCH) {
                const size_t watch_size = sizeof(u16) + sizeof(u32) * 2 + sizeof(u8) + sizeof(bool);
                if (remaining < watch_size) {
                    return make_serial_error<void>(in, "Not enough data to read group header");
                }

                std::string name    = in.readString();
                u32 address         = in.read<u32>();
                u32 size            = in.read<u32>();
                MetaType watch_type = static_cast<MetaType>(in.read<u8>());
                bool locked         = in.read<bool>();

                MetaWatch *watch = new MetaWatch(watch_type);
                watch->setWatchName(name);
                watch->setLocked(locked);
                watch->startWatch(address, size);

                _WatchIndexData *data = new _WatchIndexData;
                data->m_parent        = parent;
                data->m_self_uuid     = uuid;
                data->m_type          = type;
                data->m_watch         = watch;

                ModelIndex index = ModelIndex(getUUID(), uuid);
                index.setData(data);

                m_index_map[index.getUUID()] = index;

                signalEventListeners(index, WatchModelEventFlags::EVENT_WATCH_ADDED);
            } else {
                return make_serial_error<void>(in,
                                               "Invalid type encountered during deserialization");
            }
        }

        return Result<void, SerialError>();
    }

    std::any WatchDataModel::getData_(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        _WatchIndexData *data = index.data<_WatchIndexData>();

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            switch (data->m_type) {
            case _WatchIndexData::Type::GROUP:
                return data->m_group->getName();
            case _WatchIndexData::Type::WATCH:
                return data->m_watch->getWatchName();
            }
            return {};
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_TYPE: {
            switch (data->m_type) {
            case _WatchIndexData::Type::GROUP:
                return "Group";
            case _WatchIndexData::Type::WATCH:
                return std::string(meta_type_name(data->m_watch->getWatchType()));
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_VALUE_META: {
            switch (data->m_type) {
            case _WatchIndexData::Type::GROUP:
                return {};
            case _WatchIndexData::Type::WATCH:
                return data->m_watch->getMetaValue();
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_ADDRESS: {
            switch (data->m_type) {
            case _WatchIndexData::Type::GROUP:
                return (u32)0;
            case _WatchIndexData::Type::WATCH:
                return data->m_watch->getWatchAddress();
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_LOCK: {
            switch (data->m_type) {
            case _WatchIndexData::Type::GROUP:
                return data->m_group->isLocked();
            case _WatchIndexData::Type::WATCH:
                return data->m_watch->isLocked();
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_SIZE: {
            switch (data->m_type) {
            case _WatchIndexData::Type::GROUP:
                return (u32)data->m_group->getChildCount();
            case _WatchIndexData::Type::WATCH:
                return (u32)data->m_watch->getWatchSize();
            }
            return {};
        }
        default:
            return {};
        }
    }

    void WatchDataModel::setData_(const ModelIndex &index, std::any data, int role) {
        if (!validateIndex(index)) {
            return;
        }

        _WatchIndexData *idata = index.data<_WatchIndexData>();

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY: {
            std::string name = std::any_cast<std::string>(data);
            switch (idata->m_type) {
            case _WatchIndexData::Type::GROUP:
                idata->m_group->setName(name);
                return;
            case _WatchIndexData::Type::WATCH:
                idata->m_watch->setWatchName(name);
                return;
            }
            return;
        }
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return;
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return;
        }
        case WatchDataRole::WATCH_DATA_ROLE_TYPE: {
            return;
        }
        case WatchDataRole::WATCH_DATA_ROLE_VALUE_META: {
            return;
        }
        case WatchDataRole::WATCH_DATA_ROLE_ADDRESS: {
            switch (idata->m_type) {
            case _WatchIndexData::Type::GROUP:
                return;
            case _WatchIndexData::Type::WATCH:
                u32 watch_size = idata->m_watch->getWatchSize();
                idata->m_watch->stopWatch();
                idata->m_watch->startWatch(std::any_cast<u32>(data), watch_size);
                return;
            }
            return;
        }
        case WatchDataRole::WATCH_DATA_ROLE_LOCK: {
            bool locked = std::any_cast<bool>(data);
            switch (idata->m_type) {
            case _WatchIndexData::Type::GROUP:
                idata->m_group->setLocked(locked);
                return;
            case _WatchIndexData::Type::WATCH:
                idata->m_watch->setLocked(locked);
                return;
            }
            return;
        }
        case WatchDataRole::WATCH_DATA_ROLE_SIZE: {
            return;
        }
        default:
            return;
        }
    }

    std::string WatchDataModel::findUniqueName_(const ModelIndex &parent,
                                                const std::string &name) const {
        if (!validateIndex(parent)) {
            return name;
        }

        std::string result_name = name;
        size_t collisions       = 0;
        std::vector<std::string> child_paths;

        size_t dir_size = getRowCount_(parent);
        for (size_t i = 0; i < dir_size; ++i) {
            ModelIndex child = getIndex_(i, 0, parent);

            std::string child_name =
                std::any_cast<std::string>(getData_(child, ModelDataRole::DATA_ROLE_DISPLAY));

            child_paths.emplace_back(child_name);
        }

        for (size_t i = 0; i < child_paths.size();) {
            if (child_paths[i] == result_name) {
                collisions += 1;
                result_name = std::format("{} ({})", name, collisions);
                i           = 0;
                continue;
            }
            ++i;
        }

        return result_name;
    }

    ModelIndex WatchDataModel::getIndex_(const UUID64 &uuid) const { return m_index_map.at(uuid); }

    ModelIndex WatchDataModel::getIndex_(int64_t row, int64_t column,
                                         const ModelIndex &parent) const {
        if (row < 0 || column < 0) {
            return ModelIndex();
        }

        if (!validateIndex(parent)) {
            size_t cur_row = 0;
            for (const auto &[uuid, index] : m_index_map) {
                _WatchIndexData *data = index.data<_WatchIndexData>();
                if (data->m_parent == 0 && cur_row++ == row) {
                    return index;
                }
            }
            return ModelIndex();
        }

        _WatchIndexData *parent_data = parent.data<_WatchIndexData>();
        if (!_WatchIndexDataIsGroup(*parent_data)) {
            return ModelIndex();
        }

        UUID64 child_uuid = parent_data->m_group->getChildUUID(row);
        if (child_uuid == 0) {
            return ModelIndex();
        }

        return m_index_map.at(child_uuid);
    }

    bool WatchDataModel::removeIndex_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        return m_index_map.erase(index.getUUID()) > 0;
    }

    ModelIndex WatchDataModel::getParent_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        const UUID64 &id = index.data<_WatchIndexData>()->m_parent;
        if (id == 0) {
            return ModelIndex();
        }

        return m_index_map.at(id);
    }

    ModelIndex WatchDataModel::getSibling_(int64_t row, int64_t column,
                                           const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        const ModelIndex &parent = getParent_(index);
        return getIndex_(row, column, parent);
    }

    size_t WatchDataModel::getColumnCount_(const ModelIndex &index) const { return 1; }

    size_t WatchDataModel::getRowCount_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            size_t root_rows = 0;
            for (const auto &[uuid, xindex] : m_index_map) {
                _WatchIndexData *data = xindex.data<_WatchIndexData>();
                if (data->m_parent == 0) {
                    root_rows += 1;
                }
            }
            return root_rows;
        }

        _WatchIndexData *data = index.data<_WatchIndexData>();
        if (_WatchIndexDataIsGroup(*data)) {
            return data->m_group->getChildCount();
        }

        return 0;
    }

    int64_t WatchDataModel::getColumn_(const ModelIndex &index) const {
        return validateIndex(index) ? 0 : -1;
    }

    int64_t WatchDataModel::getRow_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return -1;
        }

        ModelIndex parent = getParent_(index);
        if (!validateIndex(parent)) {
            size_t i = 0;
            for (const auto &[k, v] : m_index_map) {
                if (v == index) {
                    return i;
                }
                i += (int)(getParent_(v) == ModelIndex());
            }
            return -1;
        }

        _WatchIndexData *parent_data = parent.data<_WatchIndexData>();
        if (!_WatchIndexDataIsGroup(*parent_data)) {
            return -1;
        }

        int64_t row = 0;
        for (const UUID64 &uuid : parent_data->m_group->getChildren()) {
            if (uuid == index.getUUID()) {
                return row;
            }
            ++row;
        }

        return -1;
    }

    bool WatchDataModel::hasChildren_(const ModelIndex &parent) const {
        if (getRowCount_(parent) > 0) {
            return true;
        }
        return pollChildren(parent) > 0;
    }

    ScopePtr<MimeData>
    WatchDataModel::createMimeData_(const std::unordered_set<ModelIndex> &indexes) const {
        TOOLBOX_ERROR("[WatchDataModel] Mimedata unimplemented!");
        return ScopePtr<MimeData>();
    }

    bool WatchDataModel::insertMimeData_(const ModelIndex &index, const MimeData &data) {
        return false;
    }

    bool WatchDataModel::canFetchMore_(const ModelIndex &index) { return true; }

    void WatchDataModel::fetchMore_(const ModelIndex &index) { return; }

    ModelIndex WatchDataModel::makeWatchIndex(const std::string &name, MetaType type,
                                              const std::vector<u32> &pointer_chain, u32 size,
                                              bool is_pointer, WatchValueBase value_base,
                                              int64_t row, const ModelIndex &parent,
                                              bool find_unique_name) {
        _WatchIndexData *parent_data = nullptr;
        if (row < 0 || pointer_chain.empty()) {
            return ModelIndex();
        }

        if (validateIndex(parent)) {
            parent_data = parent.data<_WatchIndexData>();
            if (!_WatchIndexDataIsGroup(*parent_data)) {
                TOOLBOX_ERROR("[WatchDataModel] Invalid row index!");
                return ModelIndex();
            }
            if (row > parent_data->m_group->getChildCount()) {
                TOOLBOX_ERROR_V("[WatchDataModel] Invalid row index: {} > {}", row,
                                parent_data->m_group->getChildCount());
                return ModelIndex();
            }
        }

        _WatchIndexData *data = new _WatchIndexData;
        data->m_type          = _WatchIndexData::Type::WATCH;
        data->m_watch         = new MetaWatch(type);
        data->m_value_base    = value_base;

        if (is_pointer) {
            if (!data->m_watch->startWatch(pointer_chain, size)) {
                return ModelIndex();
            }
        } else {
            if (!data->m_watch->startWatch(pointer_chain[0], size)) {
                return ModelIndex();
            }
        }

        std::string unique_name = find_unique_name ? findUniqueName_(parent, name) : name;
        data->m_group->setName(unique_name);
        data->m_group->setLocked(false);

        ModelIndex index  = ModelIndex(getUUID());
        data->m_self_uuid = index.getUUID();

        std::scoped_lock lock(m_mutex);

        if (!parent_data) {
            index.setData(data);
            m_index_map[index.getUUID()] = std::move(index);
        } else {
            data->m_parent = parent.getUUID();
            index.setData(data);

            if (_WatchIndexDataIsGroup(*parent_data)) {
                WatchGroup *parent_group = parent_data->m_group;
                parent_group->insertChild(row, index.getUUID());
            }

            m_index_map[index.getUUID()] = std::move(index);
        }

        return index;
    }

    ModelIndex WatchDataModel::makeGroupIndex(const std::string &name, int64_t row,
                                              const ModelIndex &parent, bool find_unique_name) {
        _WatchIndexData *parent_data = nullptr;
        if (row < 0) {
            return ModelIndex();
        }

        if (validateIndex(parent)) {
            parent_data = parent.data<_WatchIndexData>();
            if (!_WatchIndexDataIsGroup(*parent_data)) {
                TOOLBOX_ERROR("[WatchDataModel] Invalid row index!");
                return ModelIndex();
            }
            if (row > parent_data->m_group->getChildCount()) {
                TOOLBOX_ERROR_V("[WatchDataModel] Invalid row index: {} > {}", row,
                                parent_data->m_group->getChildCount());
                return ModelIndex();
            }
        }

        _WatchIndexData *data = new _WatchIndexData;
        data->m_type          = _WatchIndexData::Type::GROUP;
        data->m_group         = new WatchGroup;

        std::string unique_name = find_unique_name ? findUniqueName_(parent, name) : name;
        data->m_group->setName(unique_name);
        data->m_group->setLocked(false);

        ModelIndex index  = ModelIndex(getUUID());
        data->m_self_uuid = index.getUUID();

        std::scoped_lock lock(m_mutex);

        if (!validateIndex(parent)) {
            index.setData(data);
            m_index_map[index.getUUID()] = std::move(index);
        } else {
            data->m_parent = parent.getUUID();
            index.setData(data);

            if (_WatchIndexDataIsGroup(*parent_data)) {
                WatchGroup *parent_group = parent_data->m_group;
                parent_group->insertChild(row, index.getUUID());
            }

            m_index_map[index.getUUID()] = std::move(index);
        }

        return index;
    }

    void WatchDataModel::processWatches() {
        std::scoped_lock lock(m_mutex);

        for (auto &pair : m_index_map) {
            ModelIndex &index = pair.second;
            if (!validateIndex(index)) {
                continue;
            }
            _WatchIndexData *data = index.data<_WatchIndexData>();
            if (!_WatchIndexDataIsGroup(*data)) {
                data->m_watch->processWatch();
            }
        }
    }

    size_t WatchDataModel::pollChildren(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 0;
        }

        _WatchIndexData *data = index.data<_WatchIndexData>();
        if (!_WatchIndexDataIsGroup(*data)) {
            return 0;
        }

        return data->m_group->getChildCount();
    }

    void WatchDataModel::signalEventListeners(const ModelIndex &index, WatchModelEventFlags flags) {
        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != WatchModelEventFlags::NONE) {
                listener.first(index, flags);
            }
        }
    }

    RefPtr<WatchDataModel> WatchDataModelSortFilterProxy::getSourceModel() const {
        return m_source_model;
    }

    void WatchDataModelSortFilterProxy::setSourceModel(RefPtr<WatchDataModel> model) {
        if (m_source_model == model) {
            return;
        }

        if (m_source_model) {
            m_source_model->removeEventListener(getUUID());
        }

        m_source_model = model;

        if (m_source_model) {
            m_source_model->addEventListener(getUUID(), TOOLBOX_BIND_EVENT_FN(watchDataUpdateEvent),
                                             WatchModelEventFlags::EVENT_ANY);
        }
    }

    ModelSortOrder WatchDataModelSortFilterProxy::getSortOrder() const { return m_sort_order; }
    void WatchDataModelSortFilterProxy::setSortOrder(ModelSortOrder order) { m_sort_order = order; }

    WatchModelSortRole WatchDataModelSortFilterProxy::getSortRole() const { return m_sort_role; }
    void WatchDataModelSortFilterProxy::setSortRole(WatchModelSortRole role) { m_sort_role = role; }

    bool WatchDataModelSortFilterProxy::isIndexGroup(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->isIndexGroup(source_index);
    }

    const std::string &WatchDataModelSortFilterProxy::getFilter() const & { return m_filter; }
    void WatchDataModelSortFilterProxy::setFilter(const std::string &filter) { m_filter = filter; }

    std::any WatchDataModelSortFilterProxy::getData(const ModelIndex &index, int role) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getData(std::move(source_index), role);
    }

    void WatchDataModelSortFilterProxy::setData(const ModelIndex &index, std::any data, int role) {
        ModelIndex &&source_index = toSourceIndex(index);
        m_source_model->setData(std::move(source_index), data, role);
    }

    ModelIndex WatchDataModelSortFilterProxy::getIndex(const UUID64 &uuid) const {
        ModelIndex index = m_source_model->getIndex(uuid);
        return toProxyIndex(index);
    }

    ModelIndex WatchDataModelSortFilterProxy::getIndex(int64_t row, int64_t column,
                                                       const ModelIndex &parent) const {
        ModelIndex parent_src = toSourceIndex(parent);
        return toProxyIndex(row, column, parent_src);
    }

    bool WatchDataModelSortFilterProxy::removeIndex(const ModelIndex &index) {
        ModelIndex source_index = toSourceIndex(index);
        return m_source_model->removeIndex(source_index);
    }

    ModelIndex WatchDataModelSortFilterProxy::getParent(const ModelIndex &index) const {
        ModelIndex source_index = toSourceIndex(index);
        return toProxyIndex(m_source_model->getParent(std::move(source_index)));
    }

    ModelIndex WatchDataModelSortFilterProxy::getSibling(int64_t row, int64_t column,
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

    size_t WatchDataModelSortFilterProxy::getColumnCount(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getColumnCount(source_index);
    }

    size_t WatchDataModelSortFilterProxy::getRowCount(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getRowCount(source_index);
    }

    int64_t WatchDataModelSortFilterProxy::getColumn(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getColumn(source_index);
    }

    int64_t WatchDataModelSortFilterProxy::getRow(const ModelIndex &index) const {

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

    bool WatchDataModelSortFilterProxy::hasChildren(const ModelIndex &parent) const {
        ModelIndex &&source_index = toSourceIndex(parent);
        return m_source_model->hasChildren(source_index);
    }

    ScopePtr<MimeData> WatchDataModelSortFilterProxy::createMimeData(
        const std::unordered_set<ModelIndex> &indexes) const {
        std::unordered_set<ModelIndex> indexes_copy;

        for (const ModelIndex &idx : indexes) {
            indexes_copy.insert(toSourceIndex(idx));
        }

        return m_source_model->createMimeData(indexes);
    }

    bool WatchDataModelSortFilterProxy::insertMimeData(const ModelIndex &index,
                                                       const MimeData &data) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->insertMimeData(index, data);
    }

    std::vector<std::string> WatchDataModelSortFilterProxy::getSupportedMimeTypes() const {
        return m_source_model->getSupportedMimeTypes();
    }

    bool WatchDataModelSortFilterProxy::canFetchMore(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->canFetchMore(std::move(source_index));
    }

    void WatchDataModelSortFilterProxy::fetchMore(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        m_source_model->fetchMore(std::move(source_index));
    }

    void WatchDataModelSortFilterProxy::reset() {
        std::scoped_lock lock(m_cache_mutex);
        m_filter_map.clear();
        m_row_map.clear();
    }

    ModelIndex WatchDataModelSortFilterProxy::toSourceIndex(const ModelIndex &index) const {
        if (m_source_model->validateIndex(index)) {
            return index;
        }

        if (!validateIndex(index)) {
            return ModelIndex();
        }

        return m_source_model->getIndex(index.data<_WatchIndexData>()->m_self_uuid);
    }

    ModelIndex WatchDataModelSortFilterProxy::toProxyIndex(const ModelIndex &index) const {
        if (!m_source_model->validateIndex(index)) {
            return ModelIndex();
        }

        ModelIndex proxy_index = ModelIndex(getUUID(), index.getUUID());
        proxy_index.setData(index.data<_WatchIndexData>());

        return proxy_index;
    }

    ModelIndex WatchDataModelSortFilterProxy::toProxyIndex(int64_t row, int64_t column,
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

    bool WatchDataModelSortFilterProxy::isFiltered(const UUID64 &uuid) const {
        ModelIndex child_index = m_source_model->getIndex(uuid);

        _WatchIndexData *data = child_index.data<_WatchIndexData>();
        std::string name;

        if (_WatchIndexDataIsGroup(*data)) {
            name = data->m_group->getName();
        } else {
            name = data->m_watch->getWatchName();
        }

        return !name.starts_with(m_filter);
    }

    void WatchDataModelSortFilterProxy::cacheIndex(const ModelIndex &dir_index) const {
        std::scoped_lock lock(m_cache_mutex);
        cacheIndex_(dir_index);
    }

    void WatchDataModelSortFilterProxy::cacheIndex_(const ModelIndex &dir_index) const {
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
        case WatchModelSortRole::SORT_ROLE_NAME: {
            std::sort(proxy_children.begin(), proxy_children.end(),
                      [&](const UUID64 &lhs, const UUID64 &rhs) {
                          return _WatchIndexDataCompareByName(
                              *m_source_model->getIndex(lhs).data<_WatchIndexData>(),
                              *m_source_model->getIndex(rhs).data<_WatchIndexData>(), m_sort_order);
                      });
            break;
        }
        case WatchModelSortRole::SORT_ROLE_TYPE: {
            std::sort(proxy_children.begin(), proxy_children.end(),
                      [&](const UUID64 &lhs, const UUID64 &rhs) {
                          return _WatchIndexDataCompareByType(
                              *m_source_model->getIndex(lhs).data<_WatchIndexData>(),
                              *m_source_model->getIndex(rhs).data<_WatchIndexData>(), m_sort_order);
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

    void WatchDataModelSortFilterProxy::watchDataUpdateEvent(const ModelIndex &index,
                                                             WatchModelEventFlags flags) {
        if ((flags & WatchModelEventFlags::EVENT_ANY) == WatchModelEventFlags::NONE) {
            return;
        }

        if ((flags & WatchModelEventFlags::EVENT_RESET) == WatchModelEventFlags::EVENT_RESET) {
            reset();
        }

        ModelIndex proxy_index = toProxyIndex(index);
        if (!validateIndex(proxy_index)) {
            return;
        }

        if ((flags & WatchModelEventFlags::EVENT_GROUP_ADDED) != WatchModelEventFlags::NONE) {
            cacheIndex(getParent(proxy_index));
            return;
        }

        if ((flags & WatchModelEventFlags::EVENT_WATCH_ADDED) != WatchModelEventFlags::NONE) {
            cacheIndex(getParent(proxy_index));
            return;
        }

        {
            std::scoped_lock lock(m_cache_mutex);

            m_filter_map.clear();
            m_row_map.clear();
        }
    }

}  // namespace Toolbox