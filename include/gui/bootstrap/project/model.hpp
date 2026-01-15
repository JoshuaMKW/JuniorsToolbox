#pragma once

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/mimedata/mimedata.hpp"
#include "core/types.hpp"
#include "fsystem.hpp"
#include "image/imagehandle.hpp"
#include "model/model.hpp"
#include "jsonlib.hpp"
#include "unique.hpp"

#include "watchdog/fswatchdog.hpp"

using namespace std::chrono;

namespace Toolbox {

    enum ProjectDataRole {
        PROJECT_DATA_ROLE_AGE = ModelDataRole::DATA_ROLE_USER,
        PROJECT_DATA_ROLE_PINNED,
        PROJECT_DATA_ROLE_PATH,
    };

    class ProjectModel : public IDataModel {
    public:
        using json_t = nlohmann::ordered_json;

    public:
        ProjectModel() = default;
        ~ProjectModel();

        void initialize(const fs_path &info_path);
        void saveToJSON(const fs_path &info_path);

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool validateIndex(const ModelIndex &index) const override {
            return IDataModel::validateIndex(index) && m_index_map.contains(index.getUUID());
        }

        [[nodiscard]] bool isReadOnly() const override { return false; }

        [[nodiscard]] system_clock::time_point getLastAccessed(const ModelIndex &index) const {
            return std::any_cast<system_clock::time_point>(
                getData(index, ProjectDataRole::PROJECT_DATA_ROLE_AGE));
        }
        void setLastAccessed(const ModelIndex &index, system_clock::time_point time) {
            setData(index, time, ProjectDataRole::PROJECT_DATA_ROLE_AGE);
        }

        [[nodiscard]] bool getPinned(const ModelIndex &index) const {
            return std::any_cast<bool>(getData(index, ProjectDataRole::PROJECT_DATA_ROLE_PINNED));
        }
        void setPinned(const ModelIndex &index, bool pinned) {
            setData(index, pinned, ProjectDataRole::PROJECT_DATA_ROLE_PINNED);
        }

        [[nodiscard]] fs_path getProjectPath(const ModelIndex &index) const {
            return std::any_cast<fs_path>(getData(index, ProjectDataRole::PROJECT_DATA_ROLE_PATH));
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;
        void setData(const ModelIndex &index, std::any data, int role) override;

    public:
        [[nodiscard]] ModelIndex getIndex(const fs_path &path) const;
        [[nodiscard]] ModelIndex getIndex(const UUID64 &uuid) const override;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] virtual ModelIndex makeIndex(const fs_path &path, int64_t row,
                                     const ModelIndex &parent) const;
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
        [[nodiscard]] bool
        insertMimeData(const ModelIndex &index, const MimeData &data,
                       ModelInsertPolicy policy = ModelInsertPolicy::INSERT_AFTER) override;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const override;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index) override;
        void fetchMore(const ModelIndex &index) override;

        void reset() override;

        void addEventListener(UUID64 uuid, event_listener_t listener, int allowed_flags) override;
        void removeEventListener(UUID64 uuid) override;

    protected:
        using Signal      = std::pair<ModelIndex, int>;
        using SignalQueue = std::vector<Signal>;

        [[nodiscard]] virtual Signal createSignalForIndex_(const ModelIndex &index,
                                                           ModelEventFlags base_event) const;

        // Implementation of public API for mutex locking reasons
        [[nodiscard]] std::any getData_(const ModelIndex &index, int role) const;
        void setData_(const ModelIndex &index, std::any data, int role) const;

        [[nodiscard]] ModelIndex getIndex_(const fs_path &path) const;
        [[nodiscard]] ModelIndex getIndex_(const UUID64 &uuid) const;
        [[nodiscard]] ModelIndex getIndex_(int64_t row, int64_t column,
                                           const ModelIndex &parent = ModelIndex()) const;
        [[nodiscard]] bool removeIndex_(const ModelIndex &index);

        [[nodiscard]] size_t getPathHash_(const ModelIndex &index) const;

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
        [[nodiscard]] bool
        insertMimeData_(const ModelIndex &index, const MimeData &data,
                        ModelInsertPolicy policy = ModelInsertPolicy::INSERT_AFTER);

        [[nodiscard]] bool canFetchMore_(const ModelIndex &index) const;
        void fetchMore_(const ModelIndex &index) const;
        // -- END -- //

        void signalEventListeners(const ModelIndex &index, int flags);

    private:
        UUID64 m_uuid;

        mutable std::mutex m_mutex;
        std::unordered_map<UUID64, std::pair<event_listener_t, int>> m_listeners;

        mutable std::map<UUID64, ModelIndex> m_index_map;
        mutable std::unordered_map<size_t, ModelIndex> m_path_map;
    };

    class ProjectModelSortFilterProxy : public IDataModel {
    public:
        ProjectModelSortFilterProxy()  = default;
        ~ProjectModelSortFilterProxy() = default;

        [[nodiscard]] bool validateIndex(const ModelIndex &index) const override {
            ModelIndex &&src_index = toSourceIndex(index);
            return m_source_model->validateIndex(src_index);
        }

        [[nodiscard]] bool isReadOnly() const override { return m_source_model->isReadOnly(); }

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] RefPtr<ProjectModel> getSourceModel() const;
        void setSourceModel(RefPtr<ProjectModel> model);

        [[nodiscard]] const std::string &getFilter() const &;
        void setFilter(const std::string &filter);

        [[nodiscard]] system_clock::time_point getLastAccessed(const ModelIndex &index) const {
            return std::any_cast<system_clock::time_point>(
                getData(index, ProjectDataRole::PROJECT_DATA_ROLE_AGE));
        }

        [[nodiscard]] bool getPinned(const ModelIndex &index) const {
            return std::any_cast<bool>(getData(index, ProjectDataRole::PROJECT_DATA_ROLE_PINNED));
        }

        [[nodiscard]] fs_path getProjectPath(const ModelIndex &index) const {
            return std::any_cast<fs_path>(getData(index, ProjectDataRole::PROJECT_DATA_ROLE_PATH));
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;
        void setData(const ModelIndex &index, std::any data, int role) override;

        [[nodiscard]] ModelIndex getIndex(const fs_path &path) const;
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
        [[nodiscard]] bool
        insertMimeData(const ModelIndex &index, const MimeData &data,
                       ModelInsertPolicy policy = ModelInsertPolicy::INSERT_AFTER) override;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const override;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index) override;
        void fetchMore(const ModelIndex &index) override;

        void reset() override;

        void addEventListener(UUID64 uuid, event_listener_t listener, int allowed_flags) override {
            m_source_model->addEventListener(uuid, listener, allowed_flags);
        }
        void removeEventListener(UUID64 uuid) override {
            m_source_model->removeEventListener(uuid);
        }

        [[nodiscard]] ModelIndex toSourceIndex(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex toProxyIndex(const ModelIndex &index) const;

    protected:
        [[nodiscard]] ModelIndex toProxyIndex(int64_t row, int64_t column,
                                              const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] bool isSrcFiltered_(const UUID64 &uuid) const;

        u64 getCacheKey_(const ModelIndex &src_idx) const;
        void cacheIndex(const ModelIndex &src_idx) const;
        void flushCache() const;
        bool isCached(const ModelIndex &src_idx) const;

        ModelIndex makeIndex(const fs_path &path, int64_t row, const ModelIndex &parent) {
            return ModelIndex();
        }

        void modelUpdateEvent(const ModelIndex &index, int flags);

    private:
        UUID64 m_uuid;

        RefPtr<ProjectModel> m_source_model = nullptr;
        std::string m_filter                   = "";

        mutable std::mutex m_cache_mutex;
        mutable std::unordered_map<UUID64, std::vector<int64_t>> m_row_map;
    };

}  // namespace Toolbox