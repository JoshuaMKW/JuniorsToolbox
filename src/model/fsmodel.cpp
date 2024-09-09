#include <algorithm>
#include <compare>
#include <set>

#include "model/fsmodel.hpp"

namespace Toolbox {

    struct _FileSystemIndexData {
        enum class Type {
            FILE,
            DIRECTORY,
            ARCHIVE,
            UNKNOWN,
        };

        UUID64 m_parent                = 0;
        std::vector<UUID64> m_children = {};

        Type m_type = Type::UNKNOWN;
        fs_path m_path;
        size_t m_size = 0;
        Filesystem::file_time_type m_date;

        bool hasChild(UUID64 uuid) const {
            return std::find(m_children.begin(), m_children.end(), uuid) != m_children.end();
        }

        std::strong_ordering operator<=>(const _FileSystemIndexData &rhs) const {
            return m_path <=> rhs.m_path;
        }
    };

    static bool _FileSystemIndexDataIsDirLike(const _FileSystemIndexData &data) {
        return data.m_type == _FileSystemIndexData::Type::DIRECTORY ||
               data.m_type == _FileSystemIndexData::Type::ARCHIVE;
    }

    static bool _FileSystemIndexDataCompareByName(const _FileSystemIndexData &lhs,
                                                  const _FileSystemIndexData &rhs,
                                                  ModelSortOrder order) {
        const bool is_lhs_dir = _FileSystemIndexDataIsDirLike(lhs);
        const bool is_rhs_dir = _FileSystemIndexDataIsDirLike(rhs);

        // Folders come first
        if (is_lhs_dir && !is_rhs_dir) {
            return true;
        }

        if (!is_lhs_dir && is_rhs_dir) {
            return false;
        }

        // Sort by name
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs.m_path < rhs.m_path;
        } else {
            return lhs.m_path > rhs.m_path;
        }
    }

    static bool _FileSystemIndexDataCompareBySize(const _FileSystemIndexData &lhs,
                                                  const _FileSystemIndexData &rhs,
                                                  ModelSortOrder order) {
        const bool is_lhs_dir = _FileSystemIndexDataIsDirLike(lhs);
        const bool is_rhs_dir = _FileSystemIndexDataIsDirLike(rhs);

        // Folders come first
        if (is_lhs_dir && !is_rhs_dir) {
            return true;
        }

        if (!is_lhs_dir && is_rhs_dir) {
            return false;
        }

        // Sort by size
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs.m_size < rhs.m_size;
        } else {
            return lhs.m_size > rhs.m_size;
        }
    }

    static bool _FileSystemIndexDataCompareByDate(const _FileSystemIndexData &lhs,
                                                  const _FileSystemIndexData &rhs,
                                                  ModelSortOrder order) {
        const bool is_lhs_dir = _FileSystemIndexDataIsDirLike(lhs);
        const bool is_rhs_dir = _FileSystemIndexDataIsDirLike(rhs);

        // Folders come first
        if (is_lhs_dir && !is_rhs_dir) {
            return true;
        }

        if (!is_lhs_dir && is_rhs_dir) {
            return false;
        }

        // Sort by date
        if (order == ModelSortOrder::SORT_ASCENDING) {
            return lhs.m_date < rhs.m_date;
        } else {
            return lhs.m_date > rhs.m_date;
        }
    }

    const fs_path &FileSystemModel::getRoot() const & { return m_root_path; }

    void FileSystemModel::setRoot(const fs_path &path) {
        if (m_root_path == path) {
            return;
        }

        m_index_map.clear();

        m_root_path  = path;
        m_root_index = makeIndex(path, 0, ModelIndex()).getUUID();
    }

    FileSystemModelOptions FileSystemModel::getOptions() const { return m_options; }

    void FileSystemModel::setOptions(FileSystemModelOptions options) { m_options = options; }

    bool FileSystemModel::isReadOnly() const { return m_read_only; }

    void FileSystemModel::setReadOnly(bool read_only) { m_read_only = read_only; }

    bool FileSystemModel::isDirectory(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;
        }
        return index.data<_FileSystemIndexData>()->m_type == _FileSystemIndexData::Type::DIRECTORY;
    }

    bool FileSystemModel::isFile(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;
        }
        return index.data<_FileSystemIndexData>()->m_type == _FileSystemIndexData::Type::FILE;
    }

    bool FileSystemModel::isArchive(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;
        }
        return index.data<_FileSystemIndexData>()->m_type == _FileSystemIndexData::Type::ARCHIVE;
    }

    size_t FileSystemModel::getFileSize(const ModelIndex &index) {
        if (!isFile(index)) {
            return 0;
        }
        return index.data<_FileSystemIndexData>()->m_size;
    }

    size_t FileSystemModel::getDirSize(const ModelIndex &index, bool recursive) {
        if (!(isDirectory(index) || isArchive(index))) {
            return 0;
        }
        return index.data<_FileSystemIndexData>()->m_size;
    }

    const ImageHandle &FileSystemModel::getIcon(const ModelIndex &index) const & {
        if (!validateIndex(index)) {
            return TypeMap().at("_Invalid").m_icon;
        }

        if (isDirectory(index)) {
            return TypeMap().at("_Folder").m_icon;
        }

        if (isArchive(index)) {
            return TypeMap().at("_Archive").m_icon;
        }

        std::string ext = index.data<_FileSystemIndexData>()->m_path.extension().string();
        if (ext.empty()) {
            return TypeMap().at("_Folder").m_icon;
        }

        if (TypeMap().find(ext) == TypeMap().end()) {
            return TypeMap().at("_File").m_icon;
        }

        return TypeMap().at(ext).m_icon;
    }

    Filesystem::file_time_type FileSystemModel::getLastModified(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return Filesystem::file_time_type();
        }

        Filesystem::file_time_type result = Filesystem::file_time_type();

        Filesystem::last_write_time(index.data<_FileSystemIndexData>()->m_path)
            .and_then([&](Filesystem::file_time_type &&time) {
                result = std::move(time);
                return Result<Filesystem::file_time_type, FSError>();
            })
            .or_else([&](const FSError &error) {
                TOOLBOX_ERROR_V("[FileSystemModel] Failed to get last write time: {}",
                                error.m_message[0]);
                return Result<Filesystem::file_time_type, FSError>();
            });

        return result;
    }

    Filesystem::file_status FileSystemModel::getStatus(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return Filesystem::file_status();
        }

        Filesystem::file_status result = Filesystem::file_status();

        Filesystem::status(index.data<_FileSystemIndexData>()->m_path)
            .and_then([&](Filesystem::file_status &&status) {
                result = std::move(status);
                return Result<Filesystem::file_status, FSError>();
            })
            .or_else([&](const FSError &error) {
                TOOLBOX_ERROR_V("[FileSystemModel] Failed to get last write status: {}",
                                error.m_message[0]);
                return Result<Filesystem::file_status, FSError>();
            });

        return result;
    }

    std::string FileSystemModel::getType(const ModelIndex &index) const & {
        if (!validateIndex(index)) {
            return TypeMap().at("_Invalid").m_name;
        }

        if (isDirectory(index)) {
            return TypeMap().at("_Folder").m_name;
        }

        if (isArchive(index)) {
            return TypeMap().at("_Archive").m_name;
        }

        std::string ext = index.data<_FileSystemIndexData>()->m_path.extension().string();
        if (ext.empty()) {
            return TypeMap().at("_Folder").m_name;
        }

        if (TypeMap().find(ext) == TypeMap().end()) {
            return TypeMap().at("_File").m_name;
        }

        return TypeMap().at(ext).m_name;
    }

    ModelIndex FileSystemModel::mkdir(const ModelIndex &parent, const std::string &name) {
        if (!validateIndex(parent)) {
            return ModelIndex();
        }

        bool result = false;

        if (isDirectory(parent)) {
            fs_path path = parent.data<_FileSystemIndexData>()->m_path / name;
            Filesystem::create_directory(path)
                .and_then([&](bool created) {
                    if (!created) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to create directory: {}",
                                        path.string());
                        return Result<bool, FSError>();
                    }
                    result = true;
                    return Result<bool, FSError>();
                })
                .or_else([&](const FSError &error) {
                    TOOLBOX_ERROR_V("[FileSystemModel] Failed to create directory: {}",
                                    error.m_message[0]);
                    return Result<bool, FSError>();
                });
        } else if (isArchive(parent)) {
            TOOLBOX_ERROR("[FileSystemModel] Archive creation is unimplemented!");
        } else {
            TOOLBOX_ERROR("[FileSystemModel] Parent index is not a directory!");
        }

        if (!result) {
            return ModelIndex();
        }

        return makeIndex(parent.data<_FileSystemIndexData>()->m_path / name, getRowCount(parent),
                         parent);
    }

    ModelIndex FileSystemModel::touch(const ModelIndex &parent, const std::string &name) {
        if (!validateIndex(parent)) {
            return ModelIndex();
        }

        bool result = false;

        if (isDirectory(parent)) {
            fs_path path = parent.data<_FileSystemIndexData>()->m_path / name;
            std::ofstream file(path);
            if (!file.is_open()) {
                TOOLBOX_ERROR_V("[FileSystemModel] Failed to create file: {}", path.string());
                return ModelIndex();
            }
            file.close();
            result = true;
        } else if (isArchive(parent)) {
            TOOLBOX_ERROR("[FileSystemModel] Archive creation is unimplemented!");
        } else {
            TOOLBOX_ERROR("[FileSystemModel] Parent index is not a directory!");
        }

        if (!result) {
            return ModelIndex();
        }

        return makeIndex(parent.data<_FileSystemIndexData>()->m_path / name, getRowCount(parent),
                         parent);
    }

    bool FileSystemModel::rmdir(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        bool result = false;

        if (isDirectory(index)) {
            Filesystem::remove_all(index.data<_FileSystemIndexData>()->m_path)
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove directory: {}",
                                        index.data<_FileSystemIndexData>()->m_path.string());
                        return Result<bool, FSError>();
                    }
                    result = true;
                    return Result<bool, FSError>();
                })
                .or_else([&](const FSError &error) {
                    TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove directory: {}",
                                    error.m_message[0]);
                    return Result<bool, FSError>();
                });
        } else if (isArchive(index)) {
            Filesystem::remove(index.data<_FileSystemIndexData>()->m_path)
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove archive: {}",
                                        index.data<_FileSystemIndexData>()->m_path.string());
                        return Result<bool, FSError>();
                    }
                    result = true;
                    return Result<bool, FSError>();
                })
                .or_else([&](const FSError &error) {
                    TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove archive: {}",
                                    error.m_message[0]);
                    return Result<bool, FSError>();
                });
        } else {
            TOOLBOX_ERROR("[FileSystemModel] Index is not a directory!");
        }

        if (result) {
            ModelIndex parent = getParent(index);
            if (validateIndex(parent)) {
                _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();
                parent_data->m_children.erase(std::remove(parent_data->m_children.begin(),
                                                          parent_data->m_children.end(),
                                                          index.getUUID()),
                                              parent_data->m_children.end());
                m_index_map.erase(index.getUUID());
            }
        }

        return result;
    }

    bool FileSystemModel::remove(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        bool result = false;

        if (isFile(index)) {
            Filesystem::remove(index.data<_FileSystemIndexData>()->m_path)
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove file: {}",
                                        index.data<_FileSystemIndexData>()->m_path.string());
                        return Result<bool, FSError>();
                    }
                    result = true;
                    return Result<bool, FSError>();
                })
                .or_else([&](const FSError &error) {
                    TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove file: {}",
                                    error.m_message[0]);
                    return Result<bool, FSError>();
                });
        } else if (isArchive(index)) {
            TOOLBOX_ERROR("[FileSystemModel] Index is not a file!");
        } else {
            TOOLBOX_ERROR("[FileSystemModel] Index is not a file!");
        }

        if (result) {
            ModelIndex parent = getParent(index);
            if (validateIndex(parent)) {
                _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();
                parent_data->m_children.erase(std::remove(parent_data->m_children.begin(),
                                                          parent_data->m_children.end(),
                                                          index.getUUID()),
                                              parent_data->m_children.end());
                m_index_map.erase(index.getUUID());
            }
        }

        return result;
    }

    ModelIndex FileSystemModel::getIndex(const fs_path &path) const {
        if (m_index_map.empty()) {
            return ModelIndex();
        }

        for (const auto &[uuid, index] : m_index_map) {
            if (index.data<_FileSystemIndexData>()->m_path == path) {
                return index;
            }
        }

        return ModelIndex();
    }

    ModelIndex FileSystemModel::getIndex(const UUID64 &uuid) const { return m_index_map.at(uuid); }

    ModelIndex FileSystemModel::getIndex(int64_t row, int64_t column,
                                         const ModelIndex &parent) const {
        if (!validateIndex(parent)) {
            if (m_root_index != 0 && row == 0 && column == 0) {
                return m_index_map.at(m_root_index);
            }
            return ModelIndex();
        }

        _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();
        if (row < 0 || (size_t)row >= parent_data->m_children.size()) {
            return ModelIndex();
        }

        return m_index_map.at(parent_data->m_children[row]);
    }

    fs_path FileSystemModel::getPath(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return fs_path();
        }

        return index.data<_FileSystemIndexData>()->m_path;
    }

    ModelIndex FileSystemModel::getParent(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        const UUID64 &id = index.data<_FileSystemIndexData>()->m_parent;
        if (id == 0) {
            return ModelIndex();
        }

        return m_index_map.at(id);
    }

    ModelIndex FileSystemModel::getSibling(int64_t row, int64_t column,
                                           const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        const ModelIndex &parent = getParent(index);
        if (!validateIndex(parent)) {
            return ModelIndex();
        }

        return getIndex(row, column, parent);
    }

    size_t FileSystemModel::getColumnCount(const ModelIndex &index) const { return 1; }

    size_t FileSystemModel::getRowCount(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 0;
        }

        return index.data<_FileSystemIndexData>()->m_children.size();
    }

    bool FileSystemModel::hasChildren(const ModelIndex &parent) const {
        return pollChildren(parent) > 0;
    }

    ScopePtr<MimeData>
    FileSystemModel::createMimeData(const std::vector<ModelIndex> &indexes) const {
        TOOLBOX_ERROR("[FileSystemModel] Mimedata unimplemented!");
        return ScopePtr<MimeData>();
    }

    std::vector<std::string> FileSystemModel::getSupportedMimeTypes() const {
        return std::vector<std::string>();
    }

    bool FileSystemModel::canFetchMore(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        if (!isDirectory(index) && !isArchive(index)) {
            return false;
        }

        return index.data<_FileSystemIndexData>()->m_children.empty();
    }

    void FileSystemModel::fetchMore(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return;
        }

        if (isDirectory(index)) {
            fs_path path = index.data<_FileSystemIndexData>()->m_path;

            size_t i = 0;
            for (const auto &entry : Filesystem::directory_iterator(path)) {
                makeIndex(entry.path(), i++, index);
            }
        }
    }

    const ImageHandle &FileSystemModel::InvalidIcon() {
        static ImageHandle s_invalid_fs_icon = ImageHandle("Images/Icons/fs_invalid.png");
        return s_invalid_fs_icon;
    }

    const std::unordered_map<std::string, FileSystemModel::FSTypeInfo> &FileSystemModel::TypeMap() {
        // clang-format off
        static std::unordered_map<std::string, FileSystemModel::FSTypeInfo> s_type_map = {
            {"_Invalid", FSTypeInfo("Invalid",                  ImageHandle("Images/Icons/Filesystem/fs_invalid.png"))},
            {"_Folder",  FSTypeInfo("Folder",                   ImageHandle("Images/Icons/Filesystem/fs_generic_folder.png")) },
            {"_Archive", FSTypeInfo("Archive",                  ImageHandle("Images/Icons/Filesystem/fs_arc.png"))},
            {"_File",    FSTypeInfo("File",                     ImageHandle("Images/Icons/Filesystem/fs_generic_file.png"))   },
            {".txt",     FSTypeInfo("Text",                     ImageHandle("Images/Icons/Filesystem/fs_txt.png"))    },
            {".md",      FSTypeInfo("Markdown",                 ImageHandle("Images/Icons/Filesystem/fs_md.png"))     },
            {".c",       FSTypeInfo("C",                        ImageHandle("Images/Icons/Filesystem/fs_c.png"))      },
            {".h",       FSTypeInfo("Header",                   ImageHandle("Images/Icons/Filesystem/fs_h.png"))      },
            {".cpp",     FSTypeInfo("C++",                      ImageHandle("Images/Icons/Filesystem/fs_cpp.png"))    },
            {".hpp",     FSTypeInfo("Header",                   ImageHandle("Images/Icons/Filesystem/fs_hpp.png"))    },
            {".cxx",     FSTypeInfo("C++",                      ImageHandle("Images/Icons/Filesystem/fs_cpp.png"))    },
            {".hxx",     FSTypeInfo("Header",                   ImageHandle("Images/Icons/Filesystem/fs_hpp.png"))    },
            {".c++",     FSTypeInfo("C++",                      ImageHandle("Images/Icons/Filesystem/fs_cpp.png"))    },
            {".h++",     FSTypeInfo("Header",                   ImageHandle("Images/Icons/Filesystem/fs_hpp.png"))    },
            {".cc",      FSTypeInfo("C++",                      ImageHandle("Images/Icons/Filesystem/fs_cpp.png"))    },
            {".hh",      FSTypeInfo("Header",                   ImageHandle("Images/Icons/Filesystem/fs_hpp.png"))    },
            {".arc",     FSTypeInfo("Archive",                  ImageHandle("Images/Icons/Filesystem/fs_arc.png"))    },
            {".bas",     FSTypeInfo("JAudio Sequence",          ImageHandle("Images/Icons/Filesystem/fs_bas.png"))    },
            {".bck",     FSTypeInfo("J3D Bone Animation",       ImageHandle("Images/Icons/Filesystem/fs_bck.png"))    },
            {".bdl",     FSTypeInfo("J3D Model Data",           ImageHandle("Images/Icons/Filesystem/fs_bdl.png"))    },
            {".blo",     FSTypeInfo("J2D Screen Layout",        ImageHandle("Images/Icons/Filesystem/fs_blo.png"))    },
            {".bmd",     FSTypeInfo("J3D Model Data",           ImageHandle("Images/Icons/Filesystem/fs_bmd.png"))    },
            {".bmg",     FSTypeInfo("Message Data",             ImageHandle("Images/Icons/Filesystem/fs_bmg.png"))    },
            {".bmt",     FSTypeInfo("J3D Material Table",       ImageHandle("Images/Icons/Filesystem/fs_bmt.png"))    },
            {".brk",     FSTypeInfo("J3D Color Register Anim",  ImageHandle("Images/Icons/Filesystem/fs_brk.png"))    },
            {".bti",     FSTypeInfo("J2D Texture Image",        ImageHandle("Images/Icons/Filesystem/fs_bti.png"))    },
            {".btk",     FSTypeInfo("J2D Texture Animation",    ImageHandle("Images/Icons/Filesystem/fs_btk.png"))    },
            {".btp",     FSTypeInfo("J2D Texture Pattern Anim", ImageHandle("Images/Icons/Filesystem/fs_btp.png"))    },
            {".col",     FSTypeInfo("SMS Collision Data",       ImageHandle("Images/Icons/Filesystem/fs_col.png"))    },
            {".jpa",     FSTypeInfo("JParticle Data",           ImageHandle("Images/Icons/Filesystem/fs_jpa.png"))    },
            {".map",     FSTypeInfo("Executable Symbol Map",    ImageHandle("Images/Icons/Filesystem/fs_map.png"))    },
            {".me",      FSTypeInfo("Marked For Delete",        ImageHandle("Images/Icons/Filesystem/fs_me.png"))     },
            {".prm",     FSTypeInfo("SMS Parameter Data",       ImageHandle("Images/Icons/Filesystem/fs_prm.png"))    },
            {".sb",      FSTypeInfo("Sunscript",                ImageHandle("Images/Icons/Filesystem/fs_sb.png"))     },
            {".szs",     FSTypeInfo("Yaz0 Compressed Data",     ImageHandle("Images/Icons/Filesystem/fs_szs.png"))    },
            {".thp",     FSTypeInfo("DolphinOS Movie Data",     ImageHandle("Images/Icons/Filesystem/fs_thp.png"))    },
        };
        // clang-format on
        return s_type_map;
    }

    bool FileSystemModel::validateIndex(const ModelIndex &index) const {
        return index.isValid() && index.getModelUUID() == getUUID();
    }

    ModelIndex FileSystemModel::makeIndex(const fs_path &path, int64_t row,
                                          const ModelIndex &parent) {
        _FileSystemIndexData *parent_data = nullptr;

        if (!validateIndex(parent)) {
            if (row != 0) {
                TOOLBOX_ERROR("[FileSystemModel] Invalid row index!");
                return ModelIndex();
            }
        } else {
            parent_data = parent.data<_FileSystemIndexData>();
            if (row < 0 || (size_t)row > parent_data->m_children.size()) {
                TOOLBOX_ERROR("[FileSystemModel] Invalid row index!");
                return ModelIndex();
            }
        }

        _FileSystemIndexData *data = new _FileSystemIndexData;
        data->m_path               = path;
        data->m_size               = 0;

        if (Filesystem::is_directory(path).value_or(false)) {
            data->m_type = _FileSystemIndexData::Type::DIRECTORY;
        } else if (Filesystem::is_regular_file(path).value_or(false)) {
            if (false) {
                data->m_type = _FileSystemIndexData::Type::ARCHIVE;
            } else {
                data->m_type = _FileSystemIndexData::Type::FILE;
            }
        } else {
            TOOLBOX_ERROR_V("[FileSystemModel] Invalid path: {}", path.string());
            return ModelIndex();
        }

        Filesystem::last_write_time(data->m_path)
            .and_then([&](Filesystem::file_time_type &&time) {
                data->m_date = std::move(time);
                return Result<Filesystem::file_time_type, FSError>();
            })
            .or_else([&](const FSError &error) {
                TOOLBOX_ERROR_V("[FileSystemModel] Failed to get last write time: {}",
                                error.m_message[0]);
                return Result<Filesystem::file_time_type, FSError>();
            });

        ModelIndex index = ModelIndex(getUUID());

        if (!validateIndex(parent)) {
            index.setData(data);
            m_index_map[index.getUUID()] = std::move(index);
        } else {
            data->m_parent = parent.getUUID();
            index.setData(data);

            if (parent_data) {
                parent_data->m_children.insert(parent_data->m_children.begin() + row,
                                               index.getUUID());
            }

            m_index_map[index.getUUID()] = std::move(index);
        }

        return index;
    }

    void FileSystemModel::cacheIndex(ModelIndex &index) {}
    void FileSystemModel::cacheFolder(ModelIndex &index) {}
    void FileSystemModel::cacheFile(ModelIndex &index) {}
    void FileSystemModel::cacheArchive(ModelIndex &index) {}

    ModelIndex FileSystemModel::getParentArchive(const ModelIndex &index) const {
        ModelIndex parent = getParent(index);
        do {
            if (isArchive(parent)) {
                return parent;
            }
            parent = getParent(parent);
        } while (validateIndex(parent));

        return ModelIndex();
    }

    size_t FileSystemModel::pollChildren(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 0;
        }

        size_t count = 0;

        if (isDirectory(index)) {
            // Count the children in the filesystem
            for (const auto &entry :
                 Filesystem::directory_iterator(index.data<_FileSystemIndexData>()->m_path)) {
                count += 1;
            }
        } else if (isArchive(index)) {
            // Count the children in the archive
            TOOLBOX_ERROR("[FileSystemModel] Archive polling unimplemented!");
        } else if (isFile(index)) {
            // Files have no children
            count = 0;
        } else {
            // Invalid index
            count = 0;
        }

        return count;
    }

    RefPtr<FileSystemModel> FileSystemModelSortFilterProxy::getSourceModel() const {
        return m_source_model;
    }

    void FileSystemModelSortFilterProxy::setSourceModel(RefPtr<FileSystemModel> model) {
        m_source_model = model;
    }

    ModelSortOrder FileSystemModelSortFilterProxy::getSortOrder() const { return m_sort_order; }
    void FileSystemModelSortFilterProxy::setSortOrder(ModelSortOrder order) {
        m_sort_order = order;
    }

    ModelSortRole FileSystemModelSortFilterProxy::getSortRole() const { return m_sort_role; }
    void FileSystemModelSortFilterProxy::setSortRole(ModelSortRole role) { m_sort_role = role; }

    const std::string &FileSystemModelSortFilterProxy::getFilter() const & { return m_filter; }
    void FileSystemModelSortFilterProxy::setFilter(const std::string &filter) { m_filter = filter; }

    bool FileSystemModelSortFilterProxy::isReadOnly() const {
        if (!m_source_model) {
            return true;
        }
        return m_source_model->isReadOnly();
    }

    void FileSystemModelSortFilterProxy::setReadOnly(bool read_only) {
        if (!m_source_model) {
            return;
        }
        m_source_model->setReadOnly(read_only);
    }

    bool FileSystemModelSortFilterProxy::isDirectory(const ModelIndex &index) const {
        if (!m_source_model) {
            return false;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->isDirectory(source_index);
    }

    bool FileSystemModelSortFilterProxy::isFile(const ModelIndex &index) const {
        if (!m_source_model) {
            return false;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->isFile(source_index);
    }

    bool FileSystemModelSortFilterProxy::isArchive(const ModelIndex &index) const {
        if (!m_source_model) {
            return false;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->isArchive(source_index);
    }

    size_t FileSystemModelSortFilterProxy::getFileSize(const ModelIndex &index) {
        if (!m_source_model) {
            return 0;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getFileSize(source_index);
    }

    size_t FileSystemModelSortFilterProxy::getDirSize(const ModelIndex &index, bool recursive) {
        if (!m_source_model) {
            return 0;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getDirSize(source_index, recursive);
    }

    const ImageHandle &FileSystemModelSortFilterProxy::getIcon(const ModelIndex &index) const & {
        if (!m_source_model) {
            return FileSystemModel::InvalidIcon();
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getIcon(source_index);
    }

    Filesystem::file_time_type
    FileSystemModelSortFilterProxy::getLastModified(const ModelIndex &index) const {
        if (!m_source_model) {
            return Filesystem::file_time_type();
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getLastModified(source_index);
    }

    Filesystem::file_status
    FileSystemModelSortFilterProxy::getStatus(const ModelIndex &index) const {
        if (!m_source_model) {
            return Filesystem::file_status();
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getStatus(source_index);
    }

    std::string FileSystemModelSortFilterProxy::getType(const ModelIndex &index) const & {
        if (!m_source_model) {
            return "Invalid";
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getType(source_index);
    }

    ModelIndex FileSystemModelSortFilterProxy::mkdir(const ModelIndex &parent,
                                                     const std::string &name) {
        if (!m_source_model) {
            return ModelIndex();
        }

        ModelIndex &&source_index = toSourceIndex(parent);
        return m_source_model->mkdir(source_index, name);
    }

    ModelIndex FileSystemModelSortFilterProxy::touch(const ModelIndex &parent,
                                                     const std::string &name) {
        if (!m_source_model) {
            return ModelIndex();
        }

        ModelIndex &&source_index = toSourceIndex(parent);
        return m_source_model->touch(source_index, name);
    }

    bool FileSystemModelSortFilterProxy::rmdir(const ModelIndex &index) {
        if (!m_source_model) {
            return false;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->rmdir(source_index);
    }

    bool FileSystemModelSortFilterProxy::remove(const ModelIndex &index) {
        if (!m_source_model) {
            return false;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->remove(source_index);
    }

    ModelIndex FileSystemModelSortFilterProxy::getIndex(const fs_path &path) const {
        ModelIndex index = m_source_model->getIndex(path);
        if (!index.isValid()) {
            return ModelIndex();
        }

        return toProxyIndex(index);
    }

    ModelIndex FileSystemModelSortFilterProxy::getIndex(const UUID64 &uuid) const {
        ModelIndex index = m_source_model->getIndex(uuid);
        if (!index.isValid()) {
            return ModelIndex();
        }

        return toProxyIndex(index);
    }

    ModelIndex FileSystemModelSortFilterProxy::getIndex(int64_t row, int64_t column,
                                                        const ModelIndex &parent) const {
        ModelIndex parent_src = toSourceIndex(parent);

        ModelIndex index = m_source_model->getIndex(row, column, parent_src);
        if (!index.isValid()) {
            return ModelIndex();
        }

        return toProxyIndex(index);
    }

    fs_path FileSystemModelSortFilterProxy::getPath(const ModelIndex &index) const {
        if (!m_source_model) {
            return fs_path();
        }

        ModelIndex &&source_index = toSourceIndex(index);
        if (!source_index.isValid()) {
            return fs_path();
        }

        return m_source_model->getPath(source_index);
    }

    ModelIndex FileSystemModelSortFilterProxy::getParent(const ModelIndex &index) const {
        if (!m_source_model) {
            return ModelIndex();
        }

        ModelIndex &&source_index = toSourceIndex(index);
        if (!source_index.isValid()) {
            return ModelIndex();
        }

        return toProxyIndex(m_source_model->getParent(source_index));
    }

    ModelIndex FileSystemModelSortFilterProxy::getSibling(int64_t row, int64_t column,
                                                          const ModelIndex &index) const {
        if (!m_source_model) {
            return ModelIndex();
        }

        ModelIndex &&source_index = toSourceIndex(index);
        if (!source_index.isValid()) {
            return ModelIndex();
        }

        return toProxyIndex(m_source_model->getSibling(row, column, source_index));
    }

    size_t FileSystemModelSortFilterProxy::getColumnCount(const ModelIndex &index) const {
        if (!m_source_model) {
            return 0;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        if (!source_index.isValid()) {
            return 0;
        }

        return m_source_model->getColumnCount(source_index);
    }

    size_t FileSystemModelSortFilterProxy::getRowCount(const ModelIndex &index) const {
        if (!m_source_model) {
            return 0;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        if (!source_index.isValid()) {
            return 0;
        }

        return m_source_model->getRowCount(source_index);
    }

    bool FileSystemModelSortFilterProxy::hasChildren(const ModelIndex &parent) const {
        if (!m_source_model) {
            return false;
        }

        ModelIndex &&source_index = toSourceIndex(parent);
        if (!source_index.isValid()) {
            return false;
        }

        return m_source_model->hasChildren(source_index);
    }

    ScopePtr<MimeData>
    FileSystemModelSortFilterProxy::createMimeData(const std::vector<ModelIndex> &indexes) const {
        if (!m_source_model) {
            return ScopePtr<MimeData>();
        }

        std::vector<ModelIndex> indexes_copy = indexes;
        std::transform(indexes.begin(), indexes.end(), indexes_copy.begin(),
                       [&](const ModelIndex &index) { return toSourceIndex(index); });

        return m_source_model->createMimeData(indexes);
    }

    std::vector<std::string> FileSystemModelSortFilterProxy::getSupportedMimeTypes() const {
        if (!m_source_model) {
            return std::vector<std::string>();
        }
        return m_source_model->getSupportedMimeTypes();
    }

    bool FileSystemModelSortFilterProxy::canFetchMore(const ModelIndex &index) {
        if (!m_source_model) {
            return false;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        if (!source_index.isValid()) {
            return false;
        }

        return m_source_model->canFetchMore(source_index);
    }

    void FileSystemModelSortFilterProxy::fetchMore(const ModelIndex &index) {
        if (!m_source_model) {
            return;
        }

        ModelIndex &&source_index = toSourceIndex(index);
        if (!source_index.isValid()) {
            return;
        }

        m_source_model->fetchMore(source_index);
    }

    bool FileSystemModelSortFilterProxy::validateIndex(const ModelIndex &index) const {
        return index.isValid() && index.getModelUUID() == getUUID();
    }

    ModelIndex FileSystemModelSortFilterProxy::toSourceIndex(const ModelIndex &index) const {
        if (!m_source_model) {
            return ModelIndex();
        }

        if (m_source_model->validateIndex(index)) {
            return index;
        }

        if (!validateIndex(index)) {
            return ModelIndex();
        }

        return m_source_model->getIndex(index.data<_FileSystemIndexData>()->m_path);
    }

    ModelIndex FileSystemModelSortFilterProxy::toProxyIndex(const ModelIndex &index) const {
        if (!m_source_model) {
            return ModelIndex();
        }

        if (validateIndex(index)) {
            return index;
        }

        if (!m_source_model->validateIndex(index)) {
            return ModelIndex();
        }

        const ModelIndex &parent = m_source_model->getParent(index);
        if (!m_source_model->validateIndex(parent)) {
            ModelIndex proxy_index = ModelIndex(getUUID());
            proxy_index.setData(index.data<_FileSystemIndexData>());
            return proxy_index;
        }

        std::vector<UUID64> children = parent.data<_FileSystemIndexData>()->m_children;
        std::copy_if(children.begin(), children.end(), children.begin(), [&](const UUID64 &uuid) {
            ModelIndex child_index = m_source_model->getIndex(uuid);
            if (!child_index.isValid()) {
                return false;
            }

            if (m_dirs_only && m_source_model->isFile(child_index)) {
                return false;
            }

            fs_path path = child_index.data<_FileSystemIndexData>()->m_path;
            return path.filename().string().starts_with(m_filter);
        });

        switch (m_sort_role) {
        case ModelSortRole::SORT_ROLE_NAME: {
            std::sort(children.begin(), children.end(), [&](const UUID64 &lhs, const UUID64 &rhs) {
                return _FileSystemIndexDataCompareByName(
                    *m_source_model->getIndex(lhs).data<_FileSystemIndexData>(),
                    *m_source_model->getIndex(rhs).data<_FileSystemIndexData>(), m_sort_order);
            });
            break;
        }
        case ModelSortRole::SORT_ROLE_SIZE: {
            std::sort(children.begin(), children.end(), [&](const UUID64 &lhs, const UUID64 &rhs) {
                return _FileSystemIndexDataCompareBySize(
                    *m_source_model->getIndex(lhs).data<_FileSystemIndexData>(),
                    *m_source_model->getIndex(rhs).data<_FileSystemIndexData>(), m_sort_order);
            });
            break;
        }
        case ModelSortRole::SORT_ROLE_DATE:
            std::sort(children.begin(), children.end(), [&](const UUID64 &lhs, const UUID64 &rhs) {
                return _FileSystemIndexDataCompareByDate(
                    *m_source_model->getIndex(lhs).data<_FileSystemIndexData>(),
                    *m_source_model->getIndex(rhs).data<_FileSystemIndexData>(), m_sort_order);
            });
            break;
        default:
            break;
        }

        for (size_t i = 0; i < children.size(); i++) {
            if (children[i] == index.getUUID()) {
                ModelIndex proxy_index = ModelIndex(getUUID());
                proxy_index.setData(index.data<_FileSystemIndexData>());
                return proxy_index;
            }
        }

        // Should never reach here
        return ModelIndex();
    }

}  // namespace Toolbox