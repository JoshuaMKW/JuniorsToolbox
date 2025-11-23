#include "gui/project/rarc_processor.hpp"
#include "gui/logging/errors.hpp"
#include "rarc/rarc.hpp"

#include <librii/SZS.hpp>

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
                                              bool compress, task_cb on_complete) {
        std::lock_guard<std::mutex> lock(m_arc_cv_mutex);
        m_on_complete_compile = on_complete;
        m_src_path            = src_path;
        m_dest_path           = dest_path;
        m_compress            = compress;
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

        // Special case: Sunshine stage archives all have a root "scene"
        // with a unique filename, so compress the "scene" folder within and
        // name the archive after the containing folder.
        size_t subpaths = 0;
        for (const auto &dir_it : Filesystem::directory_iterator(m_src_path)) {
            subpaths += 1;
        }

        if (subpaths == 1) {
            if (Filesystem::is_directory(m_src_path / "scene")) {
                m_src_path /= "scene";
            }
        }

        Serializer out(out_file.rdbuf());

        if (m_compress) {
            std::stringstream temp_stream;
            Serializer temp_serializer(temp_stream.rdbuf());

            ResourceArchive::CreateFromPath(m_src_path)
                .and_then([&](ResourceArchive &&rarc) {
                    auto result = rarc.serialize(temp_serializer);
                    if (!result) {
                        LogError(result.error());
                    }

                    size_t temp_size = temp_stream.tellp();
                    temp_stream.seekg(0, std::ios::beg);
                    std::vector<u8> temp_data(temp_size);

                    temp_stream.read((char *)temp_data.data(), temp_size);

                    std::vector<u8> comp_data = librii::szs::encode(temp_data);
                    if (comp_data.empty()) {
                        LogError(make_fs_error<void>(std::error_code(),
                                                     {std::format("COMPILE: {}", librii::szs::getLastError())})
                                     .error());
                    } else {
                        std::span<const char> span_data((char *)comp_data.data(), comp_data.size());
                        out.writeBytes(span_data);

                        if (m_on_complete_compile) {
                            m_on_complete_compile();
                            m_on_complete_compile = nullptr;
                        }
                    }

                    return Result<ResourceArchive, FSError>(ResourceArchive("null"));
                })
                .or_else([&](const FSError &err) {
                    LogError(err);
                    return Result<ResourceArchive, FSError>(ResourceArchive("null"));
                });
        } else {
            ResourceArchive::CreateFromPath(m_src_path)
                .and_then([&](ResourceArchive &&rarc) {
                    auto result = rarc.serialize(out);
                    if (!result) {
                        LogError(result.error());
                    }

                    if (m_on_complete_compile) {
                        m_on_complete_compile();
                        m_on_complete_compile = nullptr;
                    }

                    return Result<ResourceArchive, FSError>(ResourceArchive("null"));
                })
                .or_else([&](const FSError &err) {
                    LogError(err);
                    return Result<ResourceArchive, FSError>(ResourceArchive("null"));
                });
        }

        m_src_path     = "";
        m_dest_path    = "";
        m_current_task = TaskType::NONE;
    }

    void RarcProcessor::processExtractTask() {
        std::ifstream in_file(m_src_path.string(), std::ios::binary);
        if (!in_file.is_open()) {
            LogError(
                make_fs_error<void>(std::error_code(), {"EXTRACT: Failed to open file for reading"})
                    .error());

            m_src_path     = "";
            m_dest_path    = "";
            m_current_task = TaskType::NONE;
            return;
        }

        std::vector<u8> magic_buf(8);
        in_file.read((char *)magic_buf.data(), magic_buf.size());

        in_file.seekg(0, std::ios::end);
        size_t in_size = in_file.tellg();
        in_file.seekg(0, std::ios::beg);

        if (librii::szs::isDataYaz0Compressed(magic_buf)) {
            std::vector<u8> src_data(in_size);
            in_file.read((char *)src_data.data(), in_size);

            u32 exp_size = librii::szs::getExpandedSize(magic_buf);

            std::string data_buf;
            data_buf.resize(exp_size);

            if (!librii::szs::decode(data_buf, src_data)) {
                LogError(make_fs_error<void>(std::error_code(),
                                             {"EXTRACT: Failed to decompress file for processing"})
                             .error());

                m_src_path     = "";
                m_dest_path    = "";
                m_current_task = TaskType::NONE;
                return;
            }

            std::stringstream str_str(data_buf);
            Deserializer in(str_str.rdbuf());

            ResourceArchive arc_file = ResourceArchive("_tmp_name");
            arc_file.deserialize(in)
                .and_then([this, &arc_file]() {
                    fs_path dest_folder = m_dest_path;

                    // Special case: Sunshine stage archives all have a root "scene"
                    // with a unique filename, so extract the archive within a folder
                    // that matches the original filename.
                    auto root_it        = arc_file.findNode(0);
                    if (root_it != arc_file.end()) {
                        if (root_it->name == "scene") {
                            dest_folder =
                                (dest_folder / m_src_path.filename()).replace_extension("");
                        }
                    }

                    auto result         = arc_file.extractToPath(dest_folder);
                    if (!result) {
                        LogError(result.error());
                        return Result<void, SerialError>();
                    }

                    if (m_on_complete_extract) {
                        m_on_complete_extract();
                        m_on_complete_extract = nullptr;
                    }

                    return Result<void, SerialError>();
                })
                .or_else([&](const SerialError &err) {
                    LogError(err);
                    return Result<void, SerialError>();
                });
        } else {
            Deserializer in(in_file.rdbuf());

            ResourceArchive arc_file = ResourceArchive("_tmp_name");
            arc_file.deserialize(in)
                .and_then([this, &arc_file]() {
                    fs_path dest_folder = m_dest_path;

                    // Special case: Sunshine stage archives all have a root "scene"
                    // with a unique filename, so extract the archive within a folder
                    // that matches the original filename.
                    auto root_it = arc_file.findNode(0);
                    if (root_it != arc_file.end()) {
                        if (root_it->name == "scene") {
                            dest_folder =
                                (dest_folder / m_src_path.filename()).replace_extension("");
                        }
                    }

                    auto result         = arc_file.extractToPath(dest_folder);
                    if (!result) {
                        LogError(result.error());
                        return Result<void, SerialError>();
                    }

                    if (m_on_complete_extract) {
                        m_on_complete_extract();
                        m_on_complete_extract = nullptr;
                    }

                    return Result<void, SerialError>();
                })
                .or_else([&](const SerialError &err) {
                    LogError(err);
                    return Result<void, SerialError>();
                });
        }

        m_src_path     = "";
        m_dest_path    = "";
        m_current_task = TaskType::NONE;
    }

}  // namespace Toolbox::UI