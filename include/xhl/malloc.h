#ifndef XHL_MALLOC_H
#define XHL_MALLOC_H
// Quick and dirty xmalloc

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
void xmalloc_init();
void xmalloc_shutdown();

void* xmalloc(size_t size);
void* xrealloc(void*, size_t size);
void  xfree(void*);

#ifdef __cplusplus
}
#endif
#endif // XHL_MALLOC_H

#ifdef XHL_MALLOC_IMPL
#undef XHL_MALLOC_IMPL
#include <errno.h>
#include <stdlib.h>

#ifdef _DEBUG
#include "./debug.h"
// Not recommended for debugging in multi-instance & multi-threaded contexts.
int g_num_xmallocs = 0;
#endif

void xmalloc_init()
{
#ifdef _DEBUG
    g_num_xmallocs = 0;
#endif
}

void xmalloc_shutdown()
{
#ifdef _DEBUG
    xassert(g_num_xmallocs == 0);
#endif
}

void* xmalloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
        exit(ENOMEM);
#ifdef _DEBUG
    g_num_xmallocs++;
#endif
    return ptr;
}

void* xrealloc(void* ptr, size_t new_size)
{
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL)
        exit(ENOMEM);
#ifdef _DEBUG
    if (ptr == NULL)
        g_num_xmallocs++;
#endif
    return new_ptr;
}

void xfree(void* ptr)
{
    free(ptr);
#ifdef _DEBUG
    g_num_xmallocs--;
    xassert(g_num_xmallocs >= 0);
#endif
}

#endif