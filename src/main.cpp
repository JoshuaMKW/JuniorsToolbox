#include "objlib/template.hpp"
#include <iostream>
#include <string>
#include <string_view>

using namespace Toolbox::Object;

int main(int argc, char **argv) {
    TemplateEnum test_enum("NPCClothes", MetaType::S8,
                           {
                               std::make_pair("HAT", MetaValue(static_cast<s8>(0b1))),
                               std::make_pair("SHIRT", MetaValue(static_cast<s8>(0b10))),
                               std::make_pair("PANTS", MetaValue(static_cast<s8>(0b100))),
                               std::make_pair("SHOES", MetaValue(static_cast<s8>(0b1000))),
                           },
                           true);
    test_enum.dump(std::cout, 0, 4);

    TemplateStruct test_struct_npc_info("NPCInfo", {TemplateMember("mName", MetaValue(std::string("John"))),
                                                    TemplateMember("mNoseSize", MetaValue(9000.1))});
    TemplateStruct test_struct("NPC", {TemplateMember("mHeight", MetaValue(6.2)),
                                       TemplateMember("mClothes", test_enum),
                TemplateMember("mInfo", {test_struct_npc_info, test_struct_npc_info})});

    test_struct.dump(std::cout);

    return 0;
}