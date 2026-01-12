#include <algorithm>
#include <any>
#include <compare>
#include <set>

#include "dolphin/watch.hpp"
#include "gui/application.hpp"
#include "model/watchmodel.hpp"

using namespace Toolbox::Object;

namespace Toolbox {

    bool WatchDataModel::_WatchIndexDataIsGroup(const _WatchIndexData &data) {
        return data.m_type == _WatchIndexData::Type::GROUP;
    }

    bool _WatchIndexDataCompareByName(const std::string &lhs, const std::string &rhs,
                                      ModelSortOrder order) {
        const bool is_lhs_group = lhs == "Group";
        const bool is_rhs_group = rhs == "Group";

        // Folders come first
        if (is_lhs_group && !is_rhs_group) {
            return true;
        }

        if (!is_lhs_group && is_rhs_group) {
            return false;
        }

        // Sort by name
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs < rhs;
        } else {
            return lhs > rhs;
        }
    }

    bool _WatchIndexDataCompareByType(const std::string &lhs, const std::string &rhs,
                                      ModelSortOrder order) {
        const bool is_lhs_group = lhs == "Group";
        const bool is_rhs_group = rhs == "Group";

        // Folders come first
        if (is_lhs_group && !is_rhs_group) {
            return true;
        }

        if (!is_lhs_group && is_rhs_group) {
            return false;
        }

        // Sort by name
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs < rhs;
        } else {
            return lhs > rhs;
        }
    }

    WatchDataModel::_WatchIndexData &WatchDataModel::getIndexData_(const ModelIndex &index) {
        static _WatchIndexData invalid_data;

        auto it =
            std::find_if(m_index_map.begin(), m_index_map.end(), [&](const _WatchIndexData &data) {
                return data.m_self_uuid == index.inlineData();
            });

        if (it == m_index_map.end()) {
            return invalid_data;
        }

        return *it;
    }

    const WatchDataModel::_WatchIndexData &
    WatchDataModel::getIndexData_(const ModelIndex &index) const {
        static _WatchIndexData invalid_data;

        auto it =
            std::find_if(m_index_map.begin(), m_index_map.end(), [&](const _WatchIndexData &data) {
                return data.m_self_uuid == index.inlineData();
            });

        if (it == m_index_map.end()) {
            return invalid_data;
        }

        return *it;
    }

    void WatchDataModel::pruneRedundantIndexes(IDataModel::index_container &indexes) const {  // ---
        // Step 1: Build a fast-lookup set of all UUIDs in the list.
        // This is O(N) and has good cache locality (sequential read).
        // ---
        std::unordered_set<UUID64> list_uuids;
        list_uuids.reserve(indexes.size());
        for (const ModelIndex &index : indexes) {
            list_uuids.insert(index.getUUID());
        }

        // ---
        // Step 2: Use the "erase-remove" idiom.
        // std::remove_if partitions the vector by "shuffling" all elements
        // to be KEPT to the front. It returns an iterator to the new logical end.
        // This is one sequential pass, O(N).
        // ---
        auto new_end_it = std::remove_if(indexes.begin(), indexes.end(),
                                         // This lambda is called once for each index (N times).
                                         [this, &list_uuids](const ModelIndex &child_idx) {
                                             ModelIndex parent_idx = getParent_(child_idx);

                                             while (validateIndex(parent_idx)) {
                                                 // Check if this parent is *also* in our original
                                                 // list.
                                                 if (list_uuids.count(parent_idx.getUUID())) {
                                                     return true;
                                                 }

                                                 parent_idx = getParent_(parent_idx);
                                             }

                                             // We reached the root and found no parent in the
                                             // list. Keep this index.
                                             return false;
                                         });

        // ---
        // Step 3: Erase all the "removed" elements in one single operation.
        // This is O(K) where K is the number of elements removed.
        // ---
        indexes.erase(new_end_it, indexes.end());
    }

    WatchDataModel::~WatchDataModel() {
        m_index_map.clear();
        m_listeners.clear();
        m_running = false;
        m_watch_thread.join();
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
    }

    bool WatchDataModel::isIndexGroup(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return isIndexGroup_(index);
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
        default:
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
            watch_size = (u32)meta_type_size(watch_type);
        }

        return model.makeWatchIndex(name_str, watch_type, pointer_chain, watch_size,
                                    pointer_chain.size() > 1, value_base, row, parent, true);
    }

    Result<void, JSONError> WatchDataModel::loadFromDMEFile(const fs_path &path) {
        std::ifstream in_stream = std::ifstream(path, std::ios::in);

        json_t profile_json;
        return tryJSONWithResult(profile_json, [&](json_t &j) -> Result<void, JSONError> {
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
                            LoadWatchFromJSON(*this, child_watch, (int)child_row, group_idx);
                        if (validateIndex(new_idx)) {
                            child_row += 1;
                        }
                    }
                } else {
                    ModelIndex new_idx =
                        LoadWatchFromJSON(*this, watch_entry, (int)root_row, ModelIndex());
                    if (validateIndex(new_idx)) {
                        root_row += 1;
                    }
                }
            }

            return {};
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
    WatchDataModel::createMimeData(const IDataModel::index_container &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
    }

    bool WatchDataModel::insertMimeData(const ModelIndex &index, const MimeData &data,
                                        ModelInsertPolicy policy) {
        IDataModel::index_container indexes;

        {
            std::scoped_lock lock(m_mutex);
            indexes = insertMimeData_(index, data, policy);
        }

        for (const ModelIndex &index : indexes) {
            signalEventListeners(index, ModelEventFlags::EVENT_INSERT |
                                            ModelEventFlags::EVENT_INDEX_ADDED);
        }
        return !indexes.empty();
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

        for (const _WatchIndexData &data : m_index_map) {
            if (_WatchIndexDataIsGroup(data)) {
                delete data.m_group;
            } else {
                delete data.m_watch;
            }
        }

        m_index_map.clear();

        signalEventListeners(ModelIndex(), ModelEventFlags::EVENT_RESET);
    }

    ModelIndex WatchDataModel::makeWatchIndex(const std::string &name, MetaType type,
                                              const std::vector<u32> &pointer_chain, u32 size,
                                              bool is_pointer, WatchValueBase value_base,
                                              int64_t row, const ModelIndex &parent,
                                              bool find_unique_name) {
        std::scoped_lock lock(m_mutex);
        return makeWatchIndex_(name, type, pointer_chain, size, is_pointer, value_base, row, parent,
                               find_unique_name);
    }

    ModelIndex WatchDataModel::makeGroupIndex(const std::string &name, int64_t row,
                                              const ModelIndex &parent, bool find_unique_name) {
        std::scoped_lock lock(m_mutex);
        return makeGroupIndex_(name, row, parent, find_unique_name);
    }

    void WatchDataModel::addEventListener(UUID64 uuid, event_listener_t listener,
                                          int allowed_flags) {
        m_listeners[uuid] = {listener, allowed_flags};
    }

    void WatchDataModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    Result<void, SerialError> WatchDataModel::serialize(Serializer &out) const {
        std::scoped_lock lock(m_mutex);

        out.write<u32>(static_cast<u32>(m_index_map.size()));

        for (const _WatchIndexData &data : m_index_map) {
            // Header stub
            // ---
            out.write<UUID64>(data.m_self_uuid);
            out.write<UUID64>(data.m_parent);
            // ---

            if (_WatchIndexDataIsGroup(data)) {
                const std::string &name             = data.m_group->getName();
                const std::vector<UUID64> &children = data.m_group->getChildren();

                out.write<u8>((u8)_WatchIndexData::Type::GROUP);
                out.writeString(name);
                out.write<bool>(data.m_group->isLocked());
                out.write<u32>((u32)children.size());
                for (const UUID64 &child_uuid : children) {
                    out.write<UUID64>(child_uuid);
                }
            } else {
                const std::string &name         = data.m_watch->getWatchName();
                const std::vector<u32> &p_chain = data.m_watch->getPointerChain();
                const u32 size                  = data.m_watch->getWatchSize();
                const MetaType type             = data.m_watch->getWatchType();
                const bool locked               = data.m_watch->isLocked();
                const WatchValueBase value_base = data.m_value_base;

                u8 p_chain_len = (u8)p_chain.size();

                out.write<u8>((u8)_WatchIndexData::Type::WATCH);
                out.writeString(name);
                out.write<u8>(p_chain_len);
                for (u8 i = 0; i < p_chain_len; ++i) {
                    out.write<u32>(p_chain[i]);
                }
                out.write<u32>(size);
                out.write<u8>(static_cast<u8>(type));
                out.write<bool>(locked);
                out.write<u8>(static_cast<u8>(value_base));
            }
        }

        return Result<void, SerialError>();
    }

    Result<void, SerialError> WatchDataModel::deserialize(Deserializer &in) {
        std::scoped_lock lock(m_mutex);

        u32 index_count = in.read<u32>();

        for (u32 i = 0; i < index_count; ++i) {
            if (!in.good()) {
                break;
            }

            UUID64 uuid                = in.read<UUID64>();
            UUID64 parent              = in.read<UUID64>();
            _WatchIndexData::Type type = (_WatchIndexData::Type)in.read<u8>();

            if (type == _WatchIndexData::Type::GROUP) {
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

                _WatchIndexData data;
                data.m_parent     = parent;
                data.m_self_uuid  = uuid;
                data.m_type       = type;
                data.m_group      = group;
                data.m_value_base = WatchValueBase::BASE_DECIMAL;

                m_index_map.emplace_back(std::move(data));
            } else if (type == _WatchIndexData::Type::WATCH) {
                std::string name = in.readString();
                u8 p_chain_len   = in.read<u8>();

                std::vector<u32> p_chain;
                p_chain.reserve(p_chain_len);
                for (u8 i = 0; i < p_chain_len; ++i) {
                    p_chain.emplace_back(in.read<u32>());
                }

                u32 size                  = in.read<u32>();
                MetaType watch_type       = static_cast<MetaType>(in.read<u8>());
                bool locked               = in.read<bool>();
                WatchValueBase value_base = static_cast<WatchValueBase>(in.read<u8>());

                MetaWatch *watch = new MetaWatch(watch_type);
                watch->setWatchName(name);
                watch->setLocked(locked);

                bool watch_started;
                if (p_chain_len == 1) {
                    watch_started = watch->startWatch(p_chain[0], size);
                } else {
                    watch_started = watch->startWatch(p_chain, size);
                }

                if (!watch_started) {
                    TOOLBOX_WARN_V("[WATCHMODEL] Failed to start watch `{}' while deserializing!",
                                   name);
                    continue;
                }

                _WatchIndexData data;
                data.m_parent     = parent;
                data.m_self_uuid  = uuid;
                data.m_type       = type;
                data.m_watch      = watch;
                data.m_value_base = value_base;

                m_index_map.emplace_back(std::move(data));
            } else {
                return make_serial_error<void>(in,
                                               "Invalid type encountered during deserialization");
            }
        }

        signalEventListeners(ModelIndex(), ModelEventFlags::EVENT_INDEX_ADDED);
        return Result<void, SerialError>();
    }

    bool WatchDataModel::isIndexGroup_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;  // Invalid index
        }

        const _WatchIndexData &data = getIndexData_(index);
        if (data.m_self_uuid == 0) {
            return false;
        }
        return _WatchIndexDataIsGroup(data);
    }

    std::any WatchDataModel::getData_(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        const _WatchIndexData &data = getIndexData_(index);

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            switch (data.m_type) {
            case _WatchIndexData::Type::GROUP:
                return data.m_group->getName();
            case _WatchIndexData::Type::WATCH:
                return data.m_watch->getWatchName();
            }
            return {};
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_TYPE: {
            switch (data.m_type) {
            case _WatchIndexData::Type::GROUP:
                return "Group";
            case _WatchIndexData::Type::WATCH:
                return std::string(meta_type_name(data.m_watch->getWatchType()));
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_VALUE_META: {
            switch (data.m_type) {
            case _WatchIndexData::Type::GROUP:
                return {};
            case _WatchIndexData::Type::WATCH:
                return data.m_watch->getMetaValue();
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_ADDRESS: {
            switch (data.m_type) {
            case _WatchIndexData::Type::GROUP:
                return (u32)0;
            case _WatchIndexData::Type::WATCH:
                return data.m_watch->getWatchAddress();
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_LOCK: {
            switch (data.m_type) {
            case _WatchIndexData::Type::GROUP:
                return data.m_group->isLocked();
            case _WatchIndexData::Type::WATCH:
                return data.m_watch->isLocked();
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_SIZE: {
            switch (data.m_type) {
            case _WatchIndexData::Type::GROUP:
                return (u32)data.m_group->getChildCount();
            case _WatchIndexData::Type::WATCH:
                return (u32)data.m_watch->getWatchSize();
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_VIEW_BASE: {
            switch (data.m_type) {
            case _WatchIndexData::Type::GROUP:
                return {};
            case _WatchIndexData::Type::WATCH:
                return data.m_value_base;
            }
            return {};
        }
        case WatchDataRole::WATCH_DATA_ROLE_POINTER_CHAIN: {
            switch (data.m_type) {
            case _WatchIndexData::Type::GROUP:
                return {};
            case _WatchIndexData::Type::WATCH:
                return data.m_watch->getPointerChain();
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

        _WatchIndexData &idata = getIndexData_(index);

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY: {
            std::string name = std::any_cast<std::string>(data);
            switch (idata.m_type) {
            case _WatchIndexData::Type::GROUP:
                idata.m_group->setName(name);
                return;
            case _WatchIndexData::Type::WATCH:
                idata.m_watch->setWatchName(name);
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
            switch (idata.m_type) {
            case _WatchIndexData::Type::GROUP:
                return;
            case _WatchIndexData::Type::WATCH:
                u32 watch_size = idata.m_watch->getWatchSize();
                idata.m_watch->stopWatch();
                (void)idata.m_watch->startWatch(std::any_cast<u32>(data), watch_size);
                return;
            }
            return;
        }
        case WatchDataRole::WATCH_DATA_ROLE_LOCK: {
            bool locked = std::any_cast<bool>(data);
            switch (idata.m_type) {
            case _WatchIndexData::Type::GROUP:
                idata.m_group->setLocked(locked);
                return;
            case _WatchIndexData::Type::WATCH:
                idata.m_watch->setLocked(locked);
                return;
            }
            return;
        }
        case WatchDataRole::WATCH_DATA_ROLE_SIZE: {
            return;
        }
        case WatchDataRole::WATCH_DATA_ROLE_VIEW_BASE: {
            switch (idata.m_type) {
            case _WatchIndexData::Type::GROUP:
                return;
            case _WatchIndexData::Type::WATCH:
                idata.m_value_base = std::any_cast<WatchValueBase>(data);
                return;
            }
            return;
        }
        default:
            return;
        }
    }

    std::string WatchDataModel::findUniqueName_(const ModelIndex &parent,
                                                const std::string &name) const {
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

    ModelIndex WatchDataModel::getIndex_(const UUID64 &uuid) const {
        auto it =
            std::find_if(m_index_map.begin(), m_index_map.end(),
                         [&](const _WatchIndexData &data) { return data.m_self_uuid == uuid; });
        if (it == m_index_map.end()) {
            return ModelIndex();
        }

        ModelIndex index(getUUID(), it->m_self_uuid);
        index.setInlineData(it->m_self_uuid);
        return index;
    }

    ModelIndex WatchDataModel::getIndex_(int64_t row, int64_t column,
                                         const ModelIndex &parent) const {
        if (row < 0 || column < 0) {
            return ModelIndex();
        }

        if (!validateIndex(parent)) {
            size_t cur_row = 0;
            for (const _WatchIndexData &data : m_index_map) {
                if (data.m_parent == 0 && cur_row++ == row) {
                    ModelIndex index(getUUID(), data.m_self_uuid);
                    index.setInlineData(data.m_self_uuid);
                    return index;
                }
            }
            return ModelIndex();
        }

        const _WatchIndexData &parent_data = getIndexData_(parent);
        if (!_WatchIndexDataIsGroup(parent_data)) {
            return ModelIndex();
        }

        UUID64 child_uuid = parent_data.m_group->getChildUUID(row);
        if (child_uuid == 0) {
            return ModelIndex();
        }

        ModelIndex index(getUUID(), child_uuid);
        index.setInlineData(child_uuid);
        return index;
    }

    bool WatchDataModel::removeIndex_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        ModelIndex parent_idx = getParent_(index);
        if (validateIndex(parent_idx)) {
            _WatchIndexData &parent_data = getIndexData_(parent_idx);
            parent_data.m_group->removeChild(index.getUUID());
        }

        return std::erase_if(m_index_map, [&](const _WatchIndexData &data) {
                   return data.m_self_uuid == index.getUUID();
               }) > 0;
    }

    ModelIndex WatchDataModel::getParent_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        const _WatchIndexData &data = getIndexData_(index);
        if (data.m_parent == 0) {
            return ModelIndex();
        }

        return getIndex_(data.m_parent);
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
            for (const _WatchIndexData &data : m_index_map) {
                if (data.m_parent == 0) {
                    root_rows += 1;
                }
            }
            return root_rows;
        }

        const _WatchIndexData &data = getIndexData_(index);
        if (_WatchIndexDataIsGroup(data)) {
            return data.m_group->getChildCount();
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
            for (const _WatchIndexData &data : m_index_map) {
                if (data.m_self_uuid == index.inlineData()) {
                    return i;
                }
                i += (int)(data.m_parent == 0);
            }
            return -1;
        }

        const _WatchIndexData &parent_data = getIndexData_(parent);
        if (!_WatchIndexDataIsGroup(parent_data)) {
            return -1;
        }

        int64_t row = 0;
        for (const UUID64 &uuid : parent_data.m_group->getChildren()) {
            if (uuid == index.inlineData()) {
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
    WatchDataModel::createMimeData_(const IDataModel::index_container &indexes) const {
        ScopePtr<MimeData> new_data = make_scoped<MimeData>();

        IDataModel::index_container pruned_indexes = indexes;
        pruneRedundantIndexes(pruned_indexes);

        std::function<json_t(const ModelIndex &index)> jsonify_index =
            [&](const ModelIndex &index) -> json_t {
            json_t this_index;
            this_index["uuid"] = static_cast<u64>(index.getUUID());

            if (isIndexGroup_(index)) {
                json_t children;
                size_t row_count = getRowCount_(index);
                for (size_t i = 0; i < row_count; ++i) {
                    ModelIndex child_idx = getIndex_(i, 0, index);
                    children.emplace_back(jsonify_index(child_idx));
                }

                this_index["groupEntries"] = std::move(children);
                this_index["groupName"] =
                    std::any_cast<std::string>(getData_(index, ModelDataRole::DATA_ROLE_DISPLAY));
            } else {
                this_index["address"] =
                    std::any_cast<u32>(getData_(index, WatchDataRole::WATCH_DATA_ROLE_ADDRESS));
                WatchValueBase value_base = std::any_cast<WatchValueBase>(
                    getData_(index, WatchDataRole::WATCH_DATA_ROLE_VIEW_BASE));

                int base_index = 0;
                switch (value_base) {
                case WatchValueBase::BASE_DECIMAL:
                    base_index = 0;
                    break;
                case WatchValueBase::BASE_HEXADECIMAL:
                    base_index = 1;
                    break;
                case WatchValueBase::BASE_OCTAL:
                    base_index = 2;
                    break;
                case WatchValueBase::BASE_BINARY:
                    base_index = 3;
                    break;
                }

                this_index["baseIndex"] = base_index;
                this_index["label"] =
                    std::any_cast<std::string>(getData_(index, ModelDataRole::DATA_ROLE_DISPLAY));
                this_index["pointerOffsets"] = std::any_cast<std::vector<u32>>(
                    getData_(index, WatchDataRole::WATCH_DATA_ROLE_POINTER_CHAIN));

                MetaType watch_type =
                    std::any_cast<MetaValue>(
                        getData_(index, WatchDataRole::WATCH_DATA_ROLE_VALUE_META))
                        .type();

                int type_index = 0;
                bool unsgned   = false;
                switch (watch_type) {
                case MetaType::S8:
                    type_index = 0;
                    unsgned    = false;
                    break;
                case MetaType::U8:
                    type_index = 0;
                    unsgned    = true;
                    break;
                case MetaType::S16:
                    type_index = 1;
                    unsgned    = false;
                    break;
                case MetaType::U16:
                    type_index = 1;
                    unsgned    = true;
                    break;
                case MetaType::S32:
                    type_index = 2;
                    unsgned    = false;
                    break;
                case MetaType::U32:
                    type_index = 2;
                    unsgned    = true;
                    break;
                case MetaType::F32:
                    type_index = 3;
                    unsgned    = false;
                    break;
                case MetaType::F64:
                    type_index = 4;
                    unsgned    = false;
                    break;
                case MetaType::STRING:
                    type_index = 5;
                    break;
                case MetaType::UNKNOWN:
                    type_index = 6;
                    break;
                default:
                    type_index = 6;
                    break;
                }

                this_index["typeIndex"] = type_index;
                this_index["unsigned"]  = unsgned;
                this_index["size"] =
                    std::any_cast<u32>(getData_(index, WatchDataRole::WATCH_DATA_ROLE_SIZE));
            }
            return this_index;
        };

        json_t data_json;
        tryJSON(data_json, [&](json_t &j) {
            json_t root_json;

            size_t root_count = pruned_indexes.size();
            for (size_t i = 0; i < root_count; ++i) {
                ModelIndex child_idx = pruned_indexes[i];
                if (!validateIndex(child_idx)) {
                    TOOLBOX_WARN_V("Tried to create mime data for invalid index {}!", i);
                    continue;
                }
                root_json.emplace_back(jsonify_index(child_idx));
            }

            j["watchList"] = std::move(root_json);
        });

        std::string result = data_json.dump(4);
        new_data->set_text(result);

        return new_data;
    }

    IDataModel::index_container WatchDataModel::insertMimeData_(const ModelIndex &index,
                                                                const MimeData &data,
                                                                ModelInsertPolicy policy) {
        auto maybe_text = data.get_text();
        if (!maybe_text.has_value()) {
            return {};
        }

        const std::string &text_data = maybe_text.value();

        json_t j;
        try {
            j = json_t::parse(text_data);
        } catch (json_t::parse_error &e) {
            TOOLBOX_WARN_V("Failed to parse MimeData JSON: {}", e.what());
            return {};
        }

        if (!j.contains("watchList") || !j.at("watchList").is_array()) {
            TOOLBOX_WARN("MimeData is invalid or missing 'watchList' array.");
            return {};
        }

        const json_t &root_json = j.at("watchList");
        if (!root_json.is_array()) {
            TOOLBOX_WARN("MimeData is invalid or missing 'watchList' array.");
            return {};
        }

        std::function<ModelIndex(const json_t &, int64_t, const ModelIndex &)> insertJSONAtIndex =
            [&](const json_t &entry_json, int64_t row, const ModelIndex &parent_index) {
                UUID64 entry_uuid = UUID64(entry_json.at("uuid"));
                if (entry_json.contains("groupName")) {
                    const std::string &group_name = entry_json.at("groupName");
                    const json_t &group_entries   = entry_json.at("groupEntries");

                    ModelIndex self_index = makeGroupIndex_(group_name, row, parent_index);

                    if (!group_entries.is_array()) {
                        TOOLBOX_WARN("MimeData is invalid or missing 'groupEntries' array.");
                        return self_index;
                    }

                    int64_t subrow = 0;
                    for (const json_t &subentry_json : group_entries) {
                        ModelIndex child_index =
                            insertJSONAtIndex(subentry_json, subrow, self_index);
                        if (validateIndex(child_index)) {
                            subrow += 1;
                        }
                    }

                    return self_index;
                } else {
                    u32 address              = entry_json.at("address");
                    int base_index           = entry_json.at("baseIndex");
                    const std::string &label = entry_json.at("label");

                    std::vector<u32> p_chain;
                    p_chain.reserve(8);

                    const json_t &p_chain_json = entry_json.at("pointerOffsets");
                    if (p_chain_json.is_array()) {
                        for (u32 ofs : p_chain_json) {
                            p_chain.emplace_back(ofs);
                        }
                    }

                    int type_index = entry_json.at("typeIndex");
                    bool unsgned   = entry_json.at("unsigned");
                    u32 size       = entry_json.at("size");

                    // Invert base_index switch to get WatchValueBase
                    WatchValueBase value_base = WatchValueBase::BASE_DECIMAL;
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
                    default:
                        TOOLBOX_WARN("Unknown baseIndex found in MimeData.");
                        break;
                    }

                    // Invert type_index/unsigned switch to get MetaType
                    MetaType watch_type = MetaType::UNKNOWN;
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
                        break;  // unsigned is ignored
                    case 4:
                        watch_type = MetaType::F64;
                        break;  // unsigned is ignored
                    case 5:
                        watch_type = MetaType::STRING;
                        break;
                    case 6:  // Fallthrough
                    default:
                        watch_type = MetaType::UNKNOWN;
                        break;
                    }

                    ModelIndex this_idx;
                    if (p_chain.empty()) {
                        this_idx = makeWatchIndex_(label, watch_type, {address}, size, false,
                                                   value_base, row, parent_index, true);
                    } else {
                        this_idx =
                            makeWatchIndex_(label, watch_type, p_chain, size, p_chain.size() > 1,
                                            value_base, row, parent_index, true);
                    }

                    return this_idx;
                }
            };

        int64_t insert_row;
        ModelIndex insert_index;
        switch (policy) {
        case ModelInsertPolicy::INSERT_BEFORE: {
            insert_row   = getRow_(index);
            insert_index = getParent_(index);
            break;
        }
        case ModelInsertPolicy::INSERT_AFTER: {
            insert_row   = getRow_(index) + 1;
            insert_index = getParent_(index);
            break;
        }
        case ModelInsertPolicy::INSERT_CHILD: {
            insert_row   = getRowCount_(index);
            insert_index = index;
            break;
        }
        }

        IDataModel::index_container out_indexes;
        out_indexes.reserve(root_json.size());

        for (const json_t &entry_json : root_json) {
            ModelIndex child_index = insertJSONAtIndex(entry_json, insert_row, insert_index);
            if (validateIndex(child_index)) {
                insert_row += 1;
                out_indexes.emplace_back(std::move(child_index));
            }
        }

        return out_indexes;
    }

    bool WatchDataModel::canFetchMore_(const ModelIndex &index) { return true; }

    void WatchDataModel::fetchMore_(const ModelIndex &index) { return; }

    ModelIndex WatchDataModel::makeWatchIndex_(const std::string &name, MetaType type,
                                               const std::vector<u32> &pointer_chain, u32 size,
                                               bool is_pointer, WatchValueBase value_base,
                                               int64_t row, const ModelIndex &parent,
                                               bool find_unique_name) {
        if (row < 0 || pointer_chain.empty()) {
            return ModelIndex();
        }

        _WatchIndexData data;
        data.m_type       = _WatchIndexData::Type::WATCH;
        data.m_watch      = new MetaWatch(type);
        data.m_value_base = value_base;
        data.m_self_uuid  = UUID64();

        if (is_pointer) {
            if (!data.m_watch->startWatch(pointer_chain, size)) {
                delete data.m_watch;
                return ModelIndex();
            }
        } else {
            if (!data.m_watch->startWatch(pointer_chain[0], size)) {
                delete data.m_watch;
                return ModelIndex();
            }
        }

        std::string unique_name = find_unique_name ? findUniqueName_(parent, name) : name;
        data.m_group->setName(unique_name);
        data.m_group->setLocked(false);

        if (!validateIndex(parent)) {
            int64_t flat_row = 0;
            bool inserted    = false;
            if (m_index_map.empty() && row == 0) {
                m_index_map.emplace_back(data);
            } else {
                for (auto it = m_index_map.begin(); it != m_index_map.end(); ++it) {
                    if (it->m_parent == 0 && ++flat_row >= row) {
                        m_index_map.emplace(++it, data);
                        inserted = true;
                        break;
                    }
                }

                if (!inserted) {
                    TOOLBOX_ERROR("[WatchDataModel] Invalid row index!");
                    delete data.m_watch;
                    return ModelIndex();
                }
            }
        } else {
            _WatchIndexData &parent_data = getIndexData_(parent);
            if (!_WatchIndexDataIsGroup(parent_data)) {
                TOOLBOX_ERROR("[WatchDataModel] Invalid row index!");
                delete data.m_watch;
                return ModelIndex();
            }
            if (row > (int64_t)parent_data.m_group->getChildCount()) {
                TOOLBOX_ERROR_V("[WatchDataModel] Invalid row index: {} > {}", row,
                                parent_data.m_group->getChildCount());
                delete data.m_watch;
                return ModelIndex();
            }

            data.m_parent = parent.getUUID();

            if (_WatchIndexDataIsGroup(parent_data)) {
                WatchGroup *parent_group = parent_data.m_group;
                parent_group->insertChild(row, data.m_self_uuid);
            }

            m_index_map.emplace_back(data);
        }

        ModelIndex index(getUUID(), data.m_self_uuid);
        index.setInlineData(data.m_self_uuid);
        return index;
    }

    ModelIndex WatchDataModel::makeGroupIndex_(const std::string &name, int64_t row,
                                               const ModelIndex &parent, bool find_unique_name) {
        if (row < 0) {
            return ModelIndex();
        }

        _WatchIndexData &parent_data = getIndexData_(parent);
        if (validateIndex(parent)) {
            if (!_WatchIndexDataIsGroup(parent_data)) {
                TOOLBOX_ERROR("[WatchDataModel] Invalid row index!");
                return ModelIndex();
            }
            if (row > (int64_t)parent_data.m_group->getChildCount()) {
                TOOLBOX_ERROR_V("[WatchDataModel] Invalid row index: {} > {}", row,
                                parent_data.m_group->getChildCount());
                return ModelIndex();
            }
        }

        _WatchIndexData data;
        data.m_type      = _WatchIndexData::Type::GROUP;
        data.m_group     = new WatchGroup;
        data.m_self_uuid = UUID64();

        std::string unique_name = find_unique_name ? findUniqueName_(parent, name) : name;
        data.m_group->setName(unique_name);
        data.m_group->setLocked(false);

        if (!validateIndex(parent)) {
            m_index_map.emplace_back(data);
        } else {
            if (!_WatchIndexDataIsGroup(parent_data)) {
                TOOLBOX_ERROR("[WatchDataModel] Invalid row index!");
                delete data.m_group;
                return ModelIndex();
            }
            if (row > (int64_t)parent_data.m_group->getChildCount()) {
                TOOLBOX_ERROR_V("[WatchDataModel] Invalid row index: {} > {}", row,
                                parent_data.m_group->getChildCount());
                delete data.m_group;
                return ModelIndex();
            }

            data.m_parent = parent.getUUID();

            if (_WatchIndexDataIsGroup(parent_data)) {
                WatchGroup *parent_group = parent_data.m_group;
                parent_group->insertChild(row, data.m_self_uuid);
            }

            m_index_map.emplace_back(data);
        }

        ModelIndex index(getUUID(), data.m_self_uuid);
        index.setInlineData(data.m_self_uuid);
        return index;
    }

    void WatchDataModel::processWatches() {
        std::scoped_lock lock(m_mutex);

        for (_WatchIndexData &data : m_index_map) {
            if (!_WatchIndexDataIsGroup(data)) {
                data.m_watch->processWatch();
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

    void WatchDataModel::signalEventListeners(const ModelIndex &index, int flags) {
        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != ModelEventFlags::EVENT_NONE) {
                listener.first(index, (listener.second & flags));
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
                                             ModelEventFlags::EVENT_ANY);
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

        if (row < (int64_t)m_row_map[map_key].size()) {
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
        const IDataModel::index_container &indexes) const {
        IDataModel::index_container indexes_copy;

        for (const ModelIndex &idx : indexes) {
            indexes_copy.emplace_back(toSourceIndex(idx));
        }

        return m_source_model->createMimeData(indexes_copy);
    }

    bool WatchDataModelSortFilterProxy::insertMimeData(const ModelIndex &index,
                                                       const MimeData &data,
                                                       ModelInsertPolicy policy) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->insertMimeData(std::move(source_index), data, policy);
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

        return m_source_model->getIndex(index.getUUID());
    }

    ModelIndex WatchDataModelSortFilterProxy::toProxyIndex(const ModelIndex &index) const {
        if (!m_source_model->validateIndex(index)) {
            return ModelIndex();
        }

        ModelIndex proxy_index = ModelIndex(getUUID(), index.getUUID());
        proxy_index.setInlineData(index.inlineData());

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

        if (row < (int64_t)m_row_map[map_key].size()) {
            int64_t the_row = m_row_map[map_key][row];
            return toProxyIndex(m_source_model->getIndex(the_row, column, src_parent));
        }

        return ModelIndex();
    }

    bool WatchDataModelSortFilterProxy::isSrcFiltered_(const UUID64 &uuid) const {
        ModelIndex child_index = m_source_model->getIndex(uuid);

        std::string name = m_source_model->getDisplayText(child_index);
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
            if (isSrcFiltered_(root_s.getUUID())) {
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
                          ModelIndex lhs_idx = m_source_model->getIndex(lhs);
                          ModelIndex rhs_idx = m_source_model->getIndex(lhs);
                          bool is_lhs_grp    = m_source_model->isIndexGroup(lhs_idx);
                          bool is_rhs_grp    = m_source_model->isIndexGroup(rhs_idx);
                          if (is_lhs_grp && !is_rhs_grp) {
                              return true;
                          }
                          if (is_rhs_grp && !is_lhs_grp) {
                              return false;
                          }
                          return _WatchIndexDataCompareByName(
                              m_source_model->getDisplayText(lhs_idx),
                              m_source_model->getDisplayText(rhs_idx), m_sort_order);
                      });
            break;
        }
        case WatchModelSortRole::SORT_ROLE_TYPE: {
            std::sort(proxy_children.begin(), proxy_children.end(),
                      [&](const UUID64 &lhs, const UUID64 &rhs) {
                          ModelIndex lhs_idx = m_source_model->getIndex(lhs);
                          ModelIndex rhs_idx = m_source_model->getIndex(lhs);
                          bool is_lhs_grp    = m_source_model->isIndexGroup(lhs_idx);
                          bool is_rhs_grp    = m_source_model->isIndexGroup(rhs_idx);
                          if (is_lhs_grp && !is_rhs_grp) {
                              return true;
                          }
                          if (is_rhs_grp && !is_lhs_grp) {
                              return false;
                          }
                          return _WatchIndexDataCompareByType(m_source_model->getWatchType(lhs_idx),
                                                              m_source_model->getWatchType(rhs_idx),
                                                              m_sort_order);
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

    void WatchDataModelSortFilterProxy::watchDataUpdateEvent(const ModelIndex &index, int flags) {
        if ((flags & ModelEventFlags::EVENT_ANY) == ModelEventFlags::EVENT_NONE) {
            return;
        }

        if ((flags & ModelEventFlags::EVENT_RESET) == ModelEventFlags::EVENT_RESET) {
            reset();
        }

        ModelIndex proxy_index = toProxyIndex(index);
        if (!validateIndex(proxy_index)) {
            return;
        }

        if ((flags & ModelEventFlags::EVENT_INDEX_ADDED) != ModelEventFlags::EVENT_NONE) {
            cacheIndex(getParent(proxy_index));
            return;
        }

        if ((flags & ModelEventFlags::EVENT_INDEX_MODIFIED) != ModelEventFlags::EVENT_NONE) {
            cacheIndex(proxy_index);
            return;
        }

        {
            std::scoped_lock lock(m_cache_mutex);

            m_filter_map.clear();
            m_row_map.clear();
        }
    }

}  // namespace Toolbox