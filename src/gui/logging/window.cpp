#include <cmath>

#include "gui/application.hpp"
#include "gui/font.hpp"
#include "gui/logging/window.hpp"
#include "gui/window.hpp"

static std::string s_message_pool;

namespace Toolbox::UI {
    void LoggingWindow::appendMessageToPool(const Log::AppLogger::LogMessage &message) {
        AppSettings &settings = GUIApplication::instance().getSettingsManager().getCurrentProfile();
        if (settings.m_log_to_cout_cerr) {
            if (message.m_level == Log::ReportLevel::REPORT_ERROR)
                std::cerr << message.m_message << std::endl;
            else
                std::cout << message.m_message << std::endl;
        }

        switch (message.m_level) {
        case Log::ReportLevel::REPORT_LOG:
            s_message_pool += std::format("[LOG]     - {:>{}}", message.m_message,
                                          message.m_indentation * 4 + message.m_message.size());
            break;
        case Log::ReportLevel::REPORT_WARNING:
            s_message_pool += std::format("[WARNING] - {:>{}}", message.m_message,
                                          message.m_indentation * 4 + message.m_message.size());
            break;
        case Log::ReportLevel::REPORT_ERROR:
            s_message_pool += std::format("[ERROR]   - {:>{}}", message.m_message,
                                          message.m_indentation * 4 + message.m_message.size());
            break;
        case Log::ReportLevel::REPORT_DEBUG:
            s_message_pool += std::format("[DEBUG]   - {:>{}}", message.m_message,
                                          message.m_indentation * 4 + message.m_message.size());
            break;
        }
        m_scroll_requested = (int)m_logging_level <= (int)message.m_level;
    }

    void LoggingWindow::onRenderMenuBar() {
        auto &logger = Log::AppLogger::instance();
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Verbosity", true)) {
                bool info_active  = m_logging_level == Log::ReportLevel::REPORT_INFO;
                bool warn_active  = m_logging_level == Log::ReportLevel::REPORT_WARNING;
                bool error_active = m_logging_level == Log::ReportLevel::REPORT_ERROR;
                if (ImGui::Checkbox("Info", &info_active)) {
                    m_logging_level = Log::ReportLevel::REPORT_INFO;
                }
                if (ImGui::Checkbox("Warning", &warn_active)) {
                    m_logging_level = Log::ReportLevel::REPORT_WARNING;
                }
                if (ImGui::Checkbox("Error", &error_active)) {
                    m_logging_level = Log::ReportLevel::REPORT_ERROR;
                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Copy")) {
                std::string clipboard_text;
                for (size_t i = 0; i < logger.messages().size(); ++i) {
                    const auto &message = logger.messages()[i];
                    switch (message.m_level) {
                    case Log::ReportLevel::REPORT_LOG:
                        if (m_logging_level != Log::ReportLevel::REPORT_LOG)
                            break;
                        clipboard_text += std::format("[INFO]    - {}", message.m_message.c_str());
                        break;
                    case Log::ReportLevel::REPORT_WARNING:
                        if (m_logging_level == Log::ReportLevel::REPORT_DEBUG ||
                            m_logging_level == Log::ReportLevel::REPORT_ERROR)
                            break;
                        clipboard_text += std::format("[WARNING] - {}", message.m_message.c_str());
                        break;
                    case Log::ReportLevel::REPORT_ERROR:
                        if (m_logging_level == Log::ReportLevel::REPORT_DEBUG)
                            break;
                        clipboard_text += std::format("[ERROR]   - {}", message.m_message.c_str());
                        break;
                    case Log::ReportLevel::REPORT_DEBUG:
                        clipboard_text += std::format("[DEBUG]   - {}", message.m_message.c_str());
                        break;
                    }
                    if (i < logger.messages().size() - 1)
                        clipboard_text += "\n";
                }
                SystemClipboard::instance().setText(clipboard_text);
            }

            if (ImGui::MenuItem("Clear")) {
                logger.clear();
            }

            ImGui::EndMenuBar();
        }
    }

    void LoggingWindow::onRenderBody(TimeStep delta_time) {
        ImFont *mono_font =
            GUIApplication::instance().getFontManager().getFont("NanumGothicCoding-Bold", 12.0f);
        if (mono_font) {
            ImGui::PushFont(mono_font);
        }

        if (ImGui::BeginChild(ImWindowComponentTitle(*this, "Log View").c_str(), {}, false,
                              ImGuiWindowFlags_AlwaysUseWindowPadding)) {
            bool is_auto_scroll_mode = ImGui::GetScrollMaxY() - ImGui::GetScrollY() < 12.0f;

            auto &messages = Log::AppLogger::instance().messages();
            size_t begin   = (size_t)std::max<s64>(
                0, messages.size() - 5000);  // Have up to 5000 messages at once displayed
            for (size_t i = begin; i < messages.size(); ++i) {
                auto &message = messages[i];
                switch (message.m_level) {
                case Log::ReportLevel::REPORT_LOG:
                    if (m_logging_level != Log::ReportLevel::REPORT_LOG)
                        break;
                    ImGui::TextColored({0.2f, 0.9f, 0.3f, 1.0f}, "[LOG]     - %s",
                                       message.m_message.c_str());
                    break;
                case Log::ReportLevel::REPORT_WARNING:
                    if (m_logging_level == Log::ReportLevel::REPORT_DEBUG ||
                        m_logging_level == Log::ReportLevel::REPORT_ERROR)
                        break;
                    ImGui::TextColored({0.7f, 0.5f, 0.1f, 1.0f}, "[WARNING] - %s",
                                       message.m_message.c_str());
                    break;
                case Log::ReportLevel::REPORT_ERROR:
                    if (m_logging_level == Log::ReportLevel::REPORT_DEBUG)
                        break;
                    ImGui::TextColored({0.9f, 0.2f, 0.1f, 1.0f}, "[ERROR]   - %s",
                                       message.m_message.c_str());
                    break;
                case Log::ReportLevel::REPORT_DEBUG:
                    ImGui::TextColored({0.3f, 0.4f, 0.9f, 1.0f}, "[DEBUG]   - %s",
                                       message.m_message.c_str());
                    break;
                }
            }

            if (m_scroll_requested) {
                if (is_auto_scroll_mode)
                    ImGui::SetScrollHereY(1.0f);
                m_scroll_requested = false;
            }
        }
        ImGui::EndChild();

        if (mono_font) {
            ImGui::PopFont();
        }
    }

}  // namespace Toolbox::UI