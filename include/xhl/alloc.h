#ifndef XHL_ALLOC_H
#define XHL_ALLOC_H
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
#endif // XHL_ALLOC_H

#ifdef XHL_ALLOC_IMPL
#undef XHL_ALLOC_IMPL
#include <errno.h>
#include <stdlib.h>

#ifndef NDEBUG
#ifdef _WIN32
#define xalloc_assert(cond) (cond) ? (void)0 : __debugbreak()
#else
#define xalloc_assert(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif
// Not recommended for debugging in multi-instance & multi-threaded contexts.
int g_num_xmallocs = 0;
#endif

void xmalloc_init()
{
#ifndef NDEBUG
    g_num_xmallocs = 0;
#endif
}

void xmalloc_shutdown()
{
#ifndef NDEBUG
    xassert(g_num_xmallocs == 0);
#endif
}

void* xmalloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
        exit(ENOMEM);
#ifndef NDEBUG
    g_num_xmallocs++;
#endif
    return ptr;
}

void* xrealloc(void* ptr, size_t new_size)
{
    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr == NULL)
        exit(ENOMEM);
#ifndef NDEBUG
    if (ptr == NULL && new_size > 0)
        g_num_xmallocs++;
#endif
    return new_ptr;
}

void xfree(void* ptr)
{
    free(ptr);
#ifndef NDEBUG
    g_num_xmallocs--;
    xalloc_assert(g_num_xmallocs >= 0);
#endif
}

#endif // XHL_ALLOC_IMPL