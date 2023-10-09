#pragma once

#include "camera.hpp"

#include <vector>
#include <filesystem>
#include <memory>

#include "clone.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"

namespace Toolbox::UI {
    class EditorContext {
        
        std::unique_ptr<Toolbox::Scene::SceneInstance> mCurrentScene;

        std::vector<std::shared_ptr<J3DModelInstance>> mRenderables;

        uint32_t mGizmoOperation { 0 };

        Camera mCamera;

        uint32_t mMainDockSpaceID;
        uint32_t mDockNodeLeftID;
        uint32_t mDockNodeUpLeftID;
        uint32_t mDockNodeDownLeftID;
        
        std::string mSelectedAddZone { "" };

        bool mOptionsOpen { false };

        bool bIsDockingSetUp { false };
        bool bIsFileDialogOpen { false };
        bool bIsSaveDialogOpen { false };
        bool mSetLights { false };
        bool mTextEditorActive { false };

        void RenderMainWindow(float deltaTime);
        void RenderPanels(float deltaTime);
        void RenderMenuBar();

        void SetLights();

        void LoadSceneFromPath(std::filesystem::path filePath);
        void SaveSceneToPath(std::filesystem::path filePath);

    public:
        EditorContext();
        ~EditorContext();

        bool Update(float deltaTime);
        void Render(float deltaTime);
    };
}