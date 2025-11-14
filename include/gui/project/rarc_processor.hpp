#pragma once

#include "core/threaded.hpp"
#include "fsystem.hpp"

namespace Toolbox::UI {

    class RarcProcessor : public Toolbox::Threaded<void> {
    protected:
        void tRun(void *param) override;

    public:
        using task_cb = std::function<void()>;

        void requestCompileArchive(const fs_path &src_path, const fs_path &dest_path, task_cb on_complete = nullptr);
        void requestExtractArchive(const fs_path &arc_path, const fs_path &dest_path, task_cb on_complete = nullptr);

    protected:
        enum class TaskType {
            NONE,
            COMPILE,
            EXTRACT,
        };

        void processCompileTask();
        void processExtractTask();

    private:
        task_cb m_on_complete_compile;
        task_cb m_on_complete_extract;
        fs_path m_src_path;
        fs_path m_dest_path;
        TaskType m_current_task = TaskType::NONE;

        std::mutex m_arc_cv_mutex;
        std::condition_variable m_arc_cv;
    };

}  // namespace Toolbox::UI