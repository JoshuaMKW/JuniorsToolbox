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
#include "unique.hpp"

namespace Toolbox {

    enum class ModelSortOrder {
        SORT_ASCENDING,
        SORT_DESCENDING,
    };

    enum class ModelSortRole {
        SORT_ROLE_NONE,
        SORT_ROLE_NAME,
        SORT_ROLE_SIZE,
        SORT_ROLE_DATE,
    };

    class ModelIndex : public IUnique {
    public:
        ModelIndex() = default;
        ModelIndex(UUID64 model_uuid) : m_model_uuid(model_uuid) {}
        ModelIndex(const ModelIndex &other)
            : m_uuid(other.m_uuid), m_model_uuid(other.m_model_uuid),
              m_user_data(other.m_user_data) {}

        bool isValid() const { return m_model_uuid != 0; }

        template <typename T = void> [[nodiscard]] T *data() const {
            return reinterpret_cast<T *>(m_user_data);
        }
        void setData(void *data) { m_user_data = data; }

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }
        [[nodiscard]] UUID64 getModelUUID() const { return m_model_uuid; }

        operator bool() const { return isValid(); }

        ModelIndex &operator=(const ModelIndex &other) {
            m_uuid       = other.m_uuid;
            m_model_uuid = other.m_model_uuid;
            m_user_data  = other.m_user_data;
            return *this;
        }

        [[nodiscard]] bool operator==(const ModelIndex &other) const {
            return m_uuid == other.m_uuid && m_model_uuid == other.m_model_uuid;
        }

    private:
        UUID64 m_uuid;
        UUID64 m_model_uuid = 0;

        void *m_user_data = nullptr;
    };

    enum class FileSystemModelOptions {
        NONE             = 0,
        DISABLE_WATCHDOG = BIT(0),
        DISABLE_SYMLINKS = BIT(1),
    };
    TOOLBOX_BITWISE_ENUM(FileSystemModelOptions)

    class FileSystemModel : public IUnique {
    public:
        struct FSTypeInfo {
            std::string m_name;
            ImageHandle m_icon;
        };

    public:
        FileSystemModel()  = default;
        ~FileSystemModel() = default;

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool validateIndex(const ModelIndex &index) const;

        [[nodiscard]] const fs_path &getRoot() const &;
        void setRoot(const fs_path &path);

        [[nodiscard]] FileSystemModelOptions getOptions() const;
        void setOptions(FileSystemModelOptions options);

        [[nodiscard]] bool isReadOnly() const;
        void setReadOnly(bool read_only);

        [[nodiscard]] bool isDirectory(const ModelIndex &index) const;
        [[nodiscard]] bool isFile(const ModelIndex &index) const;
        [[nodiscard]] bool isArchive(const ModelIndex &index) const;

        [[nodiscard]] size_t getFileSize(const ModelIndex &index);
        [[nodiscard]] size_t getDirSize(const ModelIndex &index, bool recursive);

        [[nodiscard]] const ImageHandle &getIcon(const ModelIndex &index) const &;
        [[nodiscard]] Filesystem::file_time_type getLastModified(const ModelIndex &index) const;
        [[nodiscard]] Filesystem::file_status getStatus(const ModelIndex &index) const;

        [[nodiscard]] std::string getType(const ModelIndex &index) const &;

        ModelIndex mkdir(const ModelIndex &parent, const std::string &name);
        ModelIndex touch(const ModelIndex &parent, const std::string &name);

        bool rmdir(const ModelIndex &index);
        bool remove(const ModelIndex &index);

    public:
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

        static const ImageHandle &InvalidIcon();
        static const std::unordered_map<std::string, FSTypeInfo> &TypeMap();

    protected:
        ModelIndex makeIndex(const fs_path &path, int64_t row, const ModelIndex &parent);

        void cacheIndex(ModelIndex &index);
        void cacheFolder(ModelIndex &index);
        void cacheFile(ModelIndex &index);
        void cacheArchive(ModelIndex &index);

        ModelIndex getParentArchive(const ModelIndex &index) const;

        size_t pollChildren(const ModelIndex &index) const;

    private:
        UUID64 m_uuid;

        fs_path m_root_path;

        FileSystemModelOptions m_options = FileSystemModelOptions::NONE;
        bool m_read_only                 = false;

        UUID64 m_root_index;

        mutable std::unordered_map<UUID64, ModelIndex> m_index_map;
    };

    class FileSystemModelSortFilterProxy : public IUnique {
    public:
        FileSystemModelSortFilterProxy()  = default;
        ~FileSystemModelSortFilterProxy() = default;

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }

        [[nodiscard]] bool validateIndex(const ModelIndex &index) const;

        [[nodiscard]] bool isDirsOnly() const { return m_dirs_only; }
        void setDirsOnly(bool dirs_only) { m_dirs_only = true; }

        [[nodiscard]] RefPtr<FileSystemModel> getSourceModel() const;
        void setSourceModel(RefPtr<FileSystemModel> model);

        [[nodiscard]] ModelSortOrder getSortOrder() const;
        void setSortOrder(ModelSortOrder order);

        [[nodiscard]] ModelSortRole getSortRole() const;
        void setSortRole(ModelSortRole role);

        [[nodiscard]] const std::string &getFilter() const &;
        void setFilter(const std::string &filter);

        [[nodiscard]] bool isReadOnly() const;
        void setReadOnly(bool read_only);

        [[nodiscard]] bool isDirectory(const ModelIndex &index) const;
        [[nodiscard]] bool isFile(const ModelIndex &index) const;
        [[nodiscard]] bool isArchive(const ModelIndex &index) const;

        [[nodiscard]] size_t getFileSize(const ModelIndex &index);
        [[nodiscard]] size_t getDirSize(const ModelIndex &index, bool recursive);

        [[nodiscard]] const ImageHandle &getIcon(const ModelIndex &index) const &;
        [[nodiscard]] Filesystem::file_time_type getLastModified(const ModelIndex &index) const;
        [[nodiscard]] Filesystem::file_status getStatus(const ModelIndex &index) const;

        [[nodiscard]] std::string getType(const ModelIndex &index) const &;

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

    protected:
        [[nodiscard]] ModelIndex toSourceIndex(const ModelIndex &index) const;
        [[nodiscard]] ModelIndex toProxyIndex(const ModelIndex &index) const;

    private:
        UUID64 m_uuid;

        RefPtr<FileSystemModel> m_source_model = nullptr;
        ModelSortOrder m_sort_order            = ModelSortOrder::SORT_ASCENDING;
        ModelSortRole m_sort_role              = ModelSortRole::SORT_ROLE_NONE;
        std::string m_filter                   = "";

        bool m_dirs_only = false;
    };

}  // namespace Toolbox