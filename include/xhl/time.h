#ifndef XHL_TIME_H
#define XHL_TIME_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void     xtime_init();
uint64_t xtime_now_ns();
double   xtime_convert_ns_to_ms(uint64_t ns);
double   xtime_convert_ns_to_sec(uint64_t ns);
uint64_t xtime_unix_ms();

#ifndef NDEBUG
// Quick an dirty performance timer that won't show up in release. Should be thread safe.
void xtime_stopwatch_start();
void xtime_stopwatch_log_ms(const char* msg_prefix); // Resets stopwatch
#else
#define xtime_stopwatch_start()
#define xtime_stopwatch_log_ms(...)
#endif

#ifdef __cplusplus
}
#endif
#endif // XHL_TIME_H

#ifdef XHL_TIME_IMPL
#undef XHL_TIME_IMPL

#ifdef _WIN32
#include <Windows.h>

LARGE_INTEGER xhl_perffreq;
LARGE_INTEGER xhl_timestart;
uint64_t      xhl_unixtime_init;

void xtime_init()
{
    QueryPerformanceFrequency(&xhl_perffreq);
    QueryPerformanceCounter(&xhl_timestart);

    // https://stackoverflow.com/questions/1695288/getting-the-current-time-in-milliseconds-from-the-system-clock-in-windows#1695332
    FILETIME filetime;
    GetSystemTimeAsFileTime(&filetime);
    xhl_unixtime_init  = (uint64_t)filetime.dwLowDateTime + ((uint64_t)(filetime.dwHighDateTime) << 32LL);
    xhl_unixtime_init /= 10000;             // convert units 100 nanosecods > ms
    xhl_unixtime_init -= 11644473600000ULL; // convert date from Jan 1, 1601 to Jan 1 1970.
}

uint64_t xtime_now_ns()
{
    // Algo func taken from here:
    // https://github.com/rust-lang/rust/blob/3809bbf47c8557bd149b3e52ceb47434ca8378d5/src/libstd/sys_common/mod.rs#L124
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    now.QuadPart -= xhl_timestart.QuadPart;
    INT64 q       = now.QuadPart / xhl_perffreq.QuadPart;
    INT64 r       = now.QuadPart % xhl_perffreq.QuadPart;
    return q * 1000000000 + r * 1000000000 / xhl_perffreq.QuadPart;
}

#elif defined(__APPLE__) // endif _WIN32
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>

mach_timebase_info_data_t xhl_timebase;
uint64_t                  xhl_start_machtime;
uint64_t                  xhl_unixtime_init;

void xtime_init()
{
    xhl_start_machtime = mach_absolute_time();
    mach_timebase_info(&xhl_timebase);

    CFAbsoluteTime now  = CFAbsoluteTimeGetCurrent() + kCFAbsoluteTimeIntervalSince1970;
    now                *= 1000; // convert sec > ms
    xhl_unixtime_init   = now;
}

uint64_t xtime_now_ns()
{
    // Algo func taken from here:
    // https://github.com/rust-lang/rust/blob/3809bbf47c8557bd149b3e52ceb47434ca8378d5/src/libstd/sys_common/mod.rs#L124
    uint64_t diff = mach_absolute_time() - xhl_start_machtime;
    return (diff / xhl_timebase.denom) * xhl_timebase.numer +
           (diff % xhl_timebase.denom) * xhl_timebase.numer / xhl_timebase.denom;
}

#endif // __APPLE__

double   xtime_convert_ns_to_ms(uint64_t ns) { return (double)ns / 1.e6; }
double   xtime_convert_ns_to_sec(uint64_t ns) { return (double)ns / 1.e9; }
uint64_t xtime_unix_ms() { return xhl_unixtime_init + xtime_now_ns() / 1000000; }

#ifndef NDEBUG

#ifndef XTIME_LOG
#include <stdio.h>
#define XTIME_LOG(...) fprintf(stderr, __VA_ARGS__)
#endif // XTIME_LOG

static _Thread_local uint64_t g_xhl_stopwatch = 0;

void xtime_stopwatch_start() { g_xhl_stopwatch = xtime_now_ns(); }
void xtime_stopwatch_log_ms(const char* msg_prefix)
{
    uint64_t now    = xtime_now_ns();
    double   ms     = xtime_convert_ns_to_ms(now - g_xhl_stopwatch);
    g_xhl_stopwatch = now;
    XTIME_LOG("%s: %.2fms\n", msg_prefix, ms);
}
#endif // NDEBUG

#endif // XHL_TIME_IMPL