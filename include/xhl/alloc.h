#ifndef XHL_ALLOC_H
#define XHL_ALLOC_H
// Quick and dirty xmalloc

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// For leak detector
void xalloc_init();
void xalloc_shutdown();

void* xmalloc(size_t size);
void* xrealloc(void*, size_t size);
void* xcalloc(size_t num, size_t size);
void  xfree(void*);

void* xvalloc(void* hint, uint64_t size);
void  xvfree(void* ptr, uint64_t size);

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
int g_num_xvallocs = 0;
#else
#define xalloc_assert(...)
#endif

void xalloc_init()
{
#ifndef NDEBUG
    g_num_xmallocs = 0;
    g_num_xvallocs = 0;
#endif
}

void xalloc_shutdown()
{
#ifndef NDEBUG
    xalloc_assert(g_num_xmallocs == 0);
    xalloc_assert(g_num_xvallocs == 0);
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

void* xcalloc(size_t num, size_t size)
{
    void* ptr = calloc(num, size);
    if (ptr == NULL)
        exit(ENOMEM);
#ifndef NDEBUG
    g_num_xmallocs++;
#endif
    return ptr;
}

void xfree(void* ptr)
{
    free(ptr);
#ifndef NDEBUG
    g_num_xmallocs--;
    xalloc_assert(g_num_xmallocs >= 0);
#endif
}

#ifdef _WIN32
#define XHL_MEM_COMMIT     0x00001000
#define XHL_MEM_RESERVE    0x00002000
#define XHL_PAGE_READWRITE 0x04

void* __stdcall VirtualAlloc(void* lpAddress, size_t dwSize, unsigned long flAllocationType, unsigned long flProtect);
int __stdcall VirtualFree(void* address, size_t size, unsigned long free_type);

void* xvalloc(void* hint, uint64_t size)
{
    void* ptr = VirtualAlloc(hint, size, XHL_MEM_COMMIT | XHL_MEM_RESERVE, XHL_PAGE_READWRITE);
    xalloc_assert(ptr != NULL);
#ifndef NDEBUG
    g_num_xvallocs++;
#endif
    return ptr;
}

void xvfree(void* ptr, uint64_t size)
{
    xalloc_assert(ptr != NULL);
    VirtualFree(ptr, size, 0);
#ifndef NDEBUG
    g_num_xvallocs--;
    xalloc_assert(g_num_xvallocs >= 0);
#endif
}

#else // !_WIN32

#ifdef __APPLE__
#include <sys/mman.h>
#else
#error Unknown unix
#endif

void* xvalloc(void* hint, uint64_t size)
{
    void* ptr = mmap(hint, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
#ifndef NDEBUG
    xalloc_assert((uint64_t)ptr != 0xffffffffffffffff);
    g_num_xvallocs++;
#endif
    return ptr;
}

void xvfree(void* ptr, uint64_t size)
{
    xalloc_assert(ptr != NULL);
    munmap(ptr, size);
#ifndef NDEBUG
    g_num_xvallocs--;
    xalloc_assert(g_num_xvallocs >= 0);
#endif
}

#endif

#endif // XHL_ALLOC_IMPL