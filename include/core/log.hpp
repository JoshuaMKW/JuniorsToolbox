#pragma once

#include <format>
#include <functional>
#include <stacktrace>
#include <string>
#include <vector>

#include "core/core.hpp"

namespace Toolbox::Log {

    enum class ReportLevel { INFO, LOG = INFO, WARNING, ERROR, DEBUG };

    class AppLogger {
    public:
        struct LogMessage {
            ReportLevel m_level;
            std::string m_message;
            size_t m_indentation;
        };

        using log_callback_t = std::function<void(const LogMessage &)>;

    protected:
        AppLogger()             = default;
        AppLogger(AppLogger &&) = default;

    public:
        ~AppLogger() = default;

        static AppLogger &instance() {
            static AppLogger s_logger;
            return s_logger;
        }

        void pushStack() { m_indentation++; }
        void popStack() {
            if (m_indentation > 0)
                m_indentation--;
        }

        void clear() { m_messages.clear(); }

        void log(const std::string &message) {
            log(ReportLevel::LOG, message);
        }

        void debugLog(const std::string &message) {
#ifndef TOOLBOX_DEBUG
            log(ReportLevel::DEBUG, message);
#endif
        }

        void info(const std::string &message) {
            log(ReportLevel::INFO, message);
        }

        void warn(const std::string &message) {
            log(ReportLevel::WARNING, message);
        }

        void error(const std::string &message) {
            log(ReportLevel::ERROR, message);
        }

        void trace(const std::stacktrace &stack) {
#ifndef TOOLBOX_DEBUG
            for (size_t i = 0; i < std::min(m_max_trace, stack.size()); ++i) {
                debugLog(
                    std::format("{} at line {}", stack[i].source_file(), stack[i].source_line()));
            }
#endif
        }

        void log(ReportLevel level, const std::string &message);

        void setLogCallback(log_callback_t cb) { m_log_callback = cb; }

        [[nodiscard]] const std::vector<LogMessage> &messages() const { return m_messages; }

    private:
        size_t m_max_trace                 = 8;
        size_t m_indentation               = 0;
        std::vector<LogMessage> m_messages = {};
        log_callback_t m_log_callback      = [](const LogMessage &) {};
    };

}  // namespace Toolbox::Log

#define TOOLBOX_FORMAT_FN std::format

#define TOOLBOX_TRACE(stacktrace) ::Toolbox::Log::AppLogger::instance().trace(stacktrace)
#define TOOLBOX_INFO_V(fmt, ...)                                                                     \
    ::Toolbox::Log::AppLogger::instance().info(TOOLBOX_FORMAT_FN(fmt, ##__VA_ARGS__))
#define TOOLBOX_WARN_V(fmt, ...)                                                                     \
    ::Toolbox::Log::AppLogger::instance().warn(TOOLBOX_FORMAT_FN(fmt, ##__VA_ARGS__))
#define TOOLBOX_ERROR_V(fmt, ...)                                                                    \
    ::Toolbox::Log::AppLogger::instance().error(TOOLBOX_FORMAT_FN(fmt, ##__VA_ARGS__))
#define TOOLBOX_DEBUG_LOG_V(fmt, ...)                                                                \
    ::Toolbox::Log::AppLogger::instance().debugLog(TOOLBOX_FORMAT_FN(fmt, ##__VA_ARGS__))
#define TOOLBOX_LOG_V(level, fmt, ...)                                                               \
    ::Toolbox::Log::AppLogger::instance().log(level, TOOLBOX_FORMAT_FN(fmt, ##__VA_ARGS__))

#define TOOLBOX_INFO(msg)                                                                   \
    ::Toolbox::Log::AppLogger::instance().info(msg)
#define TOOLBOX_WARN(msg)                                                                   \
    ::Toolbox::Log::AppLogger::instance().warn(msg)
#define TOOLBOX_ERROR(msg)                                                                  \
    ::Toolbox::Log::AppLogger::instance().error(msg)
#define TOOLBOX_DEBUG_LOG(msg)                                                              \
    ::Toolbox::Log::AppLogger::instance().debugLog(msg)
#define TOOLBOX_LOG(level, msg)                                                             \
    ::Toolbox::Log::AppLogger::instance().log(level, msg)

#define TOOLBOX_LOG_SCOPE_PUSH() ::Toolbox::Log::AppLogger::instance().pushStack()
#define TOOLBOX_LOG_SCOPE_POP()  ::Toolbox::Log::AppLogger::instance().popStack()

#define TOOLBOX_LOG_CALLBACK(cb) ::Toolbox::Log::AppLogger::instance().setLogCallback(cb)