#pragma once
// Minimal, array only refactor of stb_ds.h by Sean Barrett.
// https://github.com/nothings/stb/blob/master/stb_ds.h
#include <stddef.h>
#include <string.h>
#if ! (defined(XARR_REALLOC) || defined(XARR_FREE))
#include <stdlib.h>
#define XARR_REALLOC(ptr, size) realloc(ptr, size)
#define XARR_FREE(ptr) free(ptr)
#endif
struct xarray_header
{
    size_t length;   // current num elements
    size_t capacity; // max num elements
};
// clang-format off
#define xarr_header(a)        ((struct xarray_header*)(a)-1)
#define xarr_len(a)           ((a) ? xarr_header(a)->length : 0)
#define xarr_cap(a)           ((a) ? xarr_header(a)->capacity : 0)
#define xarr_free(a)          ((void)((a) ? XARR_FREE(xarr_header(a)) : (void)0), (a) = NULL)
#ifdef __cplusplus
template<class T> static T* __xarr_setcap(T* ptr, size_t N, size_t type_size)
#else
static void* __xarr_setcap(void* ptr, size_t N, size_t type_size)
#endif
{
    size_t next_cap = xarr_cap(ptr) * 2;
    next_cap = next_cap < 8 ? 8 : next_cap;
    next_cap = next_cap < N ? N : next_cap;
    const size_t alloc_size = type_size * next_cap + sizeof(struct xarray_header);
    struct xarray_header* next_ptr = (struct xarray_header*)XARR_REALLOC((ptr ? (void*)xarr_header(ptr) : (void*)ptr), alloc_size);
    next_ptr->length = 0;
    next_ptr->capacity = next_cap;
#ifdef __cplusplus
    return (T*)(next_ptr+1);
#else
    return next_ptr+1;
#endif
}
#define xarr_setcap(a, N)     (xarr_cap(a) < (N) ? (void)((a) = __xarr_setcap((a), (N), sizeof(*(a)))) : (void)0)
#define xarr_setlen(a, N)     (xarr_setcap((a), (N)), xarr_header(a)->length = (N))
#define xarr_addn(a, N)       (xarr_setcap(a, xarr_len(a) + (N)), xarr_header(a)->length += (N))
#define xarr_push(a, v)       (xarr_setcap(a, xarr_len(a) + 1), (a)[xarr_header(a)->length++] = (v))
#define xarr_insertn(a, i, N) (xarr_addn((a), (N)), memmove(&(a)[(i) + (N)],  &(a)[i], sizeof *(a) * (xarr_header(a)->length - (N) - (i))))
#define xarr_insert(a, i, v)  (xarr_insertn((a), (i), 1), (a)[i] = (v))
#define xarr_deleten(a, i, N) (memmove(&(a)[i], &(a)[(i) + (N)], sizeof *(a) * (xarr_header(a)->length - (N) - (i))), xarr_header(a)->length -= (N))
#define xarr_delete(a, i)     xarr_deleten(a, (i), 1)
#define xarr_last(a)          ((a)[xarr_header(a)->length - 1])
#define xarr_pop(a)           (xarr_header(a)->length--, (a)[xarr_header(a)->length])
// clang-format on