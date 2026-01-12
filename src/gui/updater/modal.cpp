#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <IconsForkAwesome.h>
#include <imgui.h>
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui_markdown.h>

#include <netpp/http/router.h>
#include <netpp/netpp.h>
#include <netpp/tls/security.h>

#include "gui/application.hpp"
#include "gui/updater/modal.hpp"
#include "platform/webbrowser.hpp"

#define CLIENT_HOST     "api.github.com"
#define CLIENT_PORT     "443"
#define CLIENT_KEY_NAME "client_key.pem"

static uint32_t VersionTagToUIntFIeld(const std::string &version_tag) {
    uint8_t major = 0, minor = 0, patch = 0;
    (void)sscanf(version_tag.c_str(), "v%hhu.%hhu.%hhu", &major, &minor, &patch);
    return (major << 16) | (minor << 8) | patch;
}

static void LinkCallback(ImGui::MarkdownLinkCallbackData data);
static ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data);
static void LoadFonts();
static void Markdown(const std::string &markdown_);

static ImFont *T   = nullptr;
static ImFont *Ti  = nullptr;
static ImFont *B   = nullptr;
static ImFont *Bi  = nullptr;
static ImFont *H1  = nullptr;
static ImFont *H1i = nullptr;
static ImFont *H2  = nullptr;
static ImFont *H2i = nullptr;
static ImFont *H3  = nullptr;
static ImFont *H3i = nullptr;

static ImGui::MarkdownConfig md_config;

static netpp::HTTP_Request *BounceTheRequest(const netpp::HTTP_Response *response,
                                             netpp::HTTP_Request *request) {
    if (!response->has_header("Location")) {
        return nullptr;
    }

    std::string location = response->get_header_value("Location");

    char hostname_buf[64];
    char pathname_buf[256];
    netpp::http_get_hostname_and_path_from_str(location.c_str(), hostname_buf, sizeof(hostname_buf),
                                               pathname_buf, sizeof(pathname_buf));

    std::string hostname(hostname_buf);
    std::string pathname(pathname_buf);

    netpp::HTTP_Request *new_request = netpp::HTTP_Request::create(request->method());
    new_request->set_version(request->version());
    new_request->set_path(pathname);

    for (const std::string &query : request->queries()) {
        new_request->add_query(query);
    }

    for (const netpp::HTTP_Request::HeaderPair &header : request->headers()) {
        new_request->set_header(header.first, header.second);
    }

    new_request->set_header("Host", hostname);
    return new_request;
}

namespace Toolbox::UI {

    static UpdaterModal::GitHubReleaseInfo
    CreateGitHubReleaseInfoFromJSON(const UpdaterModal::json_t &j) {
        UpdaterModal::GitHubReleaseInfo info;
        tryJSON(j, [&](const UpdaterModal::json_t &in_json) {
            info.m_version  = in_json["tag_name"];
            info.m_title    = in_json["name"];
            info.m_notes    = in_json["body"];
            info.m_page_url = in_json["html_url"];
            if (!in_json["assets"].empty() && in_json["assets"].is_array()) {
                info.m_download_url = in_json["assets"][0]["browser_download_url"];
            }
        });
        return info;
    }

    void UpdaterModal::onAttach() {
        ImModal::onAttach();

        const fs_path credentials_path = GUIApplication::instance().getAppDataPath() / ".updater";
        const fs_path key_file         = credentials_path / CLIENT_KEY_NAME;

        if (!Filesystem::exists(key_file).value_or(false)) {
            Filesystem::create_directories(key_file.parent_path());
            if (!netpp::generate_client_key_rsa_4096(key_file, "", "US", "netpp webfetch")) {
                fprintf(stderr, "Failed to generate self-signed certificate\n");
                m_is_valid = false;
            }
        }

        if (!m_is_valid) {
            return;
        }

        m_tls_factory = make_referable<netpp::TLSSecurityFactory>(
            false, key_file.c_str(), "", "", CLIENT_HOST, "", netpp::ETLSVerifyFlags::VERIFY_PEER);

        m_http_github_client = make_referable<netpp::TCP_Client>(m_tls_factory.get());
        if (!m_http_github_client->start()) {
            TOOLBOX_ERROR("[UPDATER] Failed to start HTTPS client for updater!");
            m_is_valid = false;
            close();
            return;
        }

        const char *hostname = netpp::get_ip_address_info(CLIENT_HOST).m_ipv4;
        if (!m_http_github_client->connect(hostname, CLIENT_PORT)) {
            TOOLBOX_ERROR("[UPDATER] Failed to connect HTTPS client for updater!");
            m_is_valid = false;
            close();
            return;
        } else {
            TOOLBOX_INFO(
                "[UPDATER] Successfully connected HTTPS client to api.github.com for updates!");
        }

        m_http_github_client->on_http_response(
            [&](const netpp::ISocketPipe *source,
                const netpp::HTTP_Response *response) -> netpp::HTTP_Request * {
                if (response->status_code() ==
                        netpp::EHTTP_ResponseStatusCode::E_STATUS_MOVED_PERMANENTLY ||
                    response->status_code() == netpp::EHTTP_ResponseStatusCode::E_STATUS_FOUND) {
                    netpp::HTTP_Request *bounced =
                        BounceTheRequest(response, m_http_releases_request);
                    if (!bounced) {
                        TOOLBOX_ERROR("[UPDATER] Failed to redirect the HTTPS client to the target "
                                      "location!");
                        m_is_valid = false;
                        // send_cv.notify_one();
                        return nullptr;
                    }
                    return bounced;
                }

                if (response->status_code() != netpp::EHTTP_ResponseStatusCode::E_STATUS_OK) {
                    TOOLBOX_ERROR_V("[UPDATER] Received invalid response code: {}",
                                    static_cast<int>(response->status_code()));
                    m_is_valid = false;
                    return nullptr;
                }

                const AppSettings &cur_settings =
                    GUIApplication::instance().getSettingsManager().getCurrentProfile();

                json_t body_json = json_t::parse(response->body());
                if (!body_json.is_array()) {
                    TOOLBOX_ERROR("[UPDATER] Received invalid JSON response for releases!");
                    m_is_valid = false;
                    return nullptr;
                }

                uint32_t version_mask = ~0;
                switch (cur_settings.m_update_frequency) {
                case UpdateFrequency::NEVER:
                    close();  // Should never reach here anyway since GUIApplication short circuits.
                    return nullptr;
                case UpdateFrequency::MAJOR:
                    version_mask &= ~0xFFFF;
                    break;
                case UpdateFrequency::MINOR:
                    version_mask &= ~0xFF;
                    break;
                case UpdateFrequency::PATCH:
                    break;
                }

                uint32_t current_version_int =
                    VersionTagToUIntFIeld(TOOLBOX_VERSION_TAG) & version_mask;
                for (const json_t &release_json : body_json) {
                    GitHubReleaseInfo release_info = CreateGitHubReleaseInfoFromJSON(release_json);
                    uint32_t release_version_int =
                        VersionTagToUIntFIeld(release_info.m_version) & version_mask;
                    if (release_version_int > current_version_int) {
                        m_release_infos.emplace_back(release_info);
                    }
                }

                if (m_release_infos.empty()) {
                    close();
                    return nullptr;
                }

                open();
                m_is_valid = true;
                return nullptr;
            });

        m_http_releases_request =
            netpp::HTTP_Request::create(netpp::EHTTP_RequestMethod::E_REQUEST_GET);

        m_http_releases_request->set_version("1.1");
        m_http_releases_request->set_path("/repos/DotKuribo/BetterSunshineEngine/releases");
        m_http_releases_request->set_header("Host", CLIENT_HOST);
        m_http_releases_request->set_header("Connection", "keep-alive");
        m_http_releases_request->set_header("Accept", "application/vnd.github+json");
        m_http_releases_request->set_header("X-GitHub-Api-Version", "2022-11-28");
        m_http_releases_request->set_header("Upgrade-Insecure-Requests", "1");
        m_http_releases_request->set_header("User-Agent", "JuniorsToolbox/0.0.1");

        m_http_github_client->send(m_http_releases_request);

        ResourceManager &manager = GUIApplication::instance().getResourceManager();
        UUID64 image_dir_id      = manager.getResourcePathUUID("Images");

        m_github_logo =
            manager.getImageHandle("github_logo_error.png", image_dir_id).value_or(nullptr);
    }

    void UpdaterModal::onDetach() {
        if (m_http_github_client->is_running()) {
            TOOLBOX_INFO("[UPDATER] Stopping HTTPS client for updater...");
            m_http_github_client->stop();
        }
    }

    void UpdaterModal::onWindowEvent(RefPtr<WindowEvent> ev) {
        ImModal::onWindowEvent(ev);
        if (ev->isHandled()) {
            return;
        }
    }

    void UpdaterModal::onRenderBody(TimeStep delta_time) {
        const bool valid_state = m_is_valid && !m_release_infos.empty();

        ImVec2 avail_size = ImGui::GetContentRegionAvail();

        FontManager &font_manager = FontManager::instance();
        float font_size           = font_manager.getCurrentFontSize();
        ImGui::PushFont(nullptr, font_size * 2.0f);

        if (valid_state) {
            ImGui::Text("A new update (%s) is available!", m_release_infos[0].m_version.c_str());
        }

        float buttons_height     = ImGui::GetFrameHeight();
        float subtractive_height = ImGui::GetFrameHeightWithSpacing();

        ImGui::PopFont();

        ImVec2 panel_size = ImGui::GetContentRegionAvail();
        panel_size.y -= subtractive_height;

        if (ImGui::BeginChild("UpdatesPanel", panel_size,
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX |
                                  ImGuiChildFlags_AutoResizeY,
                              ImGuiWindowFlags_NoCollapse)) {
            if (!m_is_valid) {
                if (m_github_logo) {
                    ImagePainter p;

                    float aspect_ratio =
                        (float)m_github_logo->size().first / (float)m_github_logo->size().second;
                    float logo_width  = avail_size.x * 0.8f;
                    float logo_height = logo_width / aspect_ratio;

                    ImVec2 pos = ImGui::GetCursorScreenPos();
                    pos.x += (avail_size.x / 2 - logo_width / 2);
                    pos.y += (avail_size.y / 2 - logo_height / 2);

                    p.setTintColor({1.0f, 1.0f, 1.0f, 0.5f});
                    // p.render(*m_github_logo, ImGui::GetCursorScreenPos(),
                    //          ImGui::GetContentRegionAvail());
                    p.render(*m_github_logo, pos, ImVec2(logo_width, logo_height));
                }
                ImGui::EndChild();
                return;
            }

            size_t i = 0;
            for (const GitHubReleaseInfo &info : m_release_infos) {
                std::string info_title = std::format("{} - {}", info.m_version, info.m_title);

                ImGui::PushFont(nullptr, font_size * 2.0f);
                if (ImGui::TextLink(info_title.c_str())) {
                    Platform::TryOpenBrowserURL(info.m_page_url);
                }

                ImGui::SameLine();

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() / 2);
                ImGui::Separator();
                ImGui::PopFont();

                renderGitHubReleaseInfo(info);
            }
        }
        ImGui::EndChild();

        ImGui::PushFont(nullptr, font_size * 2.0f);

        if (valid_state) {
            if (ImGui::Button("Maybe Later")) {
                close();
            }

            ImGui::SameLine();

            if (ImGui::Button("Download Update")) {
                Platform::TryOpenBrowserURL(m_release_infos[0].m_download_url);
                close();
            }
        }

        ImGui::PopFont();
    }

    void UpdaterModal::renderGitHubReleaseInfo(const GitHubReleaseInfo &info) {
        const ImGuiStyle &style = ImGui::GetStyle();

        std::string info_title = std::format("{} - {}", info.m_version, info.m_title);
        float info_width       = ImGui::GetContentRegionAvail().x;
        if (ImGui::BeginChild(info_title.c_str(), ImVec2(info_width, 0.0f),
                              ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoDecoration)) {
            if (ImGui::IsWindowHovered()) {
                ImVec2 win_min = ImGui::GetWindowPos();
                ImVec2 win_max = win_min + ImGui::GetWindowSize();

                ImGui::RenderFrame(win_min, win_max,
                                   ImGui::ColorConvertFloat4ToU32(
                                       ImGui::GetStyleColorVec4(ImGuiCol_TableRowBgAlt)),
                                   false);
            }
            ImGui::NewLine();
            Markdown(info.m_notes);
            ImGui::NewLine();
        }
        ImGui::EndChild();
    }

}  // namespace Toolbox::UI

void LinkCallback(ImGui::MarkdownLinkCallbackData data) {
    std::string url(data.link, data.linkLength);
    if (!data.isImage) {
        Toolbox::Platform::TryOpenBrowserURL(url);
    }
}

ImGui::MarkdownImageData ImageCallback(ImGui::MarkdownLinkCallbackData data) {
    if (!data.isImage) {
        return ImGui::MarkdownImageData{};
    }

    std::string url(data.link, data.linkLength);
    // TODO: Load image from URL

#ifdef IMGUI_HAS_TEXTURES
    ImTextureID image = ImGui::GetIO().Fonts->TexRef.GetTexID();
#else
    ImTextureID image = ImGui::GetIO().Fonts->TexID;
#endif

    ImGui::MarkdownImageData image_data;
    image_data.isValid         = true;
    image_data.useLinkCallback = false;
    image_data.user_texture_id = image;
    image_data.size            = ImVec2(40.0f, 20.0f);

    const ImVec2 avail_size = ImGui::GetContentRegionAvail();
    if (image_data.size.x > avail_size.x) {
        float const ratio = image_data.size.y / image_data.size.x;
        image_data.size.x = avail_size.x;
        image_data.size.y = avail_size.x * ratio;
    }

    return image_data;
}

void LoadFonts() {
    static bool s_loaded = false;
    if (s_loaded) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();

    static ImFontConfig cfg;
    cfg.MergeMode = true;
    cfg.FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
    cfg.PixelSnapH  = true;
    cfg.OversampleH = 1;  // Disable oversampling for bitmap fonts (emojis are bitmaps)
    cfg.OversampleV = 1;

    // 0x1F300 - 0x1F9FF covers most common emojis (Supplemental Symbols and Pictographs)
    static const ImWchar emoji_ranges[] = {0x1, 0x1FFFF, 0};

    fs_path cwd = Filesystem::current_path().value_or("./");
    // Base font
    T = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-Medium.ttf", 16.0f);
    io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/sequiemj.ttf", 0.0f, &cfg, emoji_ranges);

    Ti = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-MediumItalic.ttf", 16.0f);
    io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/sequiemj.ttf", 0.0f, &cfg, emoji_ranges);

    // Base font
    B = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-Bold.ttf", 16.0f);
    io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/sequiemj.ttf", 0.0f, &cfg, emoji_ranges);

    Bi = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-BoldItalic.ttf", 16.0f);
    io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/sequiemj.ttf", 0.0f, &cfg, emoji_ranges);

    // Bold headings H2 and H3
    H2 = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-Bold.ttf", 20.0f);
    io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/sequiemj.ttf", 0.0f, &cfg, emoji_ranges);

    H2i = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-BoldItalic.ttf", 20.0f);
    io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/sequiemj.ttf", 0.0f, &cfg, emoji_ranges);

    H3 = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-Bold.ttf", 18.0f);
    io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/sequiemj.ttf", 0.0f, &cfg, emoji_ranges);

    H3i = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-BoldItalic.ttf", 18.0f);
    io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/sequiemj.ttf", 0.0f, &cfg, emoji_ranges);

// bold heading H1
#ifdef IMGUI_HAS_TEXTURES  // used to detect dynamic font capability
    H1  = H2;              // size can be set in headingFormats
    H1i = H2i;
#else
    float font_sizeH1 = font_size * 1.1f;
    H1 = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-Medium.ttf", font_sizeH1);
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 0.0f, &cfg, emoji_ranges);

    H1i = io.Fonts->AddFontFromFileTTF("./Fonts/Markdown/Roboto-MediumItalic.ttf", font_sizeH1);
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", 0.0f, &cfg, emoji_ranges);
#endif

    s_loaded = true;
}

void ExampleMarkdownFormatCallback(const ImGui::MarkdownFormatInfo &markdownFormatInfo_,
                                   bool start_) {
    // Call the default first so any settings can be overwritten by our implementation.
    // Alternatively could be called or not called in a switch statement on a case by case basis.
    // See defaultMarkdownFormatCallback definition for furhter examples of how to use it.
    ImGui::defaultMarkdownFormatCallback(markdownFormatInfo_, start_);

    float font_size = FontManager::instance().getCurrentFontSize();

    switch (markdownFormatInfo_.type) {
    // example: change the colour of heading level 2
    case ImGui::MarkdownFormatType::HEADING:
        break;
    case ImGui::MarkdownFormatType::NORMAL_TEXT:
    case ImGui::MarkdownFormatType::UNORDERED_LIST:
    //case ImGui::MarkdownFormatType::ORDERED_LIST:
    case ImGui::MarkdownFormatType::LINK: {
        if (start_)
            ImGui::PushFont(T, font_size);
        else
            ImGui::PopFont();
        break;
    }
    case ImGui::MarkdownFormatType::EMPHASIS: {
        if (start_) {
            ImFont *f = T;
            if (markdownFormatInfo_.level == 1)
                f = Ti;
            if (markdownFormatInfo_.level == 2)
                f = B;
            if (markdownFormatInfo_.level == 3)
                f = Bi;
            ImGui::PushFont(f, font_size);
        } else {
            ImGui::PopFont();
        }
        break;
    }
    default: {
        break;
    }
    }
}

void Markdown(const std::string &markdown_) {
    LoadFonts();

    float font_size = FontManager::instance().getCurrentFontSize();

    // You can make your own Markdown function with your prefered string container and markdown
    // config. > C++14 can use ImGui::MarkdownConfig md_config{ LinkCallback, NULL, ImageCallback,
    // ICON_FA_LINK, { { H1, true }, { H2, true }, { H3, false } }, NULL };
    //md_config.useComplexFormatting = false;

    md_config.linkCallback    = LinkCallback;
    md_config.tooltipCallback = NULL;
    md_config.imageCallback   = ImageCallback;
    md_config.linkIcon        = ICON_FA_LINK;
#ifdef IMGUI_HAS_TEXTURES  // used to detect dynamic font capability
    md_config.headingFormats[0] = {H1, true, font_size * 2.0f};
    md_config.headingFormats[1] = {H2, true, font_size * 1.5f};
    md_config.headingFormats[2] = {H3, false, font_size * 1.17f};
#else
    md_config.headingFormats[0] = {H1, true};
    md_config.headingFormats[1] = {H2, true};
    md_config.headingFormats[2] = {H3, false};
#endif

    md_config.userData       = NULL;
    md_config.formatCallback = ExampleMarkdownFormatCallback;
    md_config.formatFlags    = ImGuiMarkdownFormatFlags_GithubStyle | ImGuiMarkdownFormatFlags_IncludeAllListPrefixes | ImGuiMarkdownFormatFlags_ExoticIndents;

    const char *text         = R"(   ## Fixes/Repairs

- dwadw
    + fwefw
          * fe4wf
                             *- fewf*
        - kfeok
              - r3r33r
        # fefef
    - sefsfe
**FA WEF A FAWF**

*fejoi*)";

    ImGui::Markdown(text, strlen(text), md_config);
}