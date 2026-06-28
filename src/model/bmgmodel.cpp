#include <algorithm>
#include <compare>
#include <set>

#include "core/types.hpp"
#include "model/bmgmodel.hpp"
#include "project/config.hpp"

using namespace std::chrono;

namespace Toolbox {

    struct _BMGIndexData {
        UUID64 m_self_uuid               = 0;
        RefPtr<const ImageHandle> m_icon = nullptr;
        BMG::MessageData::Entry m_entry;

        std::strong_ordering operator<=>(const _BMGIndexData &rhs) const {
            return m_entry.getName() <=> rhs.m_entry.getName();
        }
    };

    BMGModel::~BMGModel() { reset(); }

    void BMGModel::initialize(const BMG::MessageData &data) {
        m_message_indexes.clear();

        int64_t row = 0;
        for (const BMG::MessageData::Entry &entry : data.entries()) {
            ModelIndex index = makeIndex(entry, row++);
            if (!validateIndex(index)) {
                TOOLBOX_ERROR_V("[BMG_MODEL] Failed to initialize message at row {}!", row);
            }
        }

        m_ntsc      = data.isNTSC();
        m_flag_size = data.getFlagSize();
    }

    ScopePtr<BMG::MessageData> BMGModel::bakeToMessageData() const {
        ScopePtr<BMG::MessageData> output = make_scoped<BMG::MessageData>();

        const size_t rail_count = getRowCount(ModelIndex());
        for (size_t i = 0; i < rail_count; ++i) {
            ModelIndex entry_index = getIndex(i, 0);
            if (!validateIndex(entry_index)) {
                TOOLBOX_ERROR_V("[BMG_MODEL] Failed to fetch message at row {}!", i);
                continue;
            }
            _BMGIndexData *data = entry_index.data<_BMGIndexData>();
            output->addEntry(data->m_entry);
        }

        return output;
    }

    std::any BMGModel::getData(const ModelIndex &index, int role) const {
        std::scoped_lock lock(m_mutex);
        return getData_(index, role);
    }

    void BMGModel::setData(const ModelIndex &index, std::any data, int role) {
        std::scoped_lock lock(m_mutex);
        return setData_(index, data, role);
    }

    std::string BMGModel::findUniqueName(const ModelIndex &index, const std::string &name) const {
        std::scoped_lock lock(m_mutex);
        return findUniqueName_(index, name);
    }

    ModelIndex BMGModel::getIndex(const UUID64 &uuid) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(uuid);
    }

    ModelIndex BMGModel::getIndex(int64_t row, int64_t column, const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(row, column, parent);
    }

    bool BMGModel::removeIndex(const ModelIndex &index) {
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

    ModelIndex BMGModel::insertMessageEntry(const BMG::MessageData::Entry &entry, int64_t row) {
        ModelIndex result;

        {
            std::scoped_lock lock(m_mutex);
            result = insertMessageEntry_(entry, row);
        }

        if (validateIndex(result)) {
            const Signal index_signal =
                createSignalForIndex_(result, ModelEventFlags::EVENT_INDEX_ADDED);
            signalEventListeners(index_signal.first, index_signal.second |
                                                         ModelEventFlags::EVENT_POST |
                                                         ModelEventFlags::EVENT_SUCCESS);
        }

        return result;
    }

    ModelIndex BMGModel::getParent(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getParent_(index);
    }

    ModelIndex BMGModel::getSibling(int64_t row, int64_t column, const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getSibling_(row, column, index);
    }

    size_t BMGModel::getColumnCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumnCount_(index);
    }

    size_t BMGModel::getRowCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRowCount_(index);
    }

    int64_t BMGModel::getColumn(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumn_(index);
    }

    int64_t BMGModel::getRow(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRow_(index);
    }

    bool BMGModel::hasChildren(const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return hasChildren_(parent);
    }

    ScopePtr<MimeData> BMGModel::createMimeData(const IDataModel::index_container &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
    }

    Result<IDataModel::index_container> BMGModel::insertMimeData(const ModelIndex &index,
                                                                 const MimeData &data,
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
            signalEventListeners(pre_signal.first, pre_signal.second | ModelEventFlags::EVENT_POST |
                                                       ModelEventFlags::EVENT_SUCCESS);
        } else {
            signalEventListeners(pre_signal.first, pre_signal.second | ModelEventFlags::EVENT_POST);
        }

        return result;
    }

    std::vector<std::string> BMGModel::getSupportedMimeTypes() const {
        return std::vector<std::string>();
    }

    bool BMGModel::canFetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return canFetchMore_(index);
    }

    void BMGModel::fetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return fetchMore_(index);
    }

    void BMGModel::reset() {
        std::scoped_lock lock(m_mutex);

        for (auto &[key, index] : m_index_map) {
            _BMGIndexData *p = index.data<_BMGIndexData>();
            if (p) {
                delete p;
            }
        }

        m_message_indexes.clear();
        m_index_map.clear();
    }

    void BMGModel::addEventListener(UUID64 uuid, event_listener_t listener, int allowed_flags) {
        m_listeners[uuid] = {listener, allowed_flags};
    }

    void BMGModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    BMGModel::Signal BMGModel::createSignalForIndex_(const ModelIndex &index,
                                                     ModelEventFlags base_event) const {
        return {index, (int)base_event};
    }

    std::any BMGModel::getData_(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        const _BMGIndexData *data = index.data<_BMGIndexData>();

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            return data->m_entry.getName();
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {

            return data->m_icon;
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_NAME: {
            return std::string_view(data->m_entry.getName());
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_TEXT: {
            return std::string_view(data->m_entry.getMessage().getString());
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_SOUND_ID: {
            return data->m_entry.getSoundID();
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_START_FRAME: {
            return data->m_entry.getStartFrame();
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_END_FRAME: {
            return data->m_entry.getEndFrame();
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_ENTRY: {
            return data->m_entry;
        }
        default:
            return {};
        }
    }

    void BMGModel::setData_(const ModelIndex &index, std::any data, int role) const {
        if (!validateIndex(index)) {
            return;
        }

        _BMGIndexData *idata = index.data<_BMGIndexData>();

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            idata->m_entry.setName(std::any_cast<std::string>(data));
            return;
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return;
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return;
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_NAME: {
            idata->m_entry.setName(std::any_cast<std::string>(data));
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_TEXT: {
            BMG::CmdMessage message = BMG::CmdMessage(std::any_cast<std::string>(data));
            idata->m_entry.setMessage(message);
            return;
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_SOUND_ID: {
            idata->m_entry.setSoundID(std::any_cast<BMG::MessageSound>(data));
            return;
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_START_FRAME: {
            idata->m_entry.setStartFrame(std::any_cast<u16>(data));
            return;
        }
        case BMGDataRole::BMG_DATA_ROLE_MESSAGE_END_FRAME: {
            idata->m_entry.setEndFrame(std::any_cast<u16>(data));
            return;
        }
        default:
            return;
        }
    }

    std::string BMGModel::findUniqueName_(const ModelIndex &index, const std::string &name) const {
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

    ModelIndex BMGModel::getIndex_(const UUID64 &uuid) const {
        if (!m_index_map.contains(uuid)) {
            return ModelIndex();
        }
        return m_index_map.at(uuid);
    }

    ModelIndex BMGModel::getIndex_(int64_t row, int64_t column, const ModelIndex &parent) const {
        if (validateIndex(parent)) {
            return ModelIndex();
        }

        if (row >= m_message_indexes.size() || row < 0 || column != 0) {
            return ModelIndex();
        }
        return m_message_indexes[row];
    }

    bool BMGModel::removeIndex_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        _BMGIndexData *this_data = index.data<_BMGIndexData>();
        //delete this_data;

        m_message_indexes.erase(
            std::remove(m_message_indexes.begin(), m_message_indexes.end(), index),
            m_message_indexes.end());

        const UUID64 uuid = index.getUUID();
        m_index_map.erase(uuid);
        return true;
    }

    ModelIndex BMGModel::getParent_(const ModelIndex &index) const {
        return ModelIndex();
    }

    ModelIndex BMGModel::getSibling_(int64_t row, int64_t column, const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        ModelIndex parent_index = getParent_(index);
        return getIndex_(row, column, parent_index);
    }

    size_t BMGModel::getColumnCount_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 0;
        }

        return 1;
    }

    size_t BMGModel::getRowCount_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return m_message_indexes.size();
        }

        return 0;
    }

    int64_t BMGModel::getColumn_(const ModelIndex &index) const {
        return validateIndex(index) ? 0 : -1;
    }

    int64_t BMGModel::getRow_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return -1;
        }

        auto it = std::find_if(m_message_indexes.begin(), m_message_indexes.end(),
                               [&](const ModelIndex &rail_index) { return rail_index == index; });
        if (it == m_message_indexes.end()) {
            return -1;
        }

        return std::distance(m_message_indexes.begin(), it);
    }

    bool BMGModel::hasChildren_(const ModelIndex &parent) const { return false; }

    ScopePtr<MimeData> BMGModel::createMimeData_(const IDataModel::index_container &indexes) const {
        ScopePtr<MimeData> new_data = make_scoped<MimeData>();

        // Strips and validates indexes, ensuring either all nodes or all rails are included, and
        // that there are no duplicates or invalid indexes.
        IDataModel::index_container pruned_indexes = indexes;
        ModelIndexListTransformer transformer(reference());
        transformer.pruneRedundantsForRecursiveTree(pruned_indexes);

        std::function<Result<void, SerialError>(const ModelIndex &, Serializer &)> serialize_index =
            [&](const ModelIndex &index, Serializer &out) -> Result<void, SerialError> {
            out.write(static_cast<u64>(index.getUUID()));

            _BMGIndexData *data        = index.data<_BMGIndexData>();
            data->m_entry.serialize(out);

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

        new_data->set_data("toolbox/scene/bmg_model", std::move(mime_data));

        return new_data;
    }

    Result<IDataModel::index_container> BMGModel::insertMimeData_(const ModelIndex &index,
                                                                  const MimeData &data,
                                                                  ModelInsertPolicy policy) {
        if (!data.has_format("toolbox/scene/bmg_model")) {
            return make_error<std::vector<ModelIndex>>(
                "BMGModel", "Provided MIME data does not have BMGModel data!");
        }

        Buffer mime_data = data.get_data("toolbox/scene/bmg_model").value();

        std::stringstream instr(std::string(mime_data.buf<char>(), mime_data.size()));
        Deserializer in(instr.rdbuf());

        std::vector<ModelIndex> inserted_indexes;
        int64_t spill_row = -1;

        auto deserialize_index = [&](int64_t row, const ModelIndex &parent,
                                     Deserializer &in) -> Result<void, SerialError> {
            UUID64 index_uuid = in.read<u64>();

            BMG::MessageData::Entry new_entry;
            auto result = new_entry.deserialize(in);
            if (!result) {
                return result;
            }

            // Get a unique name
            std::string new_name = findUniqueName_(ModelIndex(), new_entry.getName());
            new_entry.setName(new_name);

            ModelIndex message_index = insertMessageEntry_(new_entry, row, index_uuid);
            if (!validateIndex(message_index)) {
                return make_serial_error<void>(in, "[BMG_MODEL] Failed to insert message into model!");
            }

            inserted_indexes.push_back(message_index);

            return {};
        };

        if (policy == ModelInsertPolicy::INSERT_CHILD) {
            if (validateIndex(index)) {
                // Inserting as a child of a message is not allowed, so we treat this as an "after"
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
                    "BMGModel", "Failed to retrieve the row for the insert index");
            }
            insert_index = getParent_(index);
            spill_row    = getRow_(insert_index);
            break;
        case ModelInsertPolicy::INSERT_AFTER:
            insert_row = getRow_(index);
            if (insert_row == -1) {
                return make_error<std::vector<ModelIndex>>(
                    "BMGModel", "Failed to retrieve the row for the insert index");
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
                return make_error<std::vector<ModelIndex>>("BMG_MODEL", result.error().m_message);
            }
        }

        return inserted_indexes;
    }

    bool BMGModel::canFetchMore_(const ModelIndex &index) const { return false; }

    void BMGModel::fetchMore_(const ModelIndex &index) const { return; }

    ModelIndex BMGModel::insertMessageEntry_(const BMG::MessageData::Entry &entry, int64_t row,
                                     std::optional<UUID64> index_uuid) {
        ModelIndex ret = makeIndex(entry, row, index_uuid);
        if (!validateIndex(ret)) {
            TOOLBOX_ERROR_V("[RAILMODEL] Failed to create index for entry \"{}\"!", entry.getName());
            return ModelIndex();
        }

        return ret;
    }

    ModelIndex BMGModel::makeIndex(const BMG::MessageData::Entry &entry, int64_t row,
                                   std::optional<UUID64> index_uuid) const {
        if (row < 0 || row > m_index_map.size()) {
            return ModelIndex();
        }

        ModelIndex new_index(getUUID(), index_uuid ? *index_uuid : UUID64());

        _BMGIndexData *new_data = new _BMGIndexData;
        new_data->m_self_uuid    = new_index.getUUID();
        new_data->m_icon         = nullptr;
        new_data->m_entry        = entry;
        if (new_data->m_entry.getName().empty()) {
            std::string default_name = findUniqueName_(ModelIndex(), "unknown_message");
            new_data->m_entry.setName(default_name);
        }

        new_index.setData(new_data);

        m_index_map[new_index.getUUID()] = new_index;
        m_message_indexes.insert(m_message_indexes.begin() + row, new_index);

        return new_index;
    }

    void BMGModel::signalEventListeners(const ModelIndex &index, int flags) {
        int this_event_flags =
            flags & ~(EVENT_SUCCESS | EVENT_PRE | EVENT_POST | EVENT_SOFT | EVENT_RESET);

        if (this_event_flags == EVENT_INDEX_MODIFIED) {
            flags |= ModelEventFlags::EVENT_SOFT;
        }

        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != ModelEventFlags::EVENT_NONE) {
                listener.first(index, (listener.second & flags));
            }
        }
    }

}  // namespace Toolbox
