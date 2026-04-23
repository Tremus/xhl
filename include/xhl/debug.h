#ifndef XHL_DEBUG_H
#define XHL_DEBUG_H

// Useful for preventing off by one bugs in debuggers
// These bugs are particularly annoying when breaking at the end of the scope, because the debugger pauses AFTER the
// scope ends, and you lose the local context within the scope
static int _xdebug_break_helper = 0;

#ifdef _WIN32
#define xdebugbreak() __debugbreak()
#else
#define xdebugbreak() __builtin_debugtrap()
#endif

#ifdef NDEBUG
#define xassert(...)
#else
#define xassert(cond) (((cond) ? (void)0 : xdebugbreak()), _xdebug_break_helper += 0)
#endif

#ifdef _WIN32
// Prints to VSCode debug console properly
#define xprintf(str, ...) (printf(str, __VA_ARGS__), fflush(stdout))
#else
#define xprintf printf
#endif

#if !defined(__cplusplus) && !defined(_MSC_VER)
#define xstatic_assert _Static_assert
#else
#include <assert.h>
#define xstatic_assert static_assert
#endif

#endif // XHL_DEBUG_H