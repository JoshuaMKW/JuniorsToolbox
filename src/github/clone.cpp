#include <thread>

#include <git2/clone.h>
#include <git2/errors.h>
#include <git2/global.h>

#include "github/clone.hpp"
#include "github/credentials.hpp"

#include "core/log.hpp"

namespace Toolbox::Git {

    static clone_progress_cb s_clone_progress_callback = nullptr;
    static std::string s_git_error_msg;

    static int IndexerUpdateCallback(const git_indexer_progress *stats, void *payload) {
        if (!s_clone_progress_callback) {
            return GIT_OK;
        }

        const float progress = stats->total_deltas + stats->total_objects > 0
                                   ? (float)(stats->indexed_deltas + (stats->received_objects + stats->indexed_objects) / 2) /
                                         (float)(stats->total_deltas + stats->total_objects)
                                   : 0.0f;
        const std::string job_name = "Receiving and Indexing Objects and Deltas";
        const std::string message  = std::format(
            "Received objects: {}/{}, Indexed objects/deltas: {}/{}", stats->received_objects,
            stats->total_objects, stats->indexed_objects + stats->indexed_deltas,
            stats->total_objects + stats->total_deltas);
        const int job_index = 0;
        const int total_jobs = 3;
        s_clone_progress_callback(message, job_name, progress, job_index, total_jobs);
        return GIT_OK;
    }

    static void ThreadedClone(const std::string &repo_url, const fs_path &parent_dir,
                              clone_progress_cb on_progress, clone_complete_cb on_complete) {
        if (git_libgit2_init() < 0) {
            TOOLBOX_ERROR("Failed to initialize libgit2");
            if (on_complete) {
                on_complete("", false);
            }
            return;
        }

        std::string repo_name;
        size_t last_slash = repo_url.find_last_of('/');
        if (last_slash != std::string::npos) {
            repo_name = repo_url.substr(last_slash + 1);
            if (repo_name.ends_with(".git")) {
                repo_name = repo_name.substr(0, repo_name.size() - 4);
            }
        } else {
            if (on_complete) {
                on_complete("", false);
            }
            return;
        }

        bool successful = false;

        const fs_path full_path                           = parent_dir / repo_name;
        git_repository *repo                              = nullptr;
        git_clone_options clone_opts                      = GIT_CLONE_OPTIONS_INIT;
        clone_opts.fetch_opts                             = GIT_FETCH_OPTIONS_INIT;
        clone_opts.fetch_opts.callbacks.credentials       = GitCredentialsProvider;
        clone_opts.fetch_opts.callbacks.transfer_progress = IndexerUpdateCallback;
        clone_opts.fetch_opts.callbacks.sideband_progress = [](const char *str, int len,
                                                               void *payload) -> int { return 0; };

        s_clone_progress_callback = on_progress;

        int error = git_clone(&repo, repo_url.c_str(), full_path.string().c_str(), &clone_opts);
        if (error != 0) {
            const git_error *e = git_error_last();
            s_git_error_msg    = e && e->message ? e->message : "Unknown error";
            TOOLBOX_ERROR_V("Git clone failed: {}", s_git_error_msg);
        } else {
            TOOLBOX_DEBUG_LOG_V("Successfully cloned repository to {}", full_path.string());
            successful = true;
        }

        if (repo) {
            git_repository_free(repo);
        }

        if (on_complete) {
            on_complete(full_path, successful);
        }

        git_libgit2_shutdown();
    }

    std::string GitCloneLastError() { return s_git_error_msg; }

    void GitCloneAsync(const std::string &repo_url, const fs_path &parent_dir,
                       clone_progress_cb on_progress, clone_complete_cb on_complete) {
        std::thread clone_thread(ThreadedClone, repo_url, parent_dir, on_progress, on_complete);
        clone_thread.detach();
    }

}  // namespace Toolbox::Git