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
uint64_t xtime_unix_ms(); // Epoch/unix timestamp. Ms since Jan. 1st, 1970

// NOTE: GMT+00 only, no DST
typedef struct XDate
{
    int year;        // Gregorian year
    int month;       // 1-12
    int day;         // 1-31
    int hour;        // 0-23
    int minute;      // 0-59
    int second;      // 0-59. Leap seconds (60) not supported
    int millisecond; // 0-999
} XDate;

XDate xtime_get_date(uint64_t unix_ms);

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

XDate xtime_get_date(uint64_t unix_ms)
{
    XDate date;

    uint64_t unix_sec            = unix_ms / 1000;   // ms to sec
    uint64_t unix_days           = unix_sec / 86400; // seconds to days, 86400 == num seconds in a day
    uint32_t sec_since_midneight = unix_sec % 86400;

    date.hour        = sec_since_midneight / 3600;
    date.minute      = (sec_since_midneight / 60) % 60;
    date.second      = sec_since_midneight % 60;
    date.millisecond = unix_ms % 1000;

    // civil_from_days
    // https://howardhinnant.github.io/date_algorithms.html#civil_from_days
    {
        int      z   = (int)unix_days + 719468;
        int      era = (z >= 0 ? z : z - 146096) / 146097;
        unsigned doe = (unsigned)(z - era * 146097);

        unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        int      y   = (int32_t)yoe + (int32_t)(era * 400);

        unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        unsigned mp  = (5 * doy + 2) / 153;
        unsigned d   = doy - (153 * mp + 2) / 5 + 1;
        unsigned m   = mp < 10 ? mp + 3 : mp - 9;
        if (mp >= 10)
            y += 1;

        date.year  = y;
        date.month = m;
        date.day   = d;
    }

    return date;
}

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
    xhl_unixtime_init -= 116444736000000000LL; // convert date from Jan 1, 1601 to Jan 1 1970.
    xhl_unixtime_init /= 10000LL;              // convert units 100 nanosecods > ms
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

#ifdef _MSC_VER
static __declspec(thread) uint64_t g_xhl_stopwatch = 0;
#else
static _Thread_local uint64_t g_xhl_stopwatch = 0;
#endif

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