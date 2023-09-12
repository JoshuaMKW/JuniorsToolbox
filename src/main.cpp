#include "objlib/template.hpp"
#include <iostream>
#include <string>
#include <string_view>

using namespace Toolbox::Object;

int main(int argc, char **argv) {
    MetaEnum test_enum("NPCClothes", MetaType::S8,
                       {
                           std::make_pair("HAT", MetaValue(static_cast<s8>(0b1))),
                           std::make_pair("SHIRT", MetaValue(static_cast<s8>(0b10))),
                           std::make_pair("PANTS", MetaValue(static_cast<s8>(0b100))),
                           std::make_pair("SHOES", MetaValue(static_cast<s8>(0b1000))),
                       },
                       true);
    test_enum.setFlag("HAT", true);
    test_enum.setFlag("PANTS", true);
    test_enum.setFlag("SHOES", true);
    test_enum.setFlag("PANTS", false);


    test_enum.dump(std::cout, 2, -1);

    MetaStruct test_struct_npc_info("NPCInfo", {MetaMember("mName", MetaValue(std::string("John"))),
                                                MetaMember("mNoseSize", MetaValue(9000.1))});
    MetaStruct test_struct(
        "NPC", {MetaMember("mHeight", MetaValue(6.2)), MetaMember("mClothes", test_enum),
                MetaMember("mInfo", {test_struct_npc_info, test_struct_npc_info})});

    test_struct.dump(std::cout, 0, 1, false);

    return 0;
}