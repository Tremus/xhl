/*
Very much a WIP. Do not use. Come back later!
*/

#ifndef XHL_STRING_H
#define XHL_STRING_H

#include <stdbool.h>
#include <stddef.h>

#if defined(__cplusplus) || __STDC_VERSION__ < 199901
#define XTR_RESTRICT
#else
#define XTR_RESTRICT restrict
#endif

#ifndef XTR_ASSERT
#ifdef NDEBUG
#define XTR_ASSERT(...)
#else
#ifdef _WIN32
#define XTR_ASSERT(cond) (cond) ? (void)0 : __debugbreak()
#else // #if __APPLE__
#define XTR_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif // _WIN32
#endif // NDEBUG
#endif // XTR_ASSERT

#ifdef __cplusplus
extern "C" {
#endif

// Returns length of 0 terminated string. 0 not included.
size_t xtr_len(const char* str);
bool   xtr_startswith(const char* str, const char* XTR_RESTRICT prefix);
bool   xtr_match(const char* a, const char* XTR_RESTRICT b);

// snprintf replacement
// Only returns bytes written. Returns 0 when bad parameters, encoding error, or offset is greater than cap-1.
// NULL terminating byte is always written. Maximum string length is cap-1
// Buf should always be the beginning of the string buffer, cap the capacity (incl. null byte), and offset the current
// write position
unsigned xtr_fmt(char* const buf, ptrdiff_t const cap, ptrdiff_t const offset, char const* const XTR_RESTRICT fmt, ...);
#define xfmt(buf, offset, fmt, ...) xtr_fmt(buf, sizeof(buf), offset, fmt, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // XHL_STRING_H
#ifdef XHL_STRING_IMPL
#undef XHL_STRING_IMPL
#include <stdarg.h>
#include <stdio.h>

size_t xtr_len(const char* str)
{
    const char* c = str;
    while (*c != 0)
        c++;
    return c - str;
}

bool xtr_startswith(const char* str, const char* XTR_RESTRICT prefix)
{
    while (*str != 0 && prefix != 0 && *str == *prefix)
    {
        str++;
        prefix++;
    }
    return *prefix == 0;
}

bool xtr_match(const char* a, const char* XTR_RESTRICT b)
{
    while (*a != 0 && *b != 0)
    {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

// TODO: replace with ginger bills snprintf() in his gb lib. I've had enough of stdio's shenanigans
unsigned xtr_fmt(char* const buf, ptrdiff_t const cap, ptrdiff_t const offset, char const* const XTR_RESTRICT fmt, ...)
{
    int early_return  = buf == NULL;
    early_return     |= cap <= 0;
    early_return     |= offset < 0;
    early_return     |= offset >= cap - 1;
    early_return     |= fmt == NULL;
    XTR_ASSERT(early_return == 0);
    if (early_return)
        return 0;

    int     n;
    va_list args;
    va_start(args, fmt);
    n = vsnprintf(buf + offset, cap - offset, fmt, args);
    va_end(args);

    XTR_ASSERT(n >= 0); // encoding error
    if (n < 0)
        n = 0;

    ptrdiff_t remaining = cap - offset - 1;
    XTR_ASSERT(remaining > n); // Your string is being truncated
    if (n > remaining)
        n = remaining;

    return n;
}

#endif // XHL_STRING_IMPL