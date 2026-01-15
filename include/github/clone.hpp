#pragma once

#include <functional>
#include <string>

#include "fsystem.hpp"

namespace Toolbox::Git {

    using clone_progress_cb =
        std::function<void(const std::string &provided_msg, const std::string &job_name, float job_progress, int completed_jobs, int total_jobs)>;
    using clone_complete_cb = std::function<void(const fs_path &out_path, bool successful)>;

    std::string GitCloneLastError();

    void GitCloneAsync(const std::string &repo_url, const fs_path &parent_dir,
                       clone_progress_cb on_progress, clone_complete_cb on_complete);

}  // namespace Toolbox::Git