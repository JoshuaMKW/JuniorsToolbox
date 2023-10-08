#include "rarc/rarc.hpp"
#include "serial.hpp"
#include "szs/szs.hpp"
#include <fstream>
#include <iostream>

// INTERNAL //

namespace Toolbox::RARC {

    struct MetaHeader {
        u32 magic;
        u32 size;
        struct {
            u32 offset;
        } nodes;
        struct {
            u32 offset;
            u32 size;
            u32 mram_size;
            u32 aram_size;
            u32 dvd_size;
        } files;
    };

    struct NodeHeader {
        struct {
            u32 count;
            u32 offset;
        } dir_nodes;
        struct {
            u32 count;
            u32 offset;
        } fs_nodes;
        struct {
            u32 size;
            u32 offset;
        } string_table;
        u16 ids_max;
        bool ids_synced;
    };

    struct DirectoryNode {
        u32 magic;
        u32 name;
        u16 hash;
        u16 children_count;
        u32 children_offset;
    };

    struct FSNode {
        u16 id;
        u16 hash;
        u16 type;
        u16 name;
        union {
            struct {
                s32 dir_node;
                u32 size;  // Always 0x10
            } folder;
            struct {
                u32 offset;
                u32 size;
            } file;
        };
    };

    struct LowResourceArchive {
        MetaHeader header;
        std::vector<DirectoryNode> dir_nodes;
        std::vector<FSNode> fs_nodes;
        std::vector<char> string_data;  // One giant buffer
        std::vector<char> file_data;    // One giant buffer
    };

    static bool isLowNodeFolder(const FSNode &node) {
        return node.type & (ResourceAttribute::DIRECTORY << 8);
    }
    static bool isSpecialPath(std::string_view name) { return name == "." || name == ".."; }

    static std::expected<std::unique_ptr<LowResourceArchive>, SerialError>
    loadLowResourceArchive(Deserializer &in);

    static void recurseLoadDirectory(ResourceArchive &arc, LowResourceArchive &low,
                                     DirectoryNode &low_dir, std::optional<FSNode> low_node,
                                     std::optional<ResourceArchive::Node> dir_parent,
                                     std::vector<ResourceArchive::Node> &out, std::size_t depth);

    std::expected<void, SerialError> ResourceArchive::deserialize(Deserializer &in) {
        auto result = loadLowResourceArchive(in);
        if (!result) {
            return std::unexpected(result.error());
        }

        auto low_archive = std::move(result.value());

        recurseLoadDirectory(*this, *low_archive, low_archive->dir_nodes[0], std::nullopt,
                             std::nullopt, m_nodes, 0);

        return {};
    }

    static std::expected<std::unique_ptr<LowResourceArchive>, SerialError>
    loadLowResourceArchive(Deserializer &in) {
        auto low_archive = std::make_unique<LowResourceArchive>();

        // Metaheader
        {
            low_archive->header.magic = in.read<u32, std::endian::big>();
            if (low_archive->header.magic != 'RARC') {
                return make_serial_error<std::unique_ptr<LowResourceArchive>>(
                    in, "Invalid magic (Expected RARC)", -4);
            }

            low_archive->header.size = in.read<u32, std::endian::big>();
            if (low_archive->header.size != in.size()) {
                return make_serial_error<std::unique_ptr<LowResourceArchive>>(
                    in, "Invalid size (Stream size doesn't match)", -4);
            }

            low_archive->header.nodes.offset = in.read<u32, std::endian::big>();

            low_archive->header.files.offset    = in.read<u32, std::endian::big>();
            low_archive->header.files.size      = in.read<u32, std::endian::big>();
            low_archive->header.files.mram_size = in.read<u32, std::endian::big>();
            low_archive->header.files.aram_size = in.read<u32, std::endian::big>();
            low_archive->header.files.dvd_size  = in.read<u32, std::endian::big>();
        }

        // Nodeheader
        NodeHeader node_header{};
        {
            in.seek(low_archive->header.nodes.offset, std::ios::beg);

            node_header.dir_nodes.count  = in.read<u32, std::endian::big>();
            node_header.dir_nodes.offset = in.read<u32, std::endian::big>();

            node_header.fs_nodes.count  = in.read<u32, std::endian::big>();
            node_header.fs_nodes.offset = in.read<u32, std::endian::big>();

            node_header.string_table.size   = in.read<u32, std::endian::big>();
            node_header.string_table.offset = in.read<u32, std::endian::big>();

            node_header.ids_max    = in.read<u16, std::endian::big>();
            node_header.ids_synced = in.read<bool>();
        }

        // Dir nodes
        {
            in.seek(low_archive->header.nodes.offset + node_header.dir_nodes.offset, std::ios::beg);

            for (size_t i = 0; i < node_header.dir_nodes.count; ++i) {
                DirectoryNode dir_node;
                {
                    dir_node.magic           = in.read<u32, std::endian::big>();
                    dir_node.name            = in.read<u32, std::endian::big>();
                    dir_node.hash            = in.read<u16, std::endian::big>();
                    dir_node.children_count  = in.read<u16, std::endian::big>();
                    dir_node.children_offset = in.read<u32, std::endian::big>();
                }
                low_archive->dir_nodes.push_back(dir_node);
            }
        }

        // FS nodes
        {
            in.seek(low_archive->header.nodes.offset + node_header.fs_nodes.offset, std::ios::beg);

            for (size_t i = 0; i < node_header.fs_nodes.count; ++i) {
                FSNode fs_node;
                {
                    fs_node.id          = in.read<u16, std::endian::big>();
                    fs_node.hash        = in.read<u16, std::endian::big>();
                    fs_node.type        = in.read<u16, std::endian::big>();
                    fs_node.name        = in.read<u16, std::endian::big>();
                    fs_node.file.offset = in.read<u32, std::endian::big>();
                    fs_node.file.size   = in.read<u32, std::endian::big>();
                }
                low_archive->fs_nodes.push_back(fs_node);
                in.seek(4);  // Skip padding
            }
        }

        // String table
        low_archive->string_data.resize(node_header.string_table.size);
        {
            in.seek(low_archive->header.nodes.offset + node_header.string_table.offset,
                    std::ios::beg);
            in.readBytes(low_archive->string_data);
        }

        low_archive->file_data.resize(low_archive->header.files.size);
        {
            in.seek(low_archive->header.nodes.offset + low_archive->header.files.offset,
                    std::ios::beg);
            in.readBytes(low_archive->file_data);
        }

        return low_archive;
    }

    static void recurseLoadDirectory(ResourceArchive &arc, LowResourceArchive &low,
                                     DirectoryNode &low_dir, std::optional<FSNode> low_node,
                                     std::optional<ResourceArchive::Node> dir_parent,
                                     std::vector<ResourceArchive::Node> &out, std::size_t depth) {}

}  // namespace Toolbox::RARC
