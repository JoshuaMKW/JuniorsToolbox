#include "smart_resource.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "gui/application.hpp"
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
    auto &app = Toolbox::UI::MainApplication::instance();
    app.setup();
    app.run();
    app.teardown();
    return 0;
}