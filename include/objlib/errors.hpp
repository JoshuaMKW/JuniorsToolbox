#pragma once

#include "core/error.hpp"
#include <stacktrace>
#include <string>
#include <variant>

namespace Toolbox::Object {

    class ISceneObject;

    struct ObjectCorruptedError;
    struct ObjectGroupError;

    using ObjectError = std::variant<ObjectGroupError, ObjectCorruptedError>;

    struct ObjectCorruptedError {
        std::string m_message;
        std::stacktrace m_stacktrace;
        ISceneObject *m_object;
    };

    struct ObjectGroupError {
        std::string m_message;
        std::stacktrace m_stacktrace;
        ISceneObject *m_object;
        std::vector<ObjectError> m_child_errors = {};
    };

}
