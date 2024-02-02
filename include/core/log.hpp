#pragma once

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

        template <typename... Args> void log(const std::string &message, Args &&...args) {
            log(ReportLevel::LOG, message, std::forward<Args>(args)...);
        }

        template <typename... Args> void debugLog(const std::string &message, Args &&...args) {
#ifndef TOOLBOX_DEBUG
            log(ReportLevel::DEBUG, message, std::forward<Args>(args)...);
#endif
        }

        template <typename... Args> void info(const std::string &message, Args &&...args) {
            log(ReportLevel::INFO, message, std::forward<Args>(args)...);
        }

        template <typename... Args> void warn(const std::string &message, Args &&...args) {
            log(ReportLevel::WARNING, message, std::forward<Args>(args)...);
        }

        template <typename... Args> void error(const std::string &message, Args &&...args) {
            log(ReportLevel::ERROR, message, std::forward<Args>(args)...);
        }

        template <typename... Args> void trace(const std::stacktrace &stack) {
#ifndef TOOLBOX_DEBUG
            for (size_t i = 0; i < std::min(m_max_trace, stack.size()); ++i) {
                debugLog(
                    std::format("{} at line {}", stack[i].source_file(), stack[i].source_line()));
            }
#endif
        }

        template <typename... Args>
        void log(ReportLevel level, const std::string &message, Args &&...args) {
            log(level, std::format(message, std::forward(args)...));
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

#define TOOLBOX_TRACE(stacktrace) ::Toolbox::Log::AppLogger::instance().trace(stacktrace)
#define TOOLBOX_INFO(msg, ...)    ::Toolbox::Log::AppLogger::instance().info(msg, ##__VA_ARGS__)
#define TOOLBOX_WARN(msg, ...)    ::Toolbox::Log::AppLogger::instance().warn(msg, ##__VA_ARGS__)
#define TOOLBOX_ERROR(msg, ...)   ::Toolbox::Log::AppLogger::instance().error(msg, ##__VA_ARGS__)
#define TOOLBOX_DEBUG_LOG(msg, ...)                                                                \
    ::Toolbox::Log::AppLogger::instance().debugLog(msg, ##__VA_ARGS__)
#define TOOLBOX_LOG(level, msg, ...)                                                               \
    ::Toolbox::Log::AppLogger::instance().log(level, msg, ##__VA_ARGS__)
#define TOOLBOX_LOG_SCOPE_PUSH() ::Toolbox::Log::AppLogger::instance().pushStack()
#define TOOLBOX_LOG_SCOPE_POP()  ::Toolbox::Log::AppLogger::instance().popStack()
#define TOOLBOX_LOG_CALLBACK(cb) ::Toolbox::Log::AppLogger::instance().setLogCallback(cb)