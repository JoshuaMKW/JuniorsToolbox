#include "scene/layout.hpp"
#include "gui/logging/errors.hpp"

using namespace Toolbox::UI;

namespace Toolbox::Scene {

    size_t SceneLayout::sceneCount() const {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            return 0;
        }
        return root->getChildren().size();
    }

    size_t SceneLayout::scenarioCount(size_t scene) const {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            return 0;
        }
        std::vector<RefPtr<ISceneObject>> scene_list = root->getChildren();
        if (scene >= scene_list.size()) {
            return 0;
        }

        return scene_list[scene]->getChildren().size();
    }

    Result<std::string, BaseError> SceneLayout::getFileName(size_t scene, size_t scenario) const {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            return make_error<std::string>("SCENE_LAYOUT", "stageArc.bin root doesn't exist!");
        }

        std::vector<RefPtr<ISceneObject>> scene_list = root->getChildren();
        if (scene >= scene_list.size()) {
            return make_error<std::string>("SCENE_LAYOUT",
                                           std::format("Scene {} doesn't exist!", scene));
        }

        std::vector<RefPtr<ISceneObject>> scenario_list = scene_list[scene]->getChildren();
        if (scenario >= scenario_list.size()) {
            return make_error<std::string>(
                "SCENE_LAYOUT", std::format("Scenario {} -> {} doesn't exist!", scene, scenario));
        }

        std::string out;

        scenario_list[scenario]
            ->getMember("Name")
            .and_then([&](RefPtr<MetaMember> member) {
                getMetaValue<std::string>(member)
                    .and_then([&](std::string &&str) { out = std::move(str); })
                    .error_or([](const MetaError &error) {
                        LogError(error);
                        return Result<std::string, MetaError>();
                    });
            })
            .error_or([](const MetaScopeError &error) {
                LogError(error);
                return Result<RefPtr<MetaMember>, MetaScopeError>();
            });

        return out;
    }

}  // namespace Toolbox::Scene