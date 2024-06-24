// Checking it compiles...
#define XHL_ALLOC_IMPL
#define XHL_FILES_IMPL

#include "./include/xhl/debug.h"

#include "./include/xhl/alloc.h"

#define XARR_REALLOC(ptr, size) xrealloc(ptr, size)
#define XARR_FREE(ptr)          xfree(ptr)
#define XFILES_MALLOC(size)     xmalloc(size)
#define XFILES_FREE(ptr)        xfree(ptr)

#include "./include/xhl/array.h"
#include "./include/xhl/files.h"

#include <stdio.h>

int main()
{
    xalloc_init();

    // TEST XARRAY
    {
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
    }

    // TEST XFILES
    {
        bool ok = false;
        char path[1024];

        ok = xfiles_get_user_directory(path, sizeof(path), XFILES_USER_DIRECTORY_DESKTOP);
        xassert(ok);
        if (ok)
        {
            static const char* temp_file = XFILES_DIR_STR "temp_file.txt";
            static const char* contents  = "Hello World!";

            strncat(path, temp_file, sizeof(path) - strlen(path) - 1);
            printf("Writing \"%s\" to: %s\n", contents, path);

            ok = xfiles_write(path, contents, 1 + strlen(contents));
            xassert(ok);
            xassert(xfiles_exists(path));
            if (ok)
            {
                char*  content     = NULL;
                size_t content_len = 0;

                const char* name = xfiles_get_name(path);
                xassert(name);
                const char* ext = xfiles_get_extension(name);
                xassert(ext);

                printf(
                    "Reading file \"%.*s\" with extension \"%.*s\"\n",
                    (int)(ext - name),
                    name,
                    (int)strlen(ext),
                    ext);

                ok = xfiles_read(path, (void**)&content, &content_len);
                xassert(ok);
                if (ok)
                {
                    printf("Contents: %.*s\n", (int)(content_len), content);
                    xfree(content);

                    printf("Moving to bin: %s\n", path);
                    ok = xfiles_delete_safely(path);
                    xassert(ok);
                    xassert(! xfiles_exists(path));
                }
            }
        }

        ok = xfiles_get_user_directory(path, sizeof(path), XFILES_USER_DIRECTORY_DOCUMENTS);
        xassert(ok);
        ok = xfiles_open_file_explorer(path);
        xassert(ok);
        printf("Enjoy closing this window: %s\n", path);
    }

    xalloc_shutdown();
    return 0;
}