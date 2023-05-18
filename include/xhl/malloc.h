#pragma once

/**
    Quick and dirty malloc debugger
*/

#include <stddef.h>

void xmalloc_init();
void xmalloc_shutdown();

void* xmalloc(size_t size);
void xfree(void*);

// TODO
// void xrealloc(void*, size_t size);

// #ifdef XHL_MALLOC_IMPL
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
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
    assert(g_num_xmallocs == 0);
#endif
}

void* xmalloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
    {
        printf("Out of memory!");
        exit(EXIT_FAILURE);
    }

    memset(ptr, 0, size);
#ifdef DEBUG
    g_num_xmallocs++;
#endif
}

void xfree(void* ptr)
{
    free(ptr);

#ifdef DEBUG
    g_num_xmallocs--;
    assert(g_num_xmallocs >= 0);
#endif
}

// #endif