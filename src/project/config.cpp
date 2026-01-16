#pragma once

#include <fstream>

#include "jsonlib.hpp"
#include "project/config.hpp"

namespace Toolbox {

    Result<void, SerialError> ProjectConfig::loadFromFile(const fs_path &path) {
        std::ifstream in_stream = std::ifstream(path, std::ios::in);
        if (in_stream.is_open() == false) {
            return make_serial_error<void>("PROJECT_CONFIG", "Failed to open project config file for reading.", 0,
                                           path.string());
        }

        json_t profile_json;
        auto result = tryJSON(profile_json, [&](json_t &j) {
            in_stream >> j;

            // General
            m_project_name    = JSONValueOr(j, "Project Name", "Unknown Project");
            m_project_version = JSONValueOr(j, "Project Version", "v1.0.0");
            m_author_name     = JSONValueOr(j, "Author Name", "Unknown Author");
            m_description     = JSONValueOr(j, "Description", "");

            m_pinned_folders = JSONValueOr(j, "Pinned Folders", std::vector<fs_path>{});

            m_bettersms_enabled = JSONValueOr(j, "Enable BetterSMS", false);
            m_bettersms_modules =
                JSONValueOr(j, "BetterSMS Modules", std::vector<BetterSMSModuleInfo>());
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message[0], err.m_reason, err.m_byte,
                                           path.string());
        }

        return Result<void, SerialError>();
    }

    Result<void, SerialError> ProjectConfig::saveToFile(const fs_path &path) const {
        std::ofstream out_stream(path, std::ios::out);

        json_t config_json;
        auto result = tryJSON(config_json, [&](json_t &j) {
            j["Project Name"]      = m_project_name;
            j["Project Version"]   = m_project_version;
            j["Author Name"]       = m_author_name;
            j["Description"]       = m_description;
            j["Pinned Folders"]    = m_pinned_folders;
            j["Enable BetterSMS"]  = m_bettersms_enabled;
            j["BetterSMS Modules"] = m_bettersms_modules;
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message[0], err.m_reason, err.m_byte,
                                           path.string());
        }

        out_stream << config_json;
        return Result<void, SerialError>();
    }

    const std::string &ProjectConfig::getProjectName() const { return m_project_name; }

    void ProjectConfig::setProjectName(const std::string &name) { m_project_name = name; }

    const std::string &ProjectConfig::getProjectVersion() const { return m_project_version; }

    void ProjectConfig::setProjectVersion(const std::string &version) {
        m_project_version = version;
    }

    const std::string &ProjectConfig::getAuthorName() const { return m_author_name; }

    void ProjectConfig::setAuthorName(const std::string &author) { m_author_name = author; }

    const std::string &ProjectConfig::getDescription() const { return m_description; }

    void ProjectConfig::setDescription(const std::string &description) {
        m_description = description;
    }

    const std::vector<fs_path> &ProjectConfig::getPinnedFolders() const { return m_pinned_folders; }

    void ProjectConfig::setPinnedFolders(const std::vector<fs_path> &folders) {
        m_pinned_folders = folders;
    }

    bool ProjectConfig::isBetterSMSEnabled() const { return m_bettersms_enabled; }

    void ProjectConfig::setBetterSMSEnabled(bool enabled) { m_bettersms_enabled = enabled; }

    const std::vector<BetterSMSModuleInfo> &ProjectConfig::getBetterSMSModules() const {
        return m_bettersms_modules;
    }

    void ProjectConfig::setBetterSMSModules(const std::vector<BetterSMSModuleInfo> &modules) {
        m_bettersms_modules = modules;
    }

}  // namespace Toolbox