#pragma once

#include "core/error.hpp"
#include "objlib/qualname.hpp"

#include <expected>
#include <stacktrace>
#include <string>
#include <variant>

namespace Toolbox::Object {

    struct MetaTypeError : public BaseError {
        std::string m_error_type;
        std::string m_expected_type;
    };

    struct MetaArrayError : public BaseError {
        size_t m_error_index;
        size_t m_array_size;
    };

    struct MetaScopeError : public BaseError {
        QualifiedName m_scope;
        size_t m_error_index;
    };

    using MetaError = std::variant<MetaTypeError, MetaArrayError, MetaScopeError>;

    template <typename _Ret>
    inline Result<_Ret, MetaError> make_meta_error(std::string_view context,
                                                          std::string_view error_type,
                                                          std::string_view expected_type) {
        MetaTypeError err = {
            std::vector<std::string>(
                {std::format("{}: TypeError: Illegal type cast to MetaValue from type {}.", context,
                         expected_type)}),
            std::stacktrace::current(),
            std::string(error_type),
            std::string(expected_type),
        };
        return std::unexpected<MetaError>(err);
    }

    template <typename _Ret>
    inline Result<_Ret, MetaArrayError>
    make_meta_error(std::string_view context, size_t error_index, size_t array_size) {
        MetaArrayError err = {
            std::vector<std::string>({std::format("{}: IndexError: Index {} exceeds array size {}.", context, error_index,
                         array_size)}),
            std::stacktrace::current(),
            error_index,
            array_size,
        };
        return std::unexpected<MetaArrayError>(err);
    }

    template <typename _Ret>
    inline Result<_Ret, MetaScopeError>
    make_meta_error(const QualifiedName &scope, size_t error_index, std::string_view reason) {
        MetaScopeError err = {
            std::vector<std::string>({std::format("ScopeError: {}", reason), scope.toString("."),
             std::string(error_index, ' ') + "^"}),
            std::stacktrace::current(),
            scope,
            error_index,
        };
        return std::unexpected<MetaScopeError>(err);
    }

}  // namespace Toolbox::Object