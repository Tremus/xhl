#ifndef XHL_DEBUG_H
#define XHL_DEBUG_H

#ifdef NDEBUG
// clang-format off
#define xassert(cond) do { (void)(cond); } while (0)
// clang-format on
#else // _DEBUG

#ifdef _WIN32
#define xassert(cond) (cond) ? (void)0 : __debugbreak()
#else
#define xassert(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif

#endif // _DEBUG

#ifdef _WIN32
// Prints to VSCode debug console properly
#define xprintf(str, ...) (printf(str, __VA_ARGS__), fflush(stdout))
#else
#define xprintf printf
#endif

#if ! defined(__cplusplus) && ! defined(_MSC_VER)
#define xstatic_assert _Static_assert
#else
#include <assert.h>
#define xstatic_assert static_assert
#endif

#endif // XHL_DEBUG_H