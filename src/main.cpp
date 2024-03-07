#include "smart_resource.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "core/application.hpp"
#include <chrono>
#include <imgui.h>
// #include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <string>
#include <string_view>

#include <gui/modelcache.hpp>

using namespace Toolbox::Object;

int main(int argc, char **argv) {
    auto &app = Toolbox::MainApplication::instance();
    if (!app.setup()) {
        return EXIT_CODE_FAILED_SETUP;
    }
    int exit_code = app.run();
    if (!app.teardown()) {
        return EXIT_CODE_FAILED_TEARDOWN;
    }
    return exit_code;
}