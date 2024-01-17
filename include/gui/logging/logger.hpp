#pragma once

#include <functional>
#include <string>
#include <vector>
#include <stacktrace>

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
        void popStack() { if (m_indentation > 0) m_indentation--; }

        void clear() { m_messages.clear(); }

        void log(const std::string &message) { log(ReportLevel::LOG, message); }

        void debugLog(const std::string &message) {
#ifndef NDEBUG
            log(ReportLevel::DEBUG, message);
#endif
        }

        void info(const std::string &message) { log(ReportLevel::INFO, message); }
        void warn(const std::string &message) { log(ReportLevel::WARNING, message); }
        void error(const std::string &message) { log(ReportLevel::ERROR, message); }
        void trace(const std::stacktrace &stack) {
#ifndef NDEBUG
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