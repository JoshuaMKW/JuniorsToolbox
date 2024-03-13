#pragma once

#include "core/error.hpp"
#include "core/log.hpp"
#include "fsystem.hpp"
#include "objlib/errors.hpp"
#include "objlib/meta/errors.hpp"
#include "serial.hpp"

#include <variant>

using namespace Toolbox::Object;

namespace Toolbox::UI {

    inline void LogError(const BaseError &error) {
        for (auto &line : error.m_message) {
            TOOLBOX_ERROR(line);
        }
        TOOLBOX_TRACE(error.m_stacktrace);
    }

    inline void LogError(const ObjectError &error);

    inline void LogError(const ObjectCorruptedError &error) {
        TOOLBOX_ERROR(error.m_message);
        TOOLBOX_TRACE(error.m_stacktrace);
    }

    inline void LogError(const ObjectGroupError &error) {
        TOOLBOX_ERROR(error.m_message);
        TOOLBOX_TRACE(error.m_stacktrace);
        TOOLBOX_LOG_SCOPE_PUSH();
        for (auto &child_error : error.m_child_errors) {
            LogError(child_error);
        }
        TOOLBOX_LOG_SCOPE_POP();
    }

    inline void LogError(const ObjectError &error) {
        if (std::holds_alternative<ObjectGroupError>(error)) {
            LogError(std::get<ObjectGroupError>(error));
        } else {
            LogError(std::get<ObjectCorruptedError>(error));
        }
    }

    inline void LogError(const MetaTypeError &error) { LogError(static_cast<BaseError>(error)); }

    inline void LogError(const MetaArrayError &error) { LogError(static_cast<BaseError>(error)); }

    inline void LogError(const MetaScopeError &error) { LogError(static_cast<BaseError>(error)); }

    inline void LogError(const MetaError &error) {
        if (std::holds_alternative<MetaTypeError>(error)) {
            LogError(std::get<MetaTypeError>(error));
        } else if (std::holds_alternative<MetaArrayError>(error)) {
            LogError(std::get<MetaArrayError>(error));
        } else if (std::holds_alternative<MetaScopeError>(error)) {
            LogError(std::get<MetaScopeError>(error));
        }
    }

    inline void LogError(const SerialError &error) { LogError(static_cast<BaseError>(error)); }

    inline void LogError(const FSError &error) { LogError(static_cast<BaseError>(error)); }

}  // namespace Toolbox::UI

#define TRY_ELSE_LOG(result_exp)                                                                   \
    {                                                                                              \
        auto result = (result_exp);                                                                \
        if (!result) {                                                                             \
            Toolbox::UI::LogError(result.error());                                                 \
            return;                                                                                \
        }                                                                                          \
    }

#define TRY_ELSE_LOG_R(result_exp, fail_ret)                                                            \
    {                                                                                              \
        auto result = (result_exp);                                                                \
        if (!result) {                                                                             \
            Toolbox::UI::LogError(result.error());                                                 \
            return (fail_ret);                                                                     \
        }                                                                                          \
    }