#pragma once

#include <expected>
#include <stacktrace>
#include <string>
#include <vector>

namespace Toolbox {

    struct BaseError {
        std::vector<std::string> m_message;
        std::stacktrace m_stacktrace;
    };

    template <typename _Ret>
    inline std::expected<_Ret, BaseError> make_error(std::string_view context,
                                                     std::vector<std::string> reason) {
        if (reason.empty()) {
            reason.emplace_back("Unknown error occured");
        }
        BaseError err = {{std::format("{}: {}.", context, reason[0])}, std::stacktrace::current()};
        err.m_message.insert(err.m_message.end(), reason.begin() + 1, reason.end());
        return std::unexpected<BaseError>(err);
    }

    template <typename _Ret>
    inline std::expected<_Ret, BaseError> make_error(std::string_view context,
                                                     std::string_view reason) {
        BaseError err = {{std::format("{}: {}.", context, reason)}, std::stacktrace::current()};
        return std::unexpected<BaseError>(err);
    }

    template <typename _Ret>
    inline std::expected<_Ret, BaseError> make_error(std::string_view context) {
        return make_error<_Ret>(context, "Unknown error occured");
    }

}  // namespace Toolbox