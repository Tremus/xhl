/*
Very much a WIP. Do not use. Come back later!
*/

#ifndef XHL_STRING_H
#define XHL_STRING_H

#include <stdarg.h>
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

// NOTE(bill): Some compilers support applying printf-style warnings to user functions.
#if defined(__clang__) || defined(__GNUC__)
#define XTR_PRINTF_ARGS(FMT) __attribute__((format(printf, FMT, (FMT + 1))))
#else
#define XTR_PRINTF_ARGS(FMT)
#endif

// Returns length of 0 terminated string. 0 not included.
size_t xtr_len(const char* str);
bool   xtr_startswith(const char* str, const char* XTR_RESTRICT prefix);
bool   xtr_match(const char* a, const char* XTR_RESTRICT b);
// case insensitive. expects a NULL terminated ANSI string
bool xtr_comparei(const char* a, const char* ext);

// Uses Natural Sort Order algorithm
// https://en.wikipedia.org/wiki/Natural_sort_order
int xtr_natural_compare(char const* a, char const* b, unsigned case_insensitive);

// snprintf replacement
// Only returns bytes written. Returns 0 when bad parameters, encoding error, or offset is greater than cap-1.
// NULL terminating byte is always written. Maximum string length is cap-1
// Buf should always be the beginning of the string buffer, cap the capacity (incl. null byte), and offset the current
// write position
// The implementation is based off of Ginger Bills 'gb_snprintf_va' from his gb lib. It contains several fixes and
// missing features
// Source: https://raw.githubusercontent.com/gingerBill/gb/refs/heads/master/gb.h
unsigned xtr_fmt(char* const buf, ptrdiff_t const cap, ptrdiff_t const offset, char const* const XTR_RESTRICT fmt, ...)
    XTR_PRINTF_ARGS(4);
#define xfmt(buf, offset, fmt, ...) xtr_fmt(buf, sizeof(buf), offset, fmt, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // XHL_STRING_H
#ifdef XHL_STRING_IMPL
#undef XHL_STRING_IMPL
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#define xtr_min(a, b) ((a) < (b) ? (a) : (b))
#define xtr_max(a, b) ((a) > (b) ? (a) : (b))

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

static inline char xtr_char_to_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return 'a' + (c - 'A');
    return c;
}

static inline int xtr_char_is_digit(char c) { return (c >= '0' && c <= '9'); }

static inline char xtr_char_to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return 'A' + (c - 'a');
    return c;
}

static inline int xtr_char_is_space(char c)
{
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v')
        return true;
    return false;
}

static inline int xtr_char_is_hex_digit(char c)
{
    return (xtr_char_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static inline int xtr_char_is_alpha(char c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return true;
    return false;
}

static inline int xtr_char_is_alphanumeric(char c) { return xtr_char_is_alpha(c) || xtr_char_is_digit(c); }

static inline int xtr_digit_to_int(char c) { return xtr_char_is_digit(c) ? c - '0' : c - 'W'; }

static inline int xtr_hex_digit_to_int(char c)
{
    if (xtr_char_is_digit(c))
        return xtr_digit_to_int(c);
    else if ('a' <= c && c <= 'f') // is between
        return c - 'a' + 10;
    else if ('A' <= c && c <= 'F') // is between
        return c - 'A' + 10;
    return -1;
}

bool xtr_comparei(const char* a, const char* ext)
{
    int i;
    for (i = 0; a[i] != 0 && xtr_char_to_lower(a[i]) == ext[i]; i++)
        ;
    return a[i] == ext[i];
}

/* -*- mode: c; c-file-style: "k&r" -*-

  strnatcmp.c -- Perform 'natural order' comparisons of strings in C.
  Copyright (C) 2000, 2004 by Martin Pool <mbp sourcefrog net>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* partial change history:
 *
 * 2004-10-10 mbp: Lift out character type dependencies into macros.
 *
 * Eric Sosman pointed out that ctype functions take a parameter whose
 * value must be that of an unsigned int, even on platforms that have
 * negative chars in their default char type.
 *
 * 2026 Tremus removes macros, libc funtions are replaced by existing library functions
 */

static int _xtr_strnatcmp_compare_right(char const* a, char const* b)
{
    int bias = 0;

    /* The longest run of digits wins.  That aside, the greatest
       value wins, but we can't know that it will until we've scanned
       both numbers to know that they have the same magnitude, so we
       remember it in BIAS. */
    for (;; a++, b++)
    {
        if (!xtr_char_is_digit(*a) && !xtr_char_is_digit(*b))
            return bias;
        if (!xtr_char_is_digit(*a))
            return -1;
        if (!xtr_char_is_digit(*b))
            return +1;
        if (*a < *b)
        {
            if (!bias)
                bias = -1;
        }
        else if (*a > *b)
        {
            if (!bias)
                bias = +1;
        }
        else if (!*a && !*b)
            return bias;
    }

    return 0;
}

static int _xtr_strnatcmp_compare_left(char const* a, char const* b)
{
    /* Compare two left-aligned numbers: the first to have a
       different value wins. */
    for (;; a++, b++)
    {
        if (!xtr_char_is_digit(*a) && !xtr_char_is_digit(*b))
            return 0;
        if (!xtr_char_is_digit(*a))
            return -1;
        if (!xtr_char_is_digit(*b))
            return +1;
        if (*a < *b)
            return -1;
        if (*a > *b)
            return +1;
    }

    return 0;
}

int xtr_natural_compare(char const* a, char const* b, unsigned fold_case)
{
    int  ai, bi;
    char ca, cb;
    int  fractional, result;

    ai = bi = 0;
    while (1)
    {
        ca = a[ai];
        cb = b[bi];

        /* skip over leading spaces or zeros */
        while (xtr_char_is_space(ca))
            ca = a[++ai];

        while (xtr_char_is_space(cb))
            cb = b[++bi];

        /* process run of digits */
        if (xtr_char_is_digit(ca) && xtr_char_is_digit(cb))
        {
            fractional = (ca == '0' || cb == '0');

            if (fractional)
            {
                if ((result = _xtr_strnatcmp_compare_left(a + ai, b + bi)) != 0)
                    return result;
            }
            else
            {
                if ((result = _xtr_strnatcmp_compare_right(a + ai, b + bi)) != 0)
                    return result;
            }
        }

        if (!ca && !cb)
        {
            /* The strings compare the same.  Perhaps the caller
               will want to call strcmp to break the tie. */
            return 0;
        }

        if (fold_case)
        {
            ca = xtr_char_to_upper(ca);
            cb = xtr_char_to_upper(cb);
        }

        if (ca < cb)
            return -1;

        if (ca > cb)
            return +1;

        ++ai;
        ++bi;
    }
}

static inline ptrdiff_t _xtr_has_zero(ptrdiff_t x) { return (x)-0x101010101010101 & ~(x) & 0x8080808080808080; }

enum
{
    XTR_FMT_MINUS = (1 << 0),
    XTR_FMT_PLUS  = (1 << 1),
    XTR_FMT_ALT   = (1 << 2),
    XTR_FMT_SPACE = (1 << 3),
    XTR_FMT_ZERO  = (1 << 4),

    XTR_FMT_CHAR   = (1 << 5),
    XTR_FMT_SHORT  = (1 << 6),
    XTR_FMT_Int    = (1 << 7),
    XTR_FMT_LONG   = (1 << 8),
    XTR_FMT_LLONG  = (1 << 9),
    XTR_FMT_SIZE   = (1 << 10),
    XTR_FMT_INTPTR = (1 << 11),

    XTR_FMT_UNSIGNED = (1 << 12),
    XTR_FMT_LOWER    = (1 << 13),
    XTR_FMT_UPPER    = (1 << 14),

    XTR_FMT_DONE = (1 << 30),

    XTR_FMT_INTS =
        XTR_FMT_CHAR | XTR_FMT_SHORT | XTR_FMT_Int | XTR_FMT_LONG | XTR_FMT_LLONG | XTR_FMT_SIZE | XTR_FMT_INTPTR
};

typedef struct XTRFmtInfo
{
    int base;
    int flags;
    int width;
    int precision;
} XTRFmtInfo;

static inline ptrdiff_t xtr_strlen(char const* str)
{
    char const*      begin = str;
    ptrdiff_t const* w;
    if (str == NULL)
    {
        return 0;
    }
    while ((uintptr_t)str % sizeof(size_t))
    {
        if (!*str)
            return str - begin;
        str++;
    }
    w = (ptrdiff_t const*)str;
    while (!_xtr_has_zero(*w))
    {
        w++;
    }
    str = (char const*)w;
    while (*str)
    {
        str++;
    }
    return str - begin;
}

static inline int xtr_strncmp(char const* s1, char const* s2, ptrdiff_t len)
{
    for (; len > 0; s1++, s2++, len--)
    {
        if (*s1 != *s2)
        {
            return ((s1 < s2) ? -1 : +1);
        }
        else if (*s1 == '\0')
        {
            return 0;
        }
    }
    return 0;
}

static inline void xtr_str_to_lower(char* str)
{
    if (!str)
        return;
    while (*str)
    {
        *str = xtr_char_to_lower(*str);
        str++;
    }
}

static inline void xtr_str_to_upper(char* str)
{
    if (!str)
        return;
    while (*str)
    {
        *str = xtr_char_to_upper(*str);
        str++;
    }
}

ptrdiff_t _xtr_scan_i64(char const* text, int base, int64_t* value)
{
    char const* text_begin = text;
    int64_t     result     = 0;
    int         negative   = 0;

    if (*text == '-')
    {
        negative = 1;
        text++;
    }

    if (base == 16 && xtr_strncmp(text, "0x", 2) == 0)
    {
        text += 2;
    }

    for (;;)
    {
        int64_t v;
        if (xtr_char_is_digit(*text))
        {
            v = *text - '0';
        }
        else if (base == 16 && xtr_char_is_hex_digit(*text))
        {
            v = xtr_hex_digit_to_int(*text);
        }
        else
        {
            break;
        }

        result *= base;
        result += v;
        text++;
    }

    if (value)
    {
        if (negative)
            result = -result;
        *value = result;
    }

    return (text - text_begin);
}

int64_t xtr_str_to_i64(char const* str, char** end_ptr, int base)
{
    ptrdiff_t len;
    int64_t   value;

    if (!base)
    {
        if ((xtr_strlen(str) > 2) && (xtr_strncmp(str, "0x", 2) == 0))
        {
            base = 16;
        }
        else
        {
            base = 10;
        }
    }

    len = _xtr_scan_i64(str, base, &value);
    if (end_ptr)
        *end_ptr = (char*)str + len;
    return value;
}

const char _xtr_num_to_char_table[] = "0123456789"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "abcdefghijklmnopqrstuvwxyz"
                                      "@$";

static inline char* xtr_strrev(char* str)
{
    ptrdiff_t len  = xtr_strlen(str);
    char*     a    = str + 0;
    char*     b    = str + len - 1;
    len           /= 2;
    while (len--)
    {
        char temp = *a;
        *a        = *b;
        *b        = temp;
        a++, b--;
    }
    return str;
}

static inline void xtr_u64_to_str(uint64_t value, char* string, int base, int flags)
{
    char* buf = string;

    if (value)
    {
        while (value > 0)
        {
            *buf++  = _xtr_num_to_char_table[value % base];
            value  /= base;
        }
    }
    else
    {
        *buf++ = '0';
    }
    if (flags & XTR_FMT_ALT)
    {
        *buf++ = 'x';
        *buf++ = '0';
    }
    *buf = '\0';

    xtr_strrev(string);
}

static inline ptrdiff_t xtr_strlcpy(char* dest, char const* source, ptrdiff_t len)
{
    ptrdiff_t result = 0;
    XTR_ASSERT(dest != NULL);
    if (source)
    {
        char const* source_start = source;
        char*       str          = dest;
        while (len > 0 && *source)
        {
            *str++ = *source++;
            len--;
        }
        while (len > 0)
        {
            *str++ = '\0';
            len--;
        }

        result = source - source_start;
    }
    return result;
}

void const* xtr_memchr(void const* data, uint8_t c, ptrdiff_t n)
{
    uint8_t const* s = (uint8_t const*)data;
    while (((uintptr_t)s & (sizeof(size_t) - 1)) && n && *s != c)
    {
        s++;
        n--;
    }
    if (n && *s != c)
    {
        ptrdiff_t const* w;
        ptrdiff_t        k = 0x101010101010101 * c;
        w                  = (ptrdiff_t const*)s;
        while (n >= (ptrdiff_t)sizeof(ptrdiff_t) && !_xtr_has_zero(*w ^ k))
        {
            w++;
            n -= (ptrdiff_t)sizeof(ptrdiff_t);
        }
        s = (uint8_t const*)w;
        while (n && *s != c)
        {
            s++;
            n--;
        }
    }

    return n ? (void const*)s : NULL;
}

static inline ptrdiff_t xtr_strnlen(char const* str, ptrdiff_t max_len)
{
    char const* end = (char const*)xtr_memchr(str, 0, max_len);
    if (end)
    {
        return end - str;
    }
    return max_len;
}

// TODO: make info never NULL
ptrdiff_t _xtr_print_string(char* text, ptrdiff_t max_len, const XTRFmtInfo* info, char const* str)
{
    // TODO(bill): Get precision and width to work correctly. How does it actually work?!
    // UPDATE(tremus) I think I fixed it?
    //     %s Precision specifies the maximum number of bytes to be written. If Precision is not specified, writes every
    //        byte up to and not including the first null terminator.
    //     %d Precision specifies the minimum number of digits to appear. The default precision is 1
    //     %i
    //     %o
    //     %x
    //     %X
    //     %u

    // Handled in _xtr_print_f64()
    //     %f Precision specifies the exact number of digits to appear after the decimal point character.
    //     %e
    //     %E
    //     %a Precision specifies the exact number of digits to appear after the hexadecimal point character.
    //     %A

    // TODO(bill): This looks very buggy indeed.
    ptrdiff_t res       = 0, len, delta;
    ptrdiff_t remaining = max_len;

    if (info && info->precision >= 0)
    {
        len = xtr_strnlen(str, info->precision);
    }
    else
    {
        len = xtr_strlen(str);
    }
    XTR_ASSERT((res + len) <= remaining); // WARNING: Buffer truncated. Allocate more space!
    len = xtr_min(len, remaining);
    XTR_ASSERT(len <= remaining);

    int padding_before = info && (info->width > res) && (0 == (info->flags & XTR_FMT_MINUS));
    int padding_zeros  = info && info->base > 0 && info->precision > 0;
    int padding_after  = info && info->width && !!(info->flags & XTR_FMT_MINUS);

    // TODO: combine this with the bottom
    if (padding_before)
    {
        ptrdiff_t padding = info->width - len;
        char      pad     = (info->flags & XTR_FMT_ZERO) ? '0' : ' ';
        while (padding-- > 0 && remaining > 0)
        {
            text[res++] = pad;
            remaining--;
        }
    }
    if (padding_zeros)
    {
        ptrdiff_t padding = info->precision - len;
        char      pad     = '0';
        while (padding-- > 0 && remaining > 0)
        {
            text[res++] = pad;
            remaining--;
        }
    }

    if (info && info->precision > 0)
    {
        len = info->precision < len ? info->precision : len;
    }

    XTR_ASSERT((res + len) <= remaining); // WARNING: Buffer truncated. Allocate more space!

    len        = xtr_min(len, remaining);
    delta      = xtr_strlcpy(text + res, str, len);
    res       += delta;
    remaining -= delta;

    if (padding_after)
    {
        ptrdiff_t padding = info->width - len;
        char      pad     = (info->flags & XTR_FMT_ZERO) ? '0' : ' ';
        while (padding-- > 0 && remaining > 0)
        {
            XTR_ASSERT(remaining > 0);
            text[res++] = pad;
            remaining--;
        }
    }

    if (info)
    {
        if (info->flags & XTR_FMT_UPPER)
        {
            xtr_str_to_upper(text);
        }
        else if (info->flags & XTR_FMT_LOWER)
        {
            xtr_str_to_lower(text);
        }
    }

    XTR_ASSERT(res <= max_len);
    return res;
}

// NOTE: 'info' may be null
ptrdiff_t _xtr_print_u64(char* text, ptrdiff_t max_len, XTRFmtInfo* info, uint64_t value)
{
    char num[130];
    xtr_u64_to_str(value, num, info ? info->base : 10, info ? info->flags : 0);
    return _xtr_print_string(text, max_len, info, num);
}

ptrdiff_t _xtr_print_f64(char* text, ptrdiff_t max_len, XTRFmtInfo* info, double arg)
{
    // TODO(bill): Handle exponent notation
    ptrdiff_t width, len, remaining = max_len;
    ptrdiff_t virtual_len = 0; // length of non truncated string. Useful for setting correct padding
    char*     text_begin  = text;

    int is_nan = isnan(arg);
    int is_inf = isinf(arg);
    if (is_nan || is_inf)
    {
        if (remaining && arg < 0)
        {
            remaining--;
            *text++ = '-';
        }
        XTR_ASSERT(3 <= remaining); // WARNING: Buffer truncated. Allocate more space!
        len        = xtr_min(3, remaining);
        len        = xtr_strlcpy(text, is_nan ? "nan" : "inf", len);
        remaining -= len;
        text      += len;

        XTR_ASSERT(remaining >= 0);                 // Critical error!
        XTR_ASSERT((text - text_begin) <= max_len); // Critical error!
        return (text - text_begin);
    }

    if (arg)
    {
        uint64_t value;
        if (arg < 0)
        {
            if (remaining > 0)
            {
                *text = '-', remaining--;
            }
            text++;
            arg = -arg;
        }
        else if (info->flags & XTR_FMT_MINUS)
        {
            if (remaining > 0)
            {
                *text = '+', remaining--;
            }
            text++;
        }

        value = (uint64_t)arg;
        if (info && info->precision == 0)
            value = (uint64_t)round(arg);
        len = _xtr_print_u64(text, remaining, NULL, value);
        XTR_ASSERT(len <= remaining); // Critical error!
        text      += len;
        remaining -= len;

        arg -= value;

        if (info->precision < 0)
        {
            info->precision = 6;
        }

        virtual_len = (text - text_begin) + info->precision;

        int alt           = !!(info->flags & XTR_FMT_ALT);
        int has_precision = info->precision > 0;
        if (alt || has_precision)
        {
            int64_t mult = 10;
            if (remaining > 0)
            {
                *text++ = '.', remaining--;
            }
            virtual_len++;
            XTR_ASSERT(remaining >= 0);                 // Critical error!
            XTR_ASSERT((text - text_begin) <= max_len); // Critical error!
            while (info->precision-- > 0)
            {
                value = (uint64_t)(arg * mult);
                len   = _xtr_print_u64(text, remaining, NULL, value);
                XTR_ASSERT(len <= remaining); // Critical error!
                text      += len;
                remaining -= len;
                arg       -= (double)value / mult;
                mult      *= 10;
            }
        }
        XTR_ASSERT(remaining >= 0);                 // Critical error!
        XTR_ASSERT((text - text_begin) <= max_len); // Critical error!
    }
    else
    {
        if (remaining > 0)
        {
            *text = '0', remaining--;
            text++;
        }
        if (info->flags & XTR_FMT_ALT)
        {
            if (remaining > 0)
            {
                *text = '.', remaining--;
                text++;
            }
        }
        XTR_ASSERT(remaining >= 0);                 // Critical error!
        XTR_ASSERT((text - text_begin) <= max_len); // Critical error!
        virtual_len = (text - text_begin);
    }

    // Padding
    len = (text - text_begin);
    XTR_ASSERT(virtual_len >= len);
    width = xtr_min(info->width - virtual_len, max_len);
    if (width > 0)
    {
        char  fill = (info->flags & XTR_FMT_ZERO) ? '0' : ' ';
        char* end  = text + remaining;

        // memmove
        while (len--)
        {
            if ((text_begin + len + width) < end)
            {
                *(text_begin + len + width) = *(text_begin + len);
            }
        }

        len        = xtr_min(remaining, width);
        text      += len;
        remaining -= len;
        XTR_ASSERT((text - text_begin) <= max_len); // Critical error!
        XTR_ASSERT(remaining >= 0);                 // Critical error!

        len = width;
        while (len--) // add padding if len = >= 1
        {
            if (text_begin + len < end)
            {
                text_begin[len] = fill;
            }
        }
    }

    len = text - text_begin;
    XTR_ASSERT(remaining >= 0); // Critical error!
    XTR_ASSERT(len <= max_len); // Critical error!
    return len;
}

ptrdiff_t _xtr_print_char(char* text, ptrdiff_t max_len, XTRFmtInfo* info, char arg)
{
    char str[2] = "";
    str[0]      = arg;
    return _xtr_print_string(text, max_len, info, str);
}

static inline void xtr_i64_to_str(int64_t value, char* string, int base, int flags)
{
    char*    buf      = string;
    int      negative = 0;
    uint64_t v;
    if (value < 0)
    {
        negative = 1;
        value    = -value;
    }
    v = (uint64_t)value;
    if (v != 0)
    {
        while (v > 0)
        {
            *buf++  = _xtr_num_to_char_table[v % base];
            v      /= base;
        }
    }
    else
    {
        *buf++ = '0';
    }
    if (negative)
    {
        *buf++ = '-';
    }
    else if (flags & XTR_FMT_PLUS)
    {
        *buf++ = '+';
    }
    if (base == 8 && value > 0 && (flags & XTR_FMT_ALT))
    {
        *buf++ = '0';
    }
    *buf = '\0';
    xtr_strrev(string);
}

// NOTE: 'info' may be null
// TODO: make it never null
ptrdiff_t _xtr_print_i64(char* text, ptrdiff_t max_len, XTRFmtInfo* info, int64_t value)
{
    char num[130];
    xtr_i64_to_str(value, num, info ? info->base : 10, info ? info->flags : 0);
    return _xtr_print_string(text, max_len, info, num);
}

ptrdiff_t xtr_fmt_va(char* text, ptrdiff_t max_len, char const* fmt, va_list va)
{
    char const* text_begin = text;
    ptrdiff_t   remaining = max_len - 1, res = 0;

    while (*fmt && remaining > 0)
    {
        XTRFmtInfo info = {0};
        ptrdiff_t  len  = 0;
        info.precision  = -1;

        while (*fmt && *fmt != '%' && remaining > 0)
        {
            *text++ = *fmt++;
            remaining--;

            res = (text - text_begin);
            XTR_ASSERT(res <= max_len); // Critical error!
        }
        XTR_ASSERT(remaining >= 0); // Critical error!
        if (remaining == 0)
            continue;

        if (*fmt == '%')
        {
            do
            {
                switch (*++fmt)
                {
                case '-':
                    info.flags |= XTR_FMT_MINUS;
                    break;
                case '+':
                    info.flags |= XTR_FMT_PLUS;
                    break;
                case '#':
                    info.flags |= XTR_FMT_ALT;
                    break;
                case ' ':
                    info.flags |= XTR_FMT_SPACE;
                    break;
                case '0':
                    info.flags |= XTR_FMT_ZERO;
                    break;
                default:
                    info.flags |= XTR_FMT_DONE;
                    break;
                }
            }
            while (!(info.flags & XTR_FMT_DONE));
        }

        // Optional Width. eg "%.*s"
        if (*fmt == '*')
        {
            int width = va_arg(va, int);
            if (width < 0)
            {
                info.flags |= XTR_FMT_MINUS;
                info.width  = -width;
            }
            else
            {
                info.width = width;
            }
            fmt++;
        }
        else
        {
            info.width = (int)xtr_str_to_i64(fmt, (char**)&fmt, 10);
        }

        // Optional Precision. eg "%.2f", "%.6i"
        if (*fmt == '.')
        {
            fmt++;
            if (*fmt == '*')
            {
                info.precision = va_arg(va, int);
                fmt++;
            }
            else
            {
                info.precision = (int)xtr_str_to_i64(fmt, (char**)&fmt, 10);
            }
            // This line is from GB. Seems to be a bug when using formatting such as "%05.2f"
            // info.flags &= ~XTR_FMT_ZERO;
        }

        switch (*fmt++)
        {
        case 'h':
            if (*fmt == 'h')
            { // hh => char
                info.flags |= XTR_FMT_CHAR;
                fmt++;
            }
            else
            { // h => short
                info.flags |= XTR_FMT_SHORT;
            }
            break;

        case 'l':
            if (*fmt == 'l')
            { // ll => long long
                info.flags |= XTR_FMT_LLONG;
                fmt++;
            }
            else
            { // l => long
                info.flags |= XTR_FMT_LONG;
            }
            break;

            break;

        case 'z': // NOTE(bill): size_t
            info.flags |= XTR_FMT_UNSIGNED;
            // fallthrough
        case 't': // NOTE(bill): ptrdiff_t
            info.flags |= XTR_FMT_SIZE;
            break;

        default:
            fmt--;
            break;
        }

        switch (*fmt)
        {
        case 'u':
            info.flags |= XTR_FMT_UNSIGNED;
            // fallthrough
        case 'd':
        case 'i':
            info.base = 10;
            break;

        case 'o':
            info.base = 8;
            break;

        case 'x':
            info.base   = 16;
            info.flags |= (XTR_FMT_UNSIGNED | XTR_FMT_LOWER);
            break;

        case 'X':
            info.base   = 16;
            info.flags |= (XTR_FMT_UNSIGNED | XTR_FMT_UPPER);
            break;

        case 'f':
        case 'F':
        case 'g':
        case 'G':
            len = _xtr_print_f64(text, remaining, &info, va_arg(va, double));
            break;

        case 'a':
        case 'A':
            // TODO(bill):
            break;
        case 'e':
        case 'E':
            // TODO
            break;

        case 'c':
            len = _xtr_print_char(text, remaining, &info, (char)va_arg(va, int));
            break;

        case 's':
            len = _xtr_print_string(text, remaining, &info, va_arg(va, char*));
            break;

        case 'p':
            info.base   = 16;
            info.flags |= (XTR_FMT_LOWER | XTR_FMT_UNSIGNED | XTR_FMT_ALT | XTR_FMT_INTPTR);
            break;

        case '%':
            len = _xtr_print_char(text, remaining, &info, '%');
            break;

        default:
            fmt--;
            break;
        }
        res = (text - text_begin);
        XTR_ASSERT(res < max_len); // Critical error!

        if (info.base != 0)
        {
            if (info.flags & XTR_FMT_UNSIGNED)
            {
                uint64_t value = 0;
                switch (info.flags & XTR_FMT_INTS)
                {
                case XTR_FMT_CHAR:
                    value = (uint64_t)(uint8_t)va_arg(va, int);
                    break;
                case XTR_FMT_SHORT:
                    value = (uint64_t)(uint16_t)va_arg(va, int);
                    break;
                case XTR_FMT_LONG:
                    value = (uint64_t)va_arg(va, unsigned long);
                    break;
                case XTR_FMT_LLONG:
                    value = (uint64_t)va_arg(va, unsigned long long);
                    break;
                case XTR_FMT_SIZE:
                    value = (uint64_t)va_arg(va, size_t);
                    break;
                case XTR_FMT_INTPTR:
                    value = (uint64_t)va_arg(va, uintptr_t);
                    break;
                default:
                    value = (uint64_t)va_arg(va, unsigned int);
                    break;
                }

                len = _xtr_print_u64(text, remaining, &info, value);
                XTR_ASSERT(len <= remaining);
            }
            else
            {
                int64_t value = 0;
                switch (info.flags & XTR_FMT_INTS)
                {
                case XTR_FMT_CHAR:
                    value = (int64_t)(int8_t)va_arg(va, int);
                    break;
                case XTR_FMT_SHORT:
                    value = (int64_t)(int16_t)va_arg(va, int);
                    break;
                case XTR_FMT_LONG:
                    value = (int64_t)va_arg(va, long);
                    break;
                case XTR_FMT_LLONG:
                    value = (int64_t)va_arg(va, long long);
                    break;
                case XTR_FMT_SIZE:
                    value = (int64_t)va_arg(va, size_t);
                    break;
                case XTR_FMT_INTPTR:
                    value = (int64_t)va_arg(va, uintptr_t);
                    break;
                default:
                    value = (int64_t)va_arg(va, int);
                    break;
                }

                len = _xtr_print_i64(text, remaining, &info, value);
                XTR_ASSERT(len <= remaining);
            }
        }

        remaining -= len;
        text      += len;
        XTR_ASSERT(remaining >= 0); // Critical error!

        fmt++;
    }

    *text = '\0';
    res   = (text - text_begin);
    XTR_ASSERT(res < max_len); // Critical error!
    return (res >= max_len || res < 0) ? 0 : res;
}

unsigned xtr_fmt(char* const buf, ptrdiff_t const cap, ptrdiff_t const offset, char const* const XTR_RESTRICT fmt, ...)
{
    ptrdiff_t n;
    ptrdiff_t remaining = cap - offset - 1;
    va_list   args;

    int early_return  = buf == NULL;
    early_return     |= offset < 0;
    early_return     |= remaining <= 0;
    early_return     |= fmt == NULL;
    XTR_ASSERT(early_return == 0);
    if (early_return)
        return 0;

    va_start(args, fmt);
    // n = vsnprintf(buf + offset, cap - offset, fmt, args); // benchmark against libc
    n = xtr_fmt_va(buf + offset, cap - offset, fmt, args);
    va_end(args);

    XTR_ASSERT(n >= 0); // encoding error
    if (n < 0)
        n = 0;

    XTR_ASSERT(remaining > n); // Your string is being truncated
    if (n > remaining)
        n = remaining;

    return n;
}

#endif // XHL_STRING_IMPL