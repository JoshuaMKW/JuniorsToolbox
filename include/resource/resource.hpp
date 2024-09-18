#pragma once

#include <fstream>
#include <string>
#include <unordered_map>

#include "core/error.hpp"
#include "fsystem.hpp"
#include "serial.hpp"
#include "unique.hpp"

namespace Toolbox {

    class ResourcePath : public IUnique {
    public:
        ResourcePath()                     = default;
        ResourcePath(const ResourcePath &) = default;
        ResourcePath(ResourcePath &&)      = default;

        [[nodiscard]] fs_path getPath() const { return m_path; }
        void setPath(const fs_path &path) { m_path = path; }
        void setPath(fs_path &&path) { m_path = std::move(path); }

        [[nodiscard]] UUID64 getUUID() const { return m_uuid; }

    private:
        UUID64 m_uuid;
        fs_path m_path;
    };

    class ResourceManager : public IUnique {
    public:
        ResourceManager()                        = default;
        ResourceManager(const ResourceManager &) = default;
        ResourceManager(ResourceManager &&)      = default;

        ~ResourceManager();

        [[nodiscard]] UUID64 getUUID() const { return m_uuid; }

        void includePath(const fs_path &path);
        void includePath(fs_path &&path);

        void removePath(const fs_path &path);
        void removePath(fs_path &&path);

        [[nodiscard]] bool hasResourcePath(const fs_path &path) const;
        [[nodiscard]] bool hasResourcePath(fs_path &&path) const;
        [[nodiscard]] bool hasResourcePath(const UUID64 &path_uuid) const;

        [[nodiscard]] bool hasPath(const fs_path &path,
                                   const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] bool hasPath(fs_path &&path, const UUID64 &resource_path_uuid = 0) const;

        [[nodiscard]] Result<RefPtr<ImageHandle>, FSError>
        getImageHandle(const fs_path &path, const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] Result<RefPtr<ImageHandle>, FSError>
        getImageHandle(fs_path &&path, const UUID64 &resource_path_uuid = 0) const;

        [[nodiscard]] Result<Deserializer, FSError>
        getSerialData(const fs_path &path, const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] Result<Deserializer, FSError>
        getSerialData(fs_path &&path, const UUID64 &resource_path_uuid = 0) const;

        [[nodiscard]] Result<std::vector<u8>, FSError>
        getRawData(const fs_path &path, const UUID64 &resource_path_uuid = 0) const;
        [[nodiscard]] Result<std::vector<u8>, FSError>
        getRawData(fs_path &&path, const UUID64 &resource_path_uuid = 0) const;

    protected:
        std::optional<fs_path> getResourcePath(const UUID64 &path_uuid) const;
        UUID64 getResourcePathUUID(const fs_path &path) const;
        UUID64 getResourcePathUUID(fs_path &&path) const;

    private:
        UUID64 m_uuid;
        std::vector<ResourcePath> m_resource_paths;

        mutable std::unordered_map<fs_path, RefPtr<ImageHandle>> m_image_handle_cache;
    };

}  // namespace Toolbox