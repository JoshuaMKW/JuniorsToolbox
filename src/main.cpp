#include "clone.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include <iostream>
#include <string>
#include <string_view>

using namespace Toolbox::Object;

int main(int argc, char **argv) {
    auto t = TemplateFactory::create("MapObjGeneral");
    auto g = TemplateFactory::create("GroupObj");

    Toolbox::Object::GroupSceneObject group(*g.value());

    auto biancoLamp = std::make_shared<Toolbox::Object::PhysicalSceneObject>(*t.value(), "Bianco Hills Lamp");
    auto defaultObj = std::make_shared<Toolbox::Object::PhysicalSceneObject>(*t.value());

    group.addChild(biancoLamp);
    group.addChild(defaultObj);

    group.dump(std::cout, 0, 4);

    return 0;
}