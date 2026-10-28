#ifndef XHL_XTEST_H
#define XHL_XTEST_H

#include <stdio.h>

static int g_xtest_failures = 0;

#define XTEST_ASSERT(cond)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);                                          \
            g_xtest_failures++;                                                                                        \
        }                                                                                                              \
    }                                                                                                                  \
    while (0)

// Return this from main(). Non zero exit code tells CTest the test failed
static int xtest_finish(const char* name)
{
    if (g_xtest_failures == 0)
        fprintf(stderr, "%s: all tests passed\n", name);
    else
        fprintf(stderr, "%s: %d check(s) failed\n", name, g_xtest_failures);
    return g_xtest_failures == 0 ? 0 : 1;
}

#endif // XHL_XTEST_H
