#include <IconsForkAwesome.h>

#include <netpp/http/router.h>
#include <netpp/netpp.h>
#include <netpp/tls/security.h>

#include <iostream>
#include <span>
#include <string>
#include <thread>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_internal.h>

#include <stb/stb_image.h>

#include "core/core.hpp"
#include "core/input/input.hpp"
#include "github/clone.hpp"
#include "github/credentials.hpp"
#include "gui/bootstrap/application.hpp"
#include "gui/dragdrop/dragdropmanager.hpp"
#include "gui/font.hpp"
#include "gui/imgui_ext.hpp"
#include "gui/logging/errors.hpp"
#include "gui/logging/window.hpp"

#include "gui/util.hpp"
#include "platform/service.hpp"
#include "project/config.hpp"

#include <nfd_glfw3.h>

// void ImGuiSetupTheme(bool, float);

constexpr float LEFT_PANEL_RATIO   = 0.65f;
constexpr float RIGHT_PANEL_RATIO  = 0.3f;
constexpr float MIDDLE_PANEL_RATIO = (1.0f - LEFT_PANEL_RATIO - RIGHT_PANEL_RATIO);

static Toolbox::UI::ProjectAgeCategory
GetAgeCategoryForProject(const system_clock::time_point &project_date) {
    const system_clock::time_point &time_now = system_clock::now();
    const year_month_day today_ymd           = year_month_day(floor<days>(time_now));

    const year_month_day yesterday_ymd  = sys_days{today_ymd} - days(1);
    const year_month_day last_week_ymd  = sys_days{today_ymd} - weeks(1);
    const year_month_day last_month_ymd = today_ymd - months(1);
    const year_month_day last_year_ymd  = today_ymd - years(1);

    const sys_days yesterday_tp  = yesterday_ymd;
    const sys_days last_week_tp  = last_week_ymd;
    const sys_days last_month_tp = last_month_ymd;
    const sys_days last_year_tp  = last_year_ymd;

    if (project_date >= yesterday_tp) {
        return ProjectAgeCategory::AGE_DAY;
    }

    if (project_date >= last_week_tp) {
        return ProjectAgeCategory::AGE_WEEK;
    }

    if (project_date >= last_month_tp) {
        return ProjectAgeCategory::AGE_MONTH;
    }

    if (project_date >= last_year_tp) {
        return ProjectAgeCategory::AGE_YEAR;
    }

    return ProjectAgeCategory::AGE_LONG_AGO;
}

// Returns a string in the following format: "YYYY-MM-DD HH:MM:SS (AM/PM)"
static std::string GetDateStringForProject(const system_clock::time_point &project_date) {
    if (project_date == system_clock::time_point::min()) {
        return "Never Opened";
    }

    std::time_t tt = system_clock::to_time_t(project_date);
    char buffer[64];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %I:%M:%S %p", std::localtime(&tt));
    return std::string(buffer);
}

namespace Toolbox {

    BootStrapApplication::BootStrapApplication()
        : m_github_passkey_str(), m_github_username_str(), m_repo_input_str(), m_repo_path_str(),
          m_search_str() {
        m_render_window = nullptr;
    }

    BootStrapApplication &BootStrapApplication::instance() {
        static BootStrapApplication _inst;
        return _inst;
    }

    void BootStrapApplication::onInit(int argc, const char **argv) {
        if (argc > 1) {
            if (strncmp(argv[1], "--no-bootstrap", 16) == 0) {
                setExitCode(EXIT_CODE_OK);
                stop();
                return;
            }
        }

        // Initialize GLFW
        if (!glfwInit()) {
            setExitCode(EXIT_CODE_FAILED_SETUP);
            stop();
            return;
        }

        // Initialize the AppData directory
        const fs_path &app_data_path = BootStrapApplication::getAppDataPath();
        if (!Filesystem::exists(app_data_path).value_or(false)) {
            if (!Filesystem::create_directories(app_data_path).value_or(false)) {
                std::string err =
                    TOOLBOX_FORMAT_FN("[INIT] Failed to create application data directory at {}",
                                      app_data_path.string());
                fprintf(stderr, err.c_str());
                setExitCode(EXIT_CODE_FAILED_SETUP);
                stop();
                return;
            }
        }

        // TODO: Load application settings

        // Initialize the resource manager
        {
            fs_path cwd = Filesystem::current_path().value();

            m_resource_manager.includeResourcePath(cwd / "Fonts", true);
            m_resource_manager.includeResourcePath(cwd / "Fonts/Markdown", true);
            m_resource_manager.includeResourcePath(cwd / "Images", true);
            m_resource_manager.includeResourcePath(cwd / "Images/Icons", true);
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_DEPTH_BITS, 32);
        glfwWindowHint(GLFW_SAMPLES, 4);

        m_render_window = glfwCreateWindow(
            800, 700, "Junior's Toolbox " TOOLBOX_VERSION_TAG " (Launcher)", nullptr, nullptr);
        if (m_render_window == nullptr) {
            glfwTerminate();
            setExitCode(EXIT_CODE_FAILED_SETUP);
            stop();
            return;
        }

        // Set window icon
        initializeIcon();

        glfwSetCharCallback(m_render_window, ImGui_ImplGlfw_CharCallback);
        glfwSetKeyCallback(m_render_window, Input::GLFWKeyCallback);
        glfwSetCursorPosCallback(m_render_window, Input::GLFWMousePositionCallback);
        glfwSetMouseButtonCallback(m_render_window, Input::GLFWMouseButtonCallback);
        glfwSetScrollCallback(m_render_window, Input::GLFWMouseScrollCallback);

        glfwMakeContextCurrent(m_render_window);
        gladLoadGL();
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glfwSwapInterval(1);

        // Initialize imgui
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(m_render_window, false);
        ImGui_ImplOpenGL3_Init("#version 150");

        ImGuiPlatformIO &platform_io      = ImGui::GetPlatformIO();
        platform_io.Platform_CreateWindow = ImGui_ImplGlfw_CreateWindow_Ex;
        platform_io.Renderer_RenderWindow = ImGui_ImplOpenGL3_RenderWindow_Ex;

        // DragDropManager::instance().initialize();
        // m_drag_drop_source_delegate = DragDropDelegateFactory::createDragDropSourceDelegate();
        // m_drag_drop_target_delegate = DragDropDelegateFactory::createDragDropTargetDelegate();

        if (!FontManager::instance().initialize()) {
            TOOLBOX_ERROR("[INIT] Failed to initialize font manager!");
        } else {
            FontManager::instance().setCurrentFont("Markdown/Roboto-Medium", 16.0f);
        }

        determineEnvironmentConflicts();
        hookClipboardIntoGLFW();

        // if (settings.m_update_frequency != UpdateFrequency::NEVER) {
        //     RefPtr<UpdaterModal> updater_win = createWindow<UpdaterModal>("Update Checker");
        //     if (updater_win) {
        //         updater_win->hide();
        //     }
        // }

        const fs_path projects_file = getAppDataPath() / ".ToolboxProjectsCache.json";
        m_project_model             = make_referable<ProjectModel>();
        m_project_model->initialize(projects_file);

        m_sort_filter_proxy = make_referable<ProjectModelSortFilterProxy>();
        m_sort_filter_proxy->setSourceModel(m_project_model);
    }

    void BootStrapApplication::onUpdate(TimeStep delta_time) {
        // Try to make sure we return an error if anything's fucky
        if (m_render_window == nullptr || glfwWindowShouldClose(m_render_window)) {
            stop();
            return;
        }

        // Render logic
        {
            glfwPollEvents();
            Input::UpdateInputState();

            render(delta_time);

            Input::PostUpdateInputState();
        }

        if (m_results_ready) {
            stop();
        }
    }

    void BootStrapApplication::onExit() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_render_window);
        glfwTerminate();

        DragDropManager::instance().shutdown();
        FontManager::instance().teardown();

        const bool has_project_path = m_project_path.has_value();
        if (has_project_path && Filesystem::is_directory(m_project_path.value()).value_or(false)) {
            m_results.m_windows.emplace_back(BootStrapArguments::WindowArguments{
                .m_window_type = "ProjectViewWindow",
                .m_load_path   = m_project_path,
                .m_vargs       = {},
            });
        }

        const bool has_scene_path      = m_scene_path.has_value();
        const bool is_scene_of_project = [&]() {
            if (!has_project_path || !has_scene_path) {
                return false;
            }
            const fs_path project_path = m_project_path.value().lexically_normal();
            const fs_path scene_path   = m_scene_path.value().lexically_normal();
            return scene_path.string().starts_with(project_path.string());
        }();

        if (is_scene_of_project) {
            m_results.m_windows.emplace_back(BootStrapArguments::WindowArguments{
                .m_window_type = "SceneWindow",
                .m_load_path   = m_scene_path,
                .m_vargs       = {},
            });
        }

        // Add/update project to the registry
        if (has_project_path) {
            ModelIndex existing_project = m_project_model->getIndex(m_project_path.value());
            if (m_project_model->validateIndex(existing_project)) {
                m_project_model->setLastAccessed(existing_project, system_clock::now());
            } else {
                ModelIndex new_project =
                    m_project_model->makeIndex(m_project_path.value(), 0, ModelIndex());
                m_project_model->setLastAccessed(new_project, system_clock::now());
            }
            m_project_model->saveToJSON(getAppDataPath() / fs_path(".ToolboxProjectsCache.json"));
        }
    }

    void BootStrapApplication::onEvent(RefPtr<BaseEvent> ev) {
        CoreApplication::onEvent(ev);

        if (ev->getType() == EVENT_DROP) {
            DragDropManager::instance().destroyDragAction(
                DragDropManager::instance().getCurrentDragAction());
        } else if (ev->getType() == EVENT_DRAG_MOVE || ev->getType() == EVENT_DRAG_ENTER ||
                   ev->getType() == EVENT_DRAG_LEAVE) {
            if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT, true)) {
                DragDropManager::instance().destroyDragAction(
                    DragDropManager::instance().getCurrentDragAction());
            }
        }

        switch (ev->getType()) {
        case EVENT_DRAG_ENTER:
        case EVENT_DRAG_MOVE: {
            if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT, true)) {
                DragDropManager::instance().destroyDragAction(
                    DragDropManager::instance().getCurrentDragAction());
            }
            break;
        }
        case EVENT_DRAG_LEAVE: {
            if (!Input::GetMouseButton(MouseButton::BUTTON_LEFT, true)) {
                DragDropManager::instance().destroyDragAction(
                    DragDropManager::instance().getCurrentDragAction());
            } else {
                RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction();
                if (action) {
                    action->setTargetUUID(0);
                }
            }
            break;
        }
        }
    }

#ifdef _WIN32
    const fs_path &BootStrapApplication::getAppDataPath() const {
        static fs_path app_data_path = []() -> fs_path {
            char *appdata = nullptr;
            size_t len    = 0;
            _dupenv_s(&appdata, &len, "APPDATA");
            if (appdata) {
                fs_path path = fs_path(appdata) / "JuniorsToolbox";
                free(appdata);
                return path;
            } else {
                return fs_path();
            }
        }();
        return app_data_path;
    }
#else
    const fs_path &BootStrapApplication::getAppDataPath() const {
        static fs_path app_data_path = []() -> fs_path {
            char *home = std::getenv("HOME");
            if (home) {
                fs_path path = fs_path(home) / ".juniorstoolbox";
                return path;
            } else {
                return fs_path();
            }
        }();
        return app_data_path;
    }
#endif

    void BootStrapApplication::initializeIcon() {
        auto result = m_resource_manager.getRawData(
            "toolbox.png", m_resource_manager.getResourcePathUUID(fs_path("Images") / "Icons"));
        if (!result) {
            TOOLBOX_ERROR("Failed to load toolbox icon!");
            return;
        }

        std::span<u8> data_span = std::move(result.value());

        // Load image data
        {
            int width, height, channels;
            stbi_uc *data = stbi_load_from_memory(data_span.data(), data_span.size(), &width,
                                                  &height, &channels, 4);

            GLFWimage icon = {width, height, data};
            glfwSetWindowIcon(m_render_window, 1, &icon);

            stbi_image_free(data);
        }
    }

    void BootStrapApplication::render(TimeStep delta_time) {  // Begin actual rendering
        glfwMakeContextCurrent(m_render_window);

        ImGui::GetIO().FontDefault     = FontManager::instance().getCurrentFont();
        ImGui::GetStyle().FontSizeBase = FontManager::instance().getCurrentFontSize();

        // The context renders both the ImGui elements and the background elements.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::PushFont(FontManager::instance().getCurrentFont(),
                        FontManager::instance().getCurrentFontSize());

        // renderMenuBar();

        // UPDATE LOOP: We update layers here so the ImGui dockspaces can take effect first.
        {
            CoreApplication::onUpdate(delta_time);
            renderBody();
        }

        // Render drag icon
        {
            // if (m_await_drag_drop_destroy) {
            //     DragDropManager::instance().destroyDragAction(
            //         DragDropManager::instance().getCurrentDragAction());
            //     m_await_drag_drop_destroy = false;
            // }

            // if (DragDropManager::instance().getCurrentDragAction() &&
            //     Input::GetMouseButtonUp(MouseButton::BUTTON_LEFT, true)) {
            //     m_await_drag_drop_destroy = true;
            // }

            // if (RefPtr<DragAction> action = DragDropManager::instance().getCurrentDragAction()) {
            //     double x, y;
            //     Input::GetMousePosition(x, y);

            //    action->setHotSpot({(float)x, (float)y});

            //    const ImVec2 &hotspot = action->getHotSpot();

            //    ImGuiViewportP *viewport =
            //        ImGui::FindHoveredViewportFromPlatformWindowStack(hotspot);
            //    if (viewport) {
            //        const ImVec2 size = {96, 96};
            //        const ImVec2 pos  = {hotspot.x - size.x / 2.0f, hotspot.y - size.y / 1.1f};

            //        ImGuiStyle &style = ImGui::GetStyle();

            //        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            //        // ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
            //        ImGui::PushStyleColor(ImGuiCol_WindowBg,
            //                              ImGui::GetStyleColorVec4(ImGuiCol_Text));

            //        ImGui::SetNextWindowBgAlpha(1.0f);
            //        ImGui::SetNextWindowPos(pos);
            //        ImGui::SetNextWindowSize(size);
            //        if (ImGui::Begin("###Drag Icon", nullptr,
            //                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            //                             ImGuiWindowFlags_NoFocusOnAppearing |
            //                             ImGuiWindowFlags_NoMouseInputs)) {
            //            action->render(size);

            //            // ImVec2 status_size = {16, 16};
            //            // ImGui::SetCursorScreenPos(pos + size - status_size);

            //            // ImGui::DrawSquare(
            //            //     pos + size - (status_size / 2), status_size.x, IM_COL32_WHITE,
            //            // ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_HeaderActive]));

            //            const DragAction::DropState &state = action->getDropState();
            //            if (state.m_valid_target) {

            //            } else {
            //                ResourceManager &res_manager = m_resource_manager;
            //                UUID64 directory_id = res_manager.getResourcePathUUID("Images/Icons");
            //                RefPtr<const ImageHandle> img =
            //                    res_manager.getImageHandle("invalid_circle.png", directory_id)
            //                        .value_or(nullptr);
            //                if (img) {
            //                    const ImVec2 stat_size = {16.0f, 16.0f};

            //                    ImagePainter painter;
            //                    painter.render(*img, pos + size - stat_size - style.WindowPadding,
            //                                   stat_size);
            //                }
            //            }

            //            if (ImGuiWindow *win = ImGui::GetCurrentWindow()) {
            //                if (win->Viewport != ImGui::GetMainViewport()) {
            //                    Platform::SetWindowClickThrough(
            //                        (Platform::LowWindow)win->Viewport->PlatformHandleRaw, true);
            //                    m_drag_drop_viewport = win->Viewport;
            //                } else {
            //                    m_drag_drop_viewport = nullptr;
            //                }
            //            }

            //            ImGui::Dummy({0, 0});  // Submit an empty item so ImGui resizes the parent
            //                                   // window bounds.
            //        }
            //        ImGui::End();

            //        ImGui::PopStyleColor(1);
            //        ImGui::PopStyleVar(1);
            //    }
            //}
        }

        ImGui::PopFont();

        // Render imgui frame
        {
            ImGui::Render();
            finalizeFrame();
        }

        //        if (m_pending_drag_action) {
        // #if 0
        //            const MimeData &data = m_pending_drag_action->getPayload();
        //            DropType out;
        //            bool result = m_drag_drop_source_delegate->startDragDrop(
        //                m_pending_drag_window, data,
        //                m_pending_drag_action->getSupportedDropTypes(), &out);
        //            TOOLBOX_DEBUG_LOG_V("DragDropSourceDelegate::startDragDrop action type: {}",
        //            int(out));
        // #else
        //            // TODO: Fix DoDragDrop locking ImGui/GLFW
        //            DragDropManager::instance().destroyDragAction(m_pending_drag_action);
        //            m_pending_drag_action = nullptr;
        // #endif
        //        }
    }

    void BootStrapApplication::renderBody() {
        ImFont *default_font = FontManager::instance().getFont("Markdown/Roboto-Medium");
        ImFont *bold_font    = FontManager::instance().getFont("Markdown/Roboto-Bold");
        ImFont *italic_font  = FontManager::instance().getFont("Markdown/Roboto-MediumItalic");

        const ImVec2 window_size = windowSize();

        // Use the whole viewport as the window area
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
        ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);

        // Set flags to create a seamless, non-movable window
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoBringToFrontOnFocus |
                                        ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {20.0f, 20.0f});

        ImGui::PushFont(default_font, 16.0f);

        if (ImGui::Begin("###Bootstrap Main Window", nullptr, window_flags)) {
            std::unique_lock lock(m_event_mutex);
            switch (m_render_page) {
            case RenderPage::LANDING:
                renderLandingPage();
                break;
            case RenderPage::LOCAL_CONFIG:
                renderLocalConfigPage();
                break;
            case RenderPage::REPO_CONFIG:
                renderRepositoryConfigPage();
                break;
            }
        }
        ImGui::End();

        ImGui::PopFont();

        ImGui::PopStyleVar();
    }

    void BootStrapApplication::renderLandingPage() {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 4.0f});
        ImGui::PushFont(nullptr, 32.0f);
        ImGui::Text("Junior's Toolbox " TOOLBOX_VERSION_TAG);
        ImGui::NewLine();
        ImGui::PopFont();

        const ImVec2 window_avail = ImGui::GetContentRegionAvail();

        // The view for projects that have been opened
        {
            ImGuiID child_id = ImGui::GetID("Projects View");
            if (ImGui::BeginChild(child_id, {window_avail.x * LEFT_PANEL_RATIO, window_avail.y},
                                  ImGuiChildFlags_NavFlattened,
                                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoCollapse)) {
                ImGui::PushFont(nullptr, 24.0f);
                ImGui::Text("Open Recent");
                ImGui::PopFont();

                const ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_AutoSelectAll |
                                                        ImGuiInputTextFlags_EnterReturnsTrue |
                                                        ImGuiInputTextFlags_ElideLeft;
                if (ImGui::InputTextWithHint("##Project Search", "Search Projects...",
                                             m_search_str.data(), m_search_str.size(),
                                             input_flags)) {
                    std::string search_string = m_search_str.data();
                    m_sort_filter_proxy->setFilter(search_string);
                }

                ImGui::Separator();
                ImGui::NewLine();

                ProjectAgeCategory current_category = ProjectAgeCategory::AGE_NONE;
                bool should_render_projects         = true;

                const size_t projects_count = m_sort_filter_proxy->getRowCount(ModelIndex());
                for (size_t i = 0; i < projects_count; ++i) {
                    ModelIndex project_index = m_sort_filter_proxy->getIndex(i, 0, ModelIndex());
                    if (!m_sort_filter_proxy->validateIndex(project_index)) {
                        continue;
                    }

                    const system_clock::time_point project_tp =
                        m_sort_filter_proxy->getLastAccessed(project_index);
                    const ProjectAgeCategory this_category = GetAgeCategoryForProject(project_tp);

                    if (this_category != current_category) {
                        should_render_projects = renderProjectAgeGroup(this_category);
                        current_category       = this_category;
                    }

                    if (!should_render_projects) {
                        continue;
                    }

                    ImGui::Indent();
                    renderProjectRow(project_index);
                    ImGui::Unindent();
                }

                ImGui::Dummy({0.0f, 0.0f});
            }
            ImGui::EndChild();
        }

        ImGui::SameLine(0.0f, window_avail.x * MIDDLE_PANEL_RATIO);

        // The panel for starting a new project
        {
            ImGuiID child_id = ImGui::GetID("Projects Panel");
            if (ImGui::BeginChild(child_id, {window_avail.x * RIGHT_PANEL_RATIO, window_avail.y},
                                  ImGuiChildFlags_NavFlattened,
                                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoCollapse)) {
                ImGui::PushFont(nullptr, 24.0f);
                ImGui::Text("Get Started");
                ImGui::PopFont();

                ImGui::PushFont(nullptr, 24.0f);
                if (ImGui::Button("Clone a repository", {200.0f, 0.0f})) {
                    m_render_page = RenderPage::REPO_CONFIG;
                }

                if (ImGui::Button("Open a root folder", {200.0f, 0.0f})) {
                    if (!FileDialog::instance()->isAlreadyOpen()) {
                        FileDialog::instance()->openDialog(m_render_window, m_uuid, m_load_path,
                                                           true);
                    }
                }

                if (ImGui::Button("Create from ISO", {200.0f, 0.0f})) {
                    if (!FileDialog::instance()->isAlreadyOpen()) {
                        FileDialogFilter filter;
                        filter.addFilter("GameCube Disc Image", "iso");
                        FileDialog::instance()->openDialog(m_render_window, m_uuid, m_load_path,
                                                           false, filter);
                    }
                }
                ImGui::PopFont();
            }
            ImGui::EndChild();
        }

        ImGui::PopStyleVar();

        if (FileDialog::instance()->isDone(m_uuid)) {
            if (FileDialog::instance()->isOk()) {
                const fs_path selected_path = FileDialog::instance()->getFilenameResult();

                // This is an ISO
                if (Filesystem::is_regular_file(selected_path).value_or(false)) {
                    // TODO: Extract using libgcm...
                    m_load_path = selected_path.parent_path();
                }

                // This is an extracted root
                if (Filesystem::is_directory(selected_path).value_or(false)) {
                    m_load_path = selected_path;

                    m_project_path               = selected_path;
                    m_scene_path                 = std::nullopt;
                    m_results.m_settings_profile = std::nullopt;
                    m_results.m_windows          = {};
                    m_results_ready              = true;
                }
            }
        }
    }

    void BootStrapApplication::renderLocalConfigPage() {}

    void BootStrapApplication::renderRepositoryConfigPage() {
        ImFont *default_font = FontManager::instance().getFont("Markdown/Roboto-Medium");
        ImFont *bold_font    = FontManager::instance().getFont("Markdown/Roboto-Bold");
        ImFont *italic_font  = FontManager::instance().getFont("Markdown/Roboto-MediumItalic");

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4.0f, 4.0f});
        ImGui::PushFont(nullptr, 32.0f);
        ImGui::Text("Clone a repository");
        ImGui::NewLine();
        ImGui::PopFont();

        const ImVec2 window_avail = ImGui::GetContentRegionAvail();

        // The view for projects that have been opened
        {
            ImGuiID child_id = ImGui::GetID("Clone Repo View");
            if (ImGui::BeginChild(child_id, {window_avail.x * LEFT_PANEL_RATIO, window_avail.y},
                                  ImGuiChildFlags_NavFlattened,
                                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoCollapse)) {
                ImGui::PushFont(nullptr, 24.0f);
                ImGui::Text("Enter a GitHub repository URL");
                ImGui::NewLine();
                ImGui::PopFont();

                ImGui::Text("Repository location");

                const ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_AutoSelectAll |
                                                        ImGuiInputTextFlags_EnterReturnsTrue |
                                                        ImGuiInputTextFlags_ElideLeft;
                if (ImGui::InputTextWithHint(
                        "##Repository location", "https://example.com/example.git <Required>",
                        m_repo_input_str.data(), m_repo_input_str.size(), input_flags)) {
                    std::string search_string = m_repo_input_str.data();
                    m_sort_filter_proxy->setFilter(search_string);
                }

                ImGui::NewLine();

                ImGui::Text("Path");

                if (ImGui::InputTextWithHint("##Path", "Local path <Required>",
                                             m_repo_path_str.data(), m_repo_path_str.size(),
                                             input_flags)) {
                    std::string search_string = m_repo_path_str.data();
                    m_sort_filter_proxy->setFilter(search_string);
                }

                ImGui::SameLine();

                if (ImGui::Button("...")) {
                    if (!FileDialog::instance()->isAlreadyOpen()) {
                        FileDialog::instance()->openDialog(m_render_window, m_uuid, m_load_path,
                                                           true);
                    }
                }

                ImGui::NewLine();

                ImGui::Text("GitHub Username");

                if (ImGui::InputTextWithHint("##Github Username", "<Optional>",
                                             m_github_username_str.data(),
                                             m_github_username_str.size(), input_flags)) {
                    std::string search_string = m_github_username_str.data();
                    m_sort_filter_proxy->setFilter(search_string);
                }

                ImGui::NewLine();

                ImGui::Text("GitHub Passkey");

                const bool is_passkey_not_null =
                    strnlen(m_github_passkey_str.data(), m_github_passkey_str.size()) > 0;
                if (is_passkey_not_null) {
                    ImGui::PushPasswordFont();
                }

                if (ImGui::InputTextWithHint(
                        "##Github Passkey", "Password or Personal Access Token <Optional>",
                        m_github_passkey_str.data(), m_github_passkey_str.size(), input_flags)) {
                    std::string search_string = m_github_passkey_str.data();
                    m_sort_filter_proxy->setFilter(search_string);
                }

                if (is_passkey_not_null) {
                    ImGui::PopPasswordFont();
                }

                ImGui::PushFont(italic_font, 14.0f);
                ImVec4 faded_text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                faded_text.w      = 0.5f;
                ImGui::PushStyleColor(ImGuiCol_Text, faded_text);
                ImGui::Text("Personal Access Tokens are the recommended method of passkey for "
                            "GitHub authorization.");
                ImGui::PopStyleColor();
                ImGui::TextLinkOpenURL("Click here to sign in and create a new PAT",
                                       "https://github.com/settings/tokens");
                ImGui::PopFont();

                ImGui::Separator();
            }
            ImGui::EndChild();
        }

        ImGui::SameLine(0.0f, window_avail.x * MIDDLE_PANEL_RATIO);

        const bool is_git_busy_static = m_is_git_busy;

        // The panel for starting a new project
        {
            const ImVec2 window_size = {window_avail.x * RIGHT_PANEL_RATIO, window_avail.y};
            ImGuiID child_id         = ImGui::GetID("Control Panel");
            if (ImGui::BeginChild(child_id, window_size, ImGuiChildFlags_NavFlattened,
                                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoCollapse)) {
                ImGui::SetCursorPos(ImVec2(window_size.x - 160.0f - ImGui::GetStyle().ItemSpacing.x,
                                           window_size.y - ImGui::GetTextLineHeight() -
                                               ImGui::GetStyle().FramePadding.y * 2));

                if (is_git_busy_static) {
                    ImGui::BeginDisabled();
                }

                if (ImGui::Button("Back", {80.0f, 0.0f})) {
                    m_render_page = RenderPage::LANDING;
                }

                ImGui::SameLine();

                if (ImGui::Button("Clone", {80.0f, 0.0f})) {
                    m_is_git_busy = true;

                    const std::string repo_url = m_repo_input_str.data();
                    const fs_path repo_path    = fs_path(m_repo_path_str.data());

                    const std::string github_username = m_github_username_str.data();
                    const std::string github_passkey  = m_github_passkey_str.data();
                    Git::GitConfigureCredentials(github_username, github_passkey);

                    Git::GitCloneAsync(
                        repo_url, repo_path,
                        [this](const std::string &provided_msg, const std::string &job_name,
                               float progress, int job_index, int job_max) {
                            std::unique_lock<std::mutex> lock(m_event_mutex);
                            m_git_clone_msg       = provided_msg;
                            m_git_clone_job       = job_name;
                            m_git_clone_progress  = progress;
                            m_git_clone_job_index = job_index;
                            m_git_clone_job_max   = job_max;
                        },
                        [this](const fs_path &out_path, bool success) {
                            std::unique_lock<std::mutex> lock(m_event_mutex);
                            if (success) {
                                m_load_path    = out_path;
                                m_project_path = [&out_path]() -> fs_path {
                                    const fs_path with_root = out_path / "root";
                                    if (Filesystem::exists(with_root).value_or(false)) {
                                        return with_root;
                                    }
                                    return out_path;
                                }();
                                m_scene_path                 = std::nullopt;
                                m_results.m_settings_profile = std::nullopt;
                                m_results.m_windows          = {};
                                m_results_ready = Filesystem::is_directory(m_project_path.value())
                                                      .value_or(false);
                                if (!m_results_ready) {
                                    m_project_error_msg = "The cloned repository is not a valid "
                                                          "project! Make sure it "
                                                          "contains the extracted ISO as \"root\"!";
                                    ImGui::OpenPopup("Project error");
                                }
                            } else {
                                m_project_error_msg = Git::GitCloneLastError();
                                ImGui::OpenPopup("git clone error");
                            }
                            m_is_git_busy = false;
                        });
                }

                if (is_git_busy_static) {
                    ImGui::EndDisabled();
                }
            }
            ImGui::EndChild();
        }

        if (is_git_busy_static) {
            ImGui::OpenPopup("git clone");
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                                {0.5f, 0.5f});
        ImGui::SetNextWindowSize({500.0f, 0.0f}, ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("git clone")) {
            ImGui::Text(m_git_clone_job.c_str());
            ImGui::ProgressBar(m_git_clone_progress, ImVec2(-FLT_MIN, 0), m_git_clone_msg.c_str());
            ImGui::EndPopup();
            if (!is_git_busy_static) {
                ImGui::ClosePopupToLevel(ImGui::GetCurrentContext()->BeginPopupStack.Size, true);
            }
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                                {0.5f, 0.5f});
        ImGui::SetNextWindowSize({500.0f, 0.0f}, ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("git clone error")) {
            ImGui::Text(m_project_error_msg.c_str());
            ImGui::EndPopup();
            if (!is_git_busy_static) {
                ImGui::ClosePopupToLevel(ImGui::GetCurrentContext()->BeginPopupStack.Size, true);
            }
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing,
                                {0.5f, 0.5f});
        ImGui::SetNextWindowSize({500.0f, 0.0f}, ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Project error")) {
            ImGui::Text(m_project_error_msg.c_str());
            ImGui::EndPopup();
            if (!is_git_busy_static) {
                ImGui::ClosePopupToLevel(ImGui::GetCurrentContext()->BeginPopupStack.Size, true);
            }
        }

        ImGui::PopStyleVar();

        if (FileDialog::instance()->isDone(m_uuid)) {
            if (FileDialog::instance()->isOk()) {
                const fs_path selected_path = FileDialog::instance()->getFilenameResult();

                // This is a GitHub project, assumes the project starts at ./root
                if (Filesystem::is_directory(selected_path).value_or(false)) {
                    m_load_path = selected_path;

                    strncpy(m_repo_path_str.data(), selected_path.string().c_str(),
                            m_repo_path_str.size() - 1);
                }
            }
        }
    }

    bool BootStrapApplication::renderProjectAgeGroup(ProjectAgeCategory category) {
        std::string category_name;
        switch (category) {
        case ProjectAgeCategory::AGE_DAY:
            category_name = "Today";
            break;
        case ProjectAgeCategory::AGE_WEEK:
            category_name = "This Week";
            break;
        case ProjectAgeCategory::AGE_MONTH:
            category_name = "This Month";
            break;
        case ProjectAgeCategory::AGE_YEAR:
            category_name = "This Year";
            break;
        case ProjectAgeCategory::AGE_LONG_AGO:
            category_name = "Long Ago";
            break;
        }

        const ImGuiStyle &style = ImGui::GetStyle();

        const ImRect arrow_bb = ImRect(ImGui::GetCursorScreenPos(),
                                       ImGui::GetCursorScreenPos() +
                                           ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize()));

        const ImGuiID arrow_id =
            ImGui::GetID(TOOLBOX_FORMAT_FN("##AgeCategoryButton{}", int(category)).c_str());
        ImGui::ItemSize(arrow_bb.GetSize(), style.FramePadding.y);
        if (!ImGui::ItemAdd(arrow_bb, arrow_id)) {
            return !m_category_closed_map[category];
        }

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(arrow_bb, arrow_id, &hovered, &held);
        if (pressed) {
            m_category_closed_map[category] ^= true;
        }

        ImGui::RenderArrow(ImGui::GetWindowDrawList(), arrow_bb.Min,
                           ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text)),
                           m_category_closed_map[category] ? ImGuiDir_Right : ImGuiDir_Down);

        const ImVec2 text_size = ImGui::CalcTextSize(
            category_name.c_str(), category_name.c_str() + category_name.size(), true);
        const ImRect text_bb(
            ImVec2(arrow_bb.Max.x + style.ItemSpacing.x, arrow_bb.Min.y),
            ImVec2(arrow_bb.Max.x + style.ItemSpacing.x + text_size.x, arrow_bb.Max.y));
        ImGui::RenderTextClipped(text_bb.Min, text_bb.Max, category_name.c_str(),
                                 category_name.c_str() + category_name.size(), &text_size);

        return !m_category_closed_map[category];
    }

    void BootStrapApplication::renderProjectRow(const ModelIndex &index) {
        const fs_path project_path                   = m_sort_filter_proxy->getProjectPath(index);
        const std::string project_name               = m_sort_filter_proxy->getDisplayText(index);
        RefPtr<const ImageHandle> project_icon       = m_sort_filter_proxy->getDecoration(index);
        const bool project_pinned                    = m_sort_filter_proxy->getPinned(index);
        const system_clock::time_point last_accessed = m_sort_filter_proxy->getLastAccessed(index);

        const std::string last_accessed_str = GetDateStringForProject(last_accessed);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8.0f, 8.0f});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {5.0f, 5.0f});

        ImGui::PushStyleColor(ImGuiCol_FrameBg, {0.0f, 0.0f, 0.0f, 0.0f});

        const ImGuiStyle &style = ImGui::GetStyle();

        const ImVec2 pos  = ImGui::GetCursorScreenPos();
        const ImVec2 size = {ImGui::GetContentRegionAvail().x, 48.0f};

        const ImGuiID id =
            ImGui::GetID(TOOLBOX_FORMAT_FN("Project Row##{}", index.getUUID()).c_str());

        const ImRect bb(pos, pos + size);
        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

        // Render background
        ImU32 bg_color = held      ? ImGui::GetColorU32(ImGuiCol_FrameBgActive)
                         : hovered ? ImGui::GetColorU32(ImGuiCol_FrameBgHovered)
                                   : ImGui::GetColorU32(ImGuiCol_FrameBg);
        ImGui::RenderFrame(bb.Min, bb.Max, bg_color, true, style.FrameRounding);

        // Render project details
        {
            const ImVec2 icon_size = {96.0f, 32.0f};

            ImagePainter painter;
            painter.render(*project_icon, pos + style.FramePadding, icon_size);

            ImGui::PushFont(nullptr, 14.0f);
            const ImVec2 last_accessed_layout_size =
                ImGui::CalcTextSize(last_accessed_str.c_str(),
                                    last_accessed_str.c_str() + last_accessed_str.size(), true) +
                ImVec2(style.FramePadding.x, 0.0f);
            ImGui::PopFont();

            // Project name
            {
                const ImVec2 text_size = ImGui::CalcTextSize(
                    project_name.c_str(), project_name.c_str() + project_name.size(), true);
                const ImRect text_bb(
                    pos + ImVec2(icon_size.x + style.ItemSpacing.x + style.FramePadding.x,
                                 style.FramePadding.y),
                    pos + ImVec2(size.x - last_accessed_layout_size.x - style.ItemSpacing.x,
                                 style.FramePadding.y + text_size.y));
                ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), text_bb.Min, text_bb.Max,
                                          text_bb.Max.x, project_name.c_str(),
                                          project_name.c_str() + project_name.size(), &text_size);
            }

            // Project path
            {
                ImGui::PushFont(nullptr, 14.0f);
                const std::string project_path_str = project_path.string();
                const ImVec2 text_size =
                    ImGui::CalcTextSize(project_path_str.c_str(),
                                        project_path_str.c_str() + project_path_str.size(), true);
                const ImRect text_bb(
                    pos + ImVec2(icon_size.x + style.ItemSpacing.x + style.FramePadding.x,
                                 size.y - style.FramePadding.y - text_size.y),
                    pos + ImVec2(size.x - last_accessed_layout_size.x - style.ItemSpacing.x,
                                 size.y - style.FramePadding.y));
                ImGui::RenderTextEllipsis(ImGui::GetWindowDrawList(), text_bb.Min, text_bb.Max,
                                          text_bb.Max.x, project_path_str.c_str(),
                                          project_path_str.c_str() + project_path_str.size(),
                                          &text_size);
                ImGui::PopFont();
            }

            // Last accessed date
            {
                ImGui::PushFont(nullptr, 14.0f);
                const ImVec2 text_size =
                    ImGui::CalcTextSize(last_accessed_str.c_str(),
                                        last_accessed_str.c_str() + last_accessed_str.size(), true);
                const ImRect text_bb(
                    pos + ImVec2(size.x - last_accessed_layout_size.x, style.FramePadding.y),
                    pos + ImVec2(size.x - style.FramePadding.x, size.y - style.FramePadding.y));
                ImGui::RenderTextClipped(text_bb.Min, text_bb.Max, last_accessed_str.c_str(),
                                         last_accessed_str.c_str() + last_accessed_str.size(),
                                         &last_accessed_layout_size, {1.0f, 0.0f});
                ImGui::PopFont();
            }
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        if (pressed) {
            m_project_path               = project_path;
            m_scene_path                 = std::nullopt;
            m_results.m_settings_profile = std::nullopt;
            m_results.m_windows          = {};
            m_results_ready              = true;
        }
    }

    void BootStrapApplication::finalizeFrame() {
        // Update buffer size
        {
            int width, height;
            glfwGetFramebufferSize(m_render_window, &width, &height);
            glViewport(0, 0, width, height);
        }

        // Clear the buffer
        // glClearColor(0.100f, 0.261f, 0.402f, 1.0f);
        // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiStyle &style = ImGui::GetStyle();
        ImGuiIO &io       = ImGui::GetIO();

        // Update backend framework
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)) {
#ifndef IMGUI_ENABLE_VIEWPORT_WORKAROUND
            style.WindowRounding              = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
#endif

            GLFWwindow *backup_window = glfwGetCurrentContext();
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
            glfwMakeContextCurrent(backup_window);
            m_render_window = backup_window;
        }

        // Swap buffers to reset context for next frame
        glfwSwapBuffers(m_render_window);
    }

    bool BootStrapApplication::determineEnvironmentConflicts() { return false; }
}  // namespace Toolbox

// void ImGuiSetupTheme(bool bStyleDark_, float alpha_) {
//     ImGuiStyle &style = ImGui::GetStyle();
//
// #define HEADER_COLOR         0.96f, 0.18f, 0.12f
// #define HEADER_ACTIVE_COLOR  0.96f, 0.18f, 0.12f
// #define HEADER_HOVERED_COLOR 0.96f, 0.18f, 0.12f
// #define ACCENT_COLOR         0.88f, 0.82f, 0.34f
// #define ACCENT_HOVERED_COLOR  0.96f, 0.90f, 0.38f
// #define ACCENT_ACTIVE_COLOR         0.92f, 0.86f, 0.35f
//
//     // light style from Pacôme Danhiez (user itamago)
//     // https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
//     style.Alpha                                  = 1.0f;
//     style.FrameRounding                          = 3.0f;
//     style.Colors[ImGuiCol_Text]                  = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
//     style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
//     style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.24f, 0.58f, 0.90f, 0.94f);
//     style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
//     style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.26f, 0.60f, 0.92f, 0.94f);
//     style.Colors[ImGuiCol_Border]                = ImVec4(0.00f, 0.00f, 0.00f, 0.39f);
//     style.Colors[ImGuiCol_BorderShadow]          = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
//     style.Colors[ImGuiCol_FrameBg]               = ImVec4(1.00f, 1.00f, 1.00f, 0.94f);
//     style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(ACCENT_COLOR, 0.78f);
//     style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(ACCENT_COLOR, 0.88f);
//     style.Colors[ImGuiCol_TitleBg]               = ImVec4(HEADER_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(HEADER_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(HEADER_COLOR, 0.70f);
//     style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(HEADER_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.56f, 0.56f, 0.56f, 1.00f);
//     style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.64f, 0.64f, 0.64f, 1.00f);
//     style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
//     style.Colors[ImGuiCol_CheckMark]             = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_SliderGrab]            = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_Button]                = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(ACCENT_HOVERED_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ButtonActive]          = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_Header]                = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(ACCENT_HOVERED_COLOR, 1.00f);
//     style.Colors[ImGuiCol_HeaderActive]          = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_Separator]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
//     style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(ACCENT_HOVERED_COLOR, 0.78f);
//     style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(ACCENT_HOVERED_COLOR, 1.00f);
//     style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_Tab]                   = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TabHovered]            = ImVec4(ACCENT_HOVERED_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TabActive]             = ImVec4(ACCENT_ACTIVE_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_DockingPreview]        = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
//     style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
//     style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
//     style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
//     style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
//     style.Colors[ImGuiCol_TableHeaderBg]         = ImVec4(ACCENT_COLOR, 0.78f);
//     style.Colors[ImGuiCol_TableBorderStrong]     = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TableBorderLight]      = ImVec4(ACCENT_COLOR, 0.78f);
//     style.Colors[ImGuiCol_TableRowBg]            = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
//     style.Colors[ImGuiCol_TableRowBgAlt]         = ImVec4(ACCENT_COLOR, 1.00f);
//     style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(ACCENT_COLOR, 0.50f);
//     style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(ACCENT_COLOR, 0.50f);
//     style.Colors[ImGuiCol_NavHighlight]          = ImVec4(ACCENT_COLOR, 0.50f);
//     style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(ACCENT_COLOR, 0.50f);
//     style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
//     style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
//
//     if (bStyleDark_) {
//         for (int i = 0; i <= ImGuiCol_COUNT; i++) {
//             ImVec4 &col = style.Colors[i];
//             float H, S, V;
//             ImGui::ColorConvertRGBtoHSV(col.x, col.y, col.z, H, S, V);
//
//             if (S < 0.1f) {
//                 V = 1.0f - V;
//             }
//             ImGui::ColorConvertHSVtoRGB(H, S, V, col.x, col.y, col.z);
//             if (col.w < 1.00f) {
//                 col.w *= alpha_;
//             }
//         }
//     } else {
//         for (int i = 0; i <= ImGuiCol_COUNT; i++) {
//             ImVec4 &col = style.Colors[i];
//             if (col.w < 1.00f) {
//                 col.x *= alpha_;
//                 col.y *= alpha_;
//                 col.z *= alpha_;
//                 col.w *= alpha_;
//             }
//         }
//     }
// }
