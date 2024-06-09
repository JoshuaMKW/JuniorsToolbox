#pragma once

#include <expected>
#include <format>
#include <functional>
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
        BaseError err = {{std::format("[{}] {}.", context, reason[0])}, std::stacktrace::current()};
        err.m_message.insert(err.m_message.end(), reason.begin() + 1, reason.end());
        return std::unexpected<BaseError>(err);
    }

    template <typename _Ret>
    inline Result<_Ret, BaseError> make_error(std::string_view context, std::string_view reason) {
        BaseError err = {{std::format("[{}] {}.", context, reason)}, std::stacktrace::current()};
        return std::unexpected<BaseError>(err);
    }

    template <typename _Ret> inline Result<_Ret, BaseError> make_error(std::string_view context) {
        return make_error<_Ret>(context, "Unknown error occured");
    }

    template <typename _Res, typename _Err = BaseError> class TRY {
    public:
        inline TRY(Result<_Res, _Err> &&result) : m_result(std::move(result)) {}
        inline ~TRY() = default;

        inline TRY &ok(std::function<void(const _Res &)> &&cb) {
            if (m_result) {
                if constexpr (std::is_void_v<_Res>) {
                    cb();
                } else {
                    cb(m_result.value());
                }
            }
            return *this;
        }

        inline TRY &err(std::function<void(const _Err &)> &&cb) {
            if (!m_result) {
                if constexpr (std::is_void_v<_Err>) {
                    cb();
                } else {
                    cb(m_result.error());
                }
            }
            return *this;
        }

    private:
        Result<_Res, _Err> m_result;

    public:
        TRY()                       = delete;
        TRY(const TRY &)            = delete;
        TRY(TRY &&)                 = delete;
        TRY &operator=(const TRY &) = delete;
        TRY &operator=(TRY &&)      = delete;
    };

}  // namespace Toolbox