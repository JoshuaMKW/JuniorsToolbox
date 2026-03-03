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

        bool hasValue() const {
            return std::holds_alternative<RailData::rail_ptr_t>(m_data) ||
                   std::holds_alternative<Rail::Rail::node_ptr_t>(m_data);
        }

        RailData::rail_ptr_t getRail() const {
            if (std::holds_alternative<RailData::rail_ptr_t>(m_data)) {
                return std::get<RailData::rail_ptr_t>(m_data);
            }
            return nullptr;
        }

        Rail::Rail::node_ptr_t getNode() const {
            if (std::holds_alternative<Rail::Rail::node_ptr_t>(m_data)) {
                return std::get<Rail::Rail::node_ptr_t>(m_data);
            }
            return nullptr;
        }

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
        std::variant<std::monostate, RailData::rail_ptr_t, Rail::Rail::node_ptr_t> m_data;
    };

    RailObjModel::~RailObjModel() { reset(); }

    void RailObjModel::initialize(const RailData &data) {
        m_index_map.clear();

        int64_t row = 0;
        for (RailData::rail_ptr_t rail : data.rails()) {
            ModelIndex index = insertRail(rail, row++);
            int64_t subrow   = 0;
            for (Rail::Rail::node_ptr_t node : rail->nodes()) {
                insertRailNode(node, subrow++, index);
            }
        }
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

    bool RailObjModel::insertMimeData(const ModelIndex &index, const MimeData &data,
                                      ModelInsertPolicy policy) {
        bool result;

        {
            std::scoped_lock lock(m_mutex);
            result = insertMimeData_(index, data, policy);
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

        for (auto &[key, val] : m_index_map) {
            _RailIndexData *p = val.data<_RailIndexData>();
            if (p) {
                delete p;
            }
        }

        m_index_map.clear();
    }

    void RailObjModel::addEventListener(UUID64 uuid, event_listener_t listener, int allowed_flags) {
        m_listeners[uuid] = {listener, allowed_flags};
    }

    void RailObjModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    // const ImageHandle &RailObjModel::InvalidIcon() {
    //     static ImageHandle s_invalid_fs_icon = ImageHandle("Images/Icons/fs_invalid.png");
    //     return s_invalid_fs_icon;
    // }

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
            case ModelDataRole::DATA_ROLE_DISPLAY:
                std::optional<size_t> node_index = rail->getNodeIndex(node);
                if (!node_index.has_value()) {
                    return "Invalid Node";
                }
                return std::format("Node {}", node_index);
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
                return std::format("Node {}", node_index);
            }
            case RailObjDataRole::RAIL_DATA_ROLE_RAIL_GAME_ADDR: {
                // TODO: Currently unimplemented
                return static_cast<u32>(0);
            }
            case RailObjDataRole::RAIL_DATA_ROLE_RAIL_REF: {
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
        if (!validateIndex(index)) {
            return name;
        }

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
        for (const auto &[k, v] : m_index_map) {
            _RailIndexData *data = v.data<_RailIndexData>();
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
                return v;
            }
        }
        return ModelIndex();
    }

    ModelIndex RailObjModel::getIndex_(Rail::Rail::node_ptr_t node) const {
        for (const auto &[k, v] : m_index_map) {
            _RailIndexData *data = v.data<_RailIndexData>();
            if (!data->hasValue()) {
                continue;
            }

            // Skip indexes that reference rails when we're looking for nodes, and vice versa
            Rail::Rail::node_ptr_t other = data->getNode();
            if (!other) {
                continue;
            }

            if (other->getUUID() == node->getUUID()) {
                return v;
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
            if (row != 0 || column != 0) {
                return ModelIndex();
            }
            return m_index_map[m_root_index];
        }

        RailData::rail_ptr_t parent_obj            = parent.data<_RailIndexData>()->m_rail;
        std::vector<RailData::rail_ptr_t> children = parent_obj->getChildren();

        if (row < 0 || row >= static_cast<int64_t>(children.size()) || column != 0) {
            return ModelIndex();
        }

        RailData::rail_ptr_t child_obj = children[row];
        return getIndex_(child_obj);
    }

    bool RailObjModel::removeIndex_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        ModelIndex parent           = getParent_(index);
        _RailIndexData *parent_data = validateIndex(parent) ? parent.data<_RailIndexData>()
                                                            : nullptr;
        _RailIndexData *this_data   = index.data<_RailIndexData>();
        if (parent_data) {
            auto result = parent_data->m_rail->removeChild(this_data->m_rail);
            if (!result) {
                TOOLBOX_ERROR_V("[OBJMODEL] Failed to remove index with reason: '{}'",
                                result.error().m_message);
                return false;
            }
        }

        const UUID64 uuid = index.getUUID();
        m_index_map.erase(uuid);
        return true;
    }

    ModelIndex RailObjModel::getParent_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        _RailIndexData *data        = index.data<_RailIndexData>();
        RailData::rail_ptr_t parent = get_shared_ptr(*data->m_rail->getParent());

        return getIndex_(parent);
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
            return 1;
        }

        _RailIndexData *data = index.data<_RailIndexData>();
        if (!data) {
            return 0;
        }

        return data->m_rail->getChildren().size();
    }

    int64_t RailObjModel::getColumn_(const ModelIndex &index) const {
        return validateIndex(index) ? 0 : -1;
    }

    int64_t RailObjModel::getRow_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return -1;
        }

        _RailIndexData *data = index.data<_RailIndexData>();
        if (!data) {
            return -1;
        }

        RailData::rail_ptr_t self = data->m_rail;
        IRailObject *parent       = self->getParent();
        if (!parent) {
            return 0;
        }

        int64_t row = 0;
        for (RailData::rail_ptr_t child : parent->getChildren()) {
            if (child->getUUID() == self->getUUID()) {
                return row;
            }
            ++row;
        }

        return -1;
    }

    bool RailObjModel::hasChildren_(const ModelIndex &parent) const { return false; }

    ScopePtr<MimeData>
    RailObjModel::createMimeData_(const IDataModel::index_container &indexes) const {
        ScopePtr<MimeData> new_data = make_scoped<MimeData>();

        IDataModel::index_container pruned_indexes = indexes;
        pruneRedundantIndexes(pruned_indexes);

        std::function<Result<void, SerialError>(const ModelIndex &, Serializer &)> serialize_index =
            [&](const ModelIndex &index, Serializer &out) -> Result<void, SerialError> {
            out.write(static_cast<u64>(index.getUUID()));

            RailData::rail_ptr_t rail = index.data<_RailIndexData>()->m_rail;

            out.writeString(rail->type());
            out.writeString(rail->getNameRef().name());
            out.writeString(rail->getWizardName());

            const std::vector<MetaStruct::MemberT> &members = rail->getMembers();
            out.write<u32>(static_cast<u32>(members.size()));

            for (const auto &member : members) {
                Result<void, SerialError> result = member->serialize(out);
                if (!result) {
                    return result;
                }
            }

            size_t child_count = getRowCount_(index);
            out.write<u32>(static_cast<u32>(child_count));

            for (size_t i = 0; i < child_count; ++i) {
                ModelIndex child_index = getIndex_(i, 0, index);
                if (!validateIndex(child_index)) {
                    return make_serial_error<void>(out, "Failed to get child index for rail!");
                }
                Result<void, SerialError> result = serialize_index(child_index, out);
                if (!result) {
                    return result;
                }
            }

            return {};
        };

        std::stringstream outstr;
        Serializer out(outstr.rdbuf());

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

    bool RailObjModel::insertMimeData_(const ModelIndex &index, const MimeData &data,
                                       ModelInsertPolicy policy) {
        if (!data.has_format("toolbox/scene/rail_model")) {
            return false;
        }

        Buffer mime_data = data.get_data("toolbox/scene/rail_model").value();

        std::stringstream instr(std::string(mime_data.buf<char>(), mime_data.size()));
        Deserializer in(instr.rdbuf());

        std::function<Result<void, SerialError>(const ModelIndex &, Deserializer &)>
            deserialize_index =
                [&](const ModelIndex &index, Deserializer &in) -> Result<void, SerialError> {
            const UUID64 uuid = in.read<u64>();

            const std::string obj_type   = in.readString();
            const std::string obj_name   = in.readString();
            const std::string obj_wizard = in.readString();

            TemplateFactory::create_t obj_template =
                TemplateFactory::create(std::string_view(obj_type), true);
            if (!obj_template) {
                return make_serial_error<void>(
                    in, std::format("Failed to find template for rail type '{}'", obj_type));
            }

            RailData::rail_ptr_t rail =
                ObjectFactory::create(*obj_template.value(), obj_wizard, ".");
            if (!rail) {
                return make_serial_error<void>(
                    in, std::format("Failed to create rail of type '{}' with wizard '{}'", obj_type,
                                    obj_wizard));
            }
            rail->setNameRef(NameRef(obj_name));

            const u32 member_count                  = in.read<u32>();
            std::vector<RefPtr<MetaMember>> members = rail->getMembers();
            for (RefPtr<MetaMember> member : members) {
                member->updateReferenceToList(members);
                Result<void, SerialError> result = member->deserialize(in);
                if (!result) {
                    return result;
                }
            }

            u32 child_count = in.read<u32>();

            for (u32 i = 0; i < child_count; ++i) {
                ModelIndex child_index           = insertObject_(rail, i, index);
                Result<void, SerialError> result = deserialize_index(child_index, in);
                if (!result) {
                    return result;
                }
            }

            return {};
        };
    }

    bool RailObjModel::canFetchMore_(const ModelIndex &index) const { return false; }

    void RailObjModel::fetchMore_(const ModelIndex &index) const { return; }

    ModelIndex RailObjModel::insertObject_(RailData::rail_ptr_t rail, int64_t row,
                                           const ModelIndex &parent) {
        ModelIndex new_index = makeIndex(rail, row, parent);
        if (!validateIndex(new_index)) {
            return ModelIndex();
        }

        _RailIndexData *parent_data = validateIndex(parent) ? parent.data<_RailIndexData>()
                                                            : nullptr;
        if (parent_data) {
            auto result = parent_data->m_rail->insertChild(row, rail);
            if (!result) {
                TOOLBOX_ERROR_V("[OBJMODEL] Failed to create inde with reason: '{}'",
                                result.error().m_message);
                (void)removeIndex_(new_index);
                return ModelIndex();
            }
        }

        return new_index;
    }

    ModelIndex RailObjModel::makeIndex(RailData::rail_ptr_t rail, int64_t row,
                                       const ModelIndex &parent) const {
        if (row < 0 || row > m_index_map.size()) {
            return ModelIndex();
        }

        ModelIndex new_index(getUUID());

        _RailIndexData *new_data = new _RailIndexData;
        new_data->m_self_uuid    = new_index.getUUID();
        new_data->m_rail         = rail;
        new_data->m_icon         = nullptr;

        new_index.setData(new_data);

        m_index_map[new_index.getUUID()] = new_index;

        if (row == 0 && !validateIndex(parent)) {
            m_root_index = new_index.getUUID();
        }

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
