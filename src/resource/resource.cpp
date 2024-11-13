#include <execution>

#include "resource/resource.hpp"

namespace Toolbox {

    ResourceManager::~ResourceManager() {
        m_resource_paths.clear();
        m_image_handle_cache.clear();

        for (auto& [_, data] : m_data_preload_cache) {
            std::free(data.m_data_ptr);
        }

        m_data_preload_cache.clear();
    }

    void ResourceManager::includeResourcePath(const fs_path &path, bool preload_files) {
        if (hasResourcePath(path)) {
            return;
        }

        m_resource_paths.emplace_back(path);

        if (!preload_files) {
            return;
        }

        preloadData(path);
    }

    void ResourceManager::includeResourcePath(fs_path &&path, bool preload_files) {
        if (hasResourcePath(path)) {
            return;
        }

        m_resource_paths.emplace_back(path);

        if (!preload_files) {
            return;
        }

        preloadData(std::move(path));
    }

    void ResourceManager::removeResourcePath(const fs_path &path) {
        UUID64 path_uuid = getResourcePathUUID(path);
        removeResourcePath(path_uuid);
    }

    void ResourceManager::removeResourcePath(fs_path &&path) {
        UUID64 path_uuid = getResourcePathUUID(std::move(path));
        removeResourcePath(path_uuid);
    }

    void ResourceManager::removeResourcePath(const UUID64 &path_uuid) {
        m_resource_paths.erase(std::remove_if(m_resource_paths.begin(), m_resource_paths.end(),
                                              [&path_uuid](const ResourcePath &resource_path) {
                                                  return resource_path.getUUID() == path_uuid;
                                              }),
                               m_resource_paths.end());
    }

    bool ResourceManager::hasResourcePath(const fs_path &path) const {
        return std::any_of(
            m_resource_paths.begin(), m_resource_paths.end(),
            [&path](const ResourcePath &resource_path) { return resource_path.getPath() == path; });
    }

    bool ResourceManager::hasResourcePath(fs_path &&path) const {
        return std::any_of(
            m_resource_paths.begin(), m_resource_paths.end(),
            [path](const ResourcePath &resource_path) { return resource_path.getPath() == path; });
    }

    bool ResourceManager::hasResourcePath(const UUID64 &path_uuid) const {
        return std::any_of(
            m_resource_paths.begin(), m_resource_paths.end(),
            [&path_uuid](const ResourcePath &path) { return path.getUUID() == path_uuid; });
    }

    bool ResourceManager::hasDataPath(const fs_path &path, const UUID64 &resource_path_uuid) const {
        if (m_data_preload_cache.find(path) != m_data_preload_cache.end()) {
            return true;
        }

        fs_path resource_path;

        if (resource_path_uuid) {
            std::optional<fs_path> result = getResourcePath(resource_path_uuid);
            if (!result.has_value()) {
                return false;
            }
            resource_path = std::move(result.value());
        } else {
            std::optional<fs_path> result = findResourcePath(path);
            if (!result.has_value()) {
                return false;
            }
            resource_path = std::move(result.value());
        }

        fs_path abs_path = resource_path / path;
        if (m_data_preload_cache.find(abs_path) != m_data_preload_cache.end()) {
            return true;
        }

        bool result = false;
        Filesystem::is_regular_file(abs_path).and_then([&result](bool is_regular_file) {
            result = is_regular_file;
            return Result<bool, FSError>();
        });

        return result;
    }

    bool ResourceManager::hasDataPath(fs_path &&path, const UUID64 &resource_path_uuid) const {
        if (m_data_preload_cache.find(path) != m_data_preload_cache.end()) {
            return true;
        }

        fs_path resource_path;

        if (resource_path_uuid) {
            std::optional<fs_path> result = getResourcePath(resource_path_uuid);
            if (!result.has_value()) {
                return false;
            }
            resource_path = std::move(result.value());
        } else {
            std::optional<fs_path> result = findResourcePath(path);
            if (!result.has_value()) {
                return false;
            }
            resource_path = std::move(result.value());
        }

        fs_path abs_path = resource_path / path;
        if (m_data_preload_cache.find(abs_path) != m_data_preload_cache.end()) {
            return true;
        }

        bool result = false;
        Filesystem::is_regular_file(abs_path).and_then([&result](bool is_regular_file) {
            result = is_regular_file;
            return Result<bool, FSError>();
        });

        return result;
    }

    Result<RefPtr<const ImageData>, FSError>
    ResourceManager::getImageData(const fs_path &path, const UUID64 &resource_path_uuid) const {
        if (!hasDataPath(path, resource_path_uuid)) {
            return make_fs_error<RefPtr<const ImageData>>(std::error_code(),
                                                          {"Resource path not found"});
        }

        if (m_data_preload_cache.find(path) != m_data_preload_cache.end()) {
            const ResourceData &data = m_data_preload_cache[path];

            std::span<u8> data_span(static_cast<u8 *>(data.m_data_ptr), data.m_data_size);
            RefPtr<const ImageData> handle = make_referable<const ImageData>(data_span);

            if (!handle->isValid()) {
                return make_fs_error<RefPtr<const ImageData>>(std::error_code(),
                                                              {"Failed to load image"});
            }

            return handle;
        }

        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        fs_path abs_path      = resource_path / path;

        RefPtr<const ImageData> handle = make_referable<const ImageData>(abs_path);
        if (!handle->isValid()) {
            return make_fs_error<RefPtr<const ImageData>>(std::error_code(),
                                                            {"Failed to load image"});
        }

        return handle;
    }

    Result<RefPtr<const ImageData>, FSError>
    ResourceManager::getImageData(fs_path &&path, const UUID64 &resource_path_uuid) const {
        if (!hasDataPath(path, resource_path_uuid)) {
            return make_fs_error<RefPtr<const ImageData>>(std::error_code(),
                                                          {"Resource path not found"});
        }

        if (m_data_preload_cache.find(path) != m_data_preload_cache.end()) {
            const ResourceData &data = m_data_preload_cache[path];

            std::span<u8> data_span(static_cast<u8 *>(data.m_data_ptr), data.m_data_size);
            RefPtr<const ImageData> handle = make_referable<const ImageData>(data_span);

            if (!handle->isValid()) {
                return make_fs_error<RefPtr<const ImageData>>(std::error_code(),
                                                              {"Failed to load image"});
            }

            return handle;
        }

        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        fs_path abs_path      = resource_path / path;

        RefPtr<const ImageData> handle = make_referable<const ImageData>(abs_path);
        if (!handle->isValid()) {
            return make_fs_error<RefPtr<const ImageData>>(std::error_code(),
                                                          {"Failed to load image"});
        }

        return handle;
    }

    Result<RefPtr<const ImageHandle>, FSError>
    ResourceManager::getImageHandle(const fs_path &path, const UUID64 &resource_path_uuid) const {
        if (m_image_handle_cache.find(path) != m_image_handle_cache.end()) {
            return Result<RefPtr<const ImageHandle>, FSError>(m_image_handle_cache[path]);
        }

        if (!hasDataPath(path, resource_path_uuid)) {
            return make_fs_error<RefPtr<const ImageHandle>>(std::error_code(),
                                                      {"Resource path not found"});
        }

        if (m_data_preload_cache.find(path) != m_data_preload_cache.end()) {
            const ResourceData &data = m_data_preload_cache[path];

            std::span<u8> data_span(static_cast<u8 *>(data.m_data_ptr), data.m_data_size);
            RefPtr<const ImageHandle> handle = make_referable<const ImageHandle>(data_span);

            if (!handle->isValid()) {
                return make_fs_error<RefPtr<const ImageHandle>>(std::error_code(),
                                                          {"Failed to load image"});
            }

            m_image_handle_cache[path] = handle;
            return handle;
        }

        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        fs_path abs_path      = resource_path / path;

        RefPtr<const ImageHandle> handle = make_referable<const ImageHandle>(abs_path);
        if (!handle->isValid()) {
            return make_fs_error<RefPtr<const ImageHandle>>(std::error_code(), {"Failed to load image"});
        }

        m_image_handle_cache[path] = handle;
        return handle;
    }

    Result<RefPtr<const ImageHandle>, FSError>
    ResourceManager::getImageHandle(fs_path &&path, const UUID64 &resource_path_uuid) const {
        if (m_image_handle_cache.find(path) != m_image_handle_cache.end()) {
            return Result<RefPtr<const ImageHandle>, FSError>(m_image_handle_cache[path]);
        }

        if (!hasDataPath(path, resource_path_uuid)) {
            return make_fs_error<RefPtr<const ImageHandle>>(std::error_code(),
                                                      {"Resource path not found"});
        }

        if (m_data_preload_cache.find(path) != m_data_preload_cache.end()) {
            const ResourceData &data = m_data_preload_cache[path];

            std::span<u8> data_span(static_cast<u8 *>(data.m_data_ptr), data.m_data_size);
            RefPtr<const ImageHandle> handle = make_referable<const ImageHandle>(data_span);

            if (!handle->isValid()) {
                return make_fs_error<RefPtr<const ImageHandle>>(std::error_code(),
                                                          {"Failed to load image"});
            }

            m_image_handle_cache[path] = handle;
            return handle;
        }

        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        fs_path abs_path      = resource_path / path;

        RefPtr<const ImageHandle> handle = make_referable<const ImageHandle>(abs_path);
        if (!handle->isValid()) {
            return make_fs_error<RefPtr<const ImageHandle>>(std::error_code(), {"Failed to load image"});
        }

        m_image_handle_cache[path] = handle;
        return handle;
    }

    Result<void, FSError> ResourceManager::getSerialData(std::ifstream &in, const fs_path &path,
                                                         const UUID64 &resource_path_uuid) const {
        if (!hasDataPath(path, resource_path_uuid)) {
            return make_fs_error<void>(std::error_code(), {"Resource path not found"});
        }

        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        fs_path abs_path      = resource_path / path;

        in.open(abs_path);
        if (!in.is_open()) {
            return make_fs_error<void>(std::error_code(), {"Failed to open file"});
        }

        return {};
    }

    Result<void, FSError> ResourceManager::getSerialData(std::ifstream &in, fs_path &&path,
                                                         const UUID64 &resource_path_uuid) const {
        if (!hasDataPath(path, resource_path_uuid)) {
            return make_fs_error<void>(std::error_code(), {"Resource path not found"});
        }

        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        fs_path abs_path      = resource_path / path;

        in.open(abs_path);
        if (!in.is_open()) {
            return make_fs_error<void>(std::error_code(), {"Failed to open file"});
        }

        return {};
    }

    Result<std::span<u8>, FSError>
    ResourceManager::getRawData(const fs_path &path, const UUID64 &resource_path_uuid) const {
        if (!hasDataPath(path, resource_path_uuid)) {
            return make_fs_error<std::span<u8>>(std::error_code(), {"Resource path not found"});
        }

        if (m_data_preload_cache.find(path) != m_data_preload_cache.end()) {
            const ResourceData &data = m_data_preload_cache[path];
            return std::span<u8>(static_cast<u8 *>(data.m_data_ptr), data.m_data_size);
        }

        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        fs_path abs_path      = resource_path / path;

        if (m_data_preload_cache.find(abs_path) != m_data_preload_cache.end()) {
            const ResourceData &data = m_data_preload_cache[abs_path];
            return std::span<u8>(static_cast<u8 *>(data.m_data_ptr), data.m_data_size);
        }

        u64 file_size = 0;
        Filesystem::file_size(abs_path).and_then([&file_size](u64 size) {
            file_size = size;
            return Result<u64, FSError>();
        });

        if (file_size == 0) {
            return make_fs_error<std::span<u8>>(std::error_code(), {"File size is 0"});
        }

        void *buffer = std::malloc(file_size);
        if (!buffer) {
            return make_fs_error<std::span<u8>>(std::error_code(),
                                                {"Failed to allocate memory for file buffer"});
        }

        std::fstream in(abs_path, std::ios::in | std::ios::binary);
        in.read(static_cast<char *>(buffer), file_size);
        in.close();

        m_data_preload_cache[abs_path] = {file_size, buffer};
        return std::span<u8>(static_cast<u8 *>(buffer), file_size);
    }

    Result<std::span<u8>, FSError>
    ResourceManager::getRawData(fs_path &&path, const UUID64 &resource_path_uuid) const {
        if (!hasDataPath(path, resource_path_uuid)) {
            return make_fs_error<std::span<u8>>(std::error_code(), {"Resource path not found"});
        }

        if (m_data_preload_cache.find(path) != m_data_preload_cache.end()) {
            const ResourceData &data = m_data_preload_cache[path];
            return std::span<u8>(static_cast<u8 *>(data.m_data_ptr), data.m_data_size);
        }

        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        fs_path abs_path      = resource_path / path;

        if (m_data_preload_cache.find(abs_path) != m_data_preload_cache.end()) {
            const ResourceData &data = m_data_preload_cache[abs_path];
            return std::span<u8>(static_cast<u8 *>(data.m_data_ptr), data.m_data_size);
        }

        u64 file_size = 0;
        Filesystem::file_size(abs_path).and_then([&file_size](u64 size) {
            file_size = size;
            return Result<u64, FSError>();
        });

        if (file_size == 0) {
            return make_fs_error<std::span<u8>>(std::error_code(), {"File size is 0"});
        }

        void *buffer = std::malloc(file_size);
        if (!buffer) {
            return make_fs_error<std::span<u8>>(std::error_code(),
                                                {"Failed to allocate memory for file buffer"});
        }

        std::fstream in(abs_path, std::ios::in | std::ios::binary);
        in.read(static_cast<char *>(buffer), file_size);
        in.close();

        m_data_preload_cache[abs_path] = {file_size, buffer};
        return std::span<u8>(static_cast<u8 *>(buffer), file_size);
    }

    ResourceManager::PathIterator ResourceManager::walkIterator(UUID64 resource_path_uuid) const {
        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        return PathIterator(resource_path);
    }

    ResourceManager::RecursivePathIterator
    ResourceManager::walkIteratorRecursive(UUID64 resource_path_uuid) const {
        fs_path resource_path = getResourcePath(resource_path_uuid).value_or(fs_path());
        return RecursivePathIterator(resource_path);
    }

    std::optional<fs_path> ResourceManager::getResourcePath(const UUID64 &path_uuid) const {
        for (const ResourcePath &resource_path : m_resource_paths) {
            if (resource_path.getUUID() == path_uuid) {
                return resource_path.getPath();
            }
        }
        return std::nullopt;
    }

    std::optional<fs_path> ResourceManager::findResourcePath(const fs_path &sub_path) const {
        std::optional<fs_path> path = std::nullopt;
        for (const ResourcePath &resource_path : m_resource_paths) {
            fs_path p = resource_path.getPath() / sub_path;
            Filesystem::is_directory(p).and_then([&path, &resource_path](bool is_directory) {
                if (is_directory) {
                    path = resource_path.getPath();
                }
                return Result<bool, FSError>();
            });

            if (path.has_value()) {
                break;
            }

            Filesystem::is_regular_file(p).and_then([&path, &resource_path](bool is_regular_file) {
                if (is_regular_file) {
                    path = resource_path.getPath();
                }
                return Result<bool, FSError>();
            });

            if (path.has_value()) {
                break;
            }
        }
        return path;
    }

    UUID64 ResourceManager::getResourcePathUUID(const fs_path &path) {
        fs_path norm_path = path;
        norm_path.make_preferred();
        if (!path.is_absolute()) {
            norm_path = Filesystem::weakly_canonical(norm_path).value_or(norm_path);
            return UUID64(std::hash<fs_path>{}(norm_path));
        }
        return UUID64(std::hash<fs_path>{}(norm_path));
    }

    UUID64 ResourceManager::getResourcePathUUID(fs_path &&path) {
        path.make_preferred();
        if (!path.is_absolute()) {
            path = Filesystem::weakly_canonical(path).value_or(path);
        }
        return UUID64(std::hash<fs_path>{}(path));
    }

    void ResourceManager::preloadData(const fs_path &resource_path) const {
        std::for_each(
            Filesystem::directory_iterator(resource_path), Filesystem::directory_iterator(),
            [this](const fs_path &file) {
                Filesystem::is_regular_file(file).and_then([this, &file](bool is_regular_file) {
                    if (!is_regular_file) {
                        return Result<bool, FSError>();
                    }

                    Filesystem::file_size(file).and_then([this, &file](u64 file_size) {
                        if (file_size == 0) {
                            return Result<u64, FSError>();
                        }

                        void *buffer = std::malloc(file_size);
                        if (!buffer) {
                            return make_fs_error<uintmax_t>(
                                std::error_code(), {"Failed to allocate memory for file buffer"});
                        }

                        std::fstream in(file, std::ios::in | std::ios::binary);
                        in.read(static_cast<char *>(buffer), file_size);
                        in.close();

                        m_data_preload_cache[file] = {file_size, buffer};

                        return Result<uintmax_t, FSError>();
                    });

                    return Result<bool, FSError>();
                });
            });
    }

    ResourcePath::ResourcePath(const fs_path &path, const UUID64 &uuid) {
        m_path = path;
        m_uuid = uuid ? uuid : ResourceManager::getResourcePathUUID(path);
    }

}  // namespace Toolbox
