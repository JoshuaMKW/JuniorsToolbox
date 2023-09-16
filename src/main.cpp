#include "clone.hpp"
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


    // Demonstrate enum info dumping with an indention of 2.
    test_enum.dump(std::cout, 2);
    std::cout << std::endl;

    // Demonstrate the shared resources of Meta types
    MetaStruct test_struct_npc_info("NPCInfo", {MetaMember("mName", MetaValue(std::string("John"))),
                                                MetaMember("mNoseSize", MetaValue(9000.1))});
    MetaStruct test_struct_npc_info_copy = test_struct_npc_info;
    auto name_member = test_struct_npc_info_copy.getMember("mName");
    if (name_member) {
        auto &name = std::get<std::shared_ptr<MetaMember>>(name_member.value());
        name->value<MetaValue>(0).value()->set(std::string("Jane"));
    } else {
        std::cout << "Member not found" << std::endl;
    }

    // Demonstrate the deep copy of Meta types
    auto deep_copy = std::reinterpret_pointer_cast<MetaStruct, Toolbox::IClonable>(test_struct_npc_info_copy.clone(true));
    if (deep_copy) {
        auto name_member = deep_copy->getMember("mName");
        if (name_member) {
            auto &name = std::get<std::shared_ptr<MetaMember>>(name_member.value());
            name->value<MetaValue>(0).value()->set(std::string("Josh"));
        } else {
            std::cout << "Member not found" << std::endl;
        }
    }
    else {
        std::cout << "Deep copy failed" << std::endl;
    }

    MetaStruct test_struct(
        "NPC", {MetaMember("mHeight", MetaValue(6.2)), MetaMember("mClothes", test_enum),
                MetaMember("mInfo", {test_struct_npc_info, test_struct_npc_info_copy, *deep_copy})});


    // Demonstrate structural info dumping
    test_struct.dump(std::cout);

    // Demonstrate structural searches by printing a found member
    auto clothes = test_struct.getMember("mClothes");
    if (clothes) {
        if (std::holds_alternative<std::shared_ptr<MetaStruct>>(clothes.value())) {
            auto &struct_clothes = std::get<std::shared_ptr<MetaStruct>>(clothes.value());
            struct_clothes->dump(std::cout);
        } else {
            auto &member_clothes = std::get<std::shared_ptr<MetaMember>>(clothes.value());
            member_clothes->dump(std::cout);
        }
    } else {
        std::cout << "Member not found" << std::endl;
    }

    // Demonstrate exotic structural searches by printing a found scoped member at an array index
    auto info = test_struct.getMember(QualifiedName({"mInfo[1]", "mName"}));
    if (info) {
        if (std::holds_alternative<std::shared_ptr<MetaStruct>>(info.value())) {
            auto &struct_info = std::get<std::shared_ptr<MetaStruct>>(info.value());
            struct_info->dump(std::cout);
        } else {
            auto &member_info = std::get<std::shared_ptr<MetaMember>>(info.value());
            member_info->dump(std::cout);
        }
    } else {
        std::cout << "Member not found" << std::endl;
    }

    return 0;
}