#include "rarc/rarc.hpp"
#include "objlib/nameref.hpp"
#include "serial.hpp"
#include "szs/szs.hpp"
#include <fstream>
#include <iostream>
#include <unordered_map>

using namespace Toolbox::Object;

// INTERNAL //

namespace Toolbox::RARC {

    // LOW LEVEL //

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
    static_assert(sizeof(MetaHeader) == 32);

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
        u8 padding[5];
    };

    static_assert(sizeof(NodeHeader) == 32);

    struct DirectoryNode {
        u32 magic;
        u32 name;
        u16 hash;
        u16 children_count;
        u32 children_offset;
    };

    static_assert(sizeof(DirectoryNode) == 16);

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
        u8 padding[4];
    };

    static_assert(sizeof(FSNode) == 20);

    struct LowResourceArchive {
        MetaHeader meta_header;
        NodeHeader node_header;
        std::vector<DirectoryNode> dir_nodes;
        std::vector<FSNode> fs_nodes;
        std::vector<char> string_data;  // One giant buffer
        std::vector<char> file_data;    // One giant buffer
    };

    static bool isLowNodeFolder(const FSNode &node) {
        return node.type & (ResourceAttribute::DIRECTORY << 8);
    }
    static bool isSpecialPath(std::string_view name) { return name == "." || name == ".."; }

    static Result<ScopePtr<LowResourceArchive>, SerialError>
    loadLowResourceArchive(Deserializer &in);

    static Result<void, SerialError>
    saveLowResourceArchive(const LowResourceArchive &low_archive, Serializer &out);

    // --------- //

    // MIDDLEWARE //

    using children_info_type = std::unordered_map<std::string, std::pair<std::size_t, std::size_t>>;

    struct SpecialDirs {
        ResourceArchive::Node self;
        ResourceArchive::Node parent;
    };

    struct DirAdjustmentInfo {
        int index;
        std::size_t adjustment;
    };

    static Result<SpecialDirs, BaseError>
    createSpecialDirs(const ResourceArchive::Node &node,
                      std::optional<ResourceArchive::Node> parent);

    static Result<void>
    insertSpecialDirs(std::vector<ResourceArchive::Node> &nodes);

    static void removeSpecialDirs(std::vector<ResourceArchive::Node> &nodes);

    static Result<void>
    sortNodesForSaveRecursive(const std::vector<ResourceArchive::Node> &src,
                              std::vector<ResourceArchive::Node> &out,
                              children_info_type &children_info, std::size_t node_index);

    static Result<std::vector<ResourceArchive::Node>, BaseError>
    processNodesForSave(const std::vector<ResourceArchive::Node> &nodes,
                        children_info_type &children_info);

    static void recurseLoadDirectory(LowResourceArchive &low, DirectoryNode &low_dir,
                                     std::optional<FSNode> low_node,
                                     std::optional<ResourceArchive::Node> dir_parent,
                                     std::vector<ResourceArchive::Node> &out, std::size_t depth);

    static void getSortedDirectoryListR(const std::filesystem::path &path,
                                        std::vector<std::filesystem::path> &out);

    // ---------- //

    bool ResourceArchive::isMagicValid(u32 magic) { return magic == 'RARC'; }

    Result<ResourceArchive, FSError>
    ResourceArchive::createFromPath(const std::filesystem::path root) {
        struct Entry {
            std::string str;
            bool is_folder = false;

            std::vector<char> data;
            std::string name;

            s32 depth       = -1;
            s32 parent      = 0;  // Default files put parent as 0 for root
            s32 siblingNext = -1;
        };
        std::vector<Entry> paths;

        std::error_code err;

        paths.push_back(Entry{.str       = (std::filesystem::path(".") / root.filename()).string(),
                              .is_folder = true,
                              .name      = root.filename().string(),
                              .parent    = -1});

        std::vector<std::filesystem::path> sorted_fs_tree;
        getSortedDirectoryListR(root, sorted_fs_tree);

        for (auto &path : sorted_fs_tree) {
            auto dir_result = Toolbox::Filesystem::is_directory(path);
            if (!dir_result) {
                return std::unexpected(dir_result.error());
            }
            bool folder = dir_result.value();
            if (err) {
                std::cout << std::format("CREATE: {}\n", err.message().c_str());
                continue;
            }
            if (path.filename() == ".DS_Store") {
                continue;
            }
            std::vector<char> data;
            if (!folder) {
                auto fstrm = std::ifstream(path, std::ios::binary | std::ios::in);
                fstrm.read(data.data(), data.size());
            }

            auto path_result = Toolbox::Filesystem::relative(path, root);
            if (!path_result) {
                return std::unexpected(path_result.error());
            }
            path = std::filesystem::path(".") / path_result.value();

            // We normalize it to lowercase because otherwise games can't find it.
            {
                std::string path_string = path.string();
                std::transform(path_string.begin(), path_string.end(), path_string.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                path = std::filesystem::path(path_string);
            }

            paths.push_back(Entry{
                .str       = path.string(),
                .is_folder = folder,
                .data      = data,
                .name      = path.filename().string(),
            });
        }
        // They are almost certainly already in sorted order
        for (auto &p : paths) {
            for (auto c : p.str) {
                // relative paths are weakly_canonical
                if (c == '/' || c == '\\') {
                    ++p.depth;
                }
            }
        }
        paths[0].siblingNext = static_cast<s32>(paths.size());
        for (size_t i = 1; i < paths.size(); ++i) {
            auto &p = paths[i];
            if (p.is_folder) {
                for (size_t j = i + 1; j < paths.size(); ++j) {
                    if (paths[j].depth <= p.depth) {
                        p.siblingNext = static_cast<s32>(j);
                        break;
                    }
                    if (paths[j].depth == p.depth + 1)
                        paths[j].parent = static_cast<s32>(i);
                }
                if (p.siblingNext == -1)
                    p.siblingNext = static_cast<s32>(paths.size());
            }
        }
        for (auto &p : paths) {
            std::cout << std::format("CREATE: PATH={} (folder:{}, depth:{})\n", p.str, p.is_folder,
                                     p.depth);
        }
        ResourceArchive result;

        s16 file_id = 0, folder_id = 0;
        std::vector<ResourceArchive::Node> parent_stack;
        for (auto &p : paths) {
            ResourceArchive::Node node{.flags = 0};
            while (p.depth < parent_stack.size() && parent_stack.size() > 0) {
                parent_stack.pop_back();
            }
            if (p.is_folder) {
                node.id            = folder_id++;
                node.folder.parent = parent_stack.size() > 0 ? parent_stack[p.depth - 1].id : -1;
                node.folder.sibling_next = p.siblingNext;
                if (p.depth >= parent_stack.size()) {
                    parent_stack.push_back(node);
                }
                node.flags |= ResourceAttribute::DIRECTORY;
                file_id += 2;  // Account for hidden special dirs
            } else {
                node.id = file_id++;
                node.flags |= ResourceAttribute::FILE;
                node.flags |= ResourceAttribute::PRELOAD_TO_MRAM;

                // TODO: Take YAY0 into account when it becomes supported
                /*if (librii::szs::isDataYaz0Compressed(
                        rsl::byte_view(p.data.data(), p.data.size()))) {
                    node.flags |= ResourceAttribute::COMPRESSED;
                    node.flags |= ResourceAttribute::YAZ0_COMPRESSED;
                }*/

                node.data = p.data;
            }
            node.name = p.name;
            result.m_nodes.push_back(node);
        }

        if (!result.recalculateIDs()) {
            return make_fs_error<ResourceArchive>(std::error_code(),
                                                  {"Failed to calculate the IDs"});
        }

        return result;
    }

    ResourceArchive::node_it ResourceArchive::findNode(std::string_view name) {
        return std::find_if(m_nodes.begin(), m_nodes.end(),
                            [&](const Node &node) { return node.name == name; });
    }

    ResourceArchive::const_node_it ResourceArchive::findNode(std::string_view name) const {
        return std::find_if(m_nodes.begin(), m_nodes.end(),
                            [&](const Node &node) { return node.name == name; });
    }

    ResourceArchive::node_it ResourceArchive::findNode(s32 id) {
        return std::find_if(m_nodes.begin(), m_nodes.end(),
                            [&](const Node &node) { return node.id == id; });
    }

    ResourceArchive::const_node_it ResourceArchive::findNode(s32 id) const {
        return std::find_if(m_nodes.begin(), m_nodes.end(),
                            [&](const Node &node) { return node.id == id; });
    }

    ResourceArchive::node_it ResourceArchive::findNode(const std::filesystem::path &path) {
        if (path.empty() || m_nodes.empty()) {
            return m_nodes.end();
        }

        std::vector<std::string> paths;

        std::filesystem::path it_path = path;
        while (!it_path.empty()) {
            paths.push_back(it_path.filename().string());
            it_path = path.parent_path();
        }

        QualifiedName qual_path = QualifiedName(paths.rbegin(), paths.rend());

        auto insert_index    = m_nodes.size();
        auto dir_files_start = m_nodes.begin();
        auto dir_files_end   = m_nodes.end();

        auto current_node = dir_files_start;
        while (dir_files_start != dir_files_end) {
            if (qual_path[0] != current_node->name) {
                if (current_node->is_folder())
                    current_node = dir_files_start + current_node->folder.sibling_next;
                else
                    current_node++;
                continue;
            }
            if (qual_path.depth() > 1) {
                if (!current_node->is_folder())
                    return m_nodes.end();
                current_node++;
                continue;
            }
            return current_node;
        }

        return m_nodes.end();
    }

    ResourceArchive::const_node_it
    ResourceArchive::findNode(const std::filesystem::path &path) const {
        if (path.empty() || m_nodes.empty()) {
            return m_nodes.end();
        }

        std::vector<std::string> paths;

        std::filesystem::path it_path = path;
        while (!it_path.empty()) {
            paths.push_back(it_path.filename().string());
            it_path = path.parent_path();
        }

        QualifiedName qual_path = QualifiedName(paths.rbegin(), paths.rend());

        auto insert_index    = m_nodes.size();
        auto dir_files_start = m_nodes.begin();
        auto dir_files_end   = m_nodes.end();

        auto current_node = dir_files_start;
        while (dir_files_start != dir_files_end) {
            if (qual_path[0] != current_node->name) {
                if (current_node->is_folder())
                    current_node = dir_files_start + current_node->folder.sibling_next;
                else
                    current_node++;
                continue;
            }
            if (qual_path.depth() > 1) {
                if (!current_node->is_folder())
                    return m_nodes.end();
                current_node++;
                continue;
            }
            return current_node;
        }

        return m_nodes.end();
    }

    Result<void, FSError>
    ResourceArchive::extractToPath(const std::filesystem::path &path) const {
        return Result<void, FSError>();
    }

    Result<void, FSError>
    ResourceArchive::importFiles(const std::vector<std::filesystem::path> &files, node_it parent) {
        if (files.size() == 0)
            return {};

        std::vector<ResourceArchive::Node> new_nodes;
        for (auto &file : files) {
            std::string file_name = file.filename().string();
            std::transform(file_name.begin(), file_name.end(), file_name.begin(),
                           [](char c) { return std::tolower(c); });

            auto result = Toolbox::Filesystem::file_size(file);
            if (!result) {
                return std::unexpected(result.error());
            }

            std::ifstream fin = std::ifstream(file, std::ios::binary | std::ios::in);
            std::vector<char> fdata(result.value());

            fin.read(fdata.data(), fdata.size());

            ResourceArchive::Node node = {.id    = 0,
                                          .flags = ResourceAttribute::FILE |
                                                   ResourceAttribute::PRELOAD_TO_MRAM,
                                          .name = file_name,
                                          .data = fdata};
            new_nodes.push_back(node);
        }

        auto parent_index = std::distance(m_nodes.begin(), parent);

        // Manual sort is probably better because we can dodge DFS nonsense
        // effectively Even with O(n^2) because there typically aren't many nodes
        // anyway.
        for (auto new_node = new_nodes.begin(); new_node != new_nodes.end(); new_node++) {
            auto dir_files_start = m_nodes.begin() + parent_index + 1;
            auto dir_files_end   = m_nodes.begin() + parent->folder.sibling_next +
                                 std::distance(new_nodes.begin(), new_node);
            for (auto child = dir_files_start; /*nothing*/; child++) {
                // Always insert at the very end (alphabetically last)
                if (child == dir_files_end) {
                    m_nodes.insert(child, *new_node);
                    break;
                }
                if (child->is_folder()) {
                    // Always insert before the regular dirs
                    m_nodes.insert(child, *new_node);
                    break;
                }
                if (new_node->name < child->name) {
                    m_nodes.insert(child, *new_node);
                    break;
                }
            }
        }

        for (auto &node : m_nodes) {
            if (!node.is_folder())
                continue;
            if (node.folder.sibling_next <= parent_index)
                continue;
            node.folder.sibling_next += static_cast<s32>(files.size());
        }

        return {};
    }

    Result<void, FSError> ResourceArchive::importFolder(const std::filesystem::path &folder,
                                                               node_it parent) {
        // Generate an archive so we can steal the DFS structure.
        auto rarc_result = createFromPath(folder);
        if (!rarc_result) {
            return std::unexpected(rarc_result.error());
        }

        auto tmp_rarc = rarc_result.value();

        auto parent_index = std::distance(m_nodes.begin(), parent);

        s32 insert_index     = static_cast<s32>(m_nodes.size());
        auto dir_files_start = m_nodes.begin() + parent_index + 1;
        auto dir_files_end   = m_nodes.begin() + parent->folder.sibling_next;
        for (auto child = dir_files_start; /*nothing*/;) {
            if (child == dir_files_end) {
                // Always insert at the very end (alphabetically last)
                insert_index = static_cast<s32>(std::distance(m_nodes.begin(), child));
                for (auto &new_node : tmp_rarc.getNodes()) {
                    if (new_node.is_folder())
                        new_node.folder.sibling_next += insert_index;
                }
                m_nodes.insert(child, tmp_rarc.m_nodes.begin(), tmp_rarc.m_nodes.end());
                break;
            }

            // Never insert in the files
            if (!child->is_folder()) {
                child++;
                continue;
            }

            // We are attempting to insert the entire
            // RARC fs as a new folder here
            if (tmp_rarc.m_nodes[0].name < child->name) {
                insert_index = static_cast<s32>(std::distance(m_nodes.begin(), child));
                for (auto &new_node : tmp_rarc.m_nodes) {
                    if (new_node.is_folder())
                        new_node.folder.sibling_next += insert_index;
                }
                m_nodes.insert(child, tmp_rarc.m_nodes.begin(), tmp_rarc.m_nodes.end());
                break;
            }

            // Skip to the next dir (DFS means we jump subnodes)
            child = m_nodes.begin() + child->folder.sibling_next;
        }

        for (auto node = m_nodes.begin(); node != m_nodes.end();) {
            s32 index = static_cast<s32>(std::distance(m_nodes.begin(), node));
            // Skip files and the special dirs (they get recalculated later)
            if (!node->is_folder()) {
                node++;
                continue;
            }
            if (index == insert_index) {  // Skip inserted dir (already adjusted)
                node = m_nodes.begin() + node->folder.sibling_next;
                continue;
            }
            if (node->folder.sibling_next <
                insert_index) {  // Skip dirs that come before insertion by span alone
                node = m_nodes.begin() + node->folder.sibling_next;
                continue;
            }
            // This is a directory adjacent but not beyond the insertion, ignore.
            if (node->folder.parent >= parent->id && index < insert_index) {
                node = m_nodes.begin() + node->folder.sibling_next;
                continue;
            }
            node->folder.sibling_next += static_cast<s32>(tmp_rarc.m_nodes.size());
            node++;
        }

        return {};
    }

    Result<ResourceArchive::node_it, BaseError>
    ResourceArchive::createFolder(node_it parent, std::string_view name) {
        auto parent_index = std::distance(m_nodes.begin(), parent);

        std::string lower_name;
        std::transform(name.begin(), name.end(), lower_name.begin(),
                       [](char c) { return std::tolower(c); });

        ResourceArchive::Node folder_node = {
            .id     = 0,
            .flags  = ResourceAttribute::DIRECTORY,
            .name   = lower_name,
            .folder = {.parent = parent->id, .sibling_next = 0}
        };

        s32 insert_index     = static_cast<s32>(m_nodes.size());
        auto dir_files_start = m_nodes.begin() + parent_index + 1;
        auto dir_files_end   = m_nodes.begin() + parent->folder.sibling_next;
        for (auto child = dir_files_start; /*nothing*/;) {
            if (child == dir_files_end) {
                // Always insert at the very end (alphabetically last)
                insert_index = static_cast<s32>(std::distance(m_nodes.begin(), child));
                folder_node.folder.sibling_next = insert_index + 1;
                m_nodes.insert(child, folder_node);
                break;
            }

            // Never insert in the files
            if (!child->is_folder()) {
                child++;
                continue;
            }

            // We are attempting to insert the entire
            // RARC fs as a new folder here
            if (name < child->name) {
                insert_index = static_cast<s32>(std::distance(m_nodes.begin(), child));
                folder_node.folder.sibling_next = insert_index + 1;
                m_nodes.insert(child, folder_node);
                break;
            }

            // Skip to the next dir (DFS means we jump subnodes)
            child = m_nodes.begin() + child->folder.sibling_next;
        }

        for (auto node = m_nodes.begin(); node != m_nodes.end();) {
            auto index = std::distance(m_nodes.begin(), node);
            // Skip files (they get recalculated later)
            if (!node->is_folder()) {
                node++;
                continue;
            }
            if (index == insert_index) {  // Skip inserted dir (already adjusted)
                node = m_nodes.begin() + node->folder.sibling_next;
                continue;
            }
            if (node->folder.sibling_next <
                insert_index) {  // Skip dirs that come before insertion by span alone
                node = m_nodes.begin() + node->folder.sibling_next;
                continue;
            }
            // This is a directory adjacent but not beyond the insertion, ignore.
            if (node->folder.parent >= parent->id && index < insert_index) {
                node = m_nodes.begin() + node->folder.sibling_next;
                continue;
            }
            node->folder.sibling_next += 1;
            node++;
        }

        return {};
    }

    Result<ResourceArchive::node_it, BaseError>
    ResourceArchive::createFile(node_it parent, std::string_view name, std::span<const char> data) {
        return Result<node_it, BaseError>();
    }

    Result<void> ResourceArchive::removeNodes(std::vector<Node> &nodes) {
        if (nodes.size() == 0)
            return {};

        for (auto &to_delete : nodes) {
            for (int i = 0; i < m_nodes.size(); i++) {
                if (to_delete == m_nodes[i]) {
                    if (!to_delete.is_folder()) {
                        ResourceArchive::Node parent;
                        int parent_index = 0;
                        for (parent_index = i; parent_index >= 0; parent_index--) {
                            parent = m_nodes[parent_index];
                            if (!parent.is_folder())
                                continue;
                            // This folder should be the deepest parent
                            if (parent.folder.sibling_next > i)
                                break;
                        }
                        auto deleted_at =
                            std::distance(m_nodes.begin(), m_nodes.erase(m_nodes.begin() + i));

                        for (auto &node : m_nodes) {
                            if (!node.is_folder())
                                continue;
                            if (node.folder.sibling_next <= deleted_at)
                                continue;
                            node.folder.sibling_next--;
                        }
                    } else {
                        auto begin = m_nodes.begin() + i;
                        auto end   = m_nodes.begin() + to_delete.folder.sibling_next;
                        auto size  = std::distance(begin, end);

                        auto deleted_at = std::distance(m_nodes.begin(), m_nodes.erase(begin, end));

                        for (auto &node : m_nodes) {
                            if (!node.is_folder())
                                continue;
                            if (node.folder.sibling_next <= deleted_at)
                                continue;
                            if (node.folder.parent > to_delete.id)
                                node.folder.parent -= 1;
                            node.folder.sibling_next -= static_cast<s32>(size);
                        }
                    }
                }
            }
        }

        return {};
    }

    Result<ResourceArchive::node_it, FSError>
    ResourceArchive::replaceNode(node_it old_node, const std::filesystem::path &path) {
        {
            auto result = Toolbox::Filesystem::exists(path);
            if (!result)
                return std::unexpected(result.error());
            if (!result.value())
                return make_fs_error<node_it>(std::error_code(), {"REPLACE: Path does not exist."});
        }

        if (old_node == m_nodes.end())
            return make_fs_error<node_it>(std::error_code(), {"REPLACE: Target node not found!"});

        if (old_node->is_folder()) {
            {
                auto result = Toolbox::Filesystem::is_directory(path);
                if (!result)
                    return std::unexpected(result.error());
                if (!result.value())
                    return make_fs_error<node_it>(std::error_code(), {"REPLACE: Not a directory!"});
            }

            auto begin    = old_node;
            auto end      = m_nodes.begin() + old_node->folder.sibling_next;
            auto old_size = std::distance(begin, end);

            auto deleted_at = std::distance(m_nodes.begin(), m_nodes.erase(begin, end));

            // Generate an archive so we can steal the DFS structure.
            auto rarc_result = createFromPath(path);
            if (!rarc_result) {
                return std::unexpected(rarc_result.error());
            }

            auto tmp_rarc = rarc_result.value();

            tmp_rarc.m_nodes[0].name = old_node->name;
            tmp_rarc.m_nodes[0].folder.parent =
                old_node->folder.parent;  // This repairs the insertion node which will
                                          // recursively regenerate later

            for (auto &new_node : tmp_rarc.m_nodes) {
                if (new_node.is_folder())
                    new_node.folder.sibling_next += static_cast<s32>(deleted_at);
            }
            m_nodes.insert(m_nodes.begin() + deleted_at, tmp_rarc.m_nodes.begin(),
                           tmp_rarc.m_nodes.end());

            auto parent =
                std::find_if(m_nodes.begin(), m_nodes.end(), [&](ResourceArchive::Node node) {
                    return node.id == old_node->folder.parent;
                });

            s32 sibling_adjust = static_cast<s32>(tmp_rarc.m_nodes.size() - old_size);
            if (sibling_adjust != 0) {
                for (auto node = m_nodes.begin(); node != m_nodes.end();) {
                    auto index = std::distance(m_nodes.begin(), node);
                    // Skip files and the special dirs (they get recalculated later)
                    if (!node->is_folder()) {
                        node++;
                        continue;
                    }
                    if (index == deleted_at) {  // Skip inserted dir (already adjusted)
                        node = m_nodes.begin() + node->folder.sibling_next;
                        continue;
                    }
                    if (node->folder.sibling_next < deleted_at) {  // Skip dirs that come before
                                                                   // insertion by span alone
                        node = m_nodes.begin() + node->folder.sibling_next;
                        continue;
                    }
                    // This is a directory adjacent but not beyond the insertion, ignore.
                    if (node->folder.parent >= parent->id && index < deleted_at) {
                        node = m_nodes.begin() + node->folder.sibling_next;
                        continue;
                    }
                    node->folder.sibling_next += sibling_adjust;
                    node++;
                }
            }

            return {};
        }

        {
            auto result = Toolbox::Filesystem::is_regular_file(path);
            if (!result)
                return std::unexpected(result.error());
            if (!result.value())
                return make_fs_error<node_it>(std::error_code(), {"REPLACE: Not a file!"});
        }

        auto in = std::ifstream(path.string(), std::ios::binary | std::ios::in);

        auto size_result = Toolbox::Filesystem::file_size(path);
        if (!size_result) {
            return make_fs_error<node_it>(std::error_code(),
                                          {"REPLACE: Failed to fetch file size"});
        }
        auto fsize = size_result.value();

        old_node->data.resize(fsize);
        in.read((char *)old_node->data.data(), fsize);

        return {};
    }

    Result<void, FSError>
    ResourceArchive::extractNodeToFolder(const_node_it node_it,
                                         const std::filesystem::path &folder) {
        if (node_it == m_nodes.end())
            return make_fs_error<void>(std::error_code(), {"EXTRACT: Target node not found!"});

        {
            auto result = Toolbox::Filesystem::is_directory(folder);
            if (!result)
                return std::unexpected(result.error());
            if (!result.value())
                return make_fs_error<void>(std::error_code(), {"EXTRACT: Not a directory!"});
        }

        if (node_it->is_folder()) {
            auto tmp_rarc = ResourceArchive(m_name);

            s32 before_size  = static_cast<s32>(std::distance(m_nodes.cbegin(), node_it));
            auto parent_diff = node_it->id;

            tmp_rarc.m_nodes.insert(tmp_rarc.m_nodes.end(), node_it,
                                    m_nodes.cbegin() + node_it->folder.sibling_next);

            tmp_rarc.m_nodes[0].folder = {-1,
                                          tmp_rarc.m_nodes[0].folder.sibling_next - before_size};

            tmp_rarc.m_nodes[1].folder = {-1, tmp_rarc.m_nodes[0].folder.sibling_next};

            tmp_rarc.m_nodes[2].folder = {-1, 0};
            for (auto tnode = tmp_rarc.m_nodes.begin() + 3; tnode != tmp_rarc.m_nodes.end();
                 tnode++) {
                if (tnode->is_folder()) {
                    tnode->folder.parent -= parent_diff;
                    tnode->folder.sibling_next -= before_size;
                }
            }

            {
                auto result = tmp_rarc.recalculateIDs();
                if (!result) {
                    return make_fs_error<void>(std::error_code(),
                                               {"EXTRACT: Failed to recalculate IDs!"});
                }
            }

            {
                auto result = tmp_rarc.extractToPath(folder);
                if (!result) {
                    return std::unexpected(result.error());
                }
            }

            return {};
        }

        auto dst_path = folder / node_it->name;
        auto out      = std::ofstream(dst_path.string(), std::ios::binary | std::ios::ate);

        out.write((const char *)node_it->data.data(), node_it->data.size());
        return {};
    }

    void ResourceArchive::dump(std::ostream &out, size_t indention, size_t indention_width) const {}

    Result<void, SerialError> ResourceArchive::serialize(Serializer &out) const {
        struct OffsetInfo {
            std::size_t string_offset;
            std::size_t fs_node_offset;
        };
        std::unordered_map<std::string, OffsetInfo> offsets_map;

        // Used for generated fs_node and name data, the dir table is DFS order
        // but relies on this data too, hence children_infos
        children_info_type children_infos;
        auto result = processNodesForSave(m_nodes, children_infos);
        if (!result) {
            return make_serial_error<void>(out, "Nodes failed to process for saving");
        }
        auto processed_nodes = result.value();

        std::string strings_blob;
        {
            offsets_map["."]  = {0, 0};
            offsets_map[".."] = {2, 0};
            strings_blob += "." + std::string("\0", 1) + ".." + std::string("\0", 1);
            std::size_t fs_info_offset = 0;
            for (auto &node : m_nodes) {
                if (m_keep_matching) {
                    offsets_map[node.name + std::to_string(node.id)] = {strings_blob.size(),
                                                                        fs_info_offset};
                    strings_blob += node.name + std::string("\0", 1);
                } else {
                    if (!offsets_map.contains(node.name)) {
                        offsets_map[node.name] = {strings_blob.size(), fs_info_offset};
                        strings_blob += node.name + std::string("\0", 1);
                    }
                }
                fs_info_offset += sizeof(FSNode);
            }
        }

        std::vector<DirectoryNode> dir_nodes;
        std::vector<FSNode> fs_nodes;

        std::size_t mram_size = 0, aram_size = 0, dvd_size = 0;
        std::size_t current_child_offset = 0;

        // Directories follow DFS
        for (auto &node : m_nodes) {
            OffsetInfo offsets =
                offsets_map[m_keep_matching ? node.name + std::to_string(node.id) : node.name];

            if (!node.is_folder())
                continue;

            DirectoryNode low_dir;

            std::string s_magic = node.name;
            std::transform(s_magic.begin(), s_magic.end(), s_magic.begin(), ::toupper);
            if (s_magic.size() < 4)
                s_magic.append(4 - s_magic.size(), ' ');

            low_dir.name = static_cast<u32>(offsets.string_offset);
            low_dir.hash = Object::NameRef::calcKeyCode(node.name);

            const auto &info        = children_infos[node.name + std::to_string(node.id)];
            low_dir.children_count  = static_cast<u16>(info.first);
            low_dir.children_offset = static_cast<u32>(current_child_offset);

            current_child_offset += low_dir.children_count;

            if (node.folder.parent != -1) {
                low_dir.magic = *reinterpret_cast<u32 *>(s_magic.data());
            } else {
                low_dir.magic = 'ROOT';
            }

            dir_nodes.push_back(low_dir);
        }

        // The FS node table however does not follow DFS :P

        // We store a vector of each used data offset to know if we need
        // to actually increment the data size marker. This is because
        // many files can point to the same piece of data to save space
        std::vector<u8> low_data;
        std::vector<std::size_t> used_offsets;
        for (auto &node : processed_nodes) {
            const bool is_special_dir = isSpecialPath(node.name);
            OffsetInfo offsets =
                offsets_map[m_keep_matching && !is_special_dir ? node.name + std::to_string(node.id)
                                                               : node.name];

            FSNode low_node = {.hash = Object::NameRef::calcKeyCode(node.name),
                               .type = static_cast<u16>(node.flags << 8),
                               .name = static_cast<u16>(offsets.string_offset)};

            if (node.is_folder()) {
                // Skip the root.
                if (!is_special_dir && node.folder.parent == -1) {
                    continue;
                }

                low_node.id              = 0xFFFF;
                low_node.folder.dir_node = node.id;
                low_node.folder.size     = 0x10;

                fs_nodes.push_back(low_node);
            } else {
                // IDs are calculated by incrementing for each file and special dir
                // The difference of FS nodes and pure Dir nodes is this total
                low_node.id          = node.id;
                low_node.file.offset = static_cast<u32>(low_data.size());
                low_node.file.size   = static_cast<u32>(node.data.size());

                const bool is_shared_data =
                    std::any_of(processed_nodes.begin(), processed_nodes.end(), [&](auto &n) {
                        if (n.id == node.id)
                            return false;
                        return n.data.size() == node.data.size() &&
                               std::equal(n.data.begin(), n.data.end(), node.data.begin());
                    });

                // We can optimize RARCs by condensing redundant data and sharing offsets
                // However it doesn't seem like Nintendo does this so we should
                // opt to store all data anyway when trying to match 1:1
                if (!is_shared_data || m_keep_matching) {
                    std::size_t padded_size = (node.data.size() + 0x1F) & ~0x1F;
                    if ((node.flags & ResourceAttribute::PRELOAD_TO_MRAM)) {
                        mram_size += padded_size;
                    } else if ((node.flags & ResourceAttribute::PRELOAD_TO_ARAM)) {
                        aram_size += padded_size;
                    } else if ((node.flags & ResourceAttribute::LOAD_FROM_DVD)) {
                        dvd_size += padded_size;
                    } else {
                        return make_serial_error<void>(
                            out,
                            std::format("File \"{}\" hasn't been marked for loading!", node.name));
                    }
                    low_data.insert(low_data.end(), node.data.begin(), node.data.end());
                    low_data.insert(low_data.end(), padded_size - node.data.size(), '\0');
                }
                fs_nodes.push_back(low_node);
            }
        }

        size_t total_file_size = mram_size + aram_size + dvd_size;
        if (total_file_size != low_data.size()) {
            return make_serial_error<void>(out, "Marked data size doesn't match size of files.");
        }

        // Make low archive
        LowResourceArchive low_archive;

        low_archive.meta_header.magic = 'RARC';

        low_archive.meta_header.nodes.offset = sizeof(MetaHeader);

        low_archive.meta_header.files.size      = static_cast<u32>(total_file_size);
        low_archive.meta_header.files.mram_size = static_cast<u32>(mram_size);
        low_archive.meta_header.files.aram_size = static_cast<u32>(aram_size);
        low_archive.meta_header.files.dvd_size  = static_cast<u32>(dvd_size);

        low_archive.node_header.dir_nodes.count  = static_cast<u32>(dir_nodes.size());
        low_archive.node_header.dir_nodes.offset = sizeof(NodeHeader);

        low_archive.node_header.fs_nodes.count = static_cast<u32>(fs_nodes.size());
        low_archive.node_header.fs_nodes.offset =
            low_archive.node_header.dir_nodes.offset +
                (low_archive.node_header.dir_nodes.count * sizeof(DirectoryNode) + 0x1F) &
            ~0x1F;

        // TODO: Verify custom IDs when not synced
        low_archive.node_header.ids_max    = static_cast<u16>(fs_nodes.size());
        low_archive.node_header.ids_synced = m_ids_synced;

        low_archive.node_header.string_table.size = (strings_blob.size() + 0x1F) & ~0x1F;
        low_archive.node_header.string_table.offset =
            low_archive.node_header.fs_nodes.offset +
                (low_archive.node_header.fs_nodes.count * sizeof(FSNode) + 0x1F) &
            ~0x1F;

        low_archive.meta_header.files.offset =
            low_archive.node_header.string_table.offset +
                (low_archive.node_header.string_table.size + 0x1F) &
            ~0x1F;

        low_archive.meta_header.size = low_archive.meta_header.nodes.offset +
                                       low_archive.meta_header.files.offset +
                                       low_archive.meta_header.files.size;

        return saveLowResourceArchive(low_archive, out);
    }

    Result<void, SerialError> ResourceArchive::deserialize(Deserializer &in) {
        auto result = loadLowResourceArchive(in);
        if (!result) {
            return std::unexpected(result.error());
        }

        auto low_archive = std::move(result.value());
        if (low_archive->dir_nodes.empty()) {
            return make_serial_error<void>(in, "Archive has no directories");
        }

        m_ids_synced = low_archive->node_header.ids_synced;

        recurseLoadDirectory(*low_archive, low_archive->dir_nodes[0], std::nullopt, std::nullopt,
                             m_nodes, 0);

        m_name = m_nodes[0].name;

        return {};
    }

    ScopePtr<ISmartResource> ResourceArchive::clone(bool deep) const {
        return ScopePtr<ISmartResource>();
    }

    Result<void> ResourceArchive::recalculateIDs() {
        std::vector<int> parent_stack = {
            -1,
        };
        std::vector<int> walk_stack = {};
        int folder_id               = 0;

        for (auto node = m_nodes.begin(); node != m_nodes.end(); node++) {
            for (auto walk = walk_stack.begin(); walk != walk_stack.end();) {
                if ((*walk) == std::distance(m_nodes.begin(), node)) {
                    walk = walk_stack.erase(walk);
                    parent_stack.pop_back();
                } else {
                    walk++;
                }
            }
            if (node->is_folder()) {
                node->id            = folder_id++;
                node->folder.parent = parent_stack.back();
                walk_stack.push_back(node->folder.sibling_next);
                parent_stack.push_back(node->id);
            }
        }

        children_info_type throwaway;
        auto result = processNodesForSave(m_nodes, throwaway);
        if (!result) {
            return std::unexpected(result.error());
        }

        const std::vector<ResourceArchive::Node> &calc_list = result.value();

        s16 file_id = -1;  // Adjust for root node
        for (auto &node : calc_list) {
            if (node.is_folder()) {
                file_id++;
                continue;
            }

            for (auto &target : m_nodes) {
                if (target.id == node.id && target.name == node.name) {
                    target.id = file_id++;
                    break;
                }
            }
        }

        return {};
    }

    static Result<ScopePtr<LowResourceArchive>, SerialError>
    loadLowResourceArchive(Deserializer &in) {
        auto low_archive = make_scoped<LowResourceArchive>();

        // Metaheader
        {
            low_archive->meta_header.magic = in.read<u32, std::endian::big>();
            if (!ResourceArchive::isMagicValid(low_archive->meta_header.magic)) {
                return make_serial_error<ScopePtr<LowResourceArchive>>(
                    in, "Invalid magic (Expected RARC)", -4);
            }

            low_archive->meta_header.size = in.read<u32, std::endian::big>();
            if (low_archive->meta_header.size != in.size()) {
                return make_serial_error<ScopePtr<LowResourceArchive>>(
                    in, "Invalid size (Stream size doesn't match)", -4);
            }

            low_archive->meta_header.nodes.offset = in.read<u32, std::endian::big>();

            low_archive->meta_header.files.offset    = in.read<u32, std::endian::big>();
            low_archive->meta_header.files.size      = in.read<u32, std::endian::big>();
            low_archive->meta_header.files.mram_size = in.read<u32, std::endian::big>();
            low_archive->meta_header.files.aram_size = in.read<u32, std::endian::big>();
            low_archive->meta_header.files.dvd_size  = in.read<u32, std::endian::big>();
        }

        // Nodeheader
        NodeHeader node_header{};
        {
            in.seek(low_archive->meta_header.nodes.offset, std::ios::beg);

            node_header.dir_nodes.count  = in.read<u32, std::endian::big>();
            node_header.dir_nodes.offset = in.read<u32, std::endian::big>();

            node_header.fs_nodes.count  = in.read<u32, std::endian::big>();
            node_header.fs_nodes.offset = in.read<u32, std::endian::big>();

            node_header.string_table.size   = in.read<u32, std::endian::big>();
            node_header.string_table.offset = in.read<u32, std::endian::big>();

            node_header.ids_max    = in.read<u16, std::endian::big>();
            node_header.ids_synced = in.read<bool>();
        }

        low_archive->node_header.ids_synced = node_header.ids_synced;

        // Dir nodes
        {
            in.seek(low_archive->meta_header.nodes.offset + node_header.dir_nodes.offset,
                    std::ios::beg);

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
            in.seek(low_archive->meta_header.nodes.offset + node_header.fs_nodes.offset,
                    std::ios::beg);

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
            in.seek(low_archive->meta_header.nodes.offset + node_header.string_table.offset,
                    std::ios::beg);
            in.readBytes(low_archive->string_data);
        }

        low_archive->file_data.resize(low_archive->meta_header.files.size);
        {
            in.seek(low_archive->meta_header.nodes.offset + low_archive->meta_header.files.offset,
                    std::ios::beg);
            in.readBytes(low_archive->file_data);
        }

        return low_archive;
    }

    static Result<void, SerialError>
    saveLowResourceArchive(const LowResourceArchive &low_archive, Serializer &out) {
        // Metaheader
        {
            if (!ResourceArchive::isMagicValid(low_archive.meta_header.magic)) {
                return make_serial_error<void>(out, "Invalid magic (Expected RARC)");
            }

            out.write<u32, std::endian::big>(low_archive.meta_header.magic);
            out.write<u32, std::endian::big>(low_archive.meta_header.size);

            out.write<u32, std::endian::big>(low_archive.meta_header.nodes.offset);

            out.write<u32, std::endian::big>(low_archive.meta_header.files.offset);
            out.write<u32, std::endian::big>(low_archive.meta_header.files.size);
            out.write<u32, std::endian::big>(low_archive.meta_header.files.mram_size);
            out.write<u32, std::endian::big>(low_archive.meta_header.files.aram_size);
            out.write<u32, std::endian::big>(low_archive.meta_header.files.dvd_size);
        }

        // Nodeheader
        {
            std::vector<char> padding(low_archive.meta_header.nodes.offset - 0x20);
            out.writeBytes(padding);  // Pad zeros

            out.write<u32, std::endian::big>(low_archive.node_header.dir_nodes.count);
            out.write<u32, std::endian::big>(low_archive.node_header.dir_nodes.offset);

            out.write<u32, std::endian::big>(low_archive.node_header.fs_nodes.count);
            out.write<u32, std::endian::big>(low_archive.node_header.fs_nodes.offset);

            out.write<u32, std::endian::big>(low_archive.node_header.string_table.size);
            out.write<u32, std::endian::big>(low_archive.node_header.string_table.offset);

            out.write<u16, std::endian::big>(low_archive.node_header.ids_max);
            out.write<bool>(low_archive.node_header.ids_synced);
        }

        // Dir nodes
        {
            std::vector<char> padding(low_archive.meta_header.nodes.offset +
                                      low_archive.node_header.dir_nodes.offset -
                                      static_cast<size_t>(out.tell()));
            out.writeBytes(padding);  // Pad zeros

            for (auto &dir_node : low_archive.dir_nodes) {
                out.write<u32, std::endian::big>(dir_node.magic);
                out.write<u32, std::endian::big>(dir_node.name);
                out.write<u16, std::endian::big>(dir_node.hash);
                out.write<u16, std::endian::big>(dir_node.children_count);
                out.write<u32, std::endian::big>(dir_node.children_offset);
            }
        }

        // FS nodes
        {
            std::vector<char> padding(low_archive.meta_header.nodes.offset +
                                      low_archive.node_header.fs_nodes.offset -
                                      static_cast<size_t>(out.tell()));
            out.writeBytes(padding);  // Pad zeros

            for (auto &fs_node : low_archive.fs_nodes) {
                out.write<u16, std::endian::big>(fs_node.id);
                out.write<u16, std::endian::big>(fs_node.hash);
                out.write<u16, std::endian::big>(fs_node.type);
                out.write<u16, std::endian::big>(fs_node.name);
                out.write<u32, std::endian::big>(fs_node.file.offset);
                out.write<u32, std::endian::big>(fs_node.file.size);
                out.write<u32>(0);  // Padding
            }
        }

        // String table
        {
            std::vector<char> padding(low_archive.meta_header.nodes.offset +
                                      low_archive.node_header.string_table.offset -
                                      static_cast<size_t>(out.tell()));
            out.writeBytes(padding);  // Pad zeros

            out.writeBytes(low_archive.string_data);
        }

        {
            std::vector<char> padding(low_archive.meta_header.nodes.offset +
                                      low_archive.meta_header.files.offset -
                                      static_cast<size_t>(out.tell()));
            out.writeBytes(padding);  // Pad zeros

            out.writeBytes(low_archive.file_data);
        }

        out.padTo(32);
        return {};
    }

    static Result<SpecialDirs, BaseError>
    createSpecialDirs(const ResourceArchive::Node &node,
                      std::optional<ResourceArchive::Node> parent) {
        if (!node.is_folder())
            return make_error<SpecialDirs>("RARC Middleware",
                                           "Can't make special dirs for a file!");

        SpecialDirs nodes;

        // This is the root
        if (!parent) {
            nodes.self = {
                .id     = node.id,
                .flags  = ResourceAttribute::DIRECTORY,
                .name   = ".",
                .folder = {.parent = -1, .sibling_next = node.folder.sibling_next}
            };
            nodes.parent = {
                .id     = -1,
                .flags  = ResourceAttribute::DIRECTORY,
                .name   = "..",
                .folder = {.parent = -1, .sibling_next = 0}
            };
        } else {
            nodes.self = {
                .id     = node.id,
                .flags  = ResourceAttribute::DIRECTORY,
                .name   = ".",
                .folder = {.parent = parent->id, .sibling_next = node.folder.sibling_next}
            };
            nodes.parent = {
                .id     = parent->id,
                .flags  = ResourceAttribute::DIRECTORY,
                .name   = "..",
                .folder = {.parent       = parent->folder.parent,
                           .sibling_next = parent->folder.sibling_next}
            };
        }

        return nodes;
    }

    static Result<void>
    insertSpecialDirs(std::vector<ResourceArchive::Node> &nodes) {
        std::vector<DirAdjustmentInfo> folder_infos;
        std::size_t adjust_amount = 2;

        for (auto node = nodes.begin(); node != nodes.end(); node++) {
            if (!node->is_folder() || isSpecialPath(node->name))
                continue;

            auto this_index = std::distance(nodes.begin(), node);

            for (auto &info : folder_infos) {
                auto &folder = nodes[info.index];

                // This folder is not a parent or future sibling of the new folder
                if (folder.folder.sibling_next <= this_index)
                    continue;

                // Adjust for the subdir's special dirs
                info.adjustment += 2;
            }

            folder_infos.push_back({(int)this_index, adjust_amount});
            adjust_amount += 2;
        }

        for (auto info = folder_infos.rbegin(); info != folder_infos.rend(); info++) {
            auto &node = nodes[info->index];
            node.folder.sibling_next += static_cast<s32>(info->adjustment);

            SpecialDirs dirs;
            if (node.folder.parent != -1) {
                auto parent =
                    std::find_if(nodes.begin(), nodes.end(), [&](const ResourceArchive::Node &n) {
                        return n.is_folder() && !isSpecialPath(n.name) &&
                               n.id == node.folder.parent;
                    });
                auto result = createSpecialDirs(node, *parent);
                if (!result) {
                    return std::unexpected(result.error());
                }
                dirs = result.value();
            } else {
                auto result = createSpecialDirs(node, std::nullopt);
                if (!result) {
                    return std::unexpected(result.error());
                }
                dirs = result.value();
            }

            nodes.insert(nodes.begin() + info->index + 1, dirs.parent);
            nodes.insert(nodes.begin() + info->index + 1, dirs.self);
        }

        return {};
    }

    static void removeSpecialDirs(std::vector<ResourceArchive::Node> &nodes) {
        std::vector<DirAdjustmentInfo> folder_infos;

        for (auto node = nodes.begin(); node != nodes.end();) {
            if (!node->is_folder() || isSpecialPath(node->name))
                continue;

            for (auto info : folder_infos) {
                auto &folder = nodes[info.index];

                // This folder is not a parent or future sibling of the new folder
                if (folder.folder.sibling_next <= std::distance(nodes.begin(), node))
                    continue;

                // Adjust for the subdir's special dirs
                info.adjustment += 2;
            }

            folder_infos.push_back({(int)std::distance(nodes.begin(), node), 2});
        }

        for (auto info = folder_infos.rbegin(); info != folder_infos.rend();) {
            auto &node = nodes[info->index];
            node.folder.sibling_next -= static_cast<s32>(info->adjustment);

            // Erase the special dirs
            nodes.erase(nodes.begin() + info->index + 1, nodes.begin() + info->index + 3);
        }
    }

    static Result<void>
    sortNodesForSaveRecursive(const std::vector<ResourceArchive::Node> &src,
                              std::vector<ResourceArchive::Node> &out,
                              children_info_type &children_info, std::size_t node_index) {
        if (node_index > src.size() - 2)  // Account for the children after
            return make_error<void>("RARC Middleware", "Node index beyond nodes capacity");

        const auto &node = src.at(node_index);

        if (node.folder.sibling_next > src.size()) {  // Account for end
            return make_error<void>("RARC Middleware", "Folder has less nodes than expected");
        }

        std::vector<ResourceArchive::Node> contents;
        std::vector<ResourceArchive::Node> dirs, sub_nodes, files, special_dirs;

        // Now push all children of this folder into their respective vectors
        auto begin = src.begin() + node_index + 1;
        auto end   = src.begin() + node.folder.sibling_next;

        for (auto &child_node = begin; child_node != end; ++child_node) {
            if (child_node->is_folder()) {
                if (isSpecialPath(child_node->name)) {
                    special_dirs.push_back(*child_node);
                } else {
                    dirs.push_back(*child_node);
                    std::vector<ResourceArchive::Node> nodes;
                    auto result = sortNodesForSaveRecursive(src, nodes, children_info,
                                                            std::distance(src.begin(), child_node));
                    if (!result) {
                        return std::unexpected(result.error());
                    }
                    sub_nodes.insert(sub_nodes.end(), nodes.begin(), nodes.end());
                    child_node += nodes.size();
                }
            } else {
                files.push_back(*child_node);
            }
        }

        size_t surface_children_size = files.size() + dirs.size() + special_dirs.size();
        children_info[node.name + std::to_string(node.id)] = {
            surface_children_size, node_index + surface_children_size + 1};

        // Apply items in the order that Nintendo's tools did
        out.insert(out.end(), files.begin(), files.end());
        out.insert(out.end(), dirs.begin(), dirs.end());
        out.insert(out.end(), special_dirs.begin(), special_dirs.end());
        out.insert(out.end(), sub_nodes.begin(), sub_nodes.end());

        return {};
    }

    static Result<std::vector<ResourceArchive::Node>, BaseError>
    processNodesForSave(const std::vector<ResourceArchive::Node> &nodes,
                        children_info_type &children_info) {
        std::vector<ResourceArchive::Node> out;

        if (nodes.size() == 0 || nodes[0].folder.parent != -1) {
            return make_error<std::vector<ResourceArchive::Node>>(
                "RARC Middleware", "RARC is missing nodes or the parent is screwed up");
        }

        std::vector<ResourceArchive::Node> with_dirs;
        with_dirs.insert(with_dirs.end(), nodes.begin(), nodes.end());

        {
            auto result = insertSpecialDirs(with_dirs);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        out.push_back(nodes[0]);

        {
            auto result = sortNodesForSaveRecursive(with_dirs, out, children_info, 0);
            if (!result) {
                return std::unexpected(result.error());
            }
        }

        return out;
    }

    static void recurseLoadDirectory(LowResourceArchive &low, DirectoryNode &low_dir,
                                     std::optional<FSNode> low_node,
                                     std::optional<ResourceArchive::Node> dir_parent,
                                     std::vector<ResourceArchive::Node> &out, std::size_t depth) {
        const auto start_nodes = out.size();

        ResourceArchive::Node dir_node;
        if (low_node) {
            dir_node = {.id    = static_cast<s32>(low_node->folder.dir_node),
                        .flags = static_cast<u16>(low_node->type >> 8),
                        .name  = low.string_data.data() + low_dir.name};
        } else {
            // This is the ROOT directory (exists without an fs node to further
            // describe it)
            dir_node = {.id    = 0,
                        .flags = ResourceAttribute::DIRECTORY,
                        .name  = low.string_data.data() + low_dir.name};
        }

        dir_node.folder.parent = dir_parent ? dir_parent->id : -1;

        auto *fs_begin = low.fs_nodes.data() + (low_dir.children_offset);
        auto *fs_end   = low.fs_nodes.data() + (low_dir.children_offset + low_dir.children_count);

        for (auto *fs_node = fs_begin; fs_node != fs_end; ++fs_node) {
            if (isLowNodeFolder(*fs_node)) {
                std::string name = low.string_data.data() + fs_node->name;
                if (isSpecialPath(name))
                    continue;

                // Sub Directory
                auto &chlld_dir_n = low.dir_nodes.at(fs_node->folder.dir_node);
                recurseLoadDirectory(low, chlld_dir_n, *fs_node, dir_node, out, depth + 1);
            } else {
                ResourceArchive::Node tmp_f = {.id    = fs_node->id,
                                               .flags = static_cast<u16>(fs_node->type >> 8),
                                               .name  = low.string_data.data() + fs_node->name};

                auto data_begin = low.file_data.begin() + fs_node->file.offset;
                tmp_f.data.insert(tmp_f.data.end(), data_begin, data_begin + fs_node->file.size);
                out.push_back(tmp_f);
            }
        }

        dir_node.folder.sibling_next = static_cast<s32>(out.size() + depth) + 1;
        out.insert(out.begin() + start_nodes, dir_node);
    }

    static void getSortedDirectoryListR(const std::filesystem::path &path,
                                        std::vector<std::filesystem::path> &out) {
        std::vector<std::filesystem::path> dirs;
        for (auto &&it : std::filesystem::directory_iterator{path}) {
            if (it.is_directory())
                dirs.push_back(it.path());
            else
                out.push_back(it.path());
        }
        for (auto &dir : dirs) {
            out.push_back(dir);
            getSortedDirectoryListR(dir, out);
        }
    }

}  // namespace Toolbox::RARC
