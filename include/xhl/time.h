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

#ifdef __cplusplus
}
#endif
#endif // XHL_TIME_H

#ifdef XHL_TIME_IMPL
#undef XHL_TIME_IMPL

#ifdef _WIN32
#include <winnt.h>

LARGE_INTEGER xhl_perffreq;
LARGE_INTEGER xhl_timestart;

void xtime_init()
{
    QueryPerformanceFrequency(&xhl_perffreq);
    QueryPerformanceCounter(&xhl_timestart);
}

uint64_t xtime_now_ns()
{
    // Algo func taken from here:
    // https://github.com/rust-lang/rust/blob/3809bbf47c8557bd149b3e52ceb47434ca8378d5/src/libstd/sys_common/mod.rs#L124
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    now.QuadPart -= xhl_timestart.QuadPart;
    INT64 q      = now.QuadPart / xhl_perffreq.QuadPart;
    INT64 r      = now.QuadPart % xhl_perffreq.QuadPart;
    return q * 1000000000 + r * 1000000000 / xhl_perffreq.QuadPart;
}

#elif defined(__APPLE__) // endif _WIN32
#include <mach/mach_time.h>

mach_timebase_info_data_t xhl_timebase;
uint64_t                  xhl_start_machtime;

void xtime_init()
{
    xhl_start_machtime = mach_absolute_time();
    mach_timebase_info(&xhl_timebase);
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


double xtime_convert_ns_to_ms(uint64_t ns) { return (double)ns / 1.e6; }
double xtime_convert_ns_to_sec(uint64_t ns) { return (double)ns / 1.e9; }

#endif // XHL_TIME_IMPL