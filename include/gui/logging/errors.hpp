#pragma once

#include "core/error.hpp"
#include "core/log.hpp"
#include "objlib/errors.hpp"
#include "objlib/meta/errors.hpp"
#include "serial.hpp"
#include "fsystem.hpp"

#include <variant>

using namespace Toolbox::Object;

namespace Toolbox::UI {

    inline void logError(const BaseError &error) {
        for (auto &line : error.m_message) {
            TOOLBOX_ERROR(line);
        }
        TOOLBOX_TRACE(error.m_stacktrace);
    }

    inline void logObjectError(const ObjectError &error);

    inline void logObjectCorruptError(const ObjectCorruptedError &error) {
        TOOLBOX_ERROR(error.m_message);
        TOOLBOX_TRACE(error.m_stacktrace);
    }

    inline void logObjectGroupError(const ObjectGroupError &error) {
        TOOLBOX_ERROR(error.m_message);
        TOOLBOX_TRACE(error.m_stacktrace);
        TOOLBOX_LOG_SCOPE_PUSH();
        for (auto &child_error : error.m_child_errors) {
            logObjectError(child_error);
        }
        TOOLBOX_LOG_SCOPE_POP();
    }

    inline void logObjectError(const ObjectError &error) {
        if (std::holds_alternative<ObjectGroupError>(error)) {
            logObjectGroupError(std::get<ObjectGroupError>(error));
        } else {
            logObjectCorruptError(std::get<ObjectCorruptedError>(error));
        }
    }

    inline void logMetaTypeError(const MetaTypeError &error) { logError(error); }

    inline void logMetaArrayError(const MetaArrayError &error) { logError(error); }

    inline void logMetaScopeError(const MetaScopeError &error) { logError(error); }

    inline void logMetaError(const MetaError &error) {
        if (std::holds_alternative<MetaTypeError>(error)) {
            logMetaTypeError(std::get<MetaTypeError>(error));
        } else if (std::holds_alternative<MetaArrayError>(error)) {
            logMetaArrayError(std::get<MetaArrayError>(error));
        } else if (std::holds_alternative<MetaScopeError>(error)) {
            logMetaScopeError(std::get<MetaScopeError>(error));
        }
    }

    inline void logSerialError(const SerialError &error) { logError(error); }

    inline void logFSError(const FSError &error) { logError(error); }
}  // namespace Toolbox::UI