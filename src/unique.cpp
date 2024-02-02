#pragma once

#include "unique.hpp"

namespace Toolbox {

    static UUID64::device s_random_device;
    static UUID64::engine s_engine(s_random_device());
    static UUID64::distributor s_distributor;

    u64 UUID64::_generate() {
        return s_distributor(s_engine);
    }
}