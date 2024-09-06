#include <fstream>
#include <variant>

#include "gui/logging/errors.hpp"
#include "scene/layout.hpp"

using namespace Toolbox::UI;

namespace Toolbox::Scene {

    size_t SceneLayoutManager::sceneCount() const {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            return 0;
        }
        return root->getChildren().size();
    }

    size_t SceneLayoutManager::scenarioCount(size_t scene) const {
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

    bool SceneLayoutManager::loadFromPath(const fs_path &path) {
        bool result = true;

        Toolbox::Filesystem::is_regular_file(path)
            .and_then([&](bool is_file) {
                if (!is_file) {
                    return make_fs_error<bool>(std::error_code(),
                                               {std::format("{} is not a file!", path.string())});
                }

                std::ifstream file(path, std::ios::binary);
                Deserializer in(file.rdbuf());

                m_scene_layout->deserialize(in).or_else([&](const SerialError &error) {
                    result = false;
                    LogError(error);
                    return Result<void, SerialError>();
                });

                return Result<bool, FSError>();
            })
            .or_else([&](const FSError &error) {
                result = false;
                LogError(error);
                return Result<bool, FSError>();
            });

        return result;
    }

    bool SceneLayoutManager::saveToPath(const fs_path &path) const {
        bool result = true;

        Toolbox::Filesystem::is_directory(path.parent_path())
            .and_then([&](bool is_dir) {
                if (!is_dir) {
                    Toolbox::Filesystem::create_directories(path.parent_path())
                        .and_then([&](bool created) {
                            if (!created) {
                                return make_fs_error<void>(
                                    std::error_code(),
                                    {std::format("Failed to create directory {}!",
                                                 path.parent_path().string())});
                            }
                            return Result<void, FSError>();
                        })
                        .or_else([&](const FSError &error) {
                            result = false;
                            LogError(error);
                            return Result<void, FSError>();
                        });
                    if (!result) {
                        return Result<bool, FSError>();
                    }
                }

                std::ofstream file(path, std::ios::binary);
                Serializer out(file.rdbuf());

                m_scene_layout->serialize(out).or_else([&](const SerialError &error) {
                    result = false;
                    LogError(error);
                    return Result<void, SerialError>();
                });

                return Result<bool, FSError>();
            })
            .or_else([&](const FSError &error) {
                result = false;
                LogError(error);
                return Result<bool, FSError>();
            });

        return result;
    }

    std::string SceneLayoutManager::getFileName(size_t scene, size_t scenario) const {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::string>("SCENE_LAYOUT", "stageArc.bin root doesn't exist!")
                         .error());
            return "";
        }

        std::vector<RefPtr<ISceneObject>> scene_list = root->getChildren();
        if (scene >= scene_list.size()) {
            LogError(make_error<std::string>("SCENE_LAYOUT",
                                             std::format("Scene {} doesn't exist!", scene))
                         .error());
            return "";
        }

        std::vector<RefPtr<ISceneObject>> scenario_list = scene_list[scene]->getChildren();
        if (scenario >= scenario_list.size()) {
            LogError(make_error<std::string>(
                         "SCENE_LAYOUT",
                         std::format("Scenario {} -> {} doesn't exist!", scene, scenario))
                         .error());
            return "";
        }

        std::string out;

        scenario_list[scenario]
            ->getMember("Name")
            .and_then([&](RefPtr<MetaMember> member) {
                getMetaValue<std::string>(member)
                    .and_then([&](std::string &&str) {
                        out = std::move(str);
                        return Result<std::string, MetaError>();
                    })
                    .or_else([](const MetaError &error) {
                        LogError(error);
                        return Result<std::string, MetaError>();
                    });
                return Result<RefPtr<MetaMember>, MetaScopeError>();
            })
            .or_else([](const MetaScopeError &error) {
                LogError(error);
                return Result<RefPtr<MetaMember>, MetaScopeError>();
            });

        return out;
    }

    bool SceneLayoutManager::setFileName(const std::string &filename, size_t scene,
                                         size_t scenario) {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::string>("SCENE_LAYOUT", "stageArc.bin root doesn't exist!")
                         .error());
            return false;
        }

        std::vector<RefPtr<ISceneObject>> scene_list = root->getChildren();
        if (scene >= scene_list.size()) {
            LogError(make_error<std::string>("SCENE_LAYOUT",
                                             std::format("Scene {} doesn't exist!", scene))
                         .error());
            return false;
        }

        std::vector<RefPtr<ISceneObject>> scenario_list = scene_list[scene]->getChildren();
        if (scenario >= scenario_list.size()) {
            LogError(make_error<std::string>(
                         "SCENE_LAYOUT",
                         std::format("Scenario {} -> {} doesn't exist!", scene, scenario))
                         .error());
            return false;
        }

        bool result = true;

        scenario_list[scenario]
            ->getMember("Name")
            .and_then([&](RefPtr<MetaMember> member) {
                setMetaValue<std::string>(member, 0, filename, MetaType::STRING)
                    .or_else([&](const MetaError &error) {
                        result = false;
                        LogError(error);
                        return Result<bool, MetaError>();
                    });
                return Result<RefPtr<MetaMember>, MetaScopeError>();
            })
            .or_else([&](const MetaScopeError &error) {
                result = false;
                LogError(error);
                return Result<RefPtr<MetaMember>, MetaScopeError>();
            });

        return result;
    }

    bool SceneLayoutManager::getScenarioForFileName(const std::string &filename, size_t &scene_out,
                                                    size_t &scenario_out) const {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::string>("SCENE_LAYOUT", "stageArc.bin root doesn't exist!")
                         .error());
            return false;
        }

        std::vector<RefPtr<ISceneObject>> scene_list = root->getChildren();
        for (size_t i = 0; i < scene_list.size(); ++i) {
            std::vector<RefPtr<ISceneObject>> scenario_list = scene_list[i]->getChildren();
            for (size_t j = 0; j < scenario_list.size(); ++j) {
                std::string name;

                scenario_list[j]
                    ->getMember("Name")
                    .and_then([&](RefPtr<MetaMember> member) {
                        getMetaValue<std::string>(member)
                            .and_then([&](std::string &&str) {
                                name = std::move(str);
                                return Result<std::string, MetaError>();
                            })
                            .or_else([](const MetaError &error) {
                                LogError(error);
                                return Result<std::string, MetaError>();
                            });
                        return Result<RefPtr<MetaMember>, MetaScopeError>();
                    })
                    .or_else([](const MetaScopeError &error) {
                        LogError(error);
                        return Result<RefPtr<MetaMember>, MetaScopeError>();
                    });

                if (name == filename) {
                    scene_out    = i;
                    scenario_out = j;
                    return true;
                }
            }
        }

        return false;
    }

    std::optional<size_t> SceneLayoutManager::addScenario(const std::string &filename,
                                                          size_t scene) {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::optional<size_t>>("SCENE_LAYOUT",
                                                       "stageArc.bin root doesn't exist!")
                         .error());
            return 0;
        }

        std::vector<RefPtr<ISceneObject>> scene_list = root->getChildren();
        if (scene >= scene_list.size()) {
            LogError(make_error<std::optional<size_t>>(
                         "SCENE_LAYOUT", std::format("Scene {} doesn't exist!", scene))
                         .error());
            return 0;
        }

        bool result = true;

        TemplateFactory::create("ScenarioArchiveName")
            .and_then([&](TemplateFactory::create_ret_t obj_t) {
                RefPtr<ISceneObject> scenario = ObjectFactory::create(*obj_t, "Default");
                if (!scenario) {
                    result = false;
                    return Result<TemplateFactory::create_ret_t, TemplateFactory::create_err_t>();
                }
                scenario->setNameRef(NameRef(filename));
                scenario->getMember("Name")
                    .and_then([&](RefPtr<MetaMember> member) {
                        setMetaValue(member, 0, filename, MetaType::STRING)
                            .or_else([&](const MetaError &error) {
                                result = false;
                                LogError(error);
                                return Result<bool, MetaError>();
                            });
                        return Result<RefPtr<MetaMember>, MetaScopeError>();
                    })
                    .or_else([&](const MetaScopeError &error) {
                        result = false;
                        LogError(error);
                        return Result<RefPtr<MetaMember>, MetaScopeError>();
                    });
                if (result) {
                    scene_list[scene]->addChild(scenario);
                }
                return Result<TemplateFactory::create_ret_t, TemplateFactory::create_err_t>();
            })
            .or_else([&](const TemplateFactory::create_err_t &error) {
                if (std::holds_alternative<FSError>(error)) {
                    LogError(std::get<FSError>(error));
                } else {
                    LogError(std::get<JSONError>(error));
                }
                return Result<TemplateFactory::create_ret_t, TemplateFactory::create_err_t>();
            });

        if (!result) {
            return std::nullopt;
        }

        return scene_list[scene]->getChildren().size() - 1;
    }

    std::optional<size_t> SceneLayoutManager::addScene() {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::optional<size_t>>("SCENE_LAYOUT",
                                                       "stageArc.bin root doesn't exist!")
                         .error());
            return 0;
        }

        bool result = true;

        TemplateFactory::create("ScenarioArchiveNameTable")
            .and_then([&](TemplateFactory::create_ret_t obj_t) {
                RefPtr<ISceneObject> scene = ObjectFactory::create(*obj_t, "Default");
                if (!scene) {
                    result = false;
                    return Result<TemplateFactory::create_ret_t, TemplateFactory::create_err_t>();
                }
                scene->setNameRef(NameRef("New Scene"));
                root->addChild(scene);
                return Result<TemplateFactory::create_ret_t, TemplateFactory::create_err_t>();
            });

        if (!result) {
            return std::nullopt;
        }

        return root->getChildren().size() - 1;
    }

    bool SceneLayoutManager::moveScene(size_t src_scene, size_t dst_scene) {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::optional<size_t>>("SCENE_LAYOUT",
                                                       "stageArc.bin root doesn't exist!")
                         .error());
            return 0;
        }

        if (src_scene == dst_scene) {
            return true;
        }

        bool result = true;

        if (src_scene >= root->getChildren().size() || dst_scene >= root->getChildren().size()) {
            LogError(make_error<bool>("SCENE_LAYOUT", "Invalid scene index!").error());
            return false;
        }

        if (src_scene < dst_scene) {
            dst_scene -= 1;
        }

        RefPtr<ISceneObject> scene = root->getChildren()[src_scene];
        root->removeChild(src_scene)
            .and_then([&]() {
                root->insertChild(dst_scene, scene).or_else([&](const ObjectGroupError &error) {
                    result = false;
                    LogError(error);
                    return Result<void, ObjectGroupError>();
                });
                return Result<void, ObjectGroupError>();
            })
            .or_else([&](const ObjectGroupError &error) {
                result = false;
                LogError(error);
                return Result<void, ObjectGroupError>();
            });

        return result;
    }

    bool SceneLayoutManager::moveScenario(size_t src_scene, size_t src_scenario, size_t dst_scene,
                                          size_t dst_scenario) {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::optional<size_t>>("SCENE_LAYOUT",
                                                       "stageArc.bin root doesn't exist!")
                         .error());
            return 0;
        }

        if (src_scene == dst_scene && src_scenario == dst_scenario) {
            return true;
        }

        bool result = true;

        if (src_scene >= root->getChildren().size() || dst_scene >= root->getChildren().size()) {
            LogError(make_error<bool>("SCENE_LAYOUT", "Invalid scene index!").error());
            return false;
        }

        RefPtr<ISceneObject> src_scene_obj                  = root->getChildren()[src_scene];
        std::vector<RefPtr<ISceneObject>> src_scenario_list = src_scene_obj->getChildren();

        if (src_scenario >= src_scenario_list.size()) {
            LogError(make_error<bool>("SCENE_LAYOUT", "Invalid scenario index!").error());
            return false;
        }

        RefPtr<ISceneObject> dst_scene_obj = root->getChildren()[dst_scene];
        std::vector<RefPtr<ISceneObject>> dst_scenario_list;
        if (src_scene == dst_scene) {
            dst_scenario_list = src_scenario_list;
        } else {
            dst_scenario_list = root->getChildren()[dst_scene]->getChildren();
        }

        if (dst_scenario >= dst_scenario_list.size()) {
            LogError(make_error<bool>("SCENE_LAYOUT", "Invalid scenario index!").error());
            return false;
        }

        RefPtr<ISceneObject> scenario = src_scenario_list[src_scenario];
        if (src_scene == dst_scene) {
            if (src_scenario < dst_scenario) {
                dst_scenario -= 1;
            }
        }

        src_scene_obj->removeChild(src_scenario)
            .and_then([&]() {
                dst_scene_obj->insertChild(dst_scenario, scenario)
                    .or_else([&](const ObjectGroupError &error) {
                        result = false;
                        LogError(error);
                        return Result<void, ObjectGroupError>();
                    });
                return Result<void, ObjectGroupError>();
            })
            .or_else([&](const ObjectGroupError &error) {
                result = false;
                LogError(error);
                return Result<void, ObjectGroupError>();
            });

        return result;
    }

    bool SceneLayoutManager::removeScene(size_t scene) {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::optional<size_t>>("SCENE_LAYOUT",
                                                       "stageArc.bin root doesn't exist!")
                         .error());
            return 0;
        }

        if (scene >= root->getChildren().size()) {
            LogError(make_error<bool>("SCENE_LAYOUT", "Invalid scene index!").error());
            return false;
        }

        bool result;

        root->removeChild(scene).or_else([&](const ObjectGroupError &error) {
            result = false;
            LogError(error);
            return Result<void, ObjectGroupError>();
        });

        return result;
    }

    bool SceneLayoutManager::removeScenario(size_t scene, size_t scenario) {
        RefPtr<GroupSceneObject> root = m_scene_layout->getRoot();
        if (!root) {
            LogError(make_error<std::optional<size_t>>("SCENE_LAYOUT",
                                                       "stageArc.bin root doesn't exist!")
                         .error());
            return 0;
        }

        if (scene >= root->getChildren().size()) {
            LogError(make_error<bool>("SCENE_LAYOUT", "Invalid scene index!").error());
            return false;
        }

        RefPtr<ISceneObject> scene_obj = root->getChildren()[scene];
        std::vector<RefPtr<ISceneObject>> scenario_list = scene_obj->getChildren();

        if (scenario >= scenario_list.size()) {
            LogError(make_error<bool>("SCENE_LAYOUT", "Invalid scenario index!").error());
            return false;
        }

        bool result;

        scene_obj->removeChild(scenario).or_else([&](const ObjectGroupError &error) {
            result = false;
            LogError(error);
            return Result<void, ObjectGroupError>();
        });

        return result;
    }

}  // namespace Toolbox::Scene