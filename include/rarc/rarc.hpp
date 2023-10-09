#pragma once

#include "clone.hpp"
#include "error.hpp"
#include "fsystem.hpp"
#include "serial.hpp"
#include <array>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace Toolbox::RARC {

    enum ResourceAttribute : u8 {
        FILE            = 1 << 0,
        DIRECTORY       = 1 << 1,
        COMPRESSED      = 1 << 2,
        PRELOAD_TO_MRAM = 1 << 4,
        PRELOAD_TO_ARAM = 1 << 5,
        LOAD_FROM_DVD   = 1 << 6,
        YAZ0_COMPRESSED = 1 << 7
    };

    struct ResourceArchive_ {
        struct FolderInfo {
            s32 parent;
            s32 sibling_next;

            bool operator==(const FolderInfo &rhs) const = default;
        };

        struct Node {
            s32 id;
            u16 flags;
            std::string name;

            FolderInfo folder;
            std::vector<u8> data;

            bool is_folder() const { return (flags & DIRECTORY) != 0; }

            bool operator==(const Node &rhs) const = default;
        };

        std::vector<Node> nodes;
    };

    class ResourceArchive : public ISerializable, public IClonable {
    public:
        struct FolderInfo {
            s32 parent;
            s32 sibling_next;

            bool operator==(const FolderInfo &rhs) const = default;
        };

        struct Node {
            s32 id;
            u16 flags;
            std::string name;

            FolderInfo folder;
            std::vector<char> data;

            bool is_folder() const { return (flags & DIRECTORY) != 0; }

            bool operator==(const Node &rhs) const = default;
        };

        using node_it       = std::vector<Node>::iterator;
        using const_node_it = std::vector<Node>::const_iterator;

    public:
        ResourceArchive() = delete;
        explicit ResourceArchive(std::string_view name) : m_name(name) {}
        ResourceArchive(std::string_view name, std::vector<Node> nodes)
            : m_name(name), m_nodes(nodes) {}
        ~ResourceArchive() = default;

        static bool isDataResourceArchive(Deserializer &in);
        static std::expected<ResourceArchive, FSError>
        createFromPath(const std::filesystem::path path);

        [[nodiscard]] bool isMatchingOutput() const { return m_keep_matching; }
        void setMatchingOutput(bool matching) { m_keep_matching = matching; }

        [[nodiscard]] std::string_view getName() const { return m_name; }
        [[nodiscard]] std::vector<Node> &getNodes() { return m_nodes; }
        [[nodiscard]] const std::vector<Node> &getNodes() const { return m_nodes; }

        [[nodiscard]] node_it begin() { return m_nodes.begin(); }
        [[nodiscard]] node_it end() { return m_nodes.end(); }
        [[nodiscard]] const_node_it begin() const { return m_nodes.begin(); }
        [[nodiscard]] const_node_it end() const { return m_nodes.end(); }

        [[nodiscard]] node_it findNode(std::string_view name);
        [[nodiscard]] const_node_it findNode(std::string_view name) const;
        [[nodiscard]] node_it findNode(s32 id);
        [[nodiscard]] const_node_it findNode(s32 id) const;
        [[nodiscard]] node_it findNode(const std::filesystem::path &path);
        [[nodiscard]] const_node_it findNode(const std::filesystem::path &path) const;

        std::expected<void, FSError> extractToPath(const std::filesystem::path &path) const;

        std::expected<void, FSError> importFiles(const std::vector<std::filesystem::path> &files,
                                                 node_it parent);
        std::expected<void, FSError> importFolder(const std::filesystem::path &folder,
                                                  node_it parent);

        std::expected<node_it, BaseError> createFolder(node_it parent, std::string_view name);
        std::expected<node_it, BaseError> createFile(node_it parent, std::string_view name,
                                                     std::span<const char> data);

        std::expected<void, BaseError> removeNodes(std::vector<Node> &nodes);
        std::expected<node_it, FSError> replaceNode(node_it old_node, const std::filesystem::path &path);

        std::expected<void, FSError> extractNodeToFolder(const_node_it node,
                                                         const std::filesystem::path &folder);

        void dump(std::ostream &out, size_t indention, size_t indention_width) const;
        void dump(std::ostream &out, size_t indention) const { dump(out, indention, 2); }
        void dump(std::ostream &out) const { dump(out, 0, 2); }

        std::expected<void, SerialError> serialize(Serializer &out) const override;
        std::expected<void, SerialError> deserialize(Deserializer &in) override;

        std::unique_ptr<IClonable> clone(bool deep) const override;

    protected:
        std::expected<void, BaseError> recalculateIDs();

    private:
        std::string m_name;
        std::vector<Node> m_nodes = {};

        bool m_ids_synced = true;
        bool m_keep_matching = true;
    };

    struct ResourceArchiveNodeHasher {
        std::size_t operator()(const ResourceArchive::Node &node) const {
            std::size_t h1 = std::hash<s32>{}(node.id);
            std::size_t h2 = std::hash<std::string>{}(node.name);
            std::size_t h3 = 0;
            if (node.is_folder()) {
                h3 = std::hash<s32>{}(node.folder.parent);
            }
            // Combine hashes - This is a common technique to combine hash values
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

}  // namespace Toolbox::RARC
