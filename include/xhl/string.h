/*
Very much a WIP. Do not use. Come back later!
*/

#ifndef XHL_STRING_H
#define XHL_STRING_H

#include <stdbool.h>
#include <stddef.h>

#if defined(__cplusplus) || __STDC_VERSION__ < 199901
#define xtr_restrict
#else
#define xtr_restrict restrict
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Returns length of 0 terminated string. 0 not included.
size_t xtr_len(const char* str);
bool   xtr_startswith(const char* str, const char* xtr_restrict prefix);
bool   xtr_match(const char* a, const char* xtr_restrict b);

#ifdef __cplusplus
}
#endif

#endif // XHL_STRING_H
#ifdef XHL_STRING_IMPL
#undef XHL_STRING_IMPL

size_t xtr_len(const char* str)
{
    const char* c = str;
    while (*c != 0)
        c++;
    return c - str;
}

bool xtr_startswith(const char* str, const char* xtr_restrict prefix)
{
    while (*str != 0 && prefix != 0 && *str == *prefix)
    {
        str++;
        prefix++;
    }
    return *prefix == 0;
}

bool xtr_match(const char* a, const char* xtr_restrict b)
{
    while (*a != 0 && *b != 0)
    {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

#endif // XHL_STRING_IMPL