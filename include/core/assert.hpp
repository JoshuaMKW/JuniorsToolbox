#pragma once

#include <cassert>

#include "core/log.hpp"

#ifdef TOOLBOX_ENABLE_ASSERTS

// clang-format off
#define TOOLBOX_ASSERT_EX(cond, type, errmsg) { if (!(check)) { Toolbox::Log::AppLogger::instance().log((type), (errmsg)); } }
#define TOOLBOX_ASSERT(cond, errmsg) TOOLBOX_ASSERT_EX((cond), Toolbox::Log::ReportLevel::DEBUG, (errmsg))

#define TOOLBOX_CORE_ASSERT(cond) { assert((cond)); }

#else

#define TOOLBOX_ASSERT_EX(cond, type, errmsg)
#define TOOLBOX_ASSERT(cond, errmsg)

#define TOOLBOX_CORE_ASSERT(cond)

#endif