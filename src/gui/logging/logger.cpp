#include "core/log.hpp"

namespace Toolbox::Log {
    AppLogger &AppLogger::instance() {
        static AppLogger s_logger;
        return s_logger;
    }

    void AppLogger::log(ReportLevel level, const std::string &message) {
        m_write_mutex.lock();
        m_messages.emplace_back(level, message, m_indentation);
        m_log_callback(m_messages.back());
        m_write_mutex.unlock();
    }

}  // namespace Toolbox