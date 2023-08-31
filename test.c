// Checking it compiles...
#define XHL_MALLOC_IMPL
#include "./include/xhl/debug.h"
#include "./include/xhl/malloc.h"

#define XARR_REALLOC(ptr, size) xrealloc(ptr, size)
#define XARR_FREE(ptr) xfree(ptr)

#include "./include/xhl/array.h"

#include <stdio.h>

int main()
{
    xmalloc_init();
    const size_t N    = 4;
    int*         nums = NULL;

    xarr_setlen(nums, 0);

    for (int i = 0; i < N; i++)
        xarr_push(nums, i);

    xarr_insert(nums, 2, 69);
    xassert(xarr_len(nums) == N + 1);

    for (int i = 0; i < xarr_len(nums); i++)
        printf("[%d] num: %d\n", i, nums[i]);

    xarr_delete(nums, 2);
    xassert(xarr_len(nums) == N);

    for (int i = 0; i < xarr_len(nums); i++)
        printf("[%d] num: %d\n", i, nums[i]);

    printf("cap: %u\n", (unsigned)xarr_cap(nums));
    printf("len: %u\n", (unsigned)xarr_len(nums));

    xarr_free(nums);

    xassert(nums == NULL);
    xmalloc_shutdown();
    return 0;
}