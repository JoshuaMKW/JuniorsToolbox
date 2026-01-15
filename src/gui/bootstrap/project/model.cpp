#include <algorithm>
#include <compare>
#include <set>

#include "gui/bootstrap/project/model.hpp"
#include "project/config.hpp"

#include <gcm.hpp>

using namespace std::chrono;

namespace Toolbox {

    struct _ProjectIndexData {
        UUID64 m_self_uuid = 0;

        fs_path m_path;
        size_t m_path_hash = 0;

        ProjectConfig m_config;
        system_clock::time_point m_date;

        RefPtr<const ImageHandle> m_icon = nullptr;

        bool m_pinned = false;

        std::strong_ordering operator<=>(const _ProjectIndexData &rhs) const {
            return m_date <=> rhs.m_date;
        }
    };

    static bool _ProjectIndexDataCompareByDate(const _ProjectIndexData &lhs,
                                               const _ProjectIndexData &rhs, ModelSortOrder order) {
        // Sort by date
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs.m_date < rhs.m_date;
        } else {
            return lhs.m_date > rhs.m_date;
        }
    }

    ProjectModel::~ProjectModel() { reset(); }

    void ProjectModel::initialize(const fs_path &info_file) {
        m_index_map.clear();
        m_path_map.clear();

        if (!Filesystem::is_regular_file(info_file).value_or(false)) {
            return;
        }

        std::ifstream in_data = std::ifstream(info_file, std::ios::in);
        if (!in_data.is_open()) {
            return;
        }

        json_t the_json;
        in_data >> the_json;

        auto result = tryJSON(the_json, [this](json_t &j) {
            if (!j.is_array()) {
                return make_json_error<void>("ProjectModel", "Root of JSON was not an array!", 0);
            }

            int64_t index_row = 0;
            for (const json_t &project_json : j) {
                if (!project_json.is_object()) {
                    TOOLBOX_WARN(
                        "[ProjectModel] Skipping invalid project entry; not a JSON object!");
                    continue;
                }

                if (!project_json.contains("ProjectRootPath") ||
                    !project_json["ProjectRootPath"].is_string()) {
                    TOOLBOX_WARN("[ProjectModel] Skipping invalid project entry; missing/invalid "
                                 "`ProjectRootPath` field!");
                    continue;
                }

                const fs_path project_path = [project_json]() {
                    fs_path tmp = project_json["ProjectRootPath"];
                    return tmp.lexically_normal();
                }();

                const fs_path icon_path =
                    (project_path /
                     JSONValueOr(project_json, "ProjectIconPath", "files/opening.bnr"))
                        .lexically_normal();

                const system_clock::time_point project_accessed =
                    JSONValueOr(project_json, "ProjectLastAccessed", system_clock::now());

                const bool project_pinned = JSONValueOr(project_json, "ProjectPinned", false);

                RefPtr<const ImageHandle> icon_handle = nullptr;
                if (Filesystem::is_regular_file(icon_path).value_or(false)) {
                    ScopePtr<gcm::BNRData> data = gcm::BNRData::FromFile(icon_path.string());
                    if (data) {
                        std::vector<u8> icon_data = data->GetIconData();
                        u8 icon_width, icon_height;
                        data->GetIconRect(icon_width, icon_height);
                        std::span<u8> icon_span(icon_data.data(), icon_data.size());
                        icon_handle = make_referable<const ImageHandle>(icon_span, 4, icon_width,
                                                                        icon_height);
                    } else {
                        icon_handle = nullptr;
                    }
                }

                ModelIndex new_index = makeIndex(project_path, index_row, ModelIndex());
                setDecoration(new_index, icon_handle);
                setLastAccessed(new_index, project_accessed);
                setPinned(new_index, project_pinned);

                index_row++;
            }

            return Result<void, JSONError>();
        });
    }

    void ProjectModel::saveToJSON(const fs_path &info_path) {
        std::ofstream out_data = std::ofstream(info_path, std::ios::out);
        if (!out_data.is_open()) {
            return;
        }
        json_t the_json;

        auto result = tryJSON(the_json, [this](json_t &j) {
            for (size_t i = 0; i < getRowCount(ModelIndex()); ++i) {
                ModelIndex index = getIndex(i, 0);
                if (!validateIndex(index)) {
                    continue;
                }

                json_t proj_json;
                proj_json["ProjectRootPath"]     = getProjectPath(index);
                proj_json["ProjectIconPath"] = "files/opening.bnr";
                proj_json["ProjectLastAccessed"] =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        getLastAccessed(index).time_since_epoch())
                        .count();
                proj_json["ProjectPinned"]       = getPinned(index);
                j.emplace_back(std::move(proj_json));
            }
            return Result<void, JSONError>();
        });

        out_data << the_json;
    }

    std::any ProjectModel::getData(const ModelIndex &index, int role) const {
        std::scoped_lock lock(m_mutex);
        return getData_(index, role);
    }

    void ProjectModel::setData(const ModelIndex &index, std::any data, int role) {
        std::scoped_lock lock(m_mutex);
        setData_(index, data, role);
    }

    ModelIndex ProjectModel::getIndex(const fs_path &path) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(path);
    }

    ModelIndex ProjectModel::getIndex(const UUID64 &uuid) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(uuid);
    }

    ModelIndex ProjectModel::getIndex(int64_t row, int64_t column, const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(row, column, parent);
    }

    bool ProjectModel::removeIndex(const ModelIndex &index) {
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

    ModelIndex ProjectModel::getParent(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getParent_(index);
    }

    ModelIndex ProjectModel::getSibling(int64_t row, int64_t column,
                                        const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getSibling_(row, column, index);
    }

    size_t ProjectModel::getColumnCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumnCount_(index);
    }

    size_t ProjectModel::getRowCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRowCount_(index);
    }

    int64_t ProjectModel::getColumn(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumn_(index);
    }

    int64_t ProjectModel::getRow(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRow_(index);
    }

    bool ProjectModel::hasChildren(const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return hasChildren_(parent);
    }

    ScopePtr<MimeData>
    ProjectModel::createMimeData(const IDataModel::index_container &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
    }

    bool ProjectModel::insertMimeData(const ModelIndex &index, const MimeData &data,
                                      ModelInsertPolicy policy) {
        bool result;

        {
            std::scoped_lock lock(m_mutex);
            result = insertMimeData_(index, data, policy);
        }

        return result;
    }

    std::vector<std::string> ProjectModel::getSupportedMimeTypes() const {
        return std::vector<std::string>();
    }

    bool ProjectModel::canFetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return canFetchMore_(index);
    }

    void ProjectModel::fetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return fetchMore_(index);
    }

    void ProjectModel::reset() {
        std::scoped_lock lock(m_mutex);

        for (auto &[key, val] : m_index_map) {
            _ProjectIndexData *p = val.data<_ProjectIndexData>();
            if (p) {
                delete p;
            }
        }

        m_index_map.clear();
        m_path_map.clear();
    }

    void ProjectModel::addEventListener(UUID64 uuid, event_listener_t listener, int allowed_flags) {
        m_listeners[uuid] = {listener, allowed_flags};
    }

    void ProjectModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

    // const ImageHandle &ProjectModel::InvalidIcon() {
    //     static ImageHandle s_invalid_fs_icon = ImageHandle("Images/Icons/fs_invalid.png");
    //     return s_invalid_fs_icon;
    // }

    ProjectModel::Signal ProjectModel::createSignalForIndex_(const ModelIndex &index,
                                                             ModelEventFlags base_event) const {
        return {index, (int)base_event};
    }

    std::any ProjectModel::getData_(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            return index.data<_ProjectIndexData>()->m_config.getProjectName();
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return index.data<_ProjectIndexData>()->m_icon;
        }
        case ProjectDataRole::PROJECT_DATA_ROLE_AGE: {
            return index.data<_ProjectIndexData>()->m_date;
        }
        case ProjectDataRole::PROJECT_DATA_ROLE_PINNED: {
            return index.data<_ProjectIndexData>()->m_pinned;
        }
        case ProjectDataRole::PROJECT_DATA_ROLE_PATH: {
            return index.data<_ProjectIndexData>()->m_path;
        }
        default:
            return {};
        }
    }

    void ProjectModel::setData_(const ModelIndex &index, std::any data, int role) const {
        if (!validateIndex(index)) {
            return;
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            index.data<_ProjectIndexData>()->m_config.setProjectName(
                std::any_cast<std::string>(data));
            break;
        case ModelDataRole::DATA_ROLE_DECORATION: {
            index.data<_ProjectIndexData>()->m_icon =
                std::any_cast<RefPtr<const ImageHandle>>(data);
            break;
        }
        case ProjectDataRole::PROJECT_DATA_ROLE_AGE: {
            index.data<_ProjectIndexData>()->m_date = std::any_cast<system_clock::time_point>(data);
            break;
        }
        case ProjectDataRole::PROJECT_DATA_ROLE_PINNED: {
            index.data<_ProjectIndexData>()->m_pinned = std::any_cast<bool>(data);
            break;
        }
        case ProjectDataRole::PROJECT_DATA_ROLE_PATH: {
            index.data<_ProjectIndexData>()->m_path = std::any_cast<fs_path>(data);
            break;
        }
        default:
            return;
        }
    }

    ModelIndex ProjectModel::getIndex_(const fs_path &path) const {
        const size_t path_hash = std::hash<fs_path>()(path);
        for (const auto &[k, v] : m_index_map) {
            const size_t index_hash = getPathHash_(v);
            if (index_hash == path_hash) {
                return v;
            }
        }
        return ModelIndex();
    }

    ModelIndex ProjectModel::getIndex_(const UUID64 &uuid) const {
        if (!m_index_map.contains(uuid)) {
            return ModelIndex();
        }
        return m_index_map.at(uuid);
    }

    ModelIndex ProjectModel::getIndex_(int64_t row, int64_t column,
                                       const ModelIndex &parent) const {
        if (validateIndex(parent)) {
            return ModelIndex();
        }

        if (row < 0 || (size_t)row >= m_index_map.size()) {
            return ModelIndex();
        }

        size_t i = 0;
        for (const auto &[k, v] : m_index_map) {
            if (row == i) {
                return v;
            }
        }
        return ModelIndex();
    }

    bool ProjectModel::removeIndex_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }
        const UUID64 uuid = index.getUUID();
        m_index_map.erase(uuid);
        return true;
    }

    size_t ProjectModel::getPathHash_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 0;
        }

        return index.data<_ProjectIndexData>()->m_path_hash;
    }

    ModelIndex ProjectModel::getParent_(const ModelIndex &index) const { return ModelIndex(); }

    ModelIndex ProjectModel::getSibling_(int64_t row, int64_t column,
                                         const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        return getIndex_(row, column, ModelIndex());
    }

    size_t ProjectModel::getColumnCount_(const ModelIndex &index) const {
        if (validateIndex(index)) {
            return 0;
        }

        return 1;
    }

    size_t ProjectModel::getRowCount_(const ModelIndex &index) const {
        if (validateIndex(index)) {
            return 0;
        }

        return m_index_map.size();
    }

    int64_t ProjectModel::getColumn_(const ModelIndex &index) const {
        return validateIndex(index) ? 0 : -1;
    }

    int64_t ProjectModel::getRow_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return -1;
        }

        int64_t row = 0;
        for (const auto &[uuid, index] : m_index_map) {
            if (uuid == index.getUUID()) {
                return row;
            }
            ++row;
        }

        return -1;
    }

    bool ProjectModel::hasChildren_(const ModelIndex &parent) const { return false; }

    ScopePtr<MimeData>
    ProjectModel::createMimeData_(const IDataModel::index_container &indexes) const {
        return nullptr;
    }

    bool ProjectModel::insertMimeData_(const ModelIndex &index, const MimeData &data,
                                       ModelInsertPolicy policy) {
        return false;
    }

    bool ProjectModel::canFetchMore_(const ModelIndex &index) const { return false; }

    void ProjectModel::fetchMore_(const ModelIndex &index) const { return; }

    ModelIndex ProjectModel::makeIndex(const fs_path &path, int64_t row,
                                       const ModelIndex &parent) const {
        if (row < 0 || row > m_index_map.size()) {
            return ModelIndex();
        }

        if (!Filesystem::is_directory(path).value_or(false)) {
            return ModelIndex();
        }

        ProjectConfig project_config;
        auto result = project_config.loadFromFile(path / ".ToolboxConfig.tbox");
        if (!result) {
            TOOLBOX_DEBUG_LOG("[ProjectModel] Encountered project entry without a valid config.");
        }

        ModelIndex new_index(getUUID());

        _ProjectIndexData *new_data = new _ProjectIndexData;
        new_data->m_self_uuid       = new_index.getUUID();
        new_data->m_config          = std::move(project_config);
        new_data->m_path            = path;
        new_data->m_path_hash       = std::hash<fs_path>()(path);
        new_data->m_date            = system_clock::time_point::min();
        new_data->m_icon            = nullptr;
        new_data->m_pinned          = false;

        new_index.setData(new_data);

        if (row == m_index_map.size()) {
            m_index_map.insert(m_index_map.end(), {new_index.getUUID(), new_index});
            m_path_map[new_data->m_path_hash] = new_index;
            return new_index;
        }

        int64_t i = 0;
        for (auto it = m_index_map.begin(); it != m_index_map.end(); it = std::next(it)) {
            if (i == row) {
                m_index_map.insert(it, {new_index.getUUID(), new_index});
                m_path_map[new_data->m_path_hash] = new_index;
                return new_index;
            }
        }

        delete new_data;
        return ModelIndex();
    }

    void ProjectModel::signalEventListeners(const ModelIndex &index, int flags) {
        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != ModelEventFlags::EVENT_NONE) {
                listener.first(index, (listener.second & flags));
            }
        }
    }

    RefPtr<ProjectModel> ProjectModelSortFilterProxy::getSourceModel() const {
        return m_source_model;
    }

    void ProjectModelSortFilterProxy::setSourceModel(RefPtr<ProjectModel> model) {
        if (m_source_model == model) {
            return;
        }

        if (m_source_model) {
            m_source_model->removeEventListener(getUUID());
        }

        m_source_model = model;

        if (m_source_model) {
            m_source_model->addEventListener(getUUID(), TOOLBOX_BIND_EVENT_FN(modelUpdateEvent),
                                             ModelEventFlags::EVENT_ANY);
        }
    }

    const std::string &ProjectModelSortFilterProxy::getFilter() const & { return m_filter; }
    void ProjectModelSortFilterProxy::setFilter(const std::string &filter) {
        m_filter.clear();
        m_filter.reserve(filter.size());
        std::transform(filter.begin(), filter.end(), std::back_inserter(m_filter),
                       [](char c) { return ::tolower(static_cast<int>(c)); });

        flushCache();
    }

    std::any ProjectModelSortFilterProxy::getData(const ModelIndex &index, int role) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getData(std::move(source_index), role);
    }

    void ProjectModelSortFilterProxy::setData(const ModelIndex &index, std::any data, int role) {
        ModelIndex &&source_index = toSourceIndex(index);
        m_source_model->setData(std::move(source_index), data, role);
    }

    ModelIndex ProjectModelSortFilterProxy::getIndex(const fs_path &path) const {
        ModelIndex index = m_source_model->getIndex(path);
        return toProxyIndex(index);
    }

    ModelIndex ProjectModelSortFilterProxy::getIndex(const UUID64 &uuid) const {
        ModelIndex index = m_source_model->getIndex(uuid);
        return toProxyIndex(index);
    }

    ModelIndex ProjectModelSortFilterProxy::getIndex(int64_t row, int64_t column,
                                                     const ModelIndex &parent) const {
        ModelIndex parent_src = toSourceIndex(parent);
        return toProxyIndex(row, column, parent_src);
    }

    bool ProjectModelSortFilterProxy::removeIndex(const ModelIndex &index) {
        ModelIndex source_index = toSourceIndex(index);
        return m_source_model->removeIndex(source_index);
    }

    ModelIndex ProjectModelSortFilterProxy::getParent(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return toProxyIndex(m_source_model->getParent(std::move(source_index)));
    }

    ModelIndex ProjectModelSortFilterProxy::getSibling(int64_t row, int64_t column,
                                                       const ModelIndex &index) const {
        ModelIndex source_index = toSourceIndex(index);
        ModelIndex src_parent   = m_source_model->getParent(source_index);

        const u64 map_key = getCacheKey_(src_parent);

        if (!isCached(src_parent)) {
            cacheIndex(src_parent);
        }

        std::scoped_lock lock(m_cache_mutex);

        if (row < m_row_map[map_key].size()) {
            int64_t the_row = m_row_map[map_key][row];
            return toProxyIndex(m_source_model->getIndex(the_row, column, src_parent));
        }

        return ModelIndex();
    }

    size_t ProjectModelSortFilterProxy::getColumnCount(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getColumnCount(source_index);
    }

    size_t ProjectModelSortFilterProxy::getRowCount(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);

        const u64 map_key = getCacheKey_(source_index);

        if (!isCached(index)) {
            cacheIndex(source_index);
        }

        std::unique_lock lk(m_cache_mutex);
        return m_row_map[map_key].size();
    }

    int64_t ProjectModelSortFilterProxy::getColumn(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getColumn(source_index);
    }

    int64_t ProjectModelSortFilterProxy::getRow(const ModelIndex &index) const {

        ModelIndex &&source_index = toSourceIndex(index);
        ModelIndex src_parent     = m_source_model->getParent(source_index);
        const u64 map_key         = getCacheKey_(src_parent);

        if (!isCached(index)) {
            cacheIndex(src_parent);
        }

        int64_t src_row = m_source_model->getRow(source_index);
        if (src_row == -1) {
            return src_row;
        }

        std::unique_lock lk(m_cache_mutex);

        const std::vector<int64_t> &row_map = m_row_map[map_key];
        for (size_t i = 0; i < row_map.size(); ++i) {
            if (src_row == row_map[i]) {
                return i;
            }
        }

        return -1;
    }

    bool ProjectModelSortFilterProxy::hasChildren(const ModelIndex &parent) const {
        ModelIndex &&source_index = toSourceIndex(parent);
        return m_source_model->hasChildren(source_index);
    }

    ScopePtr<MimeData>
    ProjectModelSortFilterProxy::createMimeData(const IDataModel::index_container &indexes) const {
        IDataModel::index_container indexes_copy;

        for (const ModelIndex &idx : indexes) {
            indexes_copy.emplace_back(toSourceIndex(idx));
        }

        return m_source_model->createMimeData(indexes_copy);
    }

    bool ProjectModelSortFilterProxy::insertMimeData(const ModelIndex &index, const MimeData &data,
                                                     ModelInsertPolicy policy) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->insertMimeData(std::move(source_index), data, policy);
    }

    std::vector<std::string> ProjectModelSortFilterProxy::getSupportedMimeTypes() const {
        return m_source_model->getSupportedMimeTypes();
    }

    bool ProjectModelSortFilterProxy::canFetchMore(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->canFetchMore(std::move(source_index));
    }

    void ProjectModelSortFilterProxy::fetchMore(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        m_source_model->fetchMore(std::move(source_index));
    }

    void ProjectModelSortFilterProxy::reset() {
        std::unique_lock lock(m_cache_mutex);
        m_source_model->reset();
        m_row_map.clear();
    }

    ModelIndex ProjectModelSortFilterProxy::toSourceIndex(const ModelIndex &index) const {
        if (m_source_model->validateIndex(index)) {
            return index;
        }

        if (!IDataModel::validateIndex(index)) {
            return ModelIndex();
        }

        ModelIndex src_index =
            m_source_model->getIndex(index.data<_ProjectIndexData>()->m_self_uuid);
        if (isSrcFiltered_(src_index.getUUID())) {
            return ModelIndex();
        }

        return src_index;
    }

    ModelIndex ProjectModelSortFilterProxy::toProxyIndex(const ModelIndex &index) const {
        if (!m_source_model->validateIndex(index)) {
            return ModelIndex();
        }

        if (isSrcFiltered_(index.getUUID())) {
            return ModelIndex();
        }

        ModelIndex proxy_index = ModelIndex(getUUID());
        proxy_index.setData(index.data<_ProjectIndexData>());

        IDataModel::setIndexUUID(proxy_index, index.getUUID());

        return proxy_index;
    }

    ModelIndex ProjectModelSortFilterProxy::toProxyIndex(int64_t row, int64_t column,
                                                         const ModelIndex &src_parent) const {
        if (!isCached(src_parent)) {
            cacheIndex(src_parent);
        }

        const u64 map_key = getCacheKey_(src_parent);
        if (row < m_row_map[map_key].size()) {
            int64_t the_row = m_row_map[map_key][row];
            return toProxyIndex(m_source_model->getIndex(the_row, column, src_parent));
        }

        return ModelIndex();
    }

    bool ProjectModelSortFilterProxy::isSrcFiltered_(const UUID64 &src_uuid) const {
        ModelIndex child_index = m_source_model->getIndex(src_uuid);
        if (!m_source_model->validateIndex(child_index)) {
            return false;
        }

        std::string name = child_index.data<_ProjectIndexData>()->m_config.getProjectName();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](char c) { return ::tolower(static_cast<int>(c)); });
        return !name.starts_with(m_filter);
    }

    u64 ProjectModelSortFilterProxy::getCacheKey_(const ModelIndex &src_idx) const {
        return u64(src_idx.getUUID());
    }

    void ProjectModelSortFilterProxy::cacheIndex(const ModelIndex &src_idx) const {
        const u64 map_key = getCacheKey_(src_idx);

        if (m_source_model->canFetchMore(src_idx)) {
            m_source_model->fetchMore(src_idx);
        }

        const size_t row_count = m_source_model->getRowCount(src_idx);
        if (row_count == 0) {
            std::scoped_lock lock(m_cache_mutex);
            m_row_map[map_key].clear();
            return;
        }

        std::vector<UUID64> orig_children  = {};
        std::vector<UUID64> proxy_children = {};

        if (!m_source_model->validateIndex(src_idx)) {
            proxy_children.reserve(row_count * 0.5f);

            size_t i          = 0;
            ModelIndex root_s = m_source_model->getIndex(i++, 0);
            while (m_source_model->validateIndex(root_s)) {
                orig_children.push_back(root_s.getUUID());
                if (isSrcFiltered_(root_s.getUUID())) {
                    root_s = m_source_model->getIndex(i++, 0);
                    continue;
                }
                proxy_children.push_back(root_s.getUUID());
                root_s = m_source_model->getIndex(i++, 0);
            }
        } else {
            return;
        }

        if (proxy_children.empty()) {
            return;
        }

        struct SortItem {
            UUID64 m_uuid;
            const _ProjectIndexData *m_data;  // Pointer to the data
        };

        std::vector<SortItem> sort_vec;
        sort_vec.reserve(proxy_children.size());
        for (const auto &uuid : proxy_children) {
            sort_vec.push_back({uuid, m_source_model->getIndex(uuid).data<_ProjectIndexData>()});
        }

        using ComparatorFunc =
            std::function<bool(const _ProjectIndexData &, const _ProjectIndexData &)>;
        ComparatorFunc comparator;

        comparator = [&](const _ProjectIndexData &l, const _ProjectIndexData &r) {
            return _ProjectIndexDataCompareByDate(l, r, ModelSortOrder::SORT_ASCENDING);
        };

        // Only sort if a valid role was chosen
        if (comparator) {
            std::sort(sort_vec.begin(), sort_vec.end(),
                      [&](const SortItem &lhs, const SortItem &rhs) {
                          return comparator(*lhs.m_data, *rhs.m_data);
                      });
        }

        for (size_t i = 0; i < sort_vec.size(); ++i) {
            proxy_children[i] = sort_vec[i].m_uuid;
        }

        std::scoped_lock lock(m_cache_mutex);

        // Build the row map
        m_row_map[map_key].clear();
        m_row_map[map_key].resize(proxy_children.size());

        std::unordered_map<UUID64, size_t> orig_index_map;
        orig_index_map.reserve(orig_children.size());  // Pre-allocate memory
        for (size_t j = 0; j < orig_children.size(); j++) {
            orig_index_map[orig_children[j]] = j;
        }

        for (size_t i = 0; i < proxy_children.size(); i++) {
            m_row_map[map_key][i] = orig_index_map[proxy_children[i]];
        }
    }

    void ProjectModelSortFilterProxy::flushCache() const {
        std::unique_lock lk(m_cache_mutex);
        m_row_map.clear();
    }

    bool ProjectModelSortFilterProxy::isCached(const ModelIndex &index) const {
        const u64 map_key = getCacheKey_(index);

        std::scoped_lock lock(m_cache_mutex);
        return m_row_map.contains(map_key);
    }

    void ProjectModelSortFilterProxy::modelUpdateEvent(const ModelIndex &path, int flags) {
        if ((flags & ModelEventFlags::EVENT_ANY) == ModelEventFlags::EVENT_NONE) {
            return;
        }

        // if ((flags & ProjectModelEventFlags::EVENT_IS_FILE) &&
        //     (flags & ModelEventFlags::EVENT_INDEX_MODIFIED)) {
        //
        // }

        if ((flags & ModelEventFlags::EVENT_PRE)) {
            std::unique_lock lock(m_cache_mutex);
            m_row_map.erase(path.getUUID());

            ModelIndex parent = m_source_model->getParent(path);
            m_row_map.erase(parent.getUUID());
        }
    }

}  // namespace Toolbox
