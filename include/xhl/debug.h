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
#define xassert(cond)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        (void)(cond);                                                                                                  \
    }                                                                                                                  \
    while (0)
#endif

#ifdef __cplusplus
#define XHL_DEBUG_RESTRICT
extern "C" {
#else
#define XHL_DEBUG_RESTRICT restrict
#endif

#ifdef _WIN32
// Prints to VSCode debug console properly, unlike regular printf...
void xprintf(const char* XHL_DEBUG_RESTRICT fmt, ...);
#else
#define xprintf printf
#endif

#ifdef __cplusplus
}
#endif
#endif // XHL_DEBUG_H

#ifdef XHL_DEBUG_IMPL
#undef XHL_DEBUG_IMPL

#ifndef _DEBUG
#error "Don't print in release!"
#endif

#ifdef _WIN32
#include <stdarg.h>
#include <windows.h>

void xprintf(const char* XHL_DEBUG_RESTRICT fmt, ...)
{
    va_list args;
    char    buf[256];

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);

    OutputDebugStringA(buf);

    va_end(args);
}
#endif

#endif // XHL_DEBUG_IMPL