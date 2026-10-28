// Tests xhl/alloc.h. On Linux this is blocked by the '#error Unknown unix'
// around the mmap code
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define XHL_ALLOC_IMPL
#include "../include/xhl/alloc.h"

#include "xtest.h"

int main(void)
{
    xalloc_init();

    // xmalloc family
    {
        int* nums = xmalloc(16 * sizeof(int));
        XTEST_ASSERT(nums != NULL);
        for (int i = 0; i < 16; i++)
            nums[i] = i;

        nums = xrealloc(nums, 1024 * sizeof(int));
        XTEST_ASSERT(nums != NULL);
        XTEST_ASSERT(nums[15] == 15); // realloc keeps the old contents

        int* zeros = xcalloc(64, sizeof(int));
        XTEST_ASSERT(zeros != NULL);
        bool all_zero = true;
        for (int i = 0; i < 64; i++)
            all_zero = all_zero && zeros[i] == 0;
        XTEST_ASSERT(all_zero);

        xfree(nums);
        xfree(zeros);
        xfree(NULL); // Must do nothing
    }

    // Virtual memory. This is the platform specific part
    {
        size_t granularity = 0;
        size_t page_size   = 0;
        xvalloc_info(&granularity, &page_size);
        XTEST_ASSERT(page_size >= 4096);
        XTEST_ASSERT((page_size & (page_size - 1)) == 0); // Power of 2
        XTEST_ASSERT(granularity >= page_size);
        xvalloc_info(NULL, NULL); // Both args are optional

        // 4 pages. Should come back page aligned and zeroed
        size_t   size = page_size * 4;
        uint8_t* mem  = xvalloc(NULL, size);
        XTEST_ASSERT(mem != NULL);
        XTEST_ASSERT(((uintptr_t)mem % page_size) == 0);

        bool all_zero = true;
        for (size_t i = 0; i < size; i++)
            all_zero = all_zero && mem[i] == 0;
        XTEST_ASSERT(all_zero);

        memset(mem, 0x69, size); // Must be writable
        XTEST_ASSERT(mem[size - 1] == 0x69);

        xvfree(mem, size);
    }

    // Asserts in debug builds if anything above leaked
    xalloc_shutdown();

    return xtest_finish("test_alloc");
}
