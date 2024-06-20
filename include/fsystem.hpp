#pragma once

// CREDITS TO RIIDEFI FOR THIS CODE

#include <expected>
#include <filesystem>
#include <format>
#include <stacktrace>
#include <system_error>

#include "core/error.hpp"

namespace Toolbox {

    struct FSError : public BaseError {
        std::error_code m_errorcode;
    };

    template <typename _Ret>
    [[nodiscard]] static inline Result<_Ret, FSError>
    make_fs_error(std::error_code code, std::vector<std::string> reason) {
        std::vector<std::string> message;
        if (reason.size() > 0) {
            message.push_back(std::format("FSError: {}", reason[0]));
            if (reason.size() > 1) {
                message.insert(message.end(), reason.begin() + 1, reason.end());
            }
        } else {
            message.push_back("FSError: Unknown error.");
        }

        FSError error = {message, std::stacktrace::current(), code};
        return std::unexpected(error);
    }

    template <typename _Ret>
    [[nodiscard]] static inline Result<_Ret, FSError> make_fs_error(std::error_code code) {
        std::vector<std::string> message;
        message.push_back(std::format("FSError: {}", code.message()));
        FSError error = {message, std::stacktrace::current(), code};
        return std::unexpected(error);
    }

    namespace Filesystem {

        using path                         = std::filesystem::path;
        using file_status                  = std::filesystem::file_status;
        using perms                        = std::filesystem::perms;
        using perm_options                 = std::filesystem::perm_options;
        using copy_options                 = std::filesystem::copy_options;
        using space_info                   = std::filesystem::space_info;
        using file_time_type               = std::filesystem::file_time_type;
        using directory_entry              = std::filesystem::directory_entry;
        using directory_options            = std::filesystem::directory_options;
        using directory_iterator           = std::filesystem::directory_iterator;
        using recursive_directory_iterator = std::filesystem::recursive_directory_iterator;

        [[nodiscard]] static inline Result<path, FSError>
        absolute(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::absolute(p, ec);
            if (ec) {
                return make_fs_error<path>(ec);
            }
            return result;
        }
        [[nodiscard]] static inline Result<path, FSError>
        canonical(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::canonical(p, ec);
            if (ec) {
                return make_fs_error<path>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<path, FSError>
        weakly_canonical(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::weakly_canonical(p, ec);
            if (ec) {
                return make_fs_error<path>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<path, FSError>
        relative(const path &p,
                 const path &base = std::filesystem::current_path()) {
            std::error_code ec;
            auto result = std::filesystem::relative(p, base, ec);
            if (ec) {
                return make_fs_error<path>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<path, FSError>
        proximate(const path &p,
                  const path &base = std::filesystem::current_path()) {
            std::error_code ec;
            auto result = std::filesystem::proximate(p, base, ec);
            if (ec) {
                return make_fs_error<path>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<void, FSError>
        copy(const path &from, const path &to,
             copy_options options = copy_options::none) {
            std::error_code ec;
            std::filesystem::copy(from, to, options, ec);
            if (ec) {
                return make_fs_error<void>(ec);
            }
            return {};
        }

        [[nodiscard]] static inline Result<bool, FSError>
        copy_file(const path &from, const path &to,
                  copy_options options = copy_options::none) {
            std::error_code ec;
            auto result = std::filesystem::copy_file(from, to, options, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<void, FSError>
        copy_symlink(const path &existing_symlink,
                     const path &new_symlink) {
            std::error_code ec;
            std::filesystem::copy_symlink(existing_symlink, new_symlink, ec);
            if (ec) {
                return make_fs_error<void>(ec);
            }
            return {};
        }

        [[nodiscard]] static inline Result<bool, FSError>
        create_directory(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::create_directory(p, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError>
        create_directories(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::create_directories(p, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<void, FSError>
        create_hard_link(const path &to,
                         const path &new_hard_link) {
            std::error_code ec;
            std::filesystem::create_hard_link(to, new_hard_link, ec);
            if (ec) {
                return make_fs_error<void>(ec);
            }
            return {};
        }

        [[nodiscard]] static inline Result<void, FSError>
        create_symlink(const path &to, const path &new_symlink) {
            std::error_code ec;
            std::filesystem::create_symlink(to, new_symlink, ec);
            if (ec) {
                return make_fs_error<void>(ec);
            }
            return {};
        }

        [[nodiscard]] static inline Result<void, FSError>
        create_directory_symlink(const path &to,
                                 const path &new_symlink) {
            std::error_code ec;
            std::filesystem::create_directory_symlink(to, new_symlink, ec);
            if (ec) {
                return make_fs_error<void>(ec);
            }
            return {};
        }

        [[nodiscard]] static inline Result<path, FSError> current_path() {
            std::error_code ec;
            auto result = std::filesystem::current_path(ec);
            if (ec) {
                return make_fs_error<path>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError> exists(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::exists(p, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError>
        equivalent(const path &p1, const path &p2) {
            std::error_code ec;
            auto result = std::filesystem::equivalent(p1, p2, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<uintmax_t, FSError>
        file_size(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::file_size(p, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<uintmax_t, FSError>
        hard_link_count(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::hard_link_count(p, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<file_time_type, FSError>
        last_write_time(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::last_write_time(p, ec);
            if (ec) {
                return make_fs_error<file_time_type>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<void, FSError>
        permissions(const path &p, perms prms,
                    perm_options opts = perm_options::replace) {
            std::error_code ec;
            std::filesystem::permissions(p, prms, opts, ec);
            if (ec) {
                return make_fs_error<void>(ec);
            }
            return {};
        }

        [[nodiscard]] static inline Result<path, FSError>
        read_symlink(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::read_symlink(p, ec);
            if (ec) {
                return make_fs_error<path>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError> remove(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::remove(p, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<uintmax_t, FSError>
        remove_all(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::remove_all(p, ec);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<void, FSError> rename(const path &from,
                                                                 const path &to) {
            std::error_code ec;
            std::filesystem::rename(from, to, ec);
            if (ec) {
                return make_fs_error<void>(ec);
            }
            return {};
        }

        [[nodiscard]] static inline Result<void, FSError>
        resize_file(const path &p, uintmax_t size) {
            std::error_code ec;
            std::filesystem::resize_file(p, size, ec);
            if (ec) {
                return make_fs_error<void>(ec);
            }
            return {};
        }

        [[nodiscard]] static inline Result<space_info, FSError>
        space(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::space(p, ec);
            if (ec) {
                return make_fs_error<space_info>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<file_status, FSError>
        status(const path &p) {
            std::error_code ec;
            auto result = status(p, ec);
            if (ec) {
                return make_fs_error<file_status>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<file_status, FSError>
        symlink_status(const path &p) {
            std::error_code ec;
            auto result = symlink_status(p, ec);
            if (ec) {
                return make_fs_error<file_status>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<path, FSError> temp_directory_path() {
            std::error_code ec;
            auto result = std::filesystem::temp_directory_path(ec);
            if (ec) {
                return make_fs_error<path>(ec);
            }
            return result;
        }
        [[nodiscard]] static inline Result<bool, FSError>
        is_block_file(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_block_file(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError>
        is_character_file(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_character_file(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError>
        is_directory(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_directory(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError> is_empty(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_empty(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError> is_fifo(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_fifo(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError> is_other(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_other(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError>
        is_regular_file(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_regular_file(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError>
        is_socket(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_socket(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError>
        is_symlink(const path &p) {
            std::error_code ec;
            auto result = std::filesystem::is_symlink(p, ec);
            if (result) {
                return result;
            }
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

        [[nodiscard]] static inline Result<bool, FSError>
        status_known(const file_status &s) {
            std::error_code ec;
            auto result = std::filesystem::status_known(s);
            if (ec) {
                return make_fs_error<bool>(ec);
            }
            return result;
        }

    }  // namespace Filesystem

    using fs_path = Toolbox::Filesystem::path;

}  // namespace Toolbox