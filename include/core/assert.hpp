#pragma once

#include <cassert>

#include "core/log.hpp"

#ifndef TOOLBOX_ENABLE_ASSERTS
#ifdef TOOLBOX_DEBUG
#define TOOLBOX_ENABLE_ASSERTS
#endif
#endif

#ifdef TOOLBOX_ENABLE_ASSERTS

// clang-format off
#define TOOLBOX_ASSERT_V(cond, errmsg, ...) if (!(cond)) { TOOLBOX_ERROR_V((errmsg), ##__VA_ARGS__); }
#define TOOLBOX_ASSERT(cond, errmsg) if (!(cond)) { TOOLBOX_ERROR((errmsg)); }

#define TOOLBOX_CORE_ASSERT(cond) { assert((cond)); }

#else

#define TOOLBOX_ASSERT_V(cond, errmsg, ...)
#define TOOLBOX_ASSERT(cond, errmsg)

#define TOOLBOX_CORE_ASSERT(cond)

#endif