#ifndef XHL_XTEST_H
#define XHL_XTEST_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#endif

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
static inline int xtest_finish(const char* name)
{
    if (g_xtest_failures == 0)
        fprintf(stderr, "%s: all tests passed\n", name);
    else
        fprintf(stderr, "%s: %d check(s) failed\n", name, g_xtest_failures);
    return g_xtest_failures == 0 ? 0 : 1;
}

// Writes "<OS temp dir>/<prefix>_<pid>_<time>" to 'out', a path no other run will use. Nothing is created
// Windows: GetTempPathW(), usually C:\Users\{username}\AppData\Local\Temp
// macOS: $TMPDIR, a per user folder like /var/folders/xx/.../T
// Linux: $TMPDIR, or /tmp
static inline bool xtest_temp_path(char* out, size_t outlen, const char* prefix)
{
    char tmp[512];
#ifdef _WIN32
    const char sep = '\\';
    WCHAR      wtmp[MAX_PATH + 1];
    DWORD      wlen = GetTempPathW(MAX_PATH + 1, wtmp);
    if (wlen == 0 || !WideCharToMultiByte(CP_UTF8, 0, wtmp, -1, tmp, sizeof(tmp), NULL, NULL))
        return false;
    unsigned long      pid  = GetCurrentProcessId();
    unsigned long long tick = GetTickCount64();
#else
    const char  sep = '/';
    const char* env = getenv("TMPDIR");
    snprintf(tmp, sizeof(tmp), "%s", env && env[0] ? env : "/tmp");
    unsigned long      pid  = (unsigned long)getpid();
    unsigned long long tick = (unsigned long long)time(NULL);
#endif
    size_t len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == sep)
        tmp[--len] = '\0';

    int n = snprintf(out, outlen, "%s%c%s_%lu_%llu", tmp, sep, prefix, pid, tick);
    return n > 0 && (size_t)n < outlen;
}

#endif // XHL_XTEST_H
