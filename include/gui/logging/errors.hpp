#pragma once

#include "error.hpp"
#include "gui/logging/logger.hpp"
#include "objlib/errors.hpp"
#include "objlib/meta/errors.hpp"
#include "serial.hpp"

#include <variant>

using namespace Toolbox::Object;

namespace Toolbox::UI {

    inline void logError(const BaseError &error) {
        auto &logger = Log::AppLogger::instance();
        for (auto &line : error.m_message) {
            logger.error(line);
        }
        logger.trace(error.m_stacktrace);
    }

    inline void logObjectError(const ObjectError &error);

    inline void logObjectCorruptError(const ObjectCorruptedError &error) {
        auto &logger = Log::AppLogger::instance();
        logger.error(error.m_message);
        logger.trace(error.m_stacktrace);
    }

    inline void logObjectGroupError(const ObjectGroupError &error) {
        auto &logger = Log::AppLogger::instance();
        logger.error(error.m_message);
        logger.trace(error.m_stacktrace);
        logger.pushStack();
        for (auto &child_error : error.m_child_errors) {
            logObjectError(child_error);
        }
        logger.popStack();
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
}  // namespace Toolbox::UI