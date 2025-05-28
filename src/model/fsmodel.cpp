#include <algorithm>
#include <compare>
#include <set>

#include "gui/application.hpp"
#include "model/fsmodel.hpp"

#ifndef TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
#define TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE 0
#endif

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
        m_watchdog.tKill(true);
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

        m_watchdog.onDirAdded(TOOLBOX_BIND_EVENT_FN(folderAdded));
        m_watchdog.onDirModified(TOOLBOX_BIND_EVENT_FN(folderModified));
        m_watchdog.onFileAdded(TOOLBOX_BIND_EVENT_FN(fileAdded));
        m_watchdog.onFileModified(TOOLBOX_BIND_EVENT_FN(fileModified));
        m_watchdog.onPathRemoved(TOOLBOX_BIND_EVENT_FN(pathRemoved));
        m_watchdog.onPathRenamedSrc(TOOLBOX_BIND_EVENT_FN(pathRenamedSrc));
        m_watchdog.onPathRenamedDst(TOOLBOX_BIND_EVENT_FN(pathRenamedDst));

        m_watchdog.tStart(false, nullptr);
    }

    const fs_path &FileSystemModel::getRoot() const & { return m_root_path; }

    void FileSystemModel::setRoot(const fs_path &path) {
        if (m_root_path == path) {
            return;
        }

        {
            std::scoped_lock lock(m_mutex);

            m_watchdog.reset();
            m_watchdog.addPath(path);

            m_index_map.clear();

            m_root_path  = path;
            m_root_index = makeIndex(path, 0, ModelIndex()).getUUID();
        }
    }

    FileSystemModelOptions FileSystemModel::getOptions() const { return m_options; }

    void FileSystemModel::setOptions(FileSystemModelOptions options) { m_options = options; }

    bool FileSystemModel::isReadOnly() const { return m_read_only; }

    void FileSystemModel::setReadOnly(bool read_only) { m_read_only = read_only; }

    bool FileSystemModel::isDirectory(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return isDirectory_(index);
    }

    bool FileSystemModel::isFile(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return isFile_(index);
    }

    bool FileSystemModel::isArchive(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return isArchive_(index);
    }

    size_t FileSystemModel::getFileSize(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getFileSize_(index);
    }

    size_t FileSystemModel::getDirSize(const ModelIndex &index, bool recursive) const {
        std::scoped_lock lock(m_mutex);
        return getDirSize_(index, recursive);
    }

    std::any FileSystemModel::getData(const ModelIndex &index, int role) const {
        std::scoped_lock lock(m_mutex);
        return getData_(index, role);
    }

    std::string FileSystemModel::findUniqueName(const ModelIndex &index,
                                                const std::string &name) const {
        std::scoped_lock lock(m_mutex);
        return findUniqueName_(index, name);
    }

    ModelIndex FileSystemModel::mkdir(const ModelIndex &parent, const std::string &name) {
        std::scoped_lock lock(m_mutex);
        return mkdir_(parent, name);
    }

    ModelIndex FileSystemModel::touch(const ModelIndex &parent, const std::string &name) {
        std::scoped_lock lock(m_mutex);
        return touch_(parent, name);
    }

    bool FileSystemModel::rmdir(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return rmdir_(index);
    }

    bool FileSystemModel::remove(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return remove_(index);
    }

    ModelIndex FileSystemModel::rename(const ModelIndex &file, const std::string &new_name) {
        std::scoped_lock lock(m_mutex);
        return rename_(file, new_name);
    }
    ModelIndex FileSystemModel::copy(const fs_path &file, const ModelIndex &new_parent,
                                     const std::string &new_name) {
        std::scoped_lock lock(m_mutex);
        return copy_(file, new_parent, new_name);
    }

    ModelIndex FileSystemModel::getIndex(const fs_path &path) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(path);
    }

    ModelIndex FileSystemModel::getIndex(const UUID64 &uuid) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(uuid);
    }

    ModelIndex FileSystemModel::getIndex(int64_t row, int64_t column,
                                         const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return getIndex_(row, column, parent);
    }

    fs_path FileSystemModel::getPath(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getPath_(index);
    }

    ModelIndex FileSystemModel::getParent(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getParent_(index);
    }

    ModelIndex FileSystemModel::getSibling(int64_t row, int64_t column,
                                           const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getSibling_(row, column, index);
    }

    size_t FileSystemModel::getColumnCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumnCount_(index);
    }

    size_t FileSystemModel::getRowCount(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRowCount_(index);
    }

    int64_t FileSystemModel::getColumn(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getColumn_(index);
    }

    int64_t FileSystemModel::getRow(const ModelIndex &index) const {
        std::scoped_lock lock(m_mutex);
        return getRow_(index);
    }

    bool FileSystemModel::hasChildren(const ModelIndex &parent) const {
        std::scoped_lock lock(m_mutex);
        return hasChildren_(parent);
    }

    ScopePtr<MimeData>
    FileSystemModel::createMimeData(const std::vector<ModelIndex> &indexes) const {
        std::scoped_lock lock(m_mutex);
        return createMimeData_(indexes);
    }

    std::vector<std::string> FileSystemModel::getSupportedMimeTypes() const {
        return std::vector<std::string>();
    }

    bool FileSystemModel::canFetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return canFetchMore_(index);
    }

    void FileSystemModel::fetchMore(const ModelIndex &index) {
        std::scoped_lock lock(m_mutex);
        return fetchMore_(index);
    }

    void FileSystemModel::addEventListener(UUID64 uuid, event_listener_t listener,
                                           FileSystemModelEventFlags flags) {
        m_listeners[uuid] = {listener, flags};
    }

    void FileSystemModel::removeEventListener(UUID64 uuid) { m_listeners.erase(uuid); }

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
            {".btk",     FSTypeInfo("J2D Texture UV Anim",      "fs_btk.png")           },
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

    bool FileSystemModel::isDirectory_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;
        }
        return index.data<_FileSystemIndexData>()->m_type == _FileSystemIndexData::Type::DIRECTORY;
    }

    bool FileSystemModel::isFile_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;
        }
        return index.data<_FileSystemIndexData>()->m_type == _FileSystemIndexData::Type::FILE;
    }

    bool FileSystemModel::isArchive_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return false;
        }
        return index.data<_FileSystemIndexData>()->m_type == _FileSystemIndexData::Type::ARCHIVE;
    }

    size_t FileSystemModel::getFileSize_(const ModelIndex &index) const {
        if (!isFile_(index)) {
            return 0;
        }
        return index.data<_FileSystemIndexData>()->m_size;
    }

    size_t FileSystemModel::getDirSize_(const ModelIndex &index, bool recursive) const {
        if (!(isDirectory_(index) || isArchive_(index))) {
            return 0;
        }
        return index.data<_FileSystemIndexData>()->m_size;
    }

    std::any FileSystemModel::getData_(const ModelIndex &index, int role) const {
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

            fs_path path = getPath_(index);
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

            fs_path path = getPath_(index);
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

            if (isDirectory_(index)) {
                return TypeMap().at("_Folder").m_name;
            }

            if (isArchive_(index)) {
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

    std::string FileSystemModel::findUniqueName_(const ModelIndex &parent,
                                                 const std::string &name) const {
        if (!validateIndex(parent)) {
            return name;
        }

        if (isFile_(parent)) {
            return name;
        }

        if (isArchive_(parent)) {
            // TODO: Implement for virtual FS
            TOOLBOX_ERROR("[FileSystemModel] Archive probing is unimplemented!");
            return name;
        }

        std::string result_name = name;
        size_t collisions       = 0;
        std::vector<std::string> child_paths;

        size_t dir_size = getDirSize_(parent, false);
        for (size_t i = 0; i < dir_size; ++i) {
            ModelIndex child = getIndex_(i, 0, parent);
            child_paths.emplace_back(std::move(getPath_(child).filename().string()));
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

    ModelIndex FileSystemModel::mkdir_(const ModelIndex &parent, const std::string &name) {
        if (!validateIndex(parent)) {
            return ModelIndex();
        }

        bool result = false;

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.sleep();
#endif
        fs_path path = getPath_(parent) / name;

#if !TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.ignorePathOnce(path);
#endif

        FileSystemModelEventFlags event_flags = FileSystemModelEventFlags::EVENT_FOLDER_ADDED;
        if (isArchive_(parent) || validateIndex(getParentArchive(parent))) {
            event_flags |= FileSystemModelEventFlags::EVENT_IS_VIRTUAL;
        }

        if (isDirectory_(parent)) {
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
        } else if (isArchive_(parent)) {
            TOOLBOX_ERROR("[FileSystemModel] Archive creation is unimplemented!");
        } else {
            TOOLBOX_ERROR("[FileSystemModel] Parent index is not a directory!");
        }

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.wake();
#endif

        if (!result) {
            return ModelIndex();
        }

        ModelIndex ret_index = makeIndex(parent.data<_FileSystemIndexData>()->m_path / name,
                                         getRowCount_(parent), parent);
        signalEventListeners(path, event_flags);
        return ret_index;
    }

    ModelIndex FileSystemModel::touch_(const ModelIndex &parent, const std::string &name) {
        if (!validateIndex(parent)) {
            return ModelIndex();
        }

        bool result = false;

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.sleep();
#endif

        fs_path path = parent.data<_FileSystemIndexData>()->m_path / name;

#if !TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.ignorePathOnce(path);
#endif

        FileSystemModelEventFlags event_flags = FileSystemModelEventFlags::EVENT_FILE_ADDED;
        if (isArchive_(parent) || validateIndex(getParentArchive(parent))) {
            event_flags |= FileSystemModelEventFlags::EVENT_IS_VIRTUAL;
        }

        if (isDirectory_(parent)) {
            std::ofstream file(path);
            if (!file.is_open()) {
                TOOLBOX_ERROR_V("[FileSystemModel] Failed to create file: {}", path.string());
                return ModelIndex();
            }
            file.close();
            result = true;
        } else if (isArchive_(parent)) {
            TOOLBOX_ERROR("[FileSystemModel] Archive creation is unimplemented!");
        } else {
            TOOLBOX_ERROR("[FileSystemModel] Parent index is not a directory!");
        }

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.wake();
#endif

        if (!result) {
            return ModelIndex();
        }

        ModelIndex ret_index = makeIndex(parent.data<_FileSystemIndexData>()->m_path / name,
                                         getRowCount_(parent), parent);
        signalEventListeners(path, event_flags);
        return ret_index;
    }

    bool FileSystemModel::rmdir_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        bool result        = false;
        fs_path index_path = getPath_(index);

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.sleep();
#else
        m_watchdog.ignorePathOnce(index_path);
#endif

        FileSystemModelEventFlags event_flags = FileSystemModelEventFlags::EVENT_PATH_REMOVED;
        if (validateIndex(getParentArchive(index))) {
            event_flags |= FileSystemModelEventFlags::EVENT_IS_VIRTUAL;
        }

        if (isDirectory_(index)) {
            Filesystem::remove_all(index_path)
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove directory: {}",
                                        index_path.string());
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
        } else if (isArchive_(index)) {
            Filesystem::remove(index_path)
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove archive: {}",
                                        index_path.string());
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

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.wake();
#endif

        if (result) {
            ModelIndex parent = getParent_(index);
            if (validateIndex(parent)) {
                _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();
                parent_data->m_children.erase(std::remove(parent_data->m_children.begin(),
                                                          parent_data->m_children.end(),
                                                          index.getUUID()),
                                              parent_data->m_children.end());
                parent_data->m_size -= 1;
            }
            delete index.data<_FileSystemIndexData>();
            m_index_map.erase(index.getUUID());

            signalEventListeners(index_path, event_flags);
        }

        return result;
    }

    bool FileSystemModel::remove_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        bool result        = false;
        fs_path index_path = getPath_(index);

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.sleep();
#else
        m_watchdog.ignorePathOnce(index_path);
#endif

        FileSystemModelEventFlags event_flags = FileSystemModelEventFlags::EVENT_PATH_REMOVED;
        if (validateIndex(getParentArchive(index))) {
            event_flags |= FileSystemModelEventFlags::EVENT_IS_VIRTUAL;
        }

        if (isFile_(index)) {
            Filesystem::remove(getPath_(index))
                .and_then([&](bool removed) {
                    if (!removed) {
                        TOOLBOX_ERROR_V("[FileSystemModel] Failed to remove file: {}",
                                        getPath_(index).string());
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
        } else if (isArchive_(index)) {
            TOOLBOX_ERROR("[FileSystemModel] Index is not a file!");
        } else {
            TOOLBOX_ERROR("[FileSystemModel] Index is not a file!");
        }

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.wake();
#endif

        if (result) {
            ModelIndex parent = getParent_(index);
            if (validateIndex(parent)) {
                _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();
                parent_data->m_children.erase(std::remove(parent_data->m_children.begin(),
                                                          parent_data->m_children.end(),
                                                          index.getUUID()),
                                              parent_data->m_children.end());
                parent_data->m_size -= 1;
            }
            delete index.data<_FileSystemIndexData>();
            m_index_map.erase(index.getUUID());

            signalEventListeners(index_path, event_flags);
        }

        return result;
    }

    ModelIndex FileSystemModel::rename_(const ModelIndex &file, const std::string &new_name) {
        if (!validateIndex(file)) {
            return ModelIndex();
        }
        if (!isDirectory_(file) && !isFile_(file)) {
            TOOLBOX_ERROR("[FileSystemModel] Not a directory or file!");
            return ModelIndex();
        }

        bool result = false;

        fs_path from                      = getPath_(file);
        fs_path to                        = from.parent_path() / new_name;
        ModelIndex parent                 = getParent_(file);
        _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();

        int dest_index = std::find(parent_data->m_children.begin(), parent_data->m_children.end(),
                                   file.getUUID()) -
                         parent_data->m_children.begin();

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.sleep();
#else
        m_watchdog.ignorePathOnce(from);
        m_watchdog.ignorePathOnce(to);
#endif

        FileSystemModelEventFlags event_flags = FileSystemModelEventFlags::EVENT_PATH_RENAMED;
        if (validateIndex(getParentArchive(file))) {
            event_flags |= FileSystemModelEventFlags::EVENT_IS_VIRTUAL;
        }

        Filesystem::rename(from, to)
            .and_then([&result]() {
                result = true;
                return Result<bool, FSError>();
            })
            .or_else([&](const FSError &error) {
                TOOLBOX_ERROR_V("[FileSystemModel] Failed to rename file: {}", error.m_message[0]);
                return Result<bool, FSError>();
            });

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.wake();
#endif

        if (!result) {
            return ModelIndex();
        }

        delete file.data<_FileSystemIndexData>();
        m_index_map.erase(file.getUUID());
        parent_data->m_children.erase(std::remove(parent_data->m_children.begin(),
                                                  parent_data->m_children.end(), file.getUUID()),
                                      parent_data->m_children.end());
        parent_data->m_size -= 1;

        ModelIndex ret_index = makeIndex(to, dest_index, parent);
        signalEventListeners(to, event_flags);
        return ret_index;
    }

    ModelIndex FileSystemModel::copy_(const fs_path &file, const ModelIndex &new_parent,
                                      const std::string &new_name) {
        if (!validateIndex(new_parent)) {
            TOOLBOX_ERROR("[FileSystemModel] New parent isn't a valid index!");
            return ModelIndex();
        }
        if (!Filesystem::exists(file)) {
            TOOLBOX_ERROR_V("[FileSystemModel] \"{}\" is not a directory or file!", file.string());
            return ModelIndex();
        }

        FileSystemModelEventFlags event_flags = FileSystemModelEventFlags::EVENT_FOLDER_ADDED;
        {
            ModelIndex src_index = getIndex_(file);
            if (validateIndex(src_index)) {
                if (isFile_(src_index)) {
                    // TODO: Detect when destination path is part of a valid archive.
                    if (false) {
                        event_flags |= FileSystemModelEventFlags::EVENT_IS_VIRTUAL;
                    }
                    event_flags = FileSystemModelEventFlags::EVENT_FILE_ADDED;
                }
            } else {
                if (Filesystem::is_regular_file(file).value_or(false)) {
                    event_flags = FileSystemModelEventFlags::EVENT_FILE_ADDED;
                }
            }
        }

        bool result = false;
        fs_path to  = new_parent.data<_FileSystemIndexData>()->m_path / new_name;

        int dest_index = getRowCount_(new_parent);

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.sleep();
#else
        m_watchdog.ignorePathOnce(to);
#endif

        Filesystem::copy(file, to)
            .and_then([&result]() {
                result = true;
                return Result<bool, FSError>();
            })
            .or_else([&](const FSError &error) {
                TOOLBOX_ERROR_V("[FileSystemModel] Failed to copy file: {}", error.m_message[0]);
                return Result<bool, FSError>();
            });

#if TOOLBOX_FS_WATCHDOG_SLEEP_ON_SELF_UPDATE
        m_watchdog.wake();
#endif

        ModelIndex ret_index = makeIndex(to, getRowCount_(new_parent), new_parent);
        signalEventListeners(to, event_flags);
        return ret_index;
    }

    ModelIndex FileSystemModel::getIndex_(const fs_path &path) const {
        if (m_index_map.empty()) {
            return ModelIndex();
        }

        for (const auto &[uuid, index] : m_index_map) {
            if (getPath_(index) == path) {
                return index;
            }
        }

        return ModelIndex();
    }

    ModelIndex FileSystemModel::getIndex_(const UUID64 &uuid) const { return m_index_map.at(uuid); }

    ModelIndex FileSystemModel::getIndex_(int64_t row, int64_t column,
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

    fs_path FileSystemModel::getPath_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return fs_path();
        }

        return index.data<_FileSystemIndexData>()->m_path;
    }

    ModelIndex FileSystemModel::getParent_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        const UUID64 &id = index.data<_FileSystemIndexData>()->m_parent;
        if (id == 0) {
            return ModelIndex();
        }

        return m_index_map.at(id);
    }

    ModelIndex FileSystemModel::getSibling_(int64_t row, int64_t column,
                                            const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return ModelIndex();
        }

        const ModelIndex &parent = getParent_(index);
        if (!validateIndex(parent)) {
            return ModelIndex();
        }

        return getIndex_(row, column, parent);
    }

    size_t FileSystemModel::getColumnCount_(const ModelIndex &index) const { return 1; }

    size_t FileSystemModel::getRowCount_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 0;
        }

        return index.data<_FileSystemIndexData>()->m_children.size();
    }

    int64_t FileSystemModel::getColumn_(const ModelIndex &index) const {
        return validateIndex(index) ? 0 : -1;
    }

    int64_t FileSystemModel::getRow_(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return -1;
        }

        ModelIndex parent = getParent_(index);
        if (!validateIndex(parent)) {
            return 0;
        }

        _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();
        int64_t row                       = 0;
        for (const UUID64 &uuid : parent_data->m_children) {
            if (uuid == index.getUUID()) {
                return row;
            }
            ++row;
        }

        return -1;
    }

    bool FileSystemModel::hasChildren_(const ModelIndex &parent) const {
        if (getRowCount_(parent) > 0) {
            return true;
        }
        return pollChildren(parent) > 0;
    }

    ScopePtr<MimeData>
    FileSystemModel::createMimeData_(const std::vector<ModelIndex> &indexes) const {
        TOOLBOX_ERROR("[FileSystemModel] Mimedata unimplemented!");
        return ScopePtr<MimeData>();
    }

    bool FileSystemModel::canFetchMore_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return false;
        }

        if (!isDirectory_(index) && !isArchive_(index)) {
            return false;
        }

        return index.data<_FileSystemIndexData>()->m_children.empty();
    }

    void FileSystemModel::fetchMore_(const ModelIndex &index) {
        if (!validateIndex(index)) {
            return;
        }

        if (isDirectory_(index)) {
            fs_path path = getPath_(index);

            size_t i = 0;
            for (const auto &entry : Filesystem::directory_iterator(path)) {
                makeIndex(entry.path(), i++, index);
            }
        }
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

        if (data->m_type == _FileSystemIndexData::Type::FILE) {
            Filesystem::file_size(data->m_path)
                .and_then([&](size_t size) {
                    data->m_size = size;
                    return Result<size_t, FSError>();
                })
                .or_else([&](const FSError &error) {
                    TOOLBOX_ERROR_V("[FileSystemModel] Failed to get file size: {}",
                                    error.m_message[0]);
                    return Result<size_t, FSError>();
                });
        } else {
            data->m_size = 0;
        }

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
                parent_data->m_size += 1;
            }

            m_index_map[index.getUUID()] = std::move(index);
        }

        return index;
    }

    ModelIndex FileSystemModel::getParentArchive(const ModelIndex &index) const {
        ModelIndex parent = getParent_(index);
        do {
            if (isArchive_(parent)) {
                return parent;
            }
            parent = getParent_(parent);
        } while (validateIndex(parent));

        return ModelIndex();
    }

    size_t FileSystemModel::pollChildren(const ModelIndex &index) const {
        if (!validateIndex(index)) {
            return 0;
        }

        size_t count = 0;

        if (isDirectory_(index)) {
            // Count the children in the filesystem
            for (const auto &entry : Filesystem::directory_iterator(getPath_(index))) {
                count += 1;
            }
        } else if (isArchive_(index)) {
            // Count the children in the archive
            TOOLBOX_ERROR("[FileSystemModel] Archive polling unimplemented!");
        } else if (isFile_(index)) {
            // Files have no children
            count = 0;
        } else {
            // Invalid index
            count = 0;
        }

        return count;
    }

    void FileSystemModel::folderAdded(const fs_path &path) {
        {
            std::scoped_lock lock(m_mutex);

            ModelIndex parent = getIndex_(path.parent_path());
            if (!validateIndex(parent)) {
                return;
            }

            makeIndex(path, getRowCount_(parent), parent);
        }

        signalEventListeners(path, FileSystemModelEventFlags::EVENT_FOLDER_ADDED);
    }

    void FileSystemModel::folderModified(const fs_path &path) {
        {
            std::scoped_lock lock(m_mutex);

            ModelIndex index = getIndex_(path);
            if (!validateIndex(index)) {
                return;
            }

            _FileSystemIndexData *data = index.data<_FileSystemIndexData>();

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
        }

        signalEventListeners(path, FileSystemModelEventFlags::EVENT_FOLDER_MODIFIED);
    }

    void FileSystemModel::fileAdded(const fs_path &path) {
        {
            std::scoped_lock lock(m_mutex);

            ModelIndex parent = getIndex_(path.parent_path());
            if (!validateIndex(parent)) {
                return;
            }

            makeIndex(path, getRowCount_(parent), parent);
        }

        signalEventListeners(path, FileSystemModelEventFlags::EVENT_FILE_ADDED);
    }

    void FileSystemModel::fileModified(const fs_path &path) {
        {
            std::scoped_lock lock(m_mutex);

            ModelIndex index = getIndex_(path);
            if (!validateIndex(index)) {
                return;
            }

            _FileSystemIndexData *data = index.data<_FileSystemIndexData>();

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

            Filesystem::file_size(data->m_path)
                .and_then([&](size_t size) {
                    data->m_size = size;
                    return Result<size_t, FSError>();
                })
                .or_else([&](const FSError &error) {
                    TOOLBOX_ERROR_V("[FileSystemModel] Failed to get file size: {}",
                                    error.m_message[0]);
                    return Result<size_t, FSError>();
                });
        }

        signalEventListeners(path, FileSystemModelEventFlags::EVENT_FILE_MODIFIED);
    }

    void FileSystemModel::pathRenamedSrc(const fs_path &old_path) {
        std::scoped_lock lock(m_mutex);
        m_rename_src = old_path;
    }

    void FileSystemModel::pathRenamedDst(const fs_path &new_path) {
        {
            std::scoped_lock lock(m_mutex);

            ModelIndex index = getIndex_(m_rename_src);
            if (!validateIndex(index)) {
                return;
            }

            _FileSystemIndexData *data = index.data<_FileSystemIndexData>();
            data->m_path               = new_path;
            data->m_name               = new_path.filename().string();

            // Reparent index if necessary
            ModelIndex parent = getParent_(index);
            if (!validateIndex(parent)) {
                return;
            }

            if (parent.data<_FileSystemIndexData>()->m_path != new_path.parent_path()) {
                _FileSystemIndexData *old_parent_data = parent.data<_FileSystemIndexData>();
                old_parent_data->m_children.erase(std::remove(old_parent_data->m_children.begin(),
                                                              old_parent_data->m_children.end(),
                                                              index.getUUID()),
                                                  old_parent_data->m_children.end());
                old_parent_data->m_size -= 1;

                ModelIndex new_parent = getIndex_(new_path.parent_path());
                if (!validateIndex(new_parent)) {
                    return;
                }

                _FileSystemIndexData *new_parent_data = new_parent.data<_FileSystemIndexData>();
                new_parent_data->m_children.push_back(index.getUUID());
                new_parent_data->m_size += 1;
            }
        }

        signalEventListeners(new_path, FileSystemModelEventFlags::EVENT_PATH_RENAMED);
    }

    void FileSystemModel::pathRemoved(const fs_path &path) {
        {
            std::scoped_lock lock(m_mutex);

            ModelIndex index = getIndex_(path);
            if (!validateIndex(index)) {
                return;
            }

            ModelIndex parent = getParent_(index);
            if (!validateIndex(parent)) {
                return;
            }

            {
                _FileSystemIndexData *parent_data = parent.data<_FileSystemIndexData>();
                parent_data->m_children.erase(std::remove(parent_data->m_children.begin(),
                                                          parent_data->m_children.end(),
                                                          index.getUUID()),
                                              parent_data->m_children.end());
                parent_data->m_size -= 1;
                delete index.data<_FileSystemIndexData>();
                m_index_map.erase(index.getUUID());
            }
        }

        signalEventListeners(path, FileSystemModelEventFlags::EVENT_PATH_REMOVED);
    }

    void FileSystemModel::signalEventListeners(const fs_path &path,
                                               FileSystemModelEventFlags flags) {
        for (const auto &[key, listener] : m_listeners) {
            if ((listener.second & flags) != FileSystemModelEventFlags::NONE) {
                listener.first(path, flags);
            }
        }
    }

    RefPtr<FileSystemModel> FileSystemModelSortFilterProxy::getSourceModel() const {
        return m_source_model;
    }

    void FileSystemModelSortFilterProxy::setSourceModel(RefPtr<FileSystemModel> model) {
        if (m_source_model == model) {
            return;
        }

        if (m_source_model) {
            m_source_model->removeEventListener(getUUID());
        }

        m_source_model = model;

        if (m_source_model) {
            m_source_model->addEventListener(getUUID(), TOOLBOX_BIND_EVENT_FN(fsUpdateEvent),
                                             FileSystemModelEventFlags::EVENT_ANY);
        }
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
        std::unique_lock lock(m_cache_mutex);

        ModelIndex source_index       = toSourceIndex(index);
        ModelIndex src_parent         = m_source_model->getParent(source_index);
        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) == m_row_map.end()) {
            cacheIndex_(src_parent);
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
        return m_source_model->getRowCount(source_index);
    }

    int64_t FileSystemModelSortFilterProxy::getColumn(const ModelIndex &index) const {
        ModelIndex &&source_index = toSourceIndex(index);
        return m_source_model->getColumn(source_index);
    }

    int64_t FileSystemModelSortFilterProxy::getRow(const ModelIndex &index) const {

        ModelIndex &&source_index     = toSourceIndex(index);
        ModelIndex src_parent         = m_source_model->getParent(source_index);
        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) == m_row_map.end()) {
            cacheIndex_(src_parent);
        }

        int64_t src_row = m_source_model->getRow(source_index);
        if (src_row == -1) {
            return src_row;
        }

        const std::vector<int64_t> &row_map = m_row_map[map_key];
        for (size_t i = 0; i < row_map.size(); ++i) {
            if (src_row == row_map[i]) {
                return i;
            }
        }

        return -1;
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

        IDataModel::setIndexUUID(proxy_index, index.getUUID());

        return proxy_index;
    }

    ModelIndex FileSystemModelSortFilterProxy::toProxyIndex(int64_t row, int64_t column,
                                                            const ModelIndex &src_parent) const {
        std::unique_lock lock(m_cache_mutex);

        const UUID64 &src_parent_uuid = src_parent.getUUID();

        u64 map_key = src_parent_uuid;
        if (!m_source_model->validateIndex(src_parent)) {
            map_key = 0;
        }

        if (m_row_map.find(map_key) == m_row_map.end()) {
            cacheIndex_(src_parent);
        }

        if (row < m_row_map[map_key].size()) {
            int64_t the_row = m_row_map[map_key][row];
            return toProxyIndex(m_source_model->getIndex(the_row, column, src_parent));
        }

        return ModelIndex();
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
        std::unique_lock lock(m_cache_mutex);
        cacheIndex_(dir_index);
    }

    void FileSystemModelSortFilterProxy::cacheIndex_(const ModelIndex &dir_index) const {
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

    void FileSystemModelSortFilterProxy::fsUpdateEvent(const fs_path &path,
                                                       FileSystemModelEventFlags flags) {
        if ((flags & FileSystemModelEventFlags::EVENT_ANY) == FileSystemModelEventFlags::NONE) {
            return;
        }

        {
            std::scoped_lock lock(m_cache_mutex);

            m_filter_map.clear();
            m_row_map.clear();
        }
    }

}  // namespace Toolbox