#include <algorithm>
#include <compare>
#include <set>

#include "gui/application.hpp"
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
        UUID64 m_self_uuid             = 0;
        std::vector<UUID64> m_children = {};

        Type m_type = Type::UNKNOWN;
        fs_path m_path;
        std::string m_name;
        size_t m_size = 0;
        Filesystem::file_time_type m_date;

        RefPtr<const ImageHandle> m_icon = nullptr;

        bool hasChild(UUID64 uuid) const {
            return std::find(m_children.begin(), m_children.end(), uuid) != m_children.end();
        }

        std::strong_ordering operator<=>(const _FileSystemIndexData &rhs) const {
            return m_self_uuid <=> rhs.m_self_uuid;
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
            return lhs.m_name < rhs.m_name;
        } else {
            return lhs.m_name > rhs.m_name;
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

    FileSystemModel::~FileSystemModel() {
        for (auto &[key, value] : m_index_map) {
            delete value.data<_FileSystemIndexData>();
        }
    }

    void FileSystemModel::initialize() {
        m_icon_map.clear();
        m_index_map.clear();
        m_root_path  = fs_path();
        m_root_index = 0;
        m_options    = FileSystemModelOptions();
        m_read_only  = false;

        const ResourceManager &res_manager = GUIApplication::instance().getResourceManager();
        UUID64 fs_icons_uuid = res_manager.getResourcePathUUID("Images/Icons/Filesystem");

        for (auto &[key, value] : TypeMap()) {
            auto result = res_manager.getImageHandle(value.m_image_name, fs_icons_uuid);
            if (result) {
                m_icon_map[key] = result.value();
            } else {
                TOOLBOX_ERROR_V("[FileSystemModel] Failed to load icon: {}", value.m_image_name);
            }
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

    size_t FileSystemModel::getFileSize(const ModelIndex &index) const {
        if (!isFile(index)) {
            return 0;
        }
        return index.data<_FileSystemIndexData>()->m_size;
    }

    size_t FileSystemModel::getDirSize(const ModelIndex &index, bool recursive) const {
        if (!(isDirectory(index) || isArchive(index))) {
            return 0;
        }
        return index.data<_FileSystemIndexData>()->m_size;
    }

    std::any FileSystemModel::getData(const ModelIndex &index, int role) const {
        if (!validateIndex(index)) {
            return {};
        }

        switch (role) {
        case ModelDataRole::DATA_ROLE_DISPLAY:
            return index.data<_FileSystemIndexData>()->m_name;
        case ModelDataRole::DATA_ROLE_TOOLTIP:
            return "Tooltip unimplemented!";
        case ModelDataRole::DATA_ROLE_DECORATION: {
            return index.data<_FileSystemIndexData>()->m_icon;
        }
        case FileSystemDataRole::FS_DATA_ROLE_DATE: {

            Filesystem::file_time_type result = Filesystem::file_time_type();

            fs_path path = getPath(index);
            Filesystem::last_write_time(path)
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
        case FileSystemDataRole::FS_DATA_ROLE_STATUS: {

            Filesystem::file_status result = Filesystem::file_status();

            fs_path path = getPath(index);
            Filesystem::status(path)
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
        case FileSystemDataRole::FS_DATA_ROLE_TYPE: {

            if (isDirectory(index)) {
                return TypeMap().at("_Folder").m_name;
            }

            if (isArchive(index)) {
                return TypeMap().at("_Archive").m_name;
            }

            std::string name = index.data<_FileSystemIndexData>()->m_name;
            std::string ext  = "";
            if (size_t epos = name.find('.'); epos != std::string::npos) {
                ext = name.substr(epos);
            }

            if (ext.empty()) {
                return TypeMap().at("_Folder").m_name;
            }

            if (TypeMap().find(ext) == TypeMap().end()) {
                return TypeMap().at("_File").m_name;
            }

            return TypeMap().at(ext).m_name;
        }
        default:
            return std::any();
        }
    }

    ModelIndex FileSystemModel::mkdir(const ModelIndex &parent, const std::string &name) {
        if (!validateIndex(parent)) {
            return ModelIndex();
        }

        bool result = false;

        if (isDirectory(parent)) {
            fs_path path = getPath(parent);
            path /= name;
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
            Filesystem::remove_all(getPath(index))
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove directory: {}",
                                        getPath(index).string());
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
            Filesystem::remove(getPath(index))
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove archive: {}",
                                        getPath(index).string());
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
            }
            delete index.data<_FileSystemIndexData>();
            m_index_map.erase(index.getUUID());
        }

        return result;
    }

    bool FileSystemModel::remove(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        bool result = false;

        if (isFile(index)) {
            Filesystem::remove(getPath(index))
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove file: {}",
                                        getPath(index).string());
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
            }
            delete index.data<_FileSystemIndexData>();
            m_index_map.erase(index.getUUID());
        }

        return result;
    }

    ModelIndex FileSystemModel::rename(const ModelIndex &file, const std::string &new_name) {
        if (!validateIndex(file)) {
            return ModelIndex();
        }
        if (!isDirectory(file) && !isFile(file)) {
            TOOLBOX_ERROR("[FileSystemModel] Not a directory or file!");
            return ModelIndex();
        }
        fs_path from                      = file.data<_FileSystemIndexData>()->m_path;
        fs_path to                        = from.parent_path() / new_name;
        ModelIndex parent                 = getParent(file);
        _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();

        int dest_index = std::find(parent_data->m_children.begin(), parent_data->m_children.end(),
                                   file.getUUID()) -
                         parent_data->m_children.begin();

        Filesystem::rename(from, to);

        delete file.data<_FileSystemIndexData>();
        m_index_map.erase(file.getUUID());
        parent_data->m_children.erase(std::remove(parent_data->m_children.begin(),
                                                  parent_data->m_children.end(), file.getUUID()),
                                      parent_data->m_children.end());
        return makeIndex(to, dest_index, parent);
    }

    ModelIndex FileSystemModel::getIndex(const fs_path &path) const {
        if (m_index_map.empty()) {
            return ModelIndex();
        }

        for (const auto &[uuid, index] : m_index_map) {
            if (getPath(index) == path) {
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
        if (getRowCount(parent) > 0) {
            return true;
        }
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
            fs_path path = getPath(index);

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
            {"_Invalid", FSTypeInfo("Invalid",                  "fs_invalid.png")       },
            {"_Folder",  FSTypeInfo("Folder",                   "fs_generic_folder.png")},
            {"_Archive", FSTypeInfo("Archive",                  "fs_arc.png")           },
            {"_File",    FSTypeInfo("File",                     "fs_generic_file.png")  },
            {".txt",     FSTypeInfo("Text",                     "fs_txt.png")           },
            {".md",      FSTypeInfo("Markdown",                 "fs_md.png")            },
            {".c",       FSTypeInfo("C",                        "fs_c.png")             },
            {".h",       FSTypeInfo("Header",                   "fs_h.png")             },
            {".cpp",     FSTypeInfo("C++",                      "fs_cpp.png")           },
            {".hpp",     FSTypeInfo("Header",                   "fs_hpp.png")           },
            {".cxx",     FSTypeInfo("C++",                      "fs_cpp.png")           },
            {".hxx",     FSTypeInfo("Header",                   "fs_hpp.png")           },
            {".c++",     FSTypeInfo("C++",                      "fs_cpp.png")           },
            {".h++",     FSTypeInfo("Header",                   "fs_hpp.png")           },
            {".cc",      FSTypeInfo("C++",                      "fs_cpp.png")           },
            {".hh",      FSTypeInfo("Header",                   "fs_hpp.png")           },
            {".arc",     FSTypeInfo("Archive",                  "fs_arc.png")           },
            {".bas",     FSTypeInfo("JAudio Sequence",          "fs_bas.png")           },
            {".bck",     FSTypeInfo("J3D Bone Animation",       "fs_bck.png")           },
            {".bdl",     FSTypeInfo("J3D Model Data",           "fs_bdl.png")           },
            {".blo",     FSTypeInfo("J2D Screen Layout",        "fs_blo.png")           },
            {".bmd",     FSTypeInfo("J3D Model Data",           "fs_bmd.png")           },
            {".bmg",     FSTypeInfo("Message Data",             "fs_bmg.png")           },
            {".bmt",     FSTypeInfo("J3D Material Table",       "fs_bmt.png")           },
            {".brk",     FSTypeInfo("J3D Color Register Anim",  "fs_brk.png")           },
            {".bti",     FSTypeInfo("J2D Texture Image",        "fs_bti.png")           },
            {".btk",     FSTypeInfo("J2D Texture Animation",    "fs_btk.png")           },
            {".btp",     FSTypeInfo("J2D Texture Pattern Anim", "fs_btp.png")           },
            {".col",     FSTypeInfo("SMS Collision Data",       "fs_col.png")           },
            {".jpa",     FSTypeInfo("JParticle Data",           "fs_jpa.png")           },
            {".map",     FSTypeInfo("Executable Symbol Map",    "fs_map.png")           },
            {".me",      FSTypeInfo("Marked For Delete",        "fs_me.png")            },
            {".prm",     FSTypeInfo("SMS Parameter Data",       "fs_prm.png")           },
            {".sb",      FSTypeInfo("Sunscript",                "fs_sb.png")            },
            {".szs",     FSTypeInfo("Yaz0 Compressed Data",     "fs_szs.png")           },
            {".thp",     FSTypeInfo("DolphinOS Movie Data",     "fs_thp.png")           },
        };
        // clang-format on
        return s_type_map;
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
        data->m_name               = path.filename().string();
        data->m_size               = 0;
        data->m_children           = {};

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

        // Establish icon
        {
            std::string ext = data->m_path.extension().string();

            if (!validateIndex(index)) {
                data->m_icon = m_icon_map["_Invalid"];
            } else if (data->m_type == _FileSystemIndexData::Type::DIRECTORY) {
                data->m_icon = m_icon_map["_Folder"];
            } else if (data->m_type == _FileSystemIndexData::Type::ARCHIVE) {
                data->m_icon = m_icon_map["_Archive"];
            } else if (ext.empty()) {
                data->m_icon = m_icon_map["_Folder"];
            } else if (m_icon_map.find(ext) == m_icon_map.end()) {
                data->m_icon = m_icon_map["_File"];
            } else {
                data->m_icon = m_icon_map[ext];
            }
        }

        data->m_self_uuid = index.getUUID();

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
            for (const auto &entry : Filesystem::directory_iterator(getPath(index))) {
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

    FileSystemModelSortRole FileSystemModelSortFilterProxy::getSortRole() const {
        return m_sort_role;
    }
    void FileSystemModelSortFilterProxy::setSortRole(FileSystemModelSortRole role) {
        m_sort_role = role;
    }

    const std::string &FileSystemModelSortFilterProxy::getFilter() const & { return m_filter; }
    void FileSystemModelSortFilterProxy::setFilter(const std::string &filter) { m_filter = filter; }

    bool FileSystemModelSortFilterProxy::isReadOnly() const { return m_source_model->isReadOnly(); }

    void FileSystemModelSortFilterProxy::setReadOnly(bool read_only) {
        m_source_model->setReadOnly(read_only);
    }

    bool FileSystemModelSortFilterProxy::isDirectory(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->isDirectory(std::move(source_index));
    }

    bool FileSystemModelSortFilterProxy::isFile(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->isFile(std::move(source_index));
    }

    bool FileSystemModelSortFilterProxy::isArchive(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->isArchive(std::move(source_index));
    }

    size_t FileSystemModelSortFilterProxy::getFileSize(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getFileSize(std::move(source_index));
    }

    size_t FileSystemModelSortFilterProxy::getDirSize(const ModelIndex &index, bool recursive) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getDirSize(std::move(source_index), recursive);
    }

    Filesystem::file_time_type
    FileSystemModelSortFilterProxy::getLastModified(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getLastModified(std::move(source_index));
    }

    Filesystem::file_status
    FileSystemModelSortFilterProxy::getStatus(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getStatus(std::move(source_index));
    }

    std::string FileSystemModelSortFilterProxy::getType(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getType(std::move(source_index));
    }

    std::any FileSystemModelSortFilterProxy::getData(const ModelIndex &index, int role) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getData(std::move(source_index), role);
    }

    ModelIndex FileSystemModelSortFilterProxy::mkdir(const ModelIndex &parent,
                                                     const std::string &name) {
        ModelIndex &&source_index = toSourceIndex(parent);
        return m_source_model->mkdir(std::move(source_index), name);
    }

    ModelIndex FileSystemModelSortFilterProxy::touch(const ModelIndex &parent,
                                                     const std::string &name) {
        ModelIndex &&source_index = toSourceIndex(parent);
        return m_source_model->touch(std::move(source_index), name);
    }

    bool FileSystemModelSortFilterProxy::rmdir(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->rmdir(std::move(source_index));
    }

    bool FileSystemModelSortFilterProxy::remove(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->remove(std::move(source_index));
    }

    ModelIndex FileSystemModelSortFilterProxy::getIndex(const fs_path &path) const {
        ModelIndex index = m_source_model->getIndex(path);
        return toProxyIndex(index);
    }

    ModelIndex FileSystemModelSortFilterProxy::getIndex(const UUID64 &uuid) const {
        ModelIndex index = m_source_model->getIndex(uuid);
        return toProxyIndex(index);
    }

    ModelIndex FileSystemModelSortFilterProxy::getIndex(int64_t row, int64_t column,
                                                        const ModelIndex &parent) const {
        ModelIndex parent_src = toSourceIndex(parent);
        return toProxyIndex(row, column, parent_src);
    }

    fs_path FileSystemModelSortFilterProxy::getPath(const ModelIndex &index) const {
        ModelIndex source_index = toSourceIndex(index);
        return m_source_model->getPath(std::move(source_index));
    }

    ModelIndex FileSystemModelSortFilterProxy::getParent(const ModelIndex &index) const {
        ModelIndex source_index = toSourceIndex(index);
        return toProxyIndex(m_source_model->getParent(std::move(source_index)));
    }

    ModelIndex FileSystemModelSortFilterProxy::getSibling(int64_t row, int64_t column,
                                                          const ModelIndex &index) const {
        ModelIndex source_index       = toSourceIndex(index);
        ModelIndex src_parent         = getParent(source_index);
        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) == m_row_map.end()) {
            cacheIndex(src_parent);
        }

        if (row < m_row_map[map_key].size()) {
            int64_t the_row = m_row_map[map_key][row];
            return toProxyIndex(m_source_model->getIndex(the_row, column, src_parent));
        }

        return ModelIndex();
    }

    size_t FileSystemModelSortFilterProxy::getColumnCount(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getColumnCount(source_index);
    }

    size_t FileSystemModelSortFilterProxy::getRowCount(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);

        if (m_row_map.find(source_index.getUUID()) != m_row_map.end()) {
            return m_row_map[source_index.getUUID()].size();
        }

        return m_source_model->getRowCount(source_index);
    }

    bool FileSystemModelSortFilterProxy::hasChildren(const ModelIndex &parent) const {
        ModelIndex &&source_index = toSourceIndex(parent);
        return m_source_model->hasChildren(source_index);
    }

    ScopePtr<MimeData>
    FileSystemModelSortFilterProxy::createMimeData(const std::vector<ModelIndex> &indexes) const {
        std::vector<ModelIndex> indexes_copy = indexes;
        std::transform(indexes.begin(), indexes.end(), indexes_copy.begin(),
                       [&](const ModelIndex &index) { return toSourceIndex(index); });

        return m_source_model->createMimeData(indexes);
    }

    std::vector<std::string> FileSystemModelSortFilterProxy::getSupportedMimeTypes() const {
        return m_source_model->getSupportedMimeTypes();
    }

    bool FileSystemModelSortFilterProxy::canFetchMore(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->canFetchMore(std::move(source_index));
    }

    void FileSystemModelSortFilterProxy::fetchMore(const ModelIndex &index) {
        ModelIndex &&source_index = toSourceIndex(index);
        m_source_model->fetchMore(std::move(source_index));
    }

    ModelIndex FileSystemModelSortFilterProxy::toSourceIndex(const ModelIndex &index) const {
        if (m_source_model->validateIndex(index)) {
            return index;
        }

        if (!validateIndex(index)) {
            return ModelIndex();
        }

        return m_source_model->getIndex(index.data<_FileSystemIndexData>()->m_self_uuid);
    }

    ModelIndex FileSystemModelSortFilterProxy::toProxyIndex(const ModelIndex &index) const {
        if (!m_source_model->validateIndex(index)) {
            return ModelIndex();
        }

        ModelIndex proxy_index = ModelIndex(getUUID());
        proxy_index.setData(index.data<_FileSystemIndexData>());
        return proxy_index;
    }

    ModelIndex FileSystemModelSortFilterProxy::toProxyIndex(int64_t row, int64_t column,
                                                            const ModelIndex &src_parent) const {
        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) == m_row_map.end()) {
            cacheIndex(src_parent);
        }

        if (row < m_row_map[map_key].size()) {
            int64_t the_row = m_row_map[map_key][row];
            return toProxyIndex(m_source_model->getIndex(the_row, column, src_parent));
        }

        return ModelIndex();
    }

    UUID64 FileSystemModelSortFilterProxy::getSourceUUID(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 0;
        }

        return index.data<_FileSystemIndexData>()->m_self_uuid;
    }

    bool FileSystemModelSortFilterProxy::isFiltered(const UUID64 &uuid) const {
        ModelIndex child_index = m_source_model->getIndex(uuid);
        bool is_file =
            child_index.data<_FileSystemIndexData>()->m_type == _FileSystemIndexData::Type::FILE;

        if (isDirsOnly() && is_file) {
            return true;
        }

        const std::string &name = child_index.data<_FileSystemIndexData>()->m_name;
        return !name.starts_with(m_filter);
    }

    void FileSystemModelSortFilterProxy::cacheIndex(const ModelIndex &dir_index) const {
        std::vector<UUID64> orig_children  = {};
        std::vector<UUID64> proxy_children = {};

        const UUID64 &src_parent_uuid = dir_index.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(dir_index)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) != m_row_map.end()) {
            return;
        }

        if (!m_source_model->validateIndex(dir_index)) {
            size_t i          = 0;
            ModelIndex root_s = m_source_model->getIndex(i++, 0);
            while (m_source_model->validateIndex(root_s)) {
                orig_children.push_back(root_s.getUUID());
                if (isFiltered(root_s.getUUID())) {
                    root_s = m_source_model->getIndex(i++, 0);
                    continue;
                }
                proxy_children.push_back(root_s.getUUID());
                root_s = m_source_model->getIndex(i++, 0);
            }
        } else {
            orig_children = dir_index.data<_FileSystemIndexData>()->m_children;
            if (orig_children.empty()) {
                return;
            }

            std::copy_if(orig_children.begin(), orig_children.end(),
                         std::back_inserter(proxy_children),
                         [&](const UUID64 &uuid) { return !isFiltered(uuid); });
        }

        if (proxy_children.empty()) {
            return;
        }

        switch (m_sort_role) {
        case FileSystemModelSortRole::SORT_ROLE_NAME: {
            std::sort(proxy_children.begin(), proxy_children.end(),
                      [&](const UUID64 &lhs, const UUID64 &rhs) {
                          return _FileSystemIndexDataCompareByName(
                              *m_source_model->getIndex(lhs).data<_FileSystemIndexData>(),
                              *m_source_model->getIndex(rhs).data<_FileSystemIndexData>(),
                              m_sort_order);
                      });
            break;
        }
        case FileSystemModelSortRole::SORT_ROLE_SIZE: {
            std::sort(proxy_children.begin(), proxy_children.end(),
                      [&](const UUID64 &lhs, const UUID64 &rhs) {
                          return _FileSystemIndexDataCompareBySize(
                              *m_source_model->getIndex(lhs).data<_FileSystemIndexData>(),
                              *m_source_model->getIndex(rhs).data<_FileSystemIndexData>(),
                              m_sort_order);
                      });
            break;
        }
        case FileSystemModelSortRole::SORT_ROLE_DATE:
            std::sort(proxy_children.begin(), proxy_children.end(),
                      [&](const UUID64 &lhs, const UUID64 &rhs) {
                          return _FileSystemIndexDataCompareByDate(
                              *m_source_model->getIndex(lhs).data<_FileSystemIndexData>(),
                              *m_source_model->getIndex(rhs).data<_FileSystemIndexData>(),
                              m_sort_order);
                      });
            break;
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
                    break;
                }
            }
        }
    }

}  // namespace Toolbox