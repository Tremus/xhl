#pragma once

/**
    Quick and dirty xmalloc
*/

#include <stddef.h>

void xmalloc_init();
void xmalloc_shutdown();

void* xmalloc(size_t size);
void* xrealloc(void*, size_t size);
void  xfree(void*);

#ifdef XHL_MALLOC_IMPL
#include <errno.h>
#include <stdlib.h>

#ifdef DEBUG
#include <xhl/debug.h>
// Not recommended for debugging in multi-instance & multi-threaded contexts.
int g_num_xmallocs = 0;
#endif

void xmalloc_init()
{
#ifdef DEBUG
    g_num_xmallocs = 0;
#endif
}

void xmalloc_shutdown()
{
#ifdef DEBUG
    xassert(g_num_xmallocs == 0);
#endif
}

void* xmalloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
        exit(ENOMEM);
#ifdef DEBUG
    g_num_xmallocs++;
#endif
    return ptr;
}

void* xrealloc(void* ptr, size_t new_size)
{
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL)
        exit(ENOMEM);
#ifdef DEBUG
    if (ptr == NULL)
        g_num_xmallocs++;
#endif
    return new_ptr;
}

void xfree(void* ptr)
{
    free(ptr);
#ifdef DEBUG
    g_num_xmallocs--;
    xassert(g_num_xmallocs >= 0);
#endif
}

#endif