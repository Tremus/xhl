#ifndef XHL_DEBUG_H
#define XHL_DEBUG_H

#if defined(DEBUG) || defined(_DEBUG)

#ifdef _MSC_VER
#define XHL_DEBUG_BREAK __debugbreak()
#elif __APPLE__
#define XHL_DEBUG_BREAK __builtin_debugtrap()
#else
#error Unknown environment
#endif

#define xassert(cond) (cond) ? (void)0 : XHL_DEBUG_BREAK

#else // NDEBUG
#define XHL_DEBUG_BREAK
// clang-format off
#define xassert(cond) do { (void)(cond); } while (0)
// clang-format on
#endif

#ifdef _WIN32
// Prints to VSCode debug console properly
#define xprintf(str, ...) (printf(str, __VA_ARGS__), fflush(stdout))
#else
#define xprintf printf
#endif

#if defined(_MSC_VER) && ! defined(__clang__)
#define xstatic_assert(cond, msg) static_assert(cond, msg)
#else
#define xstatic_assert(cond, msg) _Static_assert(cond, msg)
#endif

#endif // XHL_DEBUG_H