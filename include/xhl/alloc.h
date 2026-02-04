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

void* xvalloc(void* hint, size_t size);
void  xvfree(void* ptr, size_t size);

// On windows this returns SYSTEM_INFO.dwAllocationGranularity (usually 64kB) and SYSTEM_INFO.dwPageSize (usually 4kb).
// If you want hinting to work with VirtualAlloc(), your hint must be aligned to this.
// https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info
// On macOS this returns getpagesize(), although mmap() on macOS will let you hint using 4kb alignment, and rounds up
// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/mmap.2.html
// Both arguments are optional, you may pass NULL
void xvalloc_info(size_t* valloc_granularity, size_t* page_size);

#ifdef __cplusplus
}
#endif
#endif // XHL_ALLOC_H

#ifdef XHL_ALLOC_IMPL
#undef XHL_ALLOC_IMPL
#include <errno.h>
#include <stdlib.h>

#ifdef NDEBUG
#define xalloc_assert(...)
#else // !NDEBUG

#ifdef _WIN32
#define xalloc_assert(cond) (cond) ? (void)0 : __debugbreak()
#else
#define xalloc_assert(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif

// Not recommended for debugging in multi-instance & multi-threaded contexts.
int g_num_xmallocs = 0;
int g_num_xvallocs = 0;

#ifndef XALLOC_ALLOC_ARRAY_SIZE
#define XALLOC_ALLOC_ARRAY_SIZE 1024
#endif
void* g_current_allocs[XALLOC_ALLOC_ARRAY_SIZE] = {0};

int xalloc_add_alloc(void* ptr)
{
    xalloc_assert(ptr);
    for (int i = 0; i < XALLOC_ALLOC_ARRAY_SIZE; i++)
    {
        if (g_current_allocs[i] == NULL)
        {
            g_current_allocs[i] = ptr;
            return i;
        }
    }
    xalloc_assert(0); // Oh no, you've run out of room. Increase the array size or rethink your program (>1000 allocs!?)
    return -1;
}
int xalloc_remove_alloc(void* ptr)
{
    xalloc_assert(ptr);
    for (int i = 0; i < XALLOC_ALLOC_ARRAY_SIZE; i++)
    {
        if (g_current_allocs[i] == ptr)
        {
            g_current_allocs[i] = 0;
            return i;
        }
    }
    xalloc_assert(0); // Does not exist. Double free?
    return -1;
}

#endif // NDEBUG

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
#ifndef NDEBUG
    xalloc_add_alloc(ptr);
    g_num_xmallocs++;
#endif
    if (ptr == NULL)
        exit(ENOMEM);
    return ptr;
}

void* xrealloc(void* old_ptr, size_t new_size)
{
    void* new_ptr = realloc(old_ptr, new_size);
#ifndef NDEBUG
    if (old_ptr != new_ptr)
    {
        if (old_ptr)
            xalloc_remove_alloc(old_ptr);
        xalloc_add_alloc(new_ptr);
    }
    if (old_ptr == NULL && new_size > 0)
        g_num_xmallocs++;
#endif
    if (new_ptr == NULL)
        exit(ENOMEM);
    return new_ptr;
}

void* xcalloc(size_t num, size_t size)
{
    void* ptr = calloc(num, size);
#ifndef NDEBUG
    xalloc_add_alloc(ptr);
    g_num_xmallocs++;
#endif
    if (ptr == NULL)
        exit(ENOMEM);
    return ptr;
}

void xfree(void* ptr)
{
    if (ptr != NULL) // The C standard says to do nothing with NULL pointers
    {
        free(ptr);
#ifndef NDEBUG
        xalloc_remove_alloc(ptr);
        g_num_xmallocs--;
        xalloc_assert(g_num_xmallocs >= 0);
#endif
    }
}

#ifdef _WIN32
#include <Windows.h>

void* xvalloc(void* hint, size_t size)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc
    void* ptr = VirtualAlloc(hint, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    // xalloc_assert(ptr != NULL);
    if (ptr == NULL) // Hint attempt may have failed, and we would rather just get some damn memory
        ptr = VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    xalloc_assert(ptr != NULL);
#ifndef NDEBUG
    g_num_xvallocs++;
#endif
    return ptr;
}

void xvfree(void* ptr, size_t size)
{
    xalloc_assert(ptr != NULL);
    BOOL ok = VirtualFree(ptr, 0, MEM_RELEASE);
#ifndef NDEBUG
    g_num_xvallocs--;
    xalloc_assert(g_num_xvallocs >= 0);
    DWORD err = GetLastError();
#endif
    xalloc_assert(ok);
}

void xvalloc_info(size_t* _valloc_granularity, size_t* _page_size)
{
    static DWORD AllocationGranularity = 0;
    static DWORD PageSize              = 0;
    if (!AllocationGranularity)
    {
        SYSTEM_INFO SystemInfo;
        GetSystemInfo(&SystemInfo);
        AllocationGranularity = SystemInfo.dwAllocationGranularity;
        PageSize              = SystemInfo.dwPageSize;
    }
    if (_valloc_granularity)
        *_valloc_granularity = AllocationGranularity;
    if (_page_size)
        *_valloc_granularity = PageSize;
}

#else // !_WIN32

#ifdef __APPLE__
#include <sys/mman.h>
#include <unistd.h>
#else
#error Unknown unix
#endif

void* xvalloc(void* hint, size_t size)
{
    void* ptr = mmap(hint, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
#ifndef NDEBUG
    xalloc_assert((size_t)ptr != 0xffffffffffffffff);
    g_num_xvallocs++;
#endif
    return ptr;
}

void xvfree(void* ptr, size_t size)
{
    xalloc_assert(ptr != NULL);
    munmap(ptr, size);
#ifndef NDEBUG
    g_num_xvallocs--;
    xalloc_assert(g_num_xvallocs >= 0);
#endif
}

void xvalloc_info(size_t* _valloc_granularity, size_t* _page_size)
{
    static int page_size = 0;
    if (!page_size)
    {
        // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/getpagesize.3.html
        page_size = getpagesize();
    }
    if (_valloc_granularity)
        *_valloc_granularity = page_size;
    if (_page_size)
        *_page_size = page_size;
}

#endif

#endif // XHL_ALLOC_IMPL