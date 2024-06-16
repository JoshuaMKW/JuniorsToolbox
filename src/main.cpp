#include <string>

#include "smart_resource.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "gui/application.hpp"

using namespace Toolbox::Object;

int main(int argc, char **argv) {
    auto &app = Toolbox::GUIApplication::instance();
    app.run(argc, const_cast<const char **>(argv));
    return app.getExitCode();
}