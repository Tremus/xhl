#pragma once

#ifdef _MSC_VER
#define XHL_DEBUG_BREAK __debugbreak()
#elif __APPLE__
#define XHL_DEBUG_BREAK __builtin_debugtrap()
#else
#error Unknown environment
#endif

#define xassert(cond) (cond) ? (void)0 : XHL_DEBUG_BREAK