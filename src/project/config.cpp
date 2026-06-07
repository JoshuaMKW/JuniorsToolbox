#pragma once

#include <fstream>
#include <gcm.hpp>

#include "jsonlib.hpp"
#include "project/config.hpp"

namespace Toolbox {

    Result<void, SerialError> ProjectConfig::loadFromFile(const fs_path &path) {
        std::ifstream in_stream = std::ifstream(path, std::ios::in);
        if (in_stream.is_open() == false) {
            return make_serial_error<void>("PROJECT_CONFIG",
                                           "Failed to open project config file for reading.", 0,
                                           path.string());
        }

        json_t profile_json;
        auto result = tryJSON(profile_json, [&](json_t &j) {
            in_stream >> j;

            // General
            m_project_game_code  = JSONValueOr(j, "Project Game Code", static_cast<u32>('GMSE'));
            m_project_maker_code = JSONValueOr(j, "Project Maker Code", static_cast<u16>('01'));
            m_project_name       = JSONValueOr(j, "Project Name", "Unknown Project");
            m_project_version    = JSONValueOr(j, "Project Version", "v1.0.0");
            m_author_name        = JSONValueOr(j, "Author Name", "Unknown Author");
            m_description        = JSONValueOr(j, "Description", "");

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
            j["Project Game Code"]  = m_project_game_code;
            j["Project Maker Code"] = m_project_maker_code;
            j["Project Name"]       = m_project_name;
            j["Project Version"]    = m_project_version;
            j["Author Name"]        = m_author_name;
            j["Description"]        = m_description;
            j["Pinned Folders"]     = m_pinned_folders;
            j["Enable BetterSMS"]   = m_bettersms_enabled;
            j["BetterSMS Modules"]  = m_bettersms_modules;
        });

        if (!result) {
            JSONError &err = result.error();
            return make_serial_error<void>(err.m_message[0], err.m_reason, err.m_byte,
                                           path.string());
        }

        out_stream << config_json;
        return Result<void, SerialError>();
    }

    Result<void, FSError> ProjectConfig::initFromProjectRoot(const fs_path &path) {
        const fs_path bnr_path  = (path / "files/opening.bnr").lexically_normal();
        const fs_path boot_path = (path / "sys/boot.bin").lexically_normal();

        if (!Filesystem::is_regular_file(bnr_path).value_or(false)) {
            return make_fs_error<void>(
                std::error_code(),
                {"[PROJECT_CONFIG] Failed to find files/opening.bnr for config initialization!"});
        }

        if (!Filesystem::is_regular_file(boot_path).value_or(false)) {
            return make_fs_error<void>(
                std::error_code(),
                {"[PROJECT_CONFIG] Failed to find sys/boot.bin for config initialization!"});
        }

        ScopePtr<gcm::BNRData> bnr     = gcm::BNRData::FromFile(bnr_path.string());
        ScopePtr<gcm::BootSector> boot = gcm::BootSector::FromFile(boot_path.string());

        m_project_game_code  = boot->GetGameCode();
        m_project_maker_code = boot->GetMakerCode();
        m_project_version    = std::format("v1.{}.0", boot->GetDiskVersion());
        // m_project_disk_version = boot->GetDiskVersion();
        m_project_name = bnr->GetGameName();
        // m_project_title = bnr->GetGameTitle();
        m_author_name = bnr->GetDeveloperName();
        // m_author_title = bnr->GetDeveloperTitle();
        m_description = bnr->GetGameDescription();

        m_pinned_folders = {"files/data/scene"};

        return {};
    }

    u32 ProjectConfig::getProjectGameCode() const { return m_project_game_code; }

    void ProjectConfig::setProjectGameCode(u32 code) { m_project_game_code = code; }

    u16 ProjectConfig::getProjectMakerCode() const { return m_project_maker_code; }

    void ProjectConfig::setProjectMakerCode(u16 code) { m_project_maker_code = code; }

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