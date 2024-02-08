#pragma once

#include <expected>
#include <format>
#include <stacktrace>
#include <string>
#include <vector>

namespace Toolbox {

    struct BaseError {
        std::vector<std::string> m_message;
        std::stacktrace m_stacktrace;
    };

    template <typename _ValueT, typename _ErrorT = BaseError>
    using Result = std::expected<_ValueT, _ErrorT>;

    template <typename _Ret>
    inline Result<_Ret, BaseError> make_error(std::string_view context,
                                                     std::vector<std::string> reason) {
        if (reason.empty()) {
            reason.emplace_back("Unknown error occured");
        }
        BaseError err = {{std::format("{}: {}.", context, reason[0])}, std::stacktrace::current()};
        err.m_message.insert(err.m_message.end(), reason.begin() + 1, reason.end());
        return std::unexpected<BaseError>(err);
    }

    template <typename _Ret>
    inline Result<_Ret, BaseError> make_error(std::string_view context,
                                                     std::string_view reason) {
        BaseError err = {{std::format("{}: {}.", context, reason)}, std::stacktrace::current()};
        return std::unexpected<BaseError>(err);
    }

    template <typename _Ret>
    inline Result<_Ret, BaseError> make_error(std::string_view context) {
        return make_error<_Ret>(context, "Unknown error occured");
    }

    // Credits to Riistudio

    template <typename T> auto _move(T &&t) {
        if constexpr (!std::is_void_v<typename std::remove_cvref_t<T>::value_type>) {
            return std::move(*t);
        }
    }

#define TRY(...)                                                                                   \
    ({                                                                                             \
        auto &&y = (__VA_ARGS__);                                                                  \
        static_assert(!std::is_lvalue_reference_v<decltype(_move(y))>);                            \
        if (!y) [[unlikely]] {                                                                     \
            return std::unexpected(y.error());                                                     \
        }                                                                                          \
        _move(y);                                                                                 \
    })

}  // namespace Toolbox