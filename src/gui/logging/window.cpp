#include "gui/logging/window.hpp"
#include "gui/font.hpp"

static std::string s_message_pool;

namespace Toolbox::UI {
    void LoggingWindow::appendMessageToPool(const Log::AppLogger::LogMessage &message) {
        switch (message.m_level) {
        case Log::ReportLevel::LOG:
            s_message_pool += std::format("[LOG]     - {:>{}}", message.m_message,
                                          message.m_indentation * 4 + message.m_message.size());
            break;
        case Log::ReportLevel::WARNING:
            s_message_pool += std::format("[WARNING] - {:>{}}", message.m_message,
                                          message.m_indentation * 4 + message.m_message.size());
            break;
        case Log::ReportLevel::ERROR:
            s_message_pool += std::format("[ERROR]   - {:>{}}", message.m_message,
                                          message.m_indentation * 4 + message.m_message.size());
            break;
        case Log::ReportLevel::DEBUG:
            s_message_pool += std::format("[DEBUG]   - {:>{}}", message.m_message,
                                          message.m_indentation * 4 + message.m_message.size());
            break;
        }
    }

    void LoggingWindow::renderMenuBar() {
        auto &logger = Log::AppLogger::instance();
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("Verbosity", true)) {
                bool info_active = m_logging_level == Log::ReportLevel::INFO;
                bool warn_active = m_logging_level == Log::ReportLevel::WARNING;
                bool error_active = m_logging_level == Log::ReportLevel::ERROR;
                if (ImGui::Checkbox("Info", &info_active)) {
                    m_logging_level = Log::ReportLevel::INFO;
                }
                if (ImGui::Checkbox("Warning", &warn_active)) {
                    m_logging_level = Log::ReportLevel::WARNING;
                }
                if (ImGui::Checkbox("Error", &error_active)) {
                    m_logging_level = Log::ReportLevel::ERROR;
                }
                ImGui::EndMenu();
            }

            ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});

            if (ImGui::Button("Copy")) {
                std::string clipboard_text;
                for (size_t i = 0; i < logger.messages().size(); ++i) {
                    const auto &message = logger.messages()[i];
                    switch (message.m_level) {
                    case Log::ReportLevel::LOG:
                        if (m_logging_level != Log::ReportLevel::LOG)
                            break;
                        clipboard_text +=
                            std::format("[INFO]    - {}", message.m_message.c_str());
                        break;
                    case Log::ReportLevel::WARNING:
                        if (m_logging_level == Log::ReportLevel::DEBUG ||
                            m_logging_level == Log::ReportLevel::ERROR)
                            break;
                        clipboard_text += std::format("[WARNING] - {}", message.m_message.c_str());
                        break;
                    case Log::ReportLevel::ERROR:
                        if (m_logging_level == Log::ReportLevel::DEBUG)
                            break;
                        clipboard_text += std::format("[ERROR]   - {}", message.m_message.c_str());
                        break;
                    case Log::ReportLevel::DEBUG:
                        clipboard_text += std::format("[DEBUG]   - {}", message.m_message.c_str());
                        break;
                    }
                    if (i < logger.messages().size() - 1)
                        clipboard_text += "\n";
                }
                SystemClipboard::instance().setText(clipboard_text);
            }

            if (ImGui::Button("Clear")) {
                logger.clear();
            }

            ImGui::PopStyleColor();

            ImGui::EndMenuBar();
        }
    }

    void LoggingWindow::renderBody(f32 delta_time) {
        ImFont *mono_font = FontManager::instance().getFont("NanumGothicCoding-Bold", 14.0f);
        if (mono_font) {
            ImGui::PushFont(mono_font);
        }

        if (ImGui::BeginChild(getWindowChildUID(*this, "Log View").c_str(), {},
                              ImGuiWindowFlags_AlwaysUseWindowPadding)) {
            for (auto &message : Log::AppLogger::instance().messages()) {
                switch (message.m_level) {
                case Log::ReportLevel::LOG:
                    if (m_logging_level != Log::ReportLevel::LOG)
                        break;
                    ImGui::TextColored({0.2, 0.9, 0.3, 1.0}, "[LOG]     - %s",
                                       message.m_message.c_str());
                    break;
                case Log::ReportLevel::WARNING:
                    if (m_logging_level == Log::ReportLevel::DEBUG ||
                        m_logging_level == Log::ReportLevel::ERROR)
                        break;
                    ImGui::TextColored({0.7, 0.5, 0.1, 1.0}, "[WARNING] - %s",
                                       message.m_message.c_str());
                    break;
                case Log::ReportLevel::ERROR:
                    if (m_logging_level == Log::ReportLevel::DEBUG)
                        break;
                    ImGui::TextColored({0.9, 0.2, 0.1, 1.0}, "[ERROR]   - %s",
                                       message.m_message.c_str());
                    break;
                case Log::ReportLevel::DEBUG:
                    ImGui::TextColored({0.3, 0.4, 0.9, 1.0}, "[DEBUG]   - %s",
                                       message.m_message.c_str());
                    break;
                }
            }
        }
        ImGui::EndChild();

        if (mono_font) {
            ImGui::PopFont();
        }
    }

    bool LoggingWindow::update(f32 delta_time) { return true; }

}  // namespace Toolbox::UI