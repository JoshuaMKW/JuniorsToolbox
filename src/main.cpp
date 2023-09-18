#include "clone.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include <iostream>
#include <string>
#include <string_view>

using namespace Toolbox::Object;

int main(int argc, char **argv) {
    Template template_("MapObjGeneral");

    Toolbox::Object::GroupSceneObject group(Template("GroupObj"));

    auto biancoLamp = std::make_shared<Toolbox::Object::PhysicalSceneObject>(template_, "Bianco Hills Lamp");
    auto defaultObj = std::make_shared<Toolbox::Object::PhysicalSceneObject>(template_);

    group.addChild(biancoLamp);
    group.addChild(defaultObj);

    group.dump(std::cout, 0, 4);

    return 0;
}