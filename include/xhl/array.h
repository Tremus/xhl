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
#define xarr_header(a)          ((struct xarray_header*)(a)-1)
#define xarr_len(a)             ((a) ? xarr_header(a)->length : 0)
#define xarr_cap(a)             ((a) ? xarr_header(a)->capacity : 0)
#define xarr_free(a)            ((void)((a) ? XARR_FREE(xarr_header(a)) : (void)0), (a) = NULL)
#ifdef __cplusplus
#define xarr_T T
template<class xarr_T>
#else
#define xarr_T void
#endif
static xarr_T* __xarr_setcap(xarr_T* ptr, size_t N, struct xarray_header* next_ptr) {
    if (!ptr) next_ptr->length = 0, next_ptr->capacity = 0;
    next_ptr->capacity = (N<(next_ptr->capacity*2)?(next_ptr->capacity*2):N);
    return (xarr_T*)(next_ptr+1);
}
#define xarr_setcap(a, N)       (xarr_cap(a) < (N) \
                                    ? (void)((a) = __xarr_setcap(\
                                            (a),\
                                            N,\
                                            (struct xarray_header*)XARR_REALLOC(((a) ? (void*)xarr_header(a) : (void*)a), sizeof(*a)*(N<(xarr_cap(a)*2)?(xarr_cap(a)*2):N)+sizeof(struct xarray_header))))\
                                    : (void)0)
#define xarr_setlen(a, N)       (xarr_len(a) != (N) ? (void)(xarr_setcap((a), (N)), xarr_header(a)->length = (N)) : (void)0)
#define xarr_addn(a, N)         (xarr_setlen(a, xarr_len(a) + (N)))
#define xarr_push(a, v)         (xarr_setcap(a, xarr_len(a) + 1), (a)[xarr_header(a)->length++] = (v))
#define xarr_insertn(a, i, N)   (xarr_addn((a), (N)), memmove(&(a)[(i) + (N)],  &(a)[i], sizeof *(a) * (xarr_header(a)->length - (N) - (i))))
#define xarr_insert(a, i, v)    (xarr_insertn((a), (i), 1), (a)[i] = (v))
#define xarr_deleten(a, i, N)   (memmove(&(a)[i], &(a)[(i) + (N)], sizeof *(a) * (xarr_header(a)->length - (N) - (i))), xarr_header(a)->length -= (N))
#define xarr_delete(a, i)       xarr_deleten(a, (i), 1)
#define xarr_last(a)            ((a)[xarr_header(a)->length - 1])
#define xarr_pop(a)             (xarr_header(a)->length--, (a)[xarr_header(a)->length])
#define xarr_end(a)             ((a) + xarr_len(a))
// clang-format on