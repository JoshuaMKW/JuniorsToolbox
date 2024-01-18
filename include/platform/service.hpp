#pragma once

#include <expected>
#include <string_view>

#include "fsystem.hpp"

namespace Toolbox::Platform {
    
    /*
        Platform API is read only for the sake of not being a virus lol.
    */
    std::expected<bool, BaseError> IsServiceRunning(std::string_view service_name);

}