#pragma once

#include <functional>
#include <string>
#include <vector>

namespace Toolbox::Log {

    enum class ReportLevel { INFO, LOG = INFO, WARNING, ERROR, DEBUG };

    class AppLogger {
    public:
        struct LogMessage {
            ReportLevel m_level;
            std::string m_message;
        };

        using log_callback_t = std::function<void(const LogMessage &)>;

    protected:
        AppLogger() = default;
        AppLogger(AppLogger &&) = default;

    public:
        ~AppLogger() = default;

        static AppLogger &instance() {
            static AppLogger s_logger;
            return s_logger;
        }

        void log(const std::string &message) {
#ifdef NDEBUG
            log(ReportLevel::LOG, message);
#else
            log(ReportLevel::DEBUG, message);
#endif
        }

        void info(const std::string &message) { log(ReportLevel::INFO, message); }
        void warn(const std::string &message) { log(ReportLevel::WARNING, message); }
        void error(const std::string &message) { log(ReportLevel::ERROR, message); }

        void log(ReportLevel level, const std::string &message) {
            m_messages.emplace_back(level, message);
        }

        void setLogCallback(log_callback_t cb) { m_log_callback = cb; }

        [[nodiscard]] const std::vector<LogMessage> &messages() const { return m_messages; }

    private:
        std::vector<LogMessage> m_messages = {};
        log_callback_t m_log_callback = [](const LogMessage &) {};
    };

}  // namespace Toolbox::Log