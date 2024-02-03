#include <memory>

#if !NDEBUG
#define TOOLBOX_DEBUG
#endif

#if defined(_WIN32)
#define TOOLBOX_PLATFORM_WINDOWS
#undef TOOLBOX_PLATFORM_LINUX
#elif defined(__linux__)
#undef TOOLBOX_PLATFORM_WINDOWS
#define TOOLBOX_PLATFORM_LINUX
#else
#error "Platform is unsupported!"
#endif

#ifdef TOOLBOX_DEBUG
#if defined(TOOLBOX_PLATFORM_WINDOWS)
#define TOOLBOX_DEBUGBREAK() __debugbreak()
#elif defined(TOOLBOX_PLATFORM_LINUX)
#include <signal.h>
#define TOOLBOX_DEBUGBREAK() raise(SIGTRAP)
#else
#error "Platform doesn't support debugbreak yet!"
#endif
#define TOOLBOX_ENABLE_ASSERTS
#else
#define TOOLBOX_DEBUGBREAK()
#endif

#define TOOLBOX_EXPAND_MACRO(x)    x
#define TOOLBOX_STRINGIFY_MACRO(x) #x

#define BIT(x) (1 << x)

#define TOOLBOX_BIND_EVENT_FN(fn)                                                                       \
    [this](auto &&...args) -> decltype(auto) {                                                     \
        return this->fn(std::forward<decltype(args)>(args)...);                                    \
    }

#include "core/log.hpp"
#include "core/assert.hpp"