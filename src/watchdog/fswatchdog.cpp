#include "watchdog/fswatchdog.hpp"

namespace Toolbox {

    void FileSystemWatchdog::reset() {
        std::scoped_lock lock(m_mutex);
        m_watch_paths.clear();
        m_path_infos.clear();
    }

    void FileSystemWatchdog::sleep() {
        std::scoped_lock lock(m_mutex);
        m_asleep = true;
    }

    void FileSystemWatchdog::wake() {
        std::scoped_lock lock(m_mutex);
        m_asleep = false;
    }

    void FileSystemWatchdog::addPath(const fs_path &path) {
        std::scoped_lock lock(m_mutex);
        m_watch_paths.insert(path);
    }

    void FileSystemWatchdog::addPath(fs_path &&path) {
        std::scoped_lock lock(m_mutex);
        m_watch_paths.emplace(std::move(path));
    }

    void FileSystemWatchdog::removePath(const fs_path &path) {
        std::scoped_lock lock(m_mutex);
        m_watch_paths.erase(path);
    }

    void FileSystemWatchdog::removePath(fs_path &&path) {
        std::scoped_lock lock(m_mutex);
        m_watch_paths.erase(std::move(path));
    }

    void FileSystemWatchdog::onFileAdded(file_added_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_file_added_cb = cb;
    }

    void FileSystemWatchdog::onFileRemoved(file_removed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_file_removed_cb = cb;
    }

    void FileSystemWatchdog::onFileModified(file_modified_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_file_modified_cb = cb;
    }

    void FileSystemWatchdog::onDirAdded(dir_added_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_dir_added_cb = cb;
    }

    void FileSystemWatchdog::onDirRemoved(dir_removed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_dir_removed_cb = cb;
    }

    void FileSystemWatchdog::onDirModified(dir_modified_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_dir_modified_cb = cb;
    }

    void FileSystemWatchdog::tRun(void *param) {
        while (!tIsSignalKill()) {
            while (m_asleep) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            {
                std::scoped_lock lock(m_mutex);
                for (const auto &path : m_watch_paths) {
                    observePath(path);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void FileSystemWatchdog::observePath(const fs_path &path) {
        if (m_path_infos.find(path) == m_path_infos.end()) {
            m_path_infos[path] = createFileInfo(path);
            return;
        }

        const FileInfo &old_info = m_path_infos[path];
        FileInfo new_info        = createFileInfo(path);

        signalChanges(path, old_info, new_info);

        if (new_info.m_is_dir) {
            for (const auto &entry : Toolbox::Filesystem::directory_iterator(path)) {
                observePathF(entry.path());
            }
        }

        m_path_infos[path] = new_info;
    }

    void FileSystemWatchdog::observePathF(const fs_path &path) {
        if (m_path_infos.find(path) == m_path_infos.end()) {
            m_path_infos[path] = createFileInfo(path);
            return;
        }

        const FileInfo &old_info = m_path_infos[path];
        FileInfo new_info        = createFileInfo(path);

        signalChanges(path, old_info, new_info);

        m_path_infos[path] = new_info;
    }

    FileSystemWatchdog::FileInfo FileSystemWatchdog::createFileInfo(const fs_path &path) {
        FileInfo info;

        info.m_is_dir = Toolbox::Filesystem::is_directory(path).value_or(false);
        info.m_exists = Toolbox::Filesystem::exists(path).value_or(false);
        info.m_time =
            Toolbox::Filesystem::last_write_time(path).value_or(Filesystem::file_time_type::min());

        if (!info.m_is_dir && info.m_exists) {
            info.m_size = Toolbox::Filesystem::file_size(path).value_or(0);
        }

        return info;
    }

    void FileSystemWatchdog::signalChanges(const fs_path &path, const FileInfo &a,
                                           const FileInfo &b) {
        if (a.m_exists != b.m_exists) {
            if (b.m_exists) {
                if (b.m_is_dir) {
                    if (m_dir_added_cb) {
                        m_dir_added_cb(path);
                    }
                } else {
                    if (m_file_added_cb) {
                        m_file_added_cb(path);
                    }
                }
            } else {
                if (b.m_is_dir) {
                    if (m_dir_removed_cb) {
                        m_dir_removed_cb(path);
                    }
                } else {
                    if (m_file_removed_cb) {
                        m_file_removed_cb(path);
                    }
                }
            }
        } else {
            if (b.m_exists) {
                if (b.m_time != a.m_time) {
                    if (b.m_is_dir) {
                        if (m_dir_modified_cb) {
                            m_dir_modified_cb(path);
                        }
                    } else {
                        if (m_file_modified_cb) {
                            m_file_modified_cb(path);
                        }
                    }
                }
            }
        }
    }

}  // namespace Toolbox
