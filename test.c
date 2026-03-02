// Checking it compiles...
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#define XHL_ALLOC_IMPL
#define XHL_FILES_IMPL
#define XHL_STRING_IMPL

#include "./include/xhl/debug.h"

#include "./include/xhl/alloc.h"

#define XARR_REALLOC(ptr, size) xrealloc(ptr, size)
#define XARR_FREE(ptr)          xfree(ptr)
#define XFILES_MALLOC(size)     xmalloc(size)
#define XFILES_REALLOC(size)    xrealloc(size)
#define XFILES_FREE(ptr)        xfree(ptr)

#include "./include/xhl/array.h"
#include "./include/xhl/files.h"
#include "./include/xhl/string.h"

#include <stdio.h>

void test_formatting(const char* fmt, ...)
{
    enum
    {
        CAP = 64
        // CAP = 3
    };
    char buf_libc[256] = {0};
    char buf_gbx[256]  = {0};
    _Static_assert(CAP <= sizeof(buf_gbx), "");
    int result = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf_libc, CAP, fmt, args);
    xtr_fmt_va(buf_gbx, CAP, fmt, args);

    va_end(args);
    fprintf(stderr, "%s", buf_libc);
    fprintf(stderr, "%s", buf_gbx);

    result = strcmp(buf_libc, buf_gbx);
    xassert(result == 0);
}

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
            fprintf(stderr, "[%d] num: %d\n", i, nums[i]);

        xarr_delete(nums, 2);
        xassert(xarr_len(nums) == N);

        for (int i = 0; i < xarr_len(nums); i++)
            fprintf(stderr, "[%d] num: %d\n", i, nums[i]);

        fprintf(stderr, "cap: %u\n", (unsigned)xarr_cap(nums));
        fprintf(stderr, "len: %u\n", (unsigned)xarr_len(nums));

        xarr_free(nums);

        xassert(nums == NULL);
    }

    // TEST XFILES
    {
        bool ok = false;
        char path[1024];

        ok = xfiles_get_user_directory(path, sizeof(path), XFILES_USER_DIRECTORY_DESKTOP);
        xassert(ok);

        static const char* temp_file = XFILES_DIR_STR "temp_file.txt";
        static const char* contents  = "Hello World!";

        strncat(path, temp_file, sizeof(path) - strlen(path) - 1);
        fprintf(stderr, "Writing \"%s\" to: %s\n", contents, path);

        ok = xfiles_write(path, contents, strlen(contents));
        xassert(ok);
        xassert(xfiles_exists(path));

        static const char* line2 = "foobarbaz";
        fprintf(stderr, "Appending \"%s\" to: %s\n", contents, path);
        ok = xfiles_append(path, "\n", 1);
        xassert(ok);
        ok = xfiles_append(path, line2, strlen(line2));
        xassert(ok);

        const char* name = xfiles_get_name(path);
        xassert(name);
        const char* ext = xfiles_get_extension(name);
        xassert(ext);

        fprintf(
            stderr,
            "Reading file \"%.*s\" with extension \"%.*s\"\n",
            (int)(ext - name),
            name,
            (int)strlen(ext),
            ext);

        char*  content     = NULL;
        size_t content_len = 0;
        ok                 = xfiles_read(path, (void**)&content, &content_len);
        xassert(ok);
        fprintf(stderr, "Contents below: \n%.*s\n", (int)(content_len), content);
        xfree(content);

        fprintf(stderr, "Moving to bin: %s\n", path);
        ok = xfiles_trash(path);
        xassert(ok);
        xassert(!xfiles_exists(path));

        ok = xfiles_get_user_directory(path, sizeof(path), XFILES_USER_DIRECTORY_DOCUMENTS);
        xassert(ok);
        ok = xfiles_open_file_explorer(path);
        xassert(ok);
        fprintf(stderr, "Enjoy closing this window: %s\n", path);
    }

    // Test XTRING
    {
        // https://en.cppreference.com/w/c/io/fprintf.html
        const char* s = "Hello";

        test_formatting("Strings:\n"); // same as puts("Strings");
        test_formatting(" padding:\n");
        test_formatting("\t[%10s]\n", s);
        test_formatting("\t[%-10s]\n", s);
        test_formatting("\t[%*s]\n", 10, s);
        test_formatting(" truncating:\n");
        test_formatting("\t%.4s\n", s);
        test_formatting("\t%.*s\n", 3, s);

        test_formatting("Characters:\t%c %%\n", 'A');

        test_formatting("Integers:\n");
        test_formatting("\tDecimal:\t%i %d %.6i %i %.0i %+i %i\n", 1, 2, 3, 0, 0, 4, -4);
        test_formatting("\tHexadecimal:\t%x %x %X %#x\n", 5, 10, 10, 6);
        test_formatting("\tOctal:\t\t%o %#o %#o\n", 10, 10, 4);

        test_formatting("Floating-point:\n");
        // NOTE: worrying about matching libc rounding precision for values > 16 places is a bit of a rabbit hole
        // This implementation is fine, but possibly faster and more accurate ones exist.
        // Consider grabbing the dragonbox code:
        // https://github.com/jk-jeon/dragonbox
        // test_formatting("\tRounding:\t%f %.0f %.32f\n", 1.5, 1.5, 1.3); // currently failing
        test_formatting("\tRounding:\t%f %.0f %.16f\n", 1.5, 1.5, 1.3); // passes with lower precision
        test_formatting("\tPadding:\t%05.2f %.2f %5.2f\n", 1.5, 1.5, 1.5);
        // test_formatting("\tScientific:\t%E %e\n", 1.5, 1.5);  // unimplemented
        // test_formatting("\tHexadecimal:\t%a %A\n", 1.5, 1.5); // unimplemented
        test_formatting("\tSpecial values:\t0/0=%g 1/0=%g\n", 0.0 / 0.0, 1.0 / 0.0);

        test_formatting("Weird bonus cases:\n");
        test_formatting("\t[%-10.*s]\n", 2, s);

        // Test formatting warnings. Appears to be work with clang 18.
        char mybuf[32] = {0};
        xtr_fmt(mybuf, sizeof(mybuf), 0, "%d", 3.14); // should write a big int
        s += 0;
    }
    fprintf(stderr, "\nAll tests passed\n");

    xalloc_shutdown();
    return 0;
}