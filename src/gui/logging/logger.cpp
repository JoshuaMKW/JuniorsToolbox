#include "core/log.hpp"
#include "gui/settings.hpp"

namespace Toolbox::Log {
    AppLogger &AppLogger::instance() {
        static AppLogger s_logger;
        return s_logger;
    }

    void AppLogger::log(ReportLevel level, const std::string &message) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();
        if (settings.m_log_to_cout_cerr) {
            if (level == ReportLevel::REPORT_ERROR)
                std::cerr << message << std::endl;
            else
                std::cout << message << std::endl;
        }
        m_write_mutex.lock();
        m_messages.emplace_back(level, message, m_indentation);
        m_log_callback(m_messages.back());
        m_write_mutex.unlock();
    }

}  // namespace Toolbox