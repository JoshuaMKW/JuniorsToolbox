#include "gui/project/rarc_processor.hpp"
#include "gui/logging/errors.hpp"
#include "rarc/rarc.hpp"

#include <fstream>

using namespace Toolbox::RARC;

namespace Toolbox::UI {

    void RarcProcessor::tRun(void *param) {
        while (!tIsSignalKill()) {
            switch (m_current_task) {
            case TaskType::COMPILE:
                processCompileTask();
                break;
            case TaskType::EXTRACT:
                processExtractTask();
                break;
            case TaskType::NONE:
            default:
                // Wait for a task to be assigned
                {
                    std::unique_lock<std::mutex> lock(m_arc_cv_mutex);
                    m_arc_cv.wait(lock, [this]() {
                        return m_current_task != TaskType::NONE || tIsSignalKill();
                    });
                }
                break;
            }
        }
    }

    void RarcProcessor::requestCompileArchive(const fs_path &src_path, const fs_path &dest_path,
                                              task_cb on_complete) {
        std::lock_guard<std::mutex> lock(m_arc_cv_mutex);
        m_on_complete_compile = on_complete;
        m_src_path            = src_path;
        m_dest_path           = dest_path;
        m_current_task        = TaskType::COMPILE;
        m_arc_cv.notify_one();
    }

    void RarcProcessor::requestExtractArchive(const fs_path &arc_path, const fs_path &dest_path,
                                              task_cb on_complete) {
        std::lock_guard<std::mutex> lock(m_arc_cv_mutex);
        m_on_complete_extract = on_complete;
        m_src_path            = arc_path;
        m_dest_path           = dest_path;
        m_current_task        = TaskType::EXTRACT;
        m_arc_cv.notify_one();
    }

    void RarcProcessor::processCompileTask() {
        std::ofstream out_file(m_dest_path.string(), std::ios::binary);
        if (!out_file.is_open()) {
            LogError(make_fs_error<void>(std::error_code(),
                                         {"COMPILE: Failed to open destination file for writing"})
                         .error());
            return;
        }

        Serializer out(out_file.rdbuf());

        ResourceArchive::CreateFromPath(m_src_path)
            .and_then([&out](ResourceArchive &&rarc) {
                auto result = rarc.serialize(out);
                if (!result) {
                    LogError(result.error());
                }
                return Result<ResourceArchive, FSError>(ResourceArchive("null"));
            })
            .or_else([&](const FSError &err) {
                LogError(err);
                return Result<ResourceArchive, FSError>(ResourceArchive("null"));
            });

        if (m_on_complete_compile) {
            m_on_complete_compile();
            m_on_complete_compile = nullptr;
        }

        m_src_path     = "";
        m_dest_path    = "";
        m_current_task = TaskType::NONE;
    }

    void RarcProcessor::processExtractTask() {
        std::ifstream in_file(m_src_path.string(), std::ios::binary);
        if (!in_file.is_open()) {
            LogError(make_fs_error<void>(std::error_code(),
                                         {"COMPILE: Failed to open destination file for writing"})
                         .error());
            return;
        }

        Deserializer in(in_file.rdbuf());

        ResourceArchive arc_file = ResourceArchive("_tmp_name");
        arc_file.deserialize(in)
            .and_then([this, &arc_file]() {
                fs_path dest_folder = m_dest_path / m_src_path.filename().replace_extension("");
                auto result = arc_file.extractToPath(dest_folder);
                if (!result) {
                    LogError(result.error());
                }
                return Result<void, SerialError>();
            })
            .or_else([&](const SerialError &err) {
                LogError(err);
                return Result<void, SerialError>();
            });

        if (m_on_complete_extract) {
            m_on_complete_extract();
            m_on_complete_extract = nullptr;
        }

        m_src_path     = "";
        m_dest_path    = "";
        m_current_task = TaskType::NONE;
    }

}  // namespace Toolbox::UI