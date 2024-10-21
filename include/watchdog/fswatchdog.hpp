#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <FileWatch.hpp>

#include "core/memory.hpp"
#include "core/threaded.hpp"
#include "fsystem.hpp"
#include "platform/process.hpp"

namespace Toolbox {

    class FileSystemWatchdog;

    class PathWatcher_ {
    public:
        PathWatcher_(FileSystemWatchdog *watchdog, const fs_path &path);
        ~PathWatcher_();

        const fs_path &getPath() const & { return m_path; }

        bool operator==(const PathWatcher_ &other) const { return m_path == other.m_path; }

    protected:
        void watchDirectory(const fs_path &path);
        void watchFile(const fs_path &path);
        void closeDirectory();
        void closeFile();

        void callback_(const fs_path &path, const filewatch::Event event);

    private:
        FileSystemWatchdog *m_watchdog;
        fs_path m_path;
        bool m_is_dir;
        bool m_is_file;
        bool m_is_open;
        ScopePtr<filewatch::FileWatch<fs_path>> m_watch;
    };

}  // namespace Toolbox

namespace std {

    // Hash for PathWatcher
    template <> struct hash<Toolbox::PathWatcher_> {
        size_t operator()(const Toolbox::PathWatcher_ &_Keyval) const noexcept {
            return std::hash<Toolbox::fs_path>{}(_Keyval.getPath());
        }
    };

}  // namespace std

namespace Toolbox {

    class FileSystemWatchdog : public Threaded<void> {
        friend class Toolbox::PathWatcher_;

    public:
        using file_changed_cb = std::function<void(const fs_path &path)>;
        using dir_changed_cb  = std::function<void(const fs_path &path)>;
        using path_changed_cb = std::function<void(const fs_path &path)>;

        FileSystemWatchdog()          = default;
        virtual ~FileSystemWatchdog() = default;

        void reset();
        void sleep();
        void wake();

        void ignorePathOnce(const fs_path &path);
        void ignorePathOnce(fs_path &&path);

        void addPath(const fs_path &path);
        void addPath(fs_path &&path);

        void removePath(const fs_path &path);
        void removePath(fs_path &&path);

    public:
        void onFileAdded(file_changed_cb cb);
        void onFileModified(file_changed_cb cb);

        void onDirAdded(dir_changed_cb cb);
        void onDirModified(dir_changed_cb cb);

        void onPathRenamedSrc(path_changed_cb cb);
        void onPathRenamedDst(path_changed_cb cb);
        void onPathRemoved(path_changed_cb cb);

    protected:
        void tRun(void *param) override;

        bool wasSleepingForAlert(const fs_path &path);

        struct FileInfo {
            Filesystem::file_status m_status;
            Filesystem::file_time_type m_time;
            size_t m_size;
            bool m_is_dir;
            bool m_exists;
        };

        FileInfo createFileInfo(const fs_path &path);
        void signalChanges(const fs_path &path, const FileInfo &a, const FileInfo &b);

    private:
        bool m_asleep = false;
        Filesystem::file_time_type m_sleep_start;
        Filesystem::file_time_type m_sleep_end;

        std::unordered_set<fs_path> m_ignore_paths;

        std::unordered_map<fs_path, FileInfo> m_path_infos;

        std::unordered_set<PathWatcher_> m_file_paths;
        std::unordered_set<PathWatcher_> m_dir_paths;

        file_changed_cb m_file_added_cb;
        file_changed_cb m_file_modified_cb;

        dir_changed_cb m_dir_added_cb;
        dir_changed_cb m_dir_modified_cb;

        path_changed_cb m_path_renamed_src_cb;
        path_changed_cb m_path_renamed_dst_cb;
        path_changed_cb m_path_removed_cb;

        std::mutex m_mutex;
    };

}  // namespace Toolbox
