#pragma once

#include "objlib/qualname.hpp"
#include <expected>
#include <stacktrace>
#include <string>
#include <variant>

namespace Toolbox::Object {

    struct MetaTypeError {
        std::string m_error_type;
        std::string m_expected_type;
        std::vector<std::string> m_message;
        std::stacktrace m_stacktrace;
    };

    struct MetaArrayError {
        int m_error_index;
        size_t m_array_size;
        std::vector<std::string> m_message;
        std::stacktrace m_stacktrace;
    };

    struct MetaScopeError {
        QualifiedName m_scope;
        size_t m_error_index;
        std::vector<std::string> m_message;
        std::stacktrace m_stacktrace;
    };

    using MetaError = std::variant<MetaTypeError, MetaArrayError, MetaScopeError>;

    template <typename _Ret>
    inline std::expected<_Ret, MetaError> make_meta_error(std::string_view context,
                                                          std::string_view error_type,
                                                          std::string_view expected_type) {
        MetaTypeError err = {
            std::string(error_type),
            std::string(expected_type),
            {std::format("{}: TypeError: Illegal type cast to MetaValue from type {}.", context,
                         expected_type)},
            std::stacktrace::current()};
        return std::unexpected<MetaError>(err);
    }

    template <typename _Ret>
    inline std::expected<_Ret, MetaError> make_meta_error(std::string_view context, int error_index,
                                                          size_t array_size) {
        MetaArrayError err;
        if (error_index >= 0) {
            err = {error_index,
                   array_size,
                   {std::format("{}: IndexError: Index {} exceeds array size {}.", context,
                                error_index, array_size)},
                   std::stacktrace::current()};
        } else {
            err = {error_index,
                   array_size,
                   {std::format("{}: IndexError: Index {} is an invalid index.", context,
                                error_index, array_size)},
                   std::stacktrace::current()};
        }
        return std::unexpected<MetaError>(err);
    }

    template <typename _Ret>
    inline std::expected<_Ret, MetaError>
    make_meta_error(const QualifiedName &scope, size_t error_index, std::string_view reason) {
        MetaTypeError err = {
            scope,
            error_index,
            {std::format("ScopeError: {}", reason), scope.toString("."),
              std::string(' ', error_index) + "^"},
            std::stacktrace::current()
        };
        return std::unexpected<MetaError>(err);
    }

}  // namespace Toolbox::Object