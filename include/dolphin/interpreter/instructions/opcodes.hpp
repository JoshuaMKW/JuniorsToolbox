#pragma once

#include "core/types.hpp"

// From Dolphin
enum class Opcode : Toolbox::u8 {
    OP_TDI    = 2,
    OP_TWI    = 3,
    OP_PS     = 4,
    OP_MULLI  = 7,
    OP_SUBFIC = 8,
    OP_CMPLI  = 10,
    OP_BC     = 16,
    OP_B      = 18,
    OP_BCLR   = 19,
    OP_BCCTR  = 19,
};