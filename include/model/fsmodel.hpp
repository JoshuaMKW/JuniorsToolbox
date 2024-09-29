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
#include "unique.hpp"

#include "watchdog/fswatchdog.hpp"

namespace Toolbox {

    enum class FileSystemModelSortRole {
        SORT_ROLE_NONE,
        SORT_ROLE_NAME,
        SORT_ROLE_SIZE,
        SORT_ROLE_DATE,
    };

    enum class FileSystemModelOptions {
        NONE             = 0,
        DISABLE_WATCHDOG = BIT(0),
        DISABLE_SYMLINKS = BIT(1),
    };
    TOOLBOX_BITWISE_ENUM(FileSystemModelOptions)

    enum class FileSystemModelEventFlags {
        NONE                  = 0,
        EVENT_FILE_ADDED      = BIT(0),
        EVENT_FILE_MODIFIED   = BIT(1),
        EVENT_FOLDER_ADDED    = BIT(2),
        EVENT_FOLDER_MODIFIED = BIT(3),
        EVENT_PATH_RENAMED    = BIT(4),
        EVENT_PATH_REMOVED    = BIT(5),
        EVENT_FILE_ANY =
            EVENT_FILE_ADDED | EVENT_FILE_MODIFIED | EVENT_PATH_RENAMED | EVENT_PATH_REMOVED,
        EVENT_FOLDER_ANY =
            EVENT_FOLDER_ADDED | EVENT_FOLDER_MODIFIED | EVENT_PATH_RENAMED | EVENT_PATH_REMOVED,
        EVENT_ANY = EVENT_FILE_ANY | EVENT_FOLDER_ANY,
    };
    TOOLBOX_BITWISE_ENUM(FileSystemModelEventFlags)

    enum FileSystemDataRole {
        FS_DATA_ROLE_DATE = ModelDataRole::DATA_ROLE_USER,
        FS_DATA_ROLE_STATUS,
        FS_DATA_ROLE_TYPE,
    };

    class FileSystemModel : public IDataModel {
    public:
        struct FSTypeInfo {
            std::string m_name;
            std::string m_image_name;
        };

    public:
        FileSystemModel() = default;
        ~FileSystemModel();

        void initialize();

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] const fs_path &getRoot() const &;
        void setRoot(const fs_path &path);

        [[nodiscard]] FileSystemModelOptions getOptions() const;
        void setOptions(FileSystemModelOptions options);

        [[nodiscard]] bool isReadOnly() const;
        void setReadOnly(bool read_only);

        [[nodiscard]] bool isDirectory(const ModelIndex &index) const;
        [[nodiscard]] bool isFile(const ModelIndex &index) const;
        [[nodiscard]] bool isArchive(const ModelIndex &index) const;

        [[nodiscard]] size_t getFileSize(const ModelIndex &index) const;
        [[nodiscard]] size_t getDirSize(const ModelIndex &index, bool recursive) const;

        [[nodiscard]] Filesystem::file_time_type getLastModified(const ModelIndex &index) const {
            return std::any_cast<Filesystem::file_time_type>(
                getData(index, FileSystemDataRole::FS_DATA_ROLE_DATE));
        }
        [[nodiscard]] Filesystem::file_status getStatus(const ModelIndex &index) const {
            return std::any_cast<Filesystem::file_status>(
                getData(index, FileSystemDataRole::FS_DATA_ROLE_STATUS));
        }

        [[nodiscard]] std::string getType(const ModelIndex &index) const {
            return std::any_cast<std::string>(
                getData(index, FileSystemDataRole::FS_DATA_ROLE_TYPE));
        }

        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;

        ModelIndex mkdir(const ModelIndex &parent, const std::string &name);
        ModelIndex touch(const ModelIndex &parent, const std::string &name);
        ModelIndex rename(const ModelIndex &file, const std::string &new_name);

        bool rmdir(const ModelIndex &index);
        bool remove(const ModelIndex &index);

    public:
        [[nodiscard]] ModelIndex getIndex(const fs_path &path) const;
        [[nodiscard]] ModelIndex getIndex(const UUID64 &path) const override;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const override;
        [[nodiscard]] fs_path getPath(const ModelIndex &index) const;

        [[nodiscard]] ModelIndex getParent(const ModelIndex &index) const override;
        [[nodiscard]] ModelIndex getSibling(int64_t row, int64_t column,
                                            const ModelIndex &index) const override;

        [[nodiscard]] size_t getColumnCount(const ModelIndex &index) const override;
        [[nodiscard]] size_t getRowCount(const ModelIndex &index) const override;

        [[nodiscard]] bool hasChildren(const ModelIndex &parent = ModelIndex()) const override;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData(const std::vector<ModelIndex> &indexes) const override;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const override;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index) override;
        void fetchMore(const ModelIndex &index) override;

        using event_listener_t = std::function<void(const fs_path &, FileSystemModelEventFlags)>;
        void addEventListener(UUID64 uuid, event_listener_t listener,
                              FileSystemModelEventFlags flags);
        void removeEventListener(UUID64 uuid);

        static const ImageHandle &InvalidIcon();
        static const std::unordered_map<std::string, FSTypeInfo> &TypeMap();

    protected:
        [[nodiscard]] bool isDirectory_(const ModelIndex &index) const;
        [[nodiscard]] bool isFile_(const ModelIndex &index) const;
        [[nodiscard]] bool isArchive_(const ModelIndex &index) const;

        [[nodiscard]] size_t getFileSize_(const ModelIndex &index) const;
        [[nodiscard]] size_t getDirSize_(const ModelIndex &index, bool recursive) const;

        // Implementation of public API for mutex locking reasons
        [[nodiscard]] std::any getData_(const ModelIndex &index, int role) const;

        ModelIndex mkdir_(const ModelIndex &parent, const std::string &name);
        ModelIndex touch_(const ModelIndex &parent, const std::string &name);
        ModelIndex rename_(const ModelIndex &file, const std::string &new_name);

        bool rmdir_(const ModelIndex &index);
        bool remove_(const ModelIndex &index);

        [[nodiscard]] ModelIndex getIndex_(const fs_path &path) const;
        [[nodiscard]] ModelIndex getIndex_(const UUID64 &path) const;
        [[nodiscard]] ModelIndex getIndex_(int64_t row, int64_t column,
                                           const ModelIndex &parent = ModelIndex()) const;
        [[nodiscard]] fs_path getPath_(const ModelIndex &index) const;

        [[nodiscard]] ModelIndex getParent_(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex getSibling_(int64_t row, int64_t column,
                                             const ModelIndex &index) const;

        [[nodiscard]] size_t getColumnCount_(const ModelIndex &index) const;
        [[nodiscard]] size_t getRowCount_(const ModelIndex &index) const;

        [[nodiscard]] bool hasChildren_(const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData_(const std::vector<ModelIndex> &indexes) const;

        [[nodiscard]] bool canFetchMore_(const ModelIndex &index);
        void fetchMore_(const ModelIndex &index);
        // -- END -- //

        ModelIndex makeIndex(const fs_path &path, int64_t row, const ModelIndex &parent) override;

        ModelIndex getParentArchive(const ModelIndex &index) const;

        size_t pollChildren(const ModelIndex &index) const;

        void folderAdded(const fs_path &path);
        void folderModified(const fs_path &path);

        void fileAdded(const fs_path &path);
        void fileModified(const fs_path &path);

        void pathRenamedSrc(const fs_path &old_path);
        void pathRenamedDst(const fs_path &new_path);

        void pathRemoved(const fs_path &path);

    private:
        UUID64 m_uuid;

        mutable std::mutex m_mutex;
        FileSystemWatchdog m_watchdog;
        std::unordered_map<UUID64, std::pair<event_listener_t, FileSystemModelEventFlags>>
            m_listeners;

        fs_path m_root_path;

        FileSystemModelOptions m_options = FileSystemModelOptions::NONE;
        bool m_read_only                 = false;

        UUID64 m_root_index;

        mutable std::unordered_map<UUID64, ModelIndex> m_index_map;
        mutable std::unordered_map<std::string, RefPtr<const ImageHandle>> m_icon_map;

        fs_path m_rename_src;
    };

    class FileSystemModelSortFilterProxy : public IDataModel {
    public:
        FileSystemModelSortFilterProxy()  = default;
        ~FileSystemModelSortFilterProxy() = default;

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool isDirsOnly() const { return m_dirs_only; }
        void setDirsOnly(bool dirs_only) { m_dirs_only = dirs_only; }

        [[nodiscard]] RefPtr<FileSystemModel> getSourceModel() const;
        void setSourceModel(RefPtr<FileSystemModel> model);

        [[nodiscard]] ModelSortOrder getSortOrder() const;
        void setSortOrder(ModelSortOrder order);

        [[nodiscard]] FileSystemModelSortRole getSortRole() const;
        void setSortRole(FileSystemModelSortRole role);

        [[nodiscard]] const std::string &getFilter() const &;
        void setFilter(const std::string &filter);

        [[nodiscard]] bool isReadOnly() const;
        void setReadOnly(bool read_only);

        [[nodiscard]] bool isDirectory(const ModelIndex &index) const;
        [[nodiscard]] bool isFile(const ModelIndex &index) const;
        [[nodiscard]] bool isArchive(const ModelIndex &index) const;

        [[nodiscard]] size_t getFileSize(const ModelIndex &index);
        [[nodiscard]] size_t getDirSize(const ModelIndex &index, bool recursive);

        [[nodiscard]] Filesystem::file_time_type getLastModified(const ModelIndex &index) const;
        [[nodiscard]] Filesystem::file_status getStatus(const ModelIndex &index) const;

        [[nodiscard]] std::string getType(const ModelIndex &index) const;
        [[nodiscard]] std::any getData(const ModelIndex &index, int role) const override;

        ModelIndex mkdir(const ModelIndex &parent, const std::string &name);
        ModelIndex touch(const ModelIndex &parent, const std::string &name);

        bool rmdir(const ModelIndex &index);
        bool remove(const ModelIndex &index);

        [[nodiscard]] ModelIndex getIndex(const fs_path &path) const;
        [[nodiscard]] ModelIndex getIndex(const UUID64 &path) const;
        [[nodiscard]] ModelIndex getIndex(int64_t row, int64_t column,
                                          const ModelIndex &parent = ModelIndex()) const;
        [[nodiscard]] fs_path getPath(const ModelIndex &index) const;

        [[nodiscard]] ModelIndex getParent(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex getSibling(int64_t row, int64_t column,
                                            const ModelIndex &index) const;

        [[nodiscard]] size_t getColumnCount(const ModelIndex &index) const;
        [[nodiscard]] size_t getRowCount(const ModelIndex &index) const;

        [[nodiscard]] bool hasChildren(const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] ScopePtr<MimeData>
        createMimeData(const std::vector<ModelIndex> &indexes) const;
        [[nodiscard]] std::vector<std::string> getSupportedMimeTypes() const;

        [[nodiscard]] bool canFetchMore(const ModelIndex &index);
        void fetchMore(const ModelIndex &index);

        [[nodiscard]] ModelIndex toSourceIndex(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex toProxyIndex(const ModelIndex &index) const;

        [[nodiscard]] UUID64 getSourceUUID(const ModelIndex &index) const;

    protected:
        [[nodiscard]] ModelIndex toProxyIndex(int64_t row, int64_t column,
                                              const ModelIndex &parent = ModelIndex()) const;

        [[nodiscard]] bool isFiltered(const UUID64 &uuid) const;

        void cacheIndex(const ModelIndex &index) const;
        void cacheIndex_(const ModelIndex &index) const;

        ModelIndex makeIndex(const fs_path &path, int64_t row, const ModelIndex &parent) {
            return ModelIndex();
        }

        void fsUpdateEvent(const fs_path &path, FileSystemModelEventFlags flags);

    private:
        UUID64 m_uuid;

        RefPtr<FileSystemModel> m_source_model = nullptr;
        ModelSortOrder m_sort_order            = ModelSortOrder::SORT_ASCENDING;
        FileSystemModelSortRole m_sort_role    = FileSystemModelSortRole::SORT_ROLE_NONE;
        std::string m_filter                   = "";

        bool m_dirs_only = false;

        mutable std::mutex m_cache_mutex;
        mutable std::unordered_map<UUID64, bool> m_filter_map;
        mutable std::unordered_map<UUID64, std::vector<int64_t>> m_row_map;
    };

}  // namespace Toolbox