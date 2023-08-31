// Checking it compiles...
#define XHL_MALLOC_IMPL
#include "./include/xhl/malloc.h"
#include "./include/xhl/debug.h"

#define XARR_REALLOC(ptr, size) xrealloc(ptr, size)
#define XARR_FREE(ptr) xfree(ptr)

#include "./include/xhl/array.h"

int main()
{
    const size_t N = 4;
    int* nums = NULL;

    xarr_setcap(nums, N);

    for (int i = 0; i < N; i++)
        xarr_push(nums, i);

    xassert(xarr_len(nums) == N);

    xarr_free(nums);

    xassert(nums == NULL);
    return 0;
}