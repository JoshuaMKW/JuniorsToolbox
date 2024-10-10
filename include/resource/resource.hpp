#pragma once

#include <fstream>
#include <string>
#include <unordered_map>

#include "core/error.hpp"
#include "fsystem.hpp"
#include "image/imagehandle.hpp"
#include "serial.hpp"
#include "unique.hpp"

namespace Toolbox {

    class ResourcePath : public IUnique {
    public:
        ResourcePath()                     = default;
        ResourcePath(const ResourcePath &) = default;
        ResourcePath(ResourcePath &&)      = default;

        ResourcePath(const fs_path &path, const UUID64 &uuid = 0);

        [[nodiscard]] fs_path getPath() const { return m_path; }
        void setPath(const fs_path &path) { m_path = path; }
        void setPath(fs_path &&path) { m_path = std::move(path); }

        [[nodiscard]] UUID64 getUUID() const { return m_uuid; }

        ResourcePath &operator=(const ResourcePath &other) {
            m_path = other.m_path;
            m_uuid = other.m_uuid;
            return *this;
        }

    private:
        UUID64 m_uuid;
        fs_path m_path;
    };

    struct ResourceData {
        size_t m_data_size;
        void *m_data_ptr;
    };

    class ResourceManager : public IUnique {
    public:
        ResourceManager()                        = default;
        ResourceManager(const ResourceManager &) = default;
        ResourceManager(ResourceManager &&)      = default;

        ~ResourceManager();

        [[nodiscard]] UUID64 getUUID() const { return m_uuid; }

        static UUID64 getResourcePathUUID(const fs_path &path);
        static UUID64 getResourcePathUUID(fs_path &&path);

        const std::vector<ResourcePath> &getResourcePaths() const & { return m_resource_paths; }
        std::optional<fs_path> getResourcePath(const UUID64 &path_uuid) const;

        void includeResourcePath(const fs_path &path, bool preload_files);
        void includeResourcePath(fs_path &&path, bool preload_files);

        void removeResourcePath(const fs_path &path);
        void removeResourcePath(fs_path &&path);
        void removeResourcePath(const UUID64 &path_uuid);

        [[nodiscard]] bool hasResourcePath(const fs_path &path) const;
        [[nodiscard]] bool hasResourcePath(fs_path &&path) const;
        [[nodiscard]] bool hasResourcePath(const UUID64 &path_uuid) const;

        [[nodiscard]] bool hasDataPath(const fs_path &path,
                                       const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] bool hasDataPath(fs_path &&path, const UUID64 &resource_path_uuid = 0) const;

        [[nodiscard]] Result<RefPtr<const ImageData>, FSError>
        getImageData(const fs_path &path, const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] Result<RefPtr<const ImageData>, FSError>
        getImageData(fs_path &&path, const UUID64 &resource_path_uuid = 0) const;

        [[nodiscard]] Result<RefPtr<const ImageHandle>, FSError>
        getImageHandle(const fs_path &path, const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] Result<RefPtr<const ImageHandle>, FSError>
        getImageHandle(fs_path &&path, const UUID64 &resource_path_uuid = 0) const;

        [[nodiscard]] Result<void, FSError>
        getSerialData(std::ifstream &in, const fs_path &path,
                      const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] Result<void, FSError>
        getSerialData(std::ifstream &in, fs_path &&path,
                      const UUID64 &resource_path_uuid = 0) const;

        [[nodiscard]] Result<std::span<u8>, FSError>
        getRawData(const fs_path &path, const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] Result<std::span<u8>, FSError>
        getRawData(fs_path &&path, const UUID64 &resource_path_uuid = 0) const;

        using PathIterator          = Filesystem::directory_iterator;
        using RecursivePathIterator = Filesystem::recursive_directory_iterator;

        PathIterator walkIterator(UUID64 resource_path_uuid) const;
        RecursivePathIterator walkIteratorRecursive(UUID64 resource_path_uuid) const;

    protected:
        std::optional<fs_path> findResourcePath(const fs_path &sub_path) const;

        void preloadData(const fs_path &resource_path) const;

    private:
        UUID64 m_uuid;
        std::vector<ResourcePath> m_resource_paths;

        mutable std::unordered_map<fs_path, RefPtr<const ImageHandle>> m_image_handle_cache;
        mutable std::unordered_map<fs_path, ResourceData> m_data_preload_cache;
    };

}  // namespace Toolbox