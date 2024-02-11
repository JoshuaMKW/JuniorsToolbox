#pragma once

#include "core/types.hpp"

enum Opcode : Toolbox::u8 {
    Invalid,
    Subtable,
    Integer,
    CR,
    SPR,
    System,
    SystemFP,
    Load,
    Store,
    LoadFP,
    StoreFP,
    DoubleFP,
    SingleFP,
    LoadPS,
    StorePS,
    PS,
    DataCache,
    InstructionCache,
    Branch,
    Unknown,
};