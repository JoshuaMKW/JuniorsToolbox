#include "clone.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include <iostream>
#include <string>
#include <string_view>

using namespace Toolbox::Object;

int main(int argc, char **argv) {
    /*auto t = TemplateFactory::create("MapObjGeneral");
    auto g = TemplateFactory::create("GroupObj");

    Toolbox::Object::GroupSceneObject group(*g.value());

    auto biancoLamp = std::make_shared<Toolbox::Object::PhysicalSceneObject>(*t.value(), "Bianco
    Hills Lamp"); auto defaultObj =
    std::make_shared<Toolbox::Object::PhysicalSceneObject>(*t.value());

    group.addChild(biancoLamp);
    group.addChild(defaultObj);

    group.dump(std::cout, 0, 4);*/

    Toolbox::Scene::SceneInstance scene("C:/Users/Kyler-Josh/Dropbox/Master_Builds/Eclipse_"
                                        "Master/files/data/scene/dolpic10.szs_ext/scene");
    scene.dump(std::cout);
    return 0;
}