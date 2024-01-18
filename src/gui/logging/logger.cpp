#include "gui/logging/logger.hpp"
#include "gui/settings.hpp"

namespace Toolbox::Log {

    void AppLogger::log(ReportLevel level, const std::string &message) {
        AppSettings &settings = SettingsManager::instance().getCurrentProfile();
        if (settings.m_log_to_cout_cerr) {
            if (level == ReportLevel::ERROR)
                std::cerr << message << std::endl;
            else
                std::cout << message << std::endl;
        }
        m_messages.emplace_back(level, message, m_indentation);
        m_log_callback(m_messages.back());
    }

}  // namespace Toolbox