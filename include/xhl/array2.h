#pragma once
#include <string.h>
// Experimenting with a rewrite of array.h
#ifdef __cplusplus
#error Not supported yet...
#endif

#if !(defined(XARR_REALLOC) || defined(XARR_FREE))
#include <stdlib.h>
#define XARR_REALLOC(ptr, size) realloc(ptr, size)
#define XARR_FREE(ptr)          free(ptr)
#endif

#ifndef XARR_ASSERT
#ifndef NDEBUG
#include <assert.h>
#define XARR_ASSERT(cond) assert(cond)
#else
#define XARR_ASSERT(...)
#endif // NDEBUG
#endif // XARR_ASSERT

struct xarray_header2
{
    size_t length;   // current num elements
    size_t capacity; // max num elements
};

#define xarr_header(a) ((struct xarray_header2*)(a) - 1)
#define xarr_len(a)    ((a) ? xarr_header(a)->length : 0)
#define xarr_cap(a)    ((a) ? xarr_header(a)->capacity : 0)
#define xarr_free(a)   ((void)((a) ? XARR_FREE(xarr_header(a)) : (void)0), (a) = NULL)

static void xarray_grow(void** arr, size_t stride, size_t nextcap)
{
    XARR_ASSERT(arr != NULL);
    XARR_ASSERT(stride > 0);
    XARR_ASSERT(nextcap > 0);
    XARR_ASSERT((size_t)(*arr) % 16 == 0);
    struct xarray_header2* head = (struct xarray_header2*)*arr;
    if (head != NULL)
    {
        head--;
        XARR_ASSERT(sizeof(*head) == (char*)(*arr) - (char*)head);
        XARR_ASSERT(head->capacity < nextcap);
        if (nextcap < head->capacity * 2)
            nextcap = head->capacity * 2;
    }

    head           = XARR_REALLOC((void*)head, sizeof(*head) + stride * nextcap);
    head->capacity = nextcap;
    if (*arr == NULL) // init new array
        head->length = 0;

    head++;
    XARR_ASSERT((size_t)(head) % 16 == 0);
    *arr = head;
}

static void xarray_setcap(void** a, size_t stride, size_t N)
{
    size_t current_cap = xarr_cap(a[0]);
    if (current_cap < N)
        xarray_grow(a, stride, N);
}

static void xarray_setlen(void** a, size_t stride, size_t N)
{
    size_t len = xarr_len(a[0]);
    if (len != N)
    {
        xarray_setcap(a, stride, N);
        xarr_header(a[0])->length = N;
    }
}

#define xarr_setcap(a, N) xarray_setcap((void**)&(a), sizeof((a)[0]), N)

#define xarr_setlen(a, N) xarray_setlen((void**)&(a), sizeof((a)[0]), N)
#define xarr_addn(a, N)   (xarr_setlen(a, xarr_len(a) + (N)))
#define xarr_push(a, v)   (xarr_setcap(a, xarr_len(a) + 1), (a)[xarr_header(a)->length++] = (v))
#define xarr_insertn(a, i, N)                                                                                          \
    (xarr_addn((a), (N)), memmove(&(a)[(i) + (N)], &(a)[i], sizeof *(a) * (xarr_header(a)->length - (N) - (i))))
#define xarr_insert(a, i, v) (xarr_insertn((a), (i), 1), (a)[i] = (v))
#define xarr_deleten(a, i, N)                                                                                          \
    (memmove(&(a)[i], &(a)[(i) + (N)], sizeof *(a) * (xarr_header(a)->length - (N) - (i))),                            \
     xarr_header(a)->length -= (N))

#define xarr_delete(a, i)   xarr_deleten(a, (i), 1)
#define xarr_last(a)        ((a)[xarr_header(a)->length - 1])
#define xarr_pop(a)         (xarr_header(a)->length--, (a)[xarr_header(a)->length])
#define xarr_end(a)         ((a) + xarr_len(a))
#define xarr_copy(src, dst) (xarr_setlen(dst, xarr_len(src)), memcpy(dst, src, sizeof((dst)[0]) * xarr_len(src)))