#include "objlib/template.hpp"
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    std::cout << "Hello, World!" << std::endl;

    Toolbox::Object::TemplateValue test(std::string("Hello world!"));
    auto value = test.get<int>();
    if (value) {
        std::cout << "The value is " << *value << std::endl;
    } else {
        std::cout << value.error() << std::endl;
        std::cout << "The value was instead type " << int(test.type()) << std::endl;
        if (test.type() == Toolbox::Object::TemplateType::STRING) {
            std::cout << "The string is " << *test.get<std::string>() << std::endl;
        }
    }

    return 0;
}