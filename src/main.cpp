#include "objlib/template.hpp"
#include <iostream>
#include <string>
#include <string_view>

using namespace Toolbox::Object;

int main(int argc, char **argv) {
    TemplateEnum test_enum("NPCClothes", TemplateType::S8,
                           {
                               std::make_pair("HAT", static_cast<s8>(0b1)),
                               std::make_pair("SHIRT", static_cast<s8>(0b10)),
                               std::make_pair("PANTS", static_cast<s8>(0b100)),
                               std::make_pair("SHOES", static_cast<s8>(0b1000)),
                           },
                           true);
    test_enum.dump(std::cout, 0, 4);

    return 0;
}