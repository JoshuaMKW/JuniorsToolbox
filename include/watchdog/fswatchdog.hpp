#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/threaded.hpp"
#include "fsystem.hpp"

namespace Toolbox {

    class FileSystemWatchdog : public Threaded<void> {
    public:
        using file_added_cb    = std::function<void(const fs_path &path)>;
        using file_removed_cb  = std::function<void(const fs_path &path)>;
        using file_modified_cb = std::function<void(const fs_path &path)>;

        using dir_added_cb    = std::function<void(const fs_path &path)>;
        using dir_removed_cb  = std::function<void(const fs_path &path)>;
        using dir_modified_cb = std::function<void(const fs_path &path)>;

        FileSystemWatchdog()          = default;
        virtual ~FileSystemWatchdog() = default;

        void reset();
        void sleep();
        void wake();

        void addPath(const fs_path &path);
        void addPath(fs_path &&path);

        void removePath(const fs_path &path);
        void removePath(fs_path &&path);

    public:
        void onFileAdded(file_added_cb cb);
        void onFileRemoved(file_removed_cb cb);
        void onFileModified(file_modified_cb cb);

        void onDirAdded(dir_added_cb cb);
        void onDirRemoved(dir_removed_cb cb);
        void onDirModified(dir_modified_cb cb);

    protected:
        void tRun(void *param) override;

        void observePath(const fs_path &path);
        void observePathF(const fs_path &path);

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

        std::unordered_map<fs_path, FileInfo> m_path_infos;
        std::unordered_set<fs_path> m_watch_paths;

        file_added_cb m_file_added_cb;
        file_removed_cb m_file_removed_cb;
        file_modified_cb m_file_modified_cb;

        dir_added_cb m_dir_added_cb;
        dir_removed_cb m_dir_removed_cb;
        dir_modified_cb m_dir_modified_cb;

        std::mutex m_mutex;
    };

}  // namespace Toolbox