#pragma once

#include <string>

#include "core/core.hpp"

namespace Toolbox::Platform {

    /*
        Service
        API is read only for the sake of not being a virus lol.
    */
    void TryOpenBrowserURL(const std::string &url);

}  // namespace Toolbox::Platform