#include "gui/scene/window.hpp"

#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuiFileDialog.h>

#include <gui/IconsForkAwesome.h>
#include <gui/modelcache.hpp>

#include <iconv.h>

namespace Toolbox::UI {

    void PacketSort(J3DRendering::SortFunctionArgs packets) {
        std::sort(packets.begin(), packets.end(),
                  [](const J3DRenderPacket &a, const J3DRenderPacket &b) -> bool {
                      if ((a.SortKey & 0x01000000) != (b.SortKey & 0x01000000)) {
                          return (a.SortKey & 0x01000000) > (b.SortKey & 0x01000000);
                      } else {
                          return a.Material->Name < b.Material->Name;
                      }
                  });
    }

    /* icu

    includes:

    #include <unicode/ucnv.h>
    #include <unicode/unistr.h>
    #include <unicode/ustring.h>

    std::string Utf8ToSjis(const std::string& value)
    {
        icu::UnicodeString src(value.c_str(), "utf8");
        int length = src.extract(0, src.length(), NULL, "shift_jis");

        std::vector<char> result(length + 1);
        src.extract(0, src.length(), &result[0], "shift_jis");

        return std::string(result.begin(), result.end() - 1);
    }

    std::string SjisToUtf8(const std::string& value)
    {
        icu::UnicodeString src(value.c_str(), "shift_jis");
        int length = src.extract(0, src.length(), NULL, "utf8");

        std::vector<char> result(length + 1);
        src.extract(0, src.length(), &result[0], "utf8");

        return std::string(result.begin(), result.end() - 1);
    }
    */

    std::string Utf8ToSjis(const std::string &value) {
        iconv_t conv = iconv_open("SHIFT_JISX0213", "UTF-8");
        if (conv == (iconv_t)(-1)) {
            throw std::runtime_error("Error opening iconv for UTF-8 to Shift-JIS conversion");
        }

        size_t inbytesleft = value.size();
        char *inbuf        = const_cast<char *>(value.data());

        size_t outbytesleft = value.size() * 2;
        std::string sjis(outbytesleft, '\0');
        char *outbuf = &sjis[0];

        if (iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)(-1)) {
            throw std::runtime_error("Error converting from UTF-8 to Shift-JIS");
        }

        sjis.resize(sjis.size() - outbytesleft);
        iconv_close(conv);
        return sjis;
    }

    std::string SjisToUtf8(const std::string &value) {
        iconv_t conv = iconv_open("UTF-8", "SHIFT_JISX0213");
        if (conv == (iconv_t)(-1)) {
            throw std::runtime_error("Error opening iconv for Shift-JIS to UTF-8 conversion");
        }

        size_t inbytesleft = value.size();
        char *inbuf        = const_cast<char *>(value.data());

        size_t outbytesleft = value.size() * 3;
        std::string utf8(outbytesleft, '\0');
        char *outbuf = &utf8[0];

        if (iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)(-1)) {
            throw std::runtime_error("Error converting from Shift-JIS to UTF-8");
        }

        utf8.resize(utf8.size() - outbytesleft);
        iconv_close(conv);
        return utf8;
    }

    void DrawTree(std::shared_ptr<Toolbox::Object::ISceneObject> root) {
        std::string node_name = std::format("{} ({})", root->type(), root->getNameRef().name());
        if (root->isGroupObject()) {
            bool open = ImGui::TreeNode(SjisToUtf8(node_name).c_str());
            if (open) {
                auto objects = std::dynamic_pointer_cast<Toolbox::Object::GroupSceneObject>(root)
                                   ->getChildren();
                if (objects.has_value()) {
                    for (auto object : objects.value()) {
                        DrawTree(object);
                    }
                }
                ImGui::TreePop();
            }
        } else {
            ImGui::Text(SjisToUtf8(node_name).c_str());
        }
    }

    SceneWindow::~SceneWindow() {
        // J3DRendering::Cleanup();
        m_renderables.erase(m_renderables.begin(), m_renderables.end());
        ModelCache.clear();
    }

    SceneWindow::SceneWindow() {
        J3DRendering::SetSortFunction(PacketSort);
    }

    bool SceneWindow::loadData(const std::filesystem::path &path) {
        if (!Toolbox::exists(path)) {
            return false;
        }

        if (Toolbox::is_directory(path)) {
            if (path.filename() != "scene") {
                return false;
            }

            ModelCache.erase(ModelCache.begin(), ModelCache.end());

            m_current_scene = std::make_unique<Toolbox::Scene::SceneInstance>(path);

            J3DModelLoader loader;
            for (const auto &entry : std::filesystem::directory_iterator(path / "mapobj")) {
                if (entry.path().extension() == ".bmd") {
                    bStream::CFileStream modelStream(entry.path().string(), bStream::Endianess::Big,
                                                     bStream::OpenMode::In);

                    ModelCache.insert({entry.path().stem().string(), loader.Load(&modelStream, 0)});
                }
            }

            for (const auto &entry : std::filesystem::directory_iterator(path / "map" / "map")) {
                if (entry.path().extension() == ".bmd") {
                    std::cout << "[gui]: loading model " << entry.path().filename().string()
                              << std::endl;
                    bStream::CFileStream modelStream(entry.path().string(), bStream::Endianess::Big,
                                                     bStream::OpenMode::In);

                    ModelCache.insert({entry.path().stem().string(), loader.Load(&modelStream, 0)});
                }
            }
            return true;
        }

        // TODO: Implement opening from archives.
        return false;
    }

    bool SceneWindow::update(f32 deltaTime) {
        m_camera.Update(deltaTime);

        return true;
    }

    void SceneWindow::renderBody(f32 deltaTime) {
        // const ImGuiViewport *mainViewport = ImGui::GetMainViewport();

        ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode |
                                       ImGuiDockNodeFlags_AutoHideTabBar |
                                       ImGuiDockNodeFlags_NoDockingInCentralNode;

        /*ImGuiWindowClass mainWindowOverride;
        mainWindowOverride.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
        ImGui::SetNextWindowClass(&mainWindowOverride);*/


        ImGui::Begin(getWindowChildUID(*this, "Hierarchy Editor").c_str(), nullptr,
                     ImGuiWindowFlags_NoTitleBar);
        ImGui::Text("Map Objects");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) /* Check if scene is loaded here*/) {
            // Add Object
        }

        ImGui::Separator();

        // Render Objects

        if (m_current_scene != nullptr) {
            auto root = m_current_scene->getObjHierarchy().getRoot();
            DrawTree(root);
        }

        ImGui::Spacing();
        ImGui::Text("Scene Info");
        ImGui::Separator();

        if (m_current_scene != nullptr) {
            auto root = m_current_scene->getTableHierarchy().getRoot();
            DrawTree(root);
        }

        ImGui::End();
        //ImGui::SetNextWindowClass(&mainWindowOverride);

        /*
        ImGuizmo::BeginFrame();
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        */

        ImGui::Begin(getWindowChildUID(*this, "Properties Editor").c_str(), nullptr,
                     ImGuiWindowFlags_NoTitleBar);
        // TODO: Render properties
        ImGui::End();

        glm::mat4 projection, view;
        projection = m_camera.GetProjectionMatrix();
        view       = m_camera.GetViewMatrix();

        J3DUniformBufferObject::SetProjAndViewMatrices(&projection, &view);

        m_renderables.clear();

        // Render Models here
        if (ModelCache.count("map") != 0) {
            m_renderables.push_back(ModelCache["map"]->GetInstance());
        }

        if (ModelCache.count("sky") != 0) {
            m_renderables.push_back(ModelCache["sky"]->GetInstance());
        }

        if (ModelCache.count("sea") != 0) {
            m_renderables.push_back(ModelCache["sea"]->GetInstance());
        }

        if (m_current_scene != nullptr) {
            m_current_scene->getObjHierarchy().getRoot()->performScene(m_renderables);
        }

        J3DRendering::Render(deltaTime, m_camera.GetPosition(), view, projection, m_renderables);

        // mGrid.Render(m_camera.GetPosition(), m_camera.GetProjectionMatrix(),
        // m_camera.GetViewMatrix());
    }

    void SceneWindow::buildDockspace(ImGuiID dockspace_id) {
        m_dock_node_left_id =
            ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
        m_dock_node_up_left_id   = ImGui::DockBuilderSplitNode(m_dock_node_left_id, ImGuiDir_Down,
                                                               0.5f, nullptr, &m_dock_node_left_id);
        m_dock_node_down_left_id = ImGui::DockBuilderSplitNode(
            m_dock_node_up_left_id, ImGuiDir_Down, 0.5f, nullptr, &m_dock_node_up_left_id);

        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Hierarchy Editor").c_str(),
                                     m_dock_node_up_left_id);
        ImGui::DockBuilderDockWindow(getWindowChildUID(*this, "Properties Editor").c_str(),
                                     m_dock_node_down_left_id);
    }

    void SceneWindow::renderMenuBar() {
    }

    void SceneWindow::setLights() {
        J3DLight lights[8];

        lights[0].Position   = glm::vec4(100000.0f, 100000.0f, 100000.0f, 1);
        lights[0].Color      = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);
        lights[0].AngleAtten = glm::vec4(1.0, 0.0, 0.0, 1);
        lights[0].DistAtten  = glm::vec4(1.0, 0.0, 0.0, 1);
        lights[0].Direction  = glm::vec4(1.0, 0.0, 0.0, 1);

        lights[1].Position   = glm::vec4(-100000.0f, -100000.0f, 100000.0f, 1);
        lights[1].Color      = glm::vec4(64 / 255, 62 / 255, 64 / 255, 0.0f);
        lights[1].AngleAtten = glm::vec4(1.0, 0.0, 0.0, 1);
        lights[1].DistAtten  = glm::vec4(1.0, 0.0, 0.0, 1);
        lights[1].Direction  = glm::vec4(1.0, 0.0, 0.0, 1);

        lights[2].Position   = glm::vec4(0, 0, 0, 0);
        lights[2].AngleAtten = glm::vec4(1.0, 0, 0, 1);
        lights[2].DistAtten  = glm::vec4(1.0, 0.0, 0.0, 1);
        lights[2].Direction  = glm::vec4(0, -1, 0, 1);
        lights[2].Color      = glm::vec4(0.0f, 0.0f, 0.0f, 0.5f);

        J3DUniformBufferObject::SetLights(lights);
    }

};  // namespace Toolbox::UI