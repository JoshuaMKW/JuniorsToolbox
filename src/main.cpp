#include <cstring>
#include <string>

#include "smart_resource.hpp"
#include "objlib/object.hpp"
#include "objlib/template.hpp"
#include "scene/scene.hpp"
#include "gui/appmain/application.hpp"
#include "gui/bootstrap/application.hpp"

using namespace Toolbox::UI;

#pragma warning(push)
#pragma warning(disable : 4996)

static bool ConvBootstrapToCArgsForMain(const BootStrapApplication::BootStrapArguments &args, int *argc, char **argv) {
    if (args.m_settings_profile.has_value()) {
        argv[(*argc)++] = const_cast<char *>("--profile");
        argv[(*argc)++] = const_cast<char *>(strdup(args.m_settings_profile->c_str()));
    }

    for (const auto &window_arg : args.m_windows) {
        argv[(*argc)++] = const_cast<char *>("--open-window");
        argv[(*argc)++] = const_cast<char *>(strdup(window_arg.m_window_type.c_str()));
        
        if (window_arg.m_load_path.has_value()) {
            argv[(*argc)++] = const_cast<char *>("--window-load-path");
            argv[(*argc)++] = const_cast<char *>(strdup(window_arg.m_load_path->string().c_str()));
        }

        std::string arg_csv;
        for (const auto &varg : window_arg.m_vargs) {
            arg_csv += varg + ",";
        }

        if (!arg_csv.empty()) {
            arg_csv.pop_back();  // Pop the trailing comma
            argv[(*argc)++] = const_cast<char *>("--window-args");
            argv[(*argc)++] = const_cast<char *>(arg_csv.c_str());
        }
    }

    return true;
}

#pragma warning(pop)

int main(int argc, char **argv) {
    BootStrapApplication &bootstrap = BootStrapApplication::instance();
    bootstrap.run(argc, const_cast<const char **>(argv));
    if (const int ec = bootstrap.getExitCode()) {
        return ec;  // Exit if bootstrap failed
    }

    const BootStrapApplication::BootStrapArguments results = bootstrap.getResults();
    
    char *argv_main[128]{};
    argv_main[0] = argv[0];
    int argc_main = 1;
    if (!ConvBootstrapToCArgsForMain(results, &argc_main, argv_main)) {
        return EXIT_CODE_FAILED_SETUP;
    }

    MainApplication &main_app = MainApplication::instance();
    main_app.run(argc_main, const_cast<const char **>(argv_main));
    return main_app.getExitCode();
}