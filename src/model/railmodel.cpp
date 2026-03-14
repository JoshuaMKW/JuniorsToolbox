#include <algorithm>
#include <compare>
#include <set>

#include "model/railmodel.hpp"
#include "project/config.hpp"
#include "rail/rail.hpp"
#include "scene/hierarchy.hpp"

#include <gcm.hpp>

using namespace std::chrono;

namespace Toolbox {

    struct _RailIndexData {
        UUID64 m_self_uuid               = 0;
        RefPtr<const ImageHandle> m_icon = nullptr;

        bool hasValue() const { return static_cast<bool>(m_rail); }

        RailData::rail_ptr_t getRail() const { return m_rail; }
        Rail::Rail::node_ptr_t getNode() const { return m_node; }

        void setRail(RailData::rail_ptr_t rail) { m_rail = rail; }
        void setNode(Rail::Rail::node_ptr_t node) { m_node = node; }

        std::strong_ordering operator<=>(const _RailIndexData &rhs) const {
            if (RailData::rail_ptr_t rail = getRail()) {
                if (RailData::rail_ptr_t rhs_rail = rhs.getRail()) {
                    return rail->name() <=> rhs_rail->name();
                }
                return std::strong_ordering::greater;
            }

            if (RailData::rail_ptr_t rhs_rail = rhs.getRail()) {
                return std::strong_ordering::less;
            }
            return std::strong_ordering::equivalent;
        }

    private:
        RailData::rail_ptr_t m_rail;
        Rail::Rail::node_ptr_t m_node;
    };

    RailObjModel::~RailObjModel() { reset(); }

    void RailObjModel::initialize(const RailData &data) {
        m_rail_indexes.clear();
        m_node_list_map.clear();

        int64_t row = 0;
        for (RailData::rail_ptr_t rail : data.rails()) {
            ModelIndex index = insertRail(rail, row++);
        }
    }

    ScopePtr<RailData> RailObjModel::bakeToRailData() const {
        ScopePtr<RailData> output = make_scoped<RailData>();
        
        const size_t rail_count = getRowCount(ModelIndex());
        for (size_t i = 0; i < rail_count; ++i) {
            ModelIndex rail_index = getIndex(i, 0);
            output->addRail(*getRailRef(rail_index));
        }

        return output;
    }

    bool RailObjModel::isIndexRailNode(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;
        }
        _RailIndexData *data = index.data<_RailIndexData>();
        return data->getNode() != nullptr;
    }

    bool RailObjModel::isIndexRail(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;
        }
        _RailIndexData *data = index.data<_RailIndexData>();
        return data->getRail() != nullptr && data->getNode() == nullptr;
    }

    std::any RailObjModel::getData(const ModelIndex &index, int role) const {
        std::scoped_lock lock(m_mutex);
        return getData_(index, role);
    }

    void RailObjModel::setData(const ModelIndex &index, std::any data, int role) {
        std::scoped_lock lock(m_mutex);
        setData_(index, data, role);
    }

    std::string RailObjModel::findUniqueName(const ModelIndex &index,
                                             const std::string &name) const {
        std::scoped_lock lock(m_mutex);
        return findUniqueName_(index, name);
    }

    ModelIndex RailObjModel::getIndex(RailData::rail_ptr_t rail) const {
        if (!rail) {
            return ModelIndex();
        }

        std::scoped_lock lock(m_mutex);
        return getIndex_(rail);
    }

    ModelIndex RailObjModel::getIndex(Rail::Rail::node_ptr_t node) const {
        if (!node) {
            return ModelIndex();
        }

        std::scoped_lock lock(m_mutex);
        return getIndex_(node);
    }

    ModelIndex RailObjModel::getIndex(const UUID64 &uuid) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(uuid);
    }

    ModelIndex RailObjModel::getIndex(int64_t row, int64_t column, const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(row, column, parent);
    }

    bool RailObjModel::removeIndex(const ModelIndex &index) {
        bool result;

        const Signal index_signal =
            createSignalForIndex_(index, ModelEventFlags::EVENT_INDEX_REMOVED);

        signalEventListeners(index_signal.first, index_signal.second | ModelEventFlags::EVENT_PRE);

        {
            std::scoped_lock lock(m_mutex);
            result = removeIndex_(index);
        }

        if (result) {
            signalEventListeners(index_signal.first, index_signal.second |
                                                         ModelEventFlags::EVENT_POST |
                                                         ModelEventFlags::EVENT_SUCCESS);
        } else {
            signalEventListeners(index_signal.first,
                                 index_signal.second | ModelEventFlags::EVENT_POST);
        }

        return result;
    }

    ModelIndex RailObjModel::insertRail(RailData::rail_ptr_t rail, int64_t row) {
        std::unique_lock lock(m_mutex);
        return insertRail_(rail, row);
    }

    ModelIndex RailObjModel::insertRailNode(Rail::Rail::node_ptr_t node, int64_t row,
                                            const ModelIndex &parent) {
        std::unique_lock lock(m_mutex);
        return insertRailNode_(node, row, parent);
    }

    ModelIndex RailObjModel::getParent(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getParent_(index);
    }

    ModelIndex RailObjModel::getSibling(int64_t row, int64_t column,
                                        const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getSibling_(row, column, index);
    }

    size_t RailObjModel::getColumnCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumnCount_(index);
    }

    size_t RailObjModel::getRowCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRowCount_(index);
    }

    int64_t RailObjModel::getColumn(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumn_(index);
    }

    int64_t RailObjModel::getRow(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRow_(index);
    }

    bool RailObjModel::hasChildren(const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return hasChildren_(parent);
    }

    ScopePtr<MimeData>
    RailObjModel::createMimeData(const IDataModel::index_container &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
    }

    Result<IDataModel::index_container>
    RailObjModel::insertMimeData(const ModelIndex &index, const MimeData &data,
                                      ModelInsertPolicy policy) {
        Result<IDataModel::index_container> result;

        const Signal pre_signal = createSignalForIndex_(index, ModelEventFlags::EVENT_INSERT);

        signalEventListeners(pre_signal.first, pre_signal.second | ModelEventFlags::EVENT_PRE);

        {
            std::scoped_lock lock(m_mutex);
            result = insertMimeData_(index, data, policy);
        }

        if (result) {
            for (const ModelIndex &new_index : result.value()) {
                Signal index_signal =
                    createSignalForIndex_(new_index, ModelEventFlags::EVENT_INDEX_ADDED);
                signalEventListeners(index_signal.first, index_signal.second |
                                                             ModelEventFlags::EVENT_POST |
                                                             ModelEventFlags::EVENT_SUCCESS);
            }
            signalEventListeners(pre_signal.first, pre_signal.second |
                                                        ModelEventFlags::EVENT_POST |
                                                        ModelEventFlags::EVENT_SUCCESS);
        } else {
            signalEventListeners(pre_signal.first,
                                 pre_signal.second | ModelEventFlags::EVENT_POST);
        }

        return result;
    }

    std::vector<std::string> RailObjModel::getSupportedMimeTypes() const {
        return std::vector<std::string>();
    }

    bool RailObjModel::canFetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return canFetchMore_(index);
    }

    void RailObjModel::fetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return fetchMore_(index);
    }

    void RailObjModel::reset() {
        std::scoped_lock lock(m_mutex);

        for (auto &[key, index] : m_index_map) {
            _RailIndexData *p = index.data<_RailIndexData>();
            if (p) {
                delete p;
            }
        }

        m_rail_indexes.clear();
        m_node_list_map.clear();
        m_index_map.clear();
    }

    void RailObjModel::addEventListener(UUID64 uuid, event_listener_t listener, int allowed_flags) {
        m_listeners[uuid] = {listener, allowed_flags};
    }

    void RailObjModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    RailObjModel::Signal RailObjModel::createSignalForIndex_(const ModelIndex &index,
                                                             ModelEventFlags base_event) const {
        return {index, (int)base_event};
    }

    std::any RailObjModel::getData_(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        _RailIndexData *data = index.data<_RailIndexData>();
        if (!data->hasValue()) {
            return {};
        }

        RailData::rail_ptr_t rail = data->getRail();
        TOOLBOX_CORE_ASSERT(rail && "Rail pointer is null for index");

        if (Rail::Rail::node_ptr_t node = data->getNode()) {
            TOOLBOX_CORE_ASSERT(node->getRailUUID() == rail->getUUID() &&
                                "RailNode's parent Rail UUID does not match Rail UUID for index");
            TOOLBOX_CORE_ASSERT(node && "RailNode pointer is null for index");

            switch (role) {
            case ModelDataRole::DATA_ROLE_DISPLAY: {
                std::optional<size_t> node_index = rail->getNodeIndex(node);
                if (!node_index.has_value()) {
                    return "Invalid Node";
                }
                return std::format("Node {}", node_index.value());
            }
            case ModelDataRole::DATA_ROLE_TOOLTIP:
                return "Tooltip unimplemented!";
            case ModelDataRole::DATA_ROLE_DECORATION: {
                return data->m_icon;
            }
            case RailObjDataRole::RAIL_DATA_ROLE_RAIL_TYPE: {
                return {};
            }
            case RailObjDataRole::RAIL_DATA_ROLE_RAIL_KEY: {
                std::optional<size_t> node_index = rail->getNodeIndex(node);
                if (!node_index.has_value()) {
                    return {};
                }
                return std::format("Node {}", node_index.value());
            }
            case RailObjDataRole::RAIL_DATA_ROLE_RAIL_GAME_ADDR: {
                // TODO: Currently unimplemented
                return static_cast<u32>(0);
            }
            case RailObjDataRole::RAIL_DATA_ROLE_RAIL_REF: {
                return rail;  // We do this since nodes are complex and may require direct
                              // interaction
            }
            case RailObjDataRole::RAIL_DATA_ROLE_RAIL_NODE_REF: {
                return node;  // We do this since nodes are complex and may require direct
                              // interaction
            }
            default:
                return {};
            }
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            return rail->name();
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {

            return data->m_icon;
        }
        case RailObjDataRole::RAIL_DATA_ROLE_RAIL_TYPE: {
            return rail->name().starts_with("S_") ? "Spline Rail" : "Linear Rail";
        }
        case RailObjDataRole::RAIL_DATA_ROLE_RAIL_KEY: {
            return rail->name();
        }
        case RailObjDataRole::RAIL_DATA_ROLE_RAIL_GAME_ADDR: {
            // TODO: Currently unimplemented
            return static_cast<u32>(0);
        }
        case RailObjDataRole::RAIL_DATA_ROLE_RAIL_REF: {
            return rail;  // We do this since rails are complex and may require direct
                          // interaction
        }
        default:
            return {};
        }
    }

    void RailObjModel::setData_(const ModelIndex &index, std::any data, int role) const {
        if (!validateIndex(index)) {
            return;
        }

        _RailIndexData *idata = index.data<_RailIndexData>();
        if (!data.has_value()) {
            return;
        }

        RailData::rail_ptr_t rail = idata->getRail();
        TOOLBOX_CORE_ASSERT(rail && "Rail pointer is null for index");

        if (Rail::Rail::node_ptr_t node = idata->getNode()) {
            TOOLBOX_CORE_ASSERT(node->getRailUUID() == rail->getUUID() &&
                                "RailNode's parent Rail UUID does not match Rail UUID for index");
            TOOLBOX_CORE_ASSERT(node && "RailNode pointer is null for index");
            return;
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            rail->setName(std::any_cast<std::string>(data));
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return;
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return;
        }
        case RailObjDataRole::RAIL_DATA_ROLE_RAIL_TYPE: {
            return;
        }
        case RailObjDataRole::RAIL_DATA_ROLE_RAIL_KEY: {
            rail->setName(std::any_cast<std::string>(data));
        }
        case RailObjDataRole::RAIL_DATA_ROLE_RAIL_GAME_ADDR: {
            return;
        }
        case RailObjDataRole::RAIL_DATA_ROLE_RAIL_REF: {
            return;
        }
        default:
            return;
        }

        return;
    }

    std::string RailObjModel::findUniqueName_(const ModelIndex &index,
                                              const std::string &name) const {
        std::string result_name = name;
        size_t collisions       = 0;
        std::vector<std::string> child_paths;

        for (size_t i = 0; i < getRowCount_(index); ++i) {
            ModelIndex child = getIndex_(i, 0, index);

            std::string child_name = std::any_cast<std::string>(
                getData_(getIndex_(i, 0, index), ModelDataRole::DATA_ROLE_DISPLAY));
            child_paths.emplace_back(std::move(child_name));
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

    ModelIndex RailObjModel::getIndex_(RailData::rail_ptr_t rail) const {
        for (ModelIndex index : m_rail_indexes) {
            _RailIndexData *data = index.data<_RailIndexData>();
            if (!data->hasValue()) {
                continue;
            }

            // Skip indexes that reference nodes when we're looking for rails, and vice versa
            Rail::Rail::node_ptr_t node = data->getNode();
            if (node) {
                continue;
            }

            RailData::rail_ptr_t other = data->getRail();
            if (other->getUUID() == rail->getUUID()) {
                return index;
            }
        }
        return ModelIndex();
    }

    ModelIndex RailObjModel::getIndex_(Rail::Rail::node_ptr_t node) const {
        for (const auto &[k, node_list] : m_node_list_map) {
            if (k != node->getRailUUID()) {
                continue;
            }

            for (ModelIndex node_index : node_list) {
                _RailIndexData *data = node_index.data<_RailIndexData>();
                if (!data->hasValue()) {
                    continue;
                }

                // Skip indexes that reference rails when we're looking for nodes, and vice versa
                Rail::Rail::node_ptr_t other = data->getNode();
                if (!other) {
                    continue;
                }

                if (other->getUUID() == node->getUUID()) {
                    return node_index;
                }
            }
        }
        return ModelIndex();
    }

    ModelIndex RailObjModel::getIndex_(const UUID64 &uuid) const {
        if (!m_index_map.contains(uuid)) {
            return ModelIndex();
        }
        return m_index_map.at(uuid);
    }

    ModelIndex RailObjModel::getIndex_(int64_t row, int64_t column,
                                       const ModelIndex &parent) const {
        if (!validateIndex(parent)) {
            if (row >= m_rail_indexes.size() || row < 0 || column != 0) {
                return ModelIndex();
            }
            return m_rail_indexes[row];
        }

        _RailIndexData *parent_data = parent.data<_RailIndexData>();
        if (!parent_data->hasValue()) {
            return ModelIndex();
        }

        RailData::rail_ptr_t parent_rail          = parent_data->getRail();
        const std::vector<ModelIndex> &rail_nodes = m_node_list_map[parent_rail->getUUID()];

        if (row >= rail_nodes.size() || row < 0 || column != 0) {
            return ModelIndex();
        }

        return rail_nodes[row];
    }

    bool RailObjModel::removeIndex_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        _RailIndexData *this_data = index.data<_RailIndexData>();
        if (!this_data->hasValue()) {
            return false;
        }

        ModelIndex parent           = getParent_(index);
        _RailIndexData *parent_data = validateIndex(parent) ? parent.data<_RailIndexData>()
                                                            : nullptr;

        if (parent_data) {
            int64_t row = getRow_(index);

            RailData::rail_ptr_t parent_rail = parent_data->getRail();
            bool result                      = parent_rail->removeNode(this_data->getNode());
            if (!result) {
                TOOLBOX_ERROR_V("[RAILMODEL] Failed to remove node index from rail \"{}\"!",
                                parent_rail->name());
                return false;
            }

            // Remove the index from the node list map
            std::vector<ModelIndex> &node_list = m_node_list_map[parent_rail->getUUID()];
            node_list.erase(std::remove(node_list.begin(), node_list.end(), index),
                            node_list.end());

            // Finally adjust connections
            for (int64_t i = 0; i < getRowCount_(parent); ++i) {
                ModelIndex node_index             = getIndex_(i, 0, parent);
                Rail::Rail::node_ptr_t other_node = node_index.data<_RailIndexData>()->getNode();
                for (u16 j = 0; j < other_node->getConnectionCount();) {
                    auto val_result = other_node->getConnectionValue(j);
                    if (!val_result) {
                        TOOLBOX_ERROR_V(
                            "[RAILMODEL] Failed to get connection value for node \"{}\"!",
                            other_node->getUUID());
                        ++j;
                        continue;
                    }

                    u16 connection_value = val_result.value();
                    if (connection_value == row) {
                        parent_rail->removeConnection(i, j);
                        continue;
                    }

                    if (connection_value > row) {
                        parent_rail->replaceConnection(i, j,
                                                       static_cast<size_t>(connection_value - 1));
                        ++j;
                        continue;
                    }

                    ++j;
                }
            }
        } else {
            RailData::rail_ptr_t this_rail = this_data->getRail();
            m_node_list_map.erase(this_rail->getUUID());
            m_rail_indexes.erase(std::remove(m_rail_indexes.begin(), m_rail_indexes.end(), index),
                                 m_rail_indexes.end());
        }

        const UUID64 uuid = index.getUUID();
        m_index_map.erase(uuid);
        return true;
    }

    ModelIndex RailObjModel::getParent_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        _RailIndexData *data = index.data<_RailIndexData>();
        if (!data->hasValue()) {
            return ModelIndex();
        }

        RailData::rail_ptr_t rail   = data->getRail();
        Rail::Rail::node_ptr_t node = data->getNode();

        if (node) {
            return getIndex_(rail);
        }

        return ModelIndex();
    }

    ModelIndex RailObjModel::getSibling_(int64_t row, int64_t column,
                                         const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        ModelIndex parent_index = getParent_(index);
        return getIndex_(row, column, parent_index);
    }

    size_t RailObjModel::getColumnCount_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 1;
        }

        return 1;
    }

    size_t RailObjModel::getRowCount_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return m_rail_indexes.size();
        }

        _RailIndexData *data = index.data<_RailIndexData>();
        if (!data->hasValue()) {
            return 0;
        }

        return m_node_list_map[data->getRail()->getUUID()].size();
    }

    int64_t RailObjModel::getColumn_(const ModelIndex &index) const {
        return validateIndex(index) ? 0 : -1;
    }

    int64_t RailObjModel::getRow_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return -1;
        }

        _RailIndexData *data = index.data<_RailIndexData>();
        if (!data->hasValue()) {
            return -1;
        }

        RailData::rail_ptr_t rail   = data->getRail();
        Rail::Rail::node_ptr_t node = data->getNode();

        if (node) {
            const std::vector<ModelIndex> &rail_nodes = m_node_list_map[rail->getUUID()];
            auto it =
                std::find_if(rail_nodes.begin(), rail_nodes.end(),
                             [&](const ModelIndex &node_index) { return node_index == index; });
            if (it == rail_nodes.end()) {
                return -1;
            }
            return std::distance(rail_nodes.begin(), it);
        }

        auto it = std::find_if(m_rail_indexes.begin(), m_rail_indexes.end(),
                               [&](const ModelIndex &rail_index) { return rail_index == index; });
        if (it == m_rail_indexes.end()) {
            return -1;
        }

        return std::distance(m_rail_indexes.begin(), it);
    }

    bool RailObjModel::hasChildren_(const ModelIndex &parent) const { return false; }

    ScopePtr<MimeData>
    RailObjModel::createMimeData_(const IDataModel::index_container &indexes) const {
        ScopePtr<MimeData> new_data = make_scoped<MimeData>();

        // Strips and validates indexes, ensuring either all nodes or all rails are included, and
        // that there are no duplicates or invalid indexes.
        IDataModel::index_container pruned_indexes = indexes;
        pruneRedundantIndexes(pruned_indexes);

        std::function<Result<void, SerialError>(const ModelIndex &, Serializer &)> serialize_index =
            [&](const ModelIndex &index, Serializer &out) -> Result<void, SerialError> {
            out.write(static_cast<u64>(index.getUUID()));

            _RailIndexData *data        = index.data<_RailIndexData>();
            RailData::rail_ptr_t rail   = data->getRail();
            Rail::Rail::node_ptr_t node = data->getNode();

            const bool is_node = node != nullptr;

            out.write<bool>(is_node);
            out.write<u64>(rail->getUUID());

            if (is_node) {
                out.write<u64>(node->getUUID());
                node->serialize(out);
            } else {
                rail->serialize(out);
            }

            return {};
        };

        std::stringstream outstr;
        Serializer out(outstr.rdbuf());

        out.write<u32>(static_cast<u32>(pruned_indexes.size()));

        for (const ModelIndex &index : pruned_indexes) {
            if (!validateIndex(index)) {
                TOOLBOX_WARN_V("Tried to create mime data for invalid index {}!", index.getUUID());
                continue;
            }

            serialize_index(index, out);
        }

        std::string_view data_view = outstr.view();

        Buffer mime_data;
        mime_data.alloc(data_view.size());
        std::memcpy(mime_data.buf(), data_view.data(), data_view.size());

        new_data->set_data("toolbox/scene/rail_model", std::move(mime_data));

        return new_data;
    }

    Result<IDataModel::index_container>
    RailObjModel::insertMimeData_(const ModelIndex &index, const MimeData &data,
                                       ModelInsertPolicy policy) {
        if (!data.has_format("toolbox/scene/rail_model")) {
            return make_error<std::vector<ModelIndex>>("RailObjModel", "Provided MIME data does not have RailObjModel data!");
        }

        Buffer mime_data = data.get_data("toolbox/scene/rail_model").value();

        std::stringstream instr(std::string(mime_data.buf<char>(), mime_data.size()));
        Deserializer in(instr.rdbuf());

        std::vector<ModelIndex> inserted_indexes;
        int64_t spill_row = -1;

        auto deserialize_index = [&](int64_t row, const ModelIndex &parent,
                                     Deserializer &in) -> Result<void, SerialError> {
            UUID64 index_uuid = in.read<u64>();

            const bool is_node = in.read<bool>();
            UUID64 rail_uuid   = in.read<u64>();

            if (is_node) {
                UUID64 node_uuid            = in.read<u64>();
                Rail::Rail::node_ptr_t node = make_referable<Rail::RailNode>();
                auto result                 = node->deserialize(in);
                if (!result) {
                    return result;
                }
                ModelIndex node_index = insertRailNode_(node, row, parent);
                if (!isIndexRailNode(node_index)) {
                    return make_serial_error<void>(in, "Failed to insert rail node into model!");
                }

                inserted_indexes.push_back(node_index);
            } else {
                #if 0
                if (isIndexRail(parent)) {
                    return make_serial_error<void>(in,
                                                   "Cannot insert rail as child of another rail!");
                }
                #else
                if (isIndexRail(parent)) {
                    row = spill_row++;
                }
                #endif

                RailData::rail_ptr_t rail = make_referable<Rail::Rail>("_dummy_name");
                auto result               = rail->deserialize(in);
                if (!result) {
                    return result;
                }

                // Get a unique name
                std::string new_name = findUniqueName_(ModelIndex(), rail->name());
                rail->setName(new_name);

                ModelIndex rail_index = insertRail_(rail, row);
                if (!isIndexRail(rail_index)) {
                    return make_serial_error<void>(in, "Failed to insert rail into model!");
                }

                inserted_indexes.push_back(rail_index);
            }

            return {};
        };

        if (policy == ModelInsertPolicy::INSERT_CHILD) {
            if (isIndexRailNode(index)) {
                // Inserting as a child of a node is not allowed, so we treat this as an "after"
                // insertion
                policy = ModelInsertPolicy::INSERT_AFTER;
            }
        }

        ModelIndex insert_index;
        int64_t insert_row;
        switch (policy) {
        case ModelInsertPolicy::INSERT_BEFORE:
            insert_row = getRow_(index);
            if (insert_row == -1) {
                return make_error<std::vector<ModelIndex>>(
                    "RailObjModel", "Failed to retrieve the row for the insert index");
            }
            insert_index = getParent_(index);
            spill_row    = getRow_(insert_index);
            break;
        case ModelInsertPolicy::INSERT_AFTER:
            insert_row = getRow_(index);
            if (insert_row == -1) {
                return make_error<std::vector<ModelIndex>>(
                    "RailObjModel", "Failed to retrieve the row for the insert index");
            }
            insert_row += 1;
            insert_index = getParent_(index);
            spill_row    = getRow_(insert_index) + 1;
            break;
        case ModelInsertPolicy::INSERT_CHILD:
            insert_row   = getRowCount_(index);
            insert_index = index;
            spill_row    = getRow_(index) + 1;
            break;
        }

        u32 node_count = in.read<u32>();

        for (u32 i = 0; i < node_count; ++i) {
            auto result = deserialize_index(insert_row + i, insert_index, in);
            if (!result) {
                TOOLBOX_ERROR_V("Failed to deserialize index from mime data: {}",
                                result.error().m_message);
                return make_error<std::vector<ModelIndex>>("RailObjModel", result.error().m_message);
            }
        }

        return inserted_indexes;
    }

    bool RailObjModel::canFetchMore_(const ModelIndex &index) const { return false; }

    void RailObjModel::fetchMore_(const ModelIndex &index) const { return; }

    ModelIndex RailObjModel::insertRail_(RailData::rail_ptr_t rail, int64_t row) {
        ModelIndex ret = makeIndex(rail, row);
        if (!validateIndex(ret)) {
            TOOLBOX_ERROR_V("[RAILMODEL] Failed to create index for rail \"{}\"!", rail->name());
            return ModelIndex();
        }

        int64_t subrow = 0;
        for (Rail::Rail::node_ptr_t rail_node : rail->nodes()) {
            ModelIndex node = makeIndex(rail_node, subrow++, ret);
            if (!validateIndex(node)) {
                TOOLBOX_ERROR_V("[RAILMODEL] Failed to insert node into model for rail \"{}\"!",
                                rail->name());
                (void)removeIndex_(ret);
                return ModelIndex();
            }
        }

        return ret;
    }

    ModelIndex RailObjModel::insertRailNode_(Rail::Rail::node_ptr_t node, int64_t row,
                                             const ModelIndex &parent) {
        ModelIndex new_index = makeIndex(node, row, parent);
        if (!validateIndex(new_index)) {
            return ModelIndex();
        }

        _RailIndexData *parent_data = validateIndex(parent) ? parent.data<_RailIndexData>()
                                                            : nullptr;
        if (parent_data) {
            RailData::rail_ptr_t rail = parent_data->getRail();
            auto result               = rail->insertNode(row, node);
            if (!result) {
                TOOLBOX_ERROR_V("[RAILMODEL] Failed to create index");
                (void)removeIndex_(new_index);
                return ModelIndex();
            }

            for (int64_t i = 0; i < getRowCount_(parent); ++i) {
                ModelIndex node_index = getIndex_(i, 0, parent);
                Rail::Rail::node_ptr_t other_node = node_index.data<_RailIndexData>()->getNode();
                for (u16 j = 0; j < other_node->getConnectionCount(); ++j) {
                    auto val_result = other_node->getConnectionValue(j);
                    if (!val_result) {
                        TOOLBOX_ERROR_V(
                            "[RAILMODEL] Failed to get connection value for node \"{}\"!",
                            other_node->getUUID());
                        continue;
                    }

                    u16 connection_value = val_result.value();
                    if (connection_value >= row) {
                        rail->replaceConnection(i, j, static_cast<size_t>(connection_value + 1));
                    }
                }
            }
        }

        return new_index;
    }

    ModelIndex RailObjModel::makeIndex(RailData::rail_ptr_t rail, int64_t row) const {
        if (row < 0 || row > m_index_map.size()) {
            return ModelIndex();
        }

        ModelIndex new_index(getUUID());

        _RailIndexData *new_data = new _RailIndexData;
        new_data->m_self_uuid    = new_index.getUUID();
        new_data->m_icon         = nullptr;
        new_data->setRail(rail);

        new_index.setData(new_data);

        m_index_map[new_index.getUUID()] = new_index;
        m_rail_indexes.insert(m_rail_indexes.begin() + row, new_index);

        return new_index;
    }

    ModelIndex RailObjModel::makeIndex(Rail::Rail::node_ptr_t node, int64_t row,
                                       const ModelIndex &parent) const {
        _RailIndexData *parent_data = validateIndex(parent) ? parent.data<_RailIndexData>()
                                                            : nullptr;
        if (!parent_data || !parent_data->hasValue()) {
            return ModelIndex();
        }

        RailData::rail_ptr_t rail          = parent_data->getRail();
        std::vector<ModelIndex> &node_list = m_node_list_map[rail->getUUID()];

        ModelIndex new_index(getUUID());

        _RailIndexData *new_data = new _RailIndexData;
        new_data->m_self_uuid    = new_index.getUUID();
        new_data->m_icon         = nullptr;
        new_data->setRail(rail);
        new_data->setNode(node);

        new_index.setData(new_data);

        m_index_map[new_index.getUUID()] = new_index;
        node_list.insert(node_list.begin() + row, new_index);

        return new_index;
    }

    void RailObjModel::signalEventListeners(const ModelIndex &index, int flags) {
        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != ModelEventFlags::EVENT_NONE) {
                listener.first(index, (listener.second & flags));
            }
        }
    }

    void RailObjModel::pruneRedundantIndexes(IDataModel::index_container &indexes) const {  // ---
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

}  // namespace Toolbox
