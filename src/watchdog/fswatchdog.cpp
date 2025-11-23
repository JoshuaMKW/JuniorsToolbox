#include <stdio.h>
#include <stdlib.h>

#include "core/core.hpp"
#include "watchdog/fswatchdog.hpp"

namespace Toolbox {

    void FileSystemWatchdog::reset() {
        std::scoped_lock lock(m_mutex);
        m_dir_paths.clear();
        m_file_paths.clear();
        m_path_infos.clear();
    }

    void FileSystemWatchdog::sleep() {
        std::scoped_lock lock(m_mutex);
        m_asleep      = true;
        m_sleep_start = Filesystem::file_time_type::clock::now();
    }

    void FileSystemWatchdog::wake() {
        std::scoped_lock lock(m_mutex);
        m_asleep    = false;
        m_sleep_end = Filesystem::file_time_type::clock::now();
    }

    void FileSystemWatchdog::ignorePathOnce(const fs_path &path) { m_ignore_paths.emplace(path); }

    void FileSystemWatchdog::ignorePathOnce(fs_path &&path) {
        m_ignore_paths.emplace(std::move(path));
    }

    void FileSystemWatchdog::flagPathVisible(const fs_path &path, bool visible) {
        if (visible) {
            m_visible_paths.emplace(path);
        } else {
            m_visible_paths.erase(path);
        }
    }

    void FileSystemWatchdog::flagPathVisible(fs_path &&path, bool visible) {
        if (visible) {
            m_visible_paths.emplace(std::move(path));
        } else {
            m_visible_paths.erase(std::move(path));
        }
    }

    void FileSystemWatchdog::addPath(const fs_path &path) {
        std::scoped_lock lock(m_mutex);
        if (Toolbox::Filesystem::is_directory(path).value_or(false)) {
            m_dir_paths.emplace(this, path);
        } else {
            m_file_paths.emplace(this, path);
        }
    }

    void FileSystemWatchdog::addPath(fs_path &&path) {
        std::scoped_lock lock(m_mutex);
        if (Toolbox::Filesystem::is_directory(path).value_or(false)) {
            m_dir_paths.emplace(this, std::move(path));
        } else {
            m_file_paths.emplace(this, std::move(path));
        }
    }

    void FileSystemWatchdog::removePath(const fs_path &path) {
        std::scoped_lock lock(m_mutex);

        for (auto it = m_dir_paths.begin(); it != m_dir_paths.end(); ++it) {
            if (it->getPath() == path) {
                m_dir_paths.erase(it);
                return;
            }
        }

        for (auto it = m_file_paths.begin(); it != m_file_paths.end(); ++it) {
            if (it->getPath() == path) {
                m_file_paths.erase(it);
                return;
            }
        }
    }

    void FileSystemWatchdog::removePath(fs_path &&path) {
        std::scoped_lock lock(m_mutex);

        for (auto it = m_dir_paths.begin(); it != m_dir_paths.end(); ++it) {
            if (it->getPath() == path) {
                m_dir_paths.erase(it);
                return;
            }
        }

        for (auto it = m_file_paths.begin(); it != m_file_paths.end(); ++it) {
            if (it->getPath() == path) {
                m_file_paths.erase(it);
                return;
            }
        }
    }

    void FileSystemWatchdog::onFileAdded(file_changed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_file_added_cb = cb;
    }

    void FileSystemWatchdog::onFileModified(file_changed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_file_modified_cb = cb;
    }

    void FileSystemWatchdog::onDirAdded(dir_changed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_dir_added_cb = cb;
    }

    void FileSystemWatchdog::onDirModified(dir_changed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_dir_modified_cb = cb;
    }

    void FileSystemWatchdog::onPathRenamedSrc(path_changed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_path_renamed_src_cb = cb;
    }

    void FileSystemWatchdog::onPathRenamedDst(path_changed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_path_renamed_dst_cb = cb;
    }

    void FileSystemWatchdog::onPathRemoved(file_changed_cb cb) {
        std::scoped_lock lock(m_mutex);
        m_path_removed_cb = cb;
    }

    void FileSystemWatchdog::tRun(void *param) {
        while (!tIsSignalKill()) {
            while (!tIsSignalKill() && m_asleep) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

#if 0
            {
                std::scoped_lock lock(m_mutex);
                for (const auto &path : m_file_paths) {
                    observePathF(path);
                }
                for (const auto &path : m_dir_paths) {
                    observePath(path);
                }
            }
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool FileSystemWatchdog::wasSleepingForAlert(const fs_path &path) {
        if (m_asleep) {
            return true;
        }

        bool result = false;
        Filesystem::last_write_time(path).and_then(
            [this, &result](const Filesystem::file_time_type &file_time) {
                result = m_sleep_start <= file_time && m_sleep_end >= file_time;
                return Result<Filesystem::file_time_type, FSError>();
            });
        return result;
    }

    PathWatcher_::PathWatcher_(FileSystemWatchdog *watchdog, const fs_path &path) {
        m_watchdog = watchdog;
        m_path     = path;
        m_is_dir   = Toolbox::Filesystem::is_directory(path).value_or(false);
        m_is_file  = !m_is_dir;
        m_is_open  = false;

        if (m_is_dir) {
            watchDirectory(path);
        } else {
            watchFile(path);
        }
    }

    PathWatcher_::~PathWatcher_() {
        if (m_is_dir) {
            closeDirectory();
        } else {
            closeFile();
        }
    }

    void PathWatcher_::watchDirectory(const fs_path &path) {
        m_watch =
            make_scoped<filewatch::FileWatch<fs_path>>(path, TOOLBOX_BIND_EVENT_FN(callback_));
    }

    void PathWatcher_::watchFile(const fs_path &path) {
        m_watch =
            make_scoped<filewatch::FileWatch<fs_path>>(path, TOOLBOX_BIND_EVENT_FN(callback_));
    }

    void PathWatcher_::closeDirectory() { m_watch.reset(); }

    void PathWatcher_::closeFile() { m_watch.reset(); }

    void PathWatcher_::callback_(const fs_path &path, const filewatch::Event event) {
        std::scoped_lock lock(m_watchdog->m_mutex);

        fs_path abs_path = m_path / path;

        if (m_watchdog->wasSleepingForAlert(abs_path)) {
            return;
        }

        if (m_watchdog->m_ignore_paths.contains(abs_path)) {
            m_watchdog->m_ignore_paths.erase(abs_path);
            return;
        }

        bool is_dir = Filesystem::is_directory(abs_path).value_or(false);

        // GUI view based optimization path. GUIs should add each path as they become
        // visible as a method to filter out invisible watch updates.
        if (!m_watchdog->m_visible_paths.contains(abs_path.parent_path())) {
            return;
        }

        switch (event) {
        case filewatch::Event::added:
            if (is_dir) {
                if (m_watchdog->m_dir_added_cb) {
                    m_watchdog->m_dir_added_cb(abs_path);
                }
            } else {
                if (m_watchdog->m_file_added_cb) {
                    m_watchdog->m_file_added_cb(abs_path);
                }
            }
            break;
        case filewatch::Event::modified:
            if (is_dir) {
                if (m_watchdog->m_dir_modified_cb) {
                    m_watchdog->m_dir_modified_cb(abs_path);
                }
            } else {
                if (m_watchdog->m_file_modified_cb) {
                    m_watchdog->m_file_modified_cb(abs_path);
                }
            }
            break;
        case filewatch::Event::removed:
            if (m_watchdog->m_path_removed_cb) {
                m_watchdog->m_path_removed_cb(abs_path);
            }
            break;
        case filewatch::Event::renamed_old:
            if (m_watchdog->m_path_renamed_src_cb) {
                m_watchdog->m_path_renamed_src_cb(abs_path);
            }
            break;
        case filewatch::Event::renamed_new:
            if (m_watchdog->m_path_renamed_dst_cb) {
                m_watchdog->m_path_renamed_dst_cb(abs_path);
            }
            break;
        }
    }

}  // namespace Toolbox
