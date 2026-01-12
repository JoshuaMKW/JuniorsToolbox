#pragma once

#include "fsystem.hpp"
#include "serial.hpp"
#include <nlohmann/json.hpp>

namespace Toolbox {

    struct BetterSMSModuleInfo {
        std::string name;
        std::string version;
    };

    inline void from_json(const nlohmann::json &j, BetterSMSModuleInfo &info) {
        if (j.contains("Name"))
            j.at("Name").get_to(info.name);
        if (j.contains("Version"))
            j.at("Version").get_to(info.version);
    }

    inline void to_json(nlohmann::json &j, const BetterSMSModuleInfo &info) {
        j["Name"]    = info.name;
        j["Version"] = info.version;
    }

    class ProjectConfig {
    public:
        using json_t = nlohmann::json;

        ProjectConfig()  = default;
        ~ProjectConfig() = default;

        Result<void, SerialError> loadFromFile(const fs_path &path);
        Result<void, SerialError> saveToFile(const fs_path &path) const;

        // Metadata

        const std::string &getProjectName() const;
        void setProjectName(const std::string &name);

        const std::string &getProjectVersion() const;
        void setProjectVersion(const std::string &version);

        const std::string &getAuthorName() const;
        void setAuthorName(const std::string &author);

        const std::string &getDescription() const;
        void setDescription(const std::string &description);

        //

        // Project State

        const std::vector<fs_path> &getPinnedFolders() const;
        void setPinnedFolders(const std::vector<fs_path> &folders);

        //

        // BetterSMS settings

        bool isBetterSMSEnabled() const;
        void setBetterSMSEnabled(bool enabled);

        const std::vector<BetterSMSModuleInfo> &getBetterSMSModules() const;
        void setBetterSMSModules(const std::vector<BetterSMSModuleInfo> &modules);

        //

    private:
        std::string m_project_name    = "Unknown Project";
        std::string m_project_version = "v1.0.0";
        std::string m_author_name     = "Unknown Author";
        std::string m_description     = "";

        std::vector<fs_path> m_pinned_folders = {"files/data/scene"};

        bool m_bettersms_enabled = false;
        std::vector<BetterSMSModuleInfo> m_bettersms_modules;
    };

}  // namespace Toolbox