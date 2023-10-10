#include "gui/context.hpp"

#include "gui/util.hpp"

#include <lib/bStream/bstream.h>

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuiFileDialog.h>

#include <gui/IconsForkAwesome.h>
#include <gui/modelcache.hpp>

#include <unicode/unistr.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>

namespace Toolbox::UI {

void PacketSort(J3DRendering::SortFunctionArgs packets) {
    std::sort(
        packets.begin(),
        packets.end(),
        [](const J3DRenderPacket& a, const J3DRenderPacket& b) -> bool {
			if((a.SortKey & 0x01000000) != (b.SortKey & 0x01000000)){
				return (a.SortKey & 0x01000000) > (b.SortKey & 0x01000000);
			} else {
            	return a.Material->Name < b.Material->Name;
			}
        }
    );
}

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

void DrawTree(std::shared_ptr<Toolbox::Object::ISceneObject> root){
	if(root->isGroupObject()){
		bool open = ImGui::TreeNode(SjisToUtf8(root->getQualifiedName().toString()).c_str());
		if(open){
			auto objects = std::dynamic_pointer_cast<Toolbox::Object::GroupSceneObject>(root)->getChildren();
			if(objects.has_value()){
				for (auto object : objects.value()){
					DrawTree(object);
				}
			}
			ImGui::TreePop();
		}
	} else {
		ImGui::Text(SjisToUtf8(root->getQualifiedName().toString()).c_str());
	}
}

EditorContext::~EditorContext(){
	//J3DRendering::Cleanup();
	mRenderables.erase(mRenderables.begin(), mRenderables.end());
	ModelCache.clear();
}

EditorContext::EditorContext(){

	ImGuiIO& io = ImGui::GetIO();
	
	if(std::filesystem::exists((std::filesystem::current_path() / "res" / "NotoSansJP-Regular.otf"))){
		io.Fonts->AddFontFromFileTTF((std::filesystem::current_path() / "res" / "NotoSansJP-Regular.otf").string().c_str(), 16.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
	}

	if(std::filesystem::exists((std::filesystem::current_path() / "res" / "forkawesome.ttf"))){
		static const ImWchar icons_ranges[] = { ICON_MIN_FK, ICON_MAX_16_FK, 0 };
		ImFontConfig icons_config; 
		icons_config.MergeMode = true; 
		icons_config.PixelSnapH = true; 
		icons_config.GlyphMinAdvanceX = 16.0f;
		io.Fonts->AddFontFromFileTTF((std::filesystem::current_path() / "res" / "forkawesome.ttf").string().c_str(), icons_config.GlyphMinAdvanceX, &icons_config, icons_ranges );
	}
	
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	J3DRendering::SetSortFunction(PacketSort);

}

bool EditorContext::Update(float deltaTime) {
	mCamera.Update(deltaTime);


	return true;
}

void EditorContext::Render(float deltaTime) {

	RenderMenuBar();
	
	const ImGuiViewport* mainViewport = ImGui::GetMainViewport();

	ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDockingInCentralNode;
	mMainDockSpaceID = ImGui::DockSpaceOverViewport(mainViewport, dockFlags);
	
	if(!bIsDockingSetUp){
		ImGui::DockBuilderRemoveNode(mMainDockSpaceID); // clear any previous layout
		ImGui::DockBuilderAddNode(mMainDockSpaceID, dockFlags | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(mMainDockSpaceID, mainViewport->Size);


		mDockNodeLeftID = ImGui::DockBuilderSplitNode(mMainDockSpaceID, ImGuiDir_Left, 0.25f, nullptr, &mMainDockSpaceID);
		mDockNodeDownLeftID = ImGui::DockBuilderSplitNode(mDockNodeLeftID, ImGuiDir_Down, 0.5f, nullptr, &mDockNodeUpLeftID);


		ImGui::DockBuilderDockWindow("mainWindow", mDockNodeUpLeftID);
		ImGui::DockBuilderDockWindow("sceneWindow", ImGui::DockBuilderSplitNode(mDockNodeUpLeftID, ImGuiDir_Down, 0.5f, nullptr, nullptr));
		ImGui::DockBuilderDockWindow("detailWindow", mDockNodeDownLeftID);

		ImGui::DockBuilderFinish(mMainDockSpaceID);
		bIsDockingSetUp = true;
	}


	ImGuiWindowClass mainWindowOverride;
	mainWindowOverride.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoTabBar;
	ImGui::SetNextWindowClass(&mainWindowOverride);
	
	ImGui::Begin("mainWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
	ImGui::Text("Scene");
		ImGui::SameLine();
        ImGui::Text(ICON_FK_PLUS_CIRCLE);
		if(ImGui::IsItemClicked(ImGuiMouseButton_Left) /* Check if scene is loaded here*/){
            // Add Object
		}

		ImGui::Separator();
		
        // Render Objects

		if(mCurrentScene != nullptr){
			auto root = mCurrentScene->getObjHierarchy().getRoot();
			DrawTree(root);
		}

	ImGui::End();

	ImGui::SetNextWindowClass(&mainWindowOverride);

	/*
	ImGuizmo::BeginFrame();
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	*/

	ImGui::Begin("detailWindow", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
		ImGui::Text("Properties");
		ImGui::Separator();
	ImGui::End();

	glm::mat4 projection, view;
	projection = mCamera.GetProjectionMatrix();
	view = mCamera.GetViewMatrix();

	J3DUniformBufferObject::SetProjAndViewMatrices(&projection, &view);

	mRenderables.clear();
	
	//Render Models here
	if(ModelCache.count("map") != 0){
		mRenderables.push_back(ModelCache["map"]->GetInstance());
	}
	
	if(ModelCache.count("sky") != 0){
		mRenderables.push_back(ModelCache["sky"]->GetInstance());
	}

	if(ModelCache.count("sea") != 0){
		mRenderables.push_back(ModelCache["sea"]->GetInstance());
	}

	if(mCurrentScene != nullptr){
		mCurrentScene->getObjHierarchy().getRoot()->performScene(mRenderables);
	}

	J3DRendering::Render(deltaTime, mCamera.GetPosition(), view, projection, mRenderables);

	//mGrid.Render(mCamera.GetPosition(), mCamera.GetProjectionMatrix(), mCamera.GetViewMatrix());
}

void EditorContext::RenderMainWindow(float deltaTime) {


}

void EditorContext::RenderMenuBar() {
	mOptionsOpen = false;
	ImGui::BeginMainMenuBar();

	if (ImGui::BeginMenu("File")) {
		if (ImGui::MenuItem(ICON_FK_FOLDER_OPEN " Open...")) {
			bIsFileDialogOpen = true;
		}
		if (ImGui::MenuItem(ICON_FK_FLOPPY_O " Save...")) {
			// Save Scene
		}

		ImGui::Separator();
		ImGui::MenuItem(ICON_FK_WINDOW_CLOSE " Close");

		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Edit")) {
		if(ImGui::MenuItem(ICON_FK_COG " Settings")){
			mOptionsOpen = true;
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu(ICON_FK_QUESTION_CIRCLE)) {
		if(ImGui::MenuItem("About")){
			mOptionsOpen = true;
		}
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();

	if (bIsFileDialogOpen) {
		ImGuiFileDialog::Instance()->OpenDialog("OpenSceneDialog", "Choose Scene Directory", nullptr, "", "");
	}

	if (ImGuiFileDialog::Instance()->Display("OpenSceneDialog")) {
		if (ImGuiFileDialog::Instance()->IsOk()) {
			std::string FilePath = ImGuiFileDialog::Instance()->GetFilePathName();

			ModelCache.erase(ModelCache.begin(), ModelCache.end());

			J3DModelLoader loader;
			for (const auto & entry : std::filesystem::directory_iterator(std::filesystem::path(FilePath) / "mapobj")){
				if(entry.path().extension() == ".bmd"){
					bStream::CFileStream modelStream(entry.path().string(), bStream::Endianess::Big, bStream::OpenMode::In);

					ModelCache.insert({entry.path().stem().string(), loader.Load(&modelStream, 0)});
				}
			}

			for (const auto & entry : std::filesystem::directory_iterator(std::filesystem::path(FilePath) / "map" / "map")){
				if(entry.path().extension() == ".bmd"){
					std::cout << "[gui]: loading model " << entry.path().filename().string() << std::endl;
					bStream::CFileStream modelStream(entry.path().string(), bStream::Endianess::Big, bStream::OpenMode::In);

					ModelCache.insert({entry.path().stem().string(), loader.Load(&modelStream, 0)});
				}
			}

            mCurrentScene = std::make_unique<Toolbox::Scene::SceneInstance>(FilePath);
			mCurrentScene->dump(std::cout);

	
			bIsFileDialogOpen = false;
		} else {
			bIsFileDialogOpen = false;
		}

		ImGuiFileDialog::Instance()->Close();
	}

	if (ImGui::BeginPopupModal("Scene Load Error", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)){
		ImGui::Text("Error Loading Scene\n\n");
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120,0))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if(mOptionsOpen){
		ImGui::OpenPopup("Options");
	}


}

void EditorContext::SetLights() {

	J3DLight lights[8];

	lights[0].Position = glm::vec4(100000.0f, 100000.0f, 100000.0f, 1);
	lights[0].Color = glm::vec4(0.5f, 0.5f, 0.5f, 0.0f);
	lights[0].AngleAtten = glm::vec4(1.0, 0.0, 0.0, 1);
	lights[0].DistAtten = glm::vec4(1.0, 0.0, 0.0, 1);
	lights[0].Direction = glm::vec4(1.0, 0.0, 0.0, 1);

	lights[1].Position = glm::vec4(-100000.0f, -100000.0f, 100000.0f, 1);
	lights[1].Color = glm::vec4(64/255, 62/255, 64/255, 0.0f);
	lights[1].AngleAtten = glm::vec4(1.0, 0.0, 0.0, 1);
	lights[1].DistAtten = glm::vec4(1.0, 0.0, 0.0, 1);
	lights[1].Direction = glm::vec4(1.0, 0.0, 0.0, 1);

	lights[2].Position = glm::vec4(0, 0, 0, 0);
	lights[2].AngleAtten = glm::vec4(1.0, 0, 0, 1);
	lights[2].DistAtten = glm::vec4(1.0, 0.0, 0.0, 1);
	lights[2].Direction = glm::vec4(0, -1, 0, 1);
	lights[2].Color = glm::vec4(0.0f, 0.0f, 0.0f, 0.5f);

	J3DUniformBufferObject::SetLights(lights);
}
};