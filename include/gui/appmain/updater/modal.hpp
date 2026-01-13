#pragma once

#include "gui/modal.hpp"

#include <atomic>
#include <nlohmann/json.hpp>
#include <netpp/client.h>

namespace Toolbox::UI {

    class UpdaterModal final : public ImModal {
    public:
        using json_t = nlohmann::json;

        explicit UpdaterModal(const std::string &name) : ImModal(name), m_is_valid(true) {}
        ~UpdaterModal() override = default;

        [[nodiscard]] std::string context() const override { return TOOLBOX_VERSION_TAG " (GitHub REST API)"; }

        [[nodiscard]] std::optional<ImVec2> defaultSize() const override { return minSize(); }
        [[nodiscard]] std::optional<ImVec2> minSize() const override {
            float font_size = FontManager::instance().getCurrentFontSize();
            return ImVec2{400.0f * (font_size / 14.0f), 600.0f * (font_size / 14.0f)};
        }
        [[nodiscard]] std::optional<ImVec2> maxSize() const override { return minSize(); }

        void onAttach() override;
        void onDetach() override;
        void onWindowEvent(RefPtr<WindowEvent> ev) override;

        void onRenderBody(TimeStep delta_time) override final;

        struct GitHubReleaseInfo {
            std::string m_version;
            std::string m_title;
            std::string m_notes;
            std::string m_page_url;
            std::string m_download_url;
        };

    protected:
        void renderGitHubReleaseInfo(const GitHubReleaseInfo &info);

    private:
        RefPtr<netpp::TLSSecurityFactory> m_tls_factory;
        RefPtr<netpp::TCP_Client> m_http_github_client;
        netpp::HTTP_Request *m_http_releases_request;
        netpp::HTTP_Request *m_http_download_request;
        std::vector<GitHubReleaseInfo> m_release_infos;

        std::atomic<bool> m_is_valid;

        RefPtr<const ImageHandle> m_github_logo;
    };

}  // namespace Toolbox::UI