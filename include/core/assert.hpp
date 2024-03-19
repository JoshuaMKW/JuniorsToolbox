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
#define TOOLBOX_ASSERT_EX_V(cond, type, errmsg, ...) { if (!(cond)) { Toolbox::Log::AppLogger::instance().log((type), (errmsg), ##__VA_ARGS__); } }
#define TOOLBOX_ASSERT_V(cond, errmsg, ...) TOOLBOX_ASSERT_EX_V((cond), Toolbox::Log::ReportLevel::DEBUG, (errmsg), ##__VA_ARGS__)

#define TOOLBOX_ASSERT_EX(cond, type, errmsg) { if (!(cond)) { Toolbox::Log::AppLogger::instance().log((type), (errmsg)); } }
#define TOOLBOX_ASSERT(cond, errmsg) TOOLBOX_ASSERT_EX((cond), Toolbox::Log::ReportLevel::DEBUG, (errmsg))

#define TOOLBOX_CORE_ASSERT(cond) { assert((cond)); }

#else

#define TOOLBOX_ASSERT_EX_V(cond, type, errmsg, ...)
#define TOOLBOX_ASSERT_V(cond, errmsg, ...)

#define TOOLBOX_ASSERT_EX(cond, type, errmsg)
#define TOOLBOX_ASSERT(cond, errmsg)

#define TOOLBOX_CORE_ASSERT(cond)

#endif