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
#include "fsystem.hpp"
#include "image/imagehandle.hpp"
#include "unique.hpp"

namespace Toolbox {

    enum class ModelSortOrder {
        SORT_ASCENDING,
        SORT_DESCENDING,
    };

    class ModelIndex : public IUnique {
    public:
        ModelIndex() = default;
        ModelIndex(int64_t row, int64_t column, UUID64 model_uuid)
            : m_row(row), m_column(column), m_model_uuid(model_uuid) {}

        bool isValid() const { return m_row >= 0 && m_column >= 0 && m_model_uuid != 0; }

        template <typename T = void *> [[nodiscard]] data() const {
            return reinterpret_cast<T *>(m_user_data);
        }
        void setData(void *data) { m_user_data = data; }

        [[nodiscard]] UUID64 getUUID() const override { return m_uuid; }
        [[nodiscard]] UUID64 getModelUUID() const { return m_model_uuid; }

    private:
        UUID64 m_uuid;
        UUID64 m_model_uuid = 0;

        int64_t m_row    = -1;
        int64_t m_column = -1;

        void *m_user_data = nullptr;
    };

    class FileSystemModel {
    public:
        enum class Options {
            DISABLE_WATCHDOG = BIT(0),
            DISABLE_SYMLINKS = BIT(1),
        };
        TOOLBOX_BITWISE_ENUM(Options)

    public:
        FileSystemModel()  = default;
        ~FileSystemModel() = default;

        const fs_path &getRoot() const &;
        void setRoot(const fs_path &path);

        const std::string &getFilter() const &;
        void setFilter(const std::string &filter);

        Options getOptions() const;
        void setOptions(Options options);

        bool isReadOnly() const;
        void setReadOnly();

        bool isDirectory(const ModelIndex &index);
        bool isFile(const ModelIndex &index);
        bool isArchive(const ModelIndex &index);

        size_t getFileSize(const ModelIndex &index);
        size_t getDirSize(const ModelIndex &index, bool recursive);

        const ImageHandle &getIcon(const ModelIndex &index) const &;
        Filesystem::file_time_type getLastModified(const ModelIndex &index) const;
        Filesystem::perms getPermissions(const ModelIndex &index) const;
        const Filesystem::file_status &getStatus(const ModelIndex &index) const &;

        std::string getType(const ModelIndex &index) const &;

        ModelIndex mkdir(const ModelIndex &parent, const std::string &name);
        ModelIndex touch(const ModelIndex &parent, const std::string &name);

        bool rmdir(const ModelIndex &index);
        bool remove(const ModelIndex &index);

    public:
        const ModelIndex &getIndex(const fs_path &path) const &;
        const ModelIndex &getIndex(int64_t row, int64_t column,
                                   ModelIndex &parent = ModelIndex()) const &;
        const fs_path &getPath(const ModelIndex &index) const &;

        const Modelindex &getParent(const ModelIndex &index) const &;
        const ModelIndex &getSibling(int64_t row, int64_t column, const ModelIndex &index);

        size_t getColumnCount(const ModelIndex &index) const;
        size_t getRowCount(const ModelIndex &index) const;

        bool hasChildren(const ModelIndex &parent = ModelIndex()) const;

        ScopePtr<MimeData> createMimeData(const std::vector<ModelIndex> &indexes) const;
        std::vector<std::string> getSupportedMimeTypes() const;

        bool canFetchMore(const ModelIndex &index);
        void fetchMore(const ModelIndex &index););

        void sort(int64_t column, ModelSortOrder order = ModelSortOrder::SORT_ASCENDING);

    protected:
        void cacheIndex(ModelIndex &index);
        void cacheFolder(ModelIndex &index);
        void cacheFile(ModelIndex &index);
        void cacheArchive(ModelIndex &index);

        const ModelIndex &getParentArchive(const ModelIndex &index);

    private:
        fs_path m_root_path;
    };

}  // namespace Toolbox