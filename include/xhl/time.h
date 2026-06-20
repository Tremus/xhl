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
void  xtime_sleep_ms(uint32_t ms);

typedef void (*xtime_timer_cb)(void* user_data);

typedef struct
{
    xtime_timer_cb callback;
    void*          user_data;
    void*          _platform; // CFRunLoopTimerRef on macOS; HWND on Windows
} XTimer;

// Warning: only call these from the main thread
void xtime_timer_start(XTimer* t, uint32_t interval_ms, xtime_timer_cb cb, void* user_data);
void xtime_timer_stop(XTimer* t);

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
#include <string.h>

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
    xhl_unixtime_init  = (uint64_t)filetime.dwLowDateTime + ((uint64_t)(filetime.dwHighDateTime) << 32ULL);
    xhl_unixtime_init -= 116444736000000000ULL; // convert date from Jan 1, 1601 to Jan 1 1970.
    xhl_unixtime_init /= 10000LL;               // convert units 100 nanosecods > ms
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

void xtime_sleep_ms(uint32_t ms) { Sleep(ms); }

// Unfortunately we can't pass user data to SetTiner on Windows. This would be much cleaner an simpler
// Slots should work fine, because in real world scenarios (when building audio plugins) this will only be called on the
// main thread
#ifndef XTIME_NUM_TIMERS
#define XTIME_NUM_TIMERS 32
#endif // XTIME_NUM_TIMERS

struct XTimerSlot
{
    UINT_PTR id;
    XTimer*  timer;
};
struct XTimerSlot G_XTIMERS[XTIME_NUM_TIMERS] = {0};

static void CALLBACK _xtime_timer_proc(HWND h, UINT m, UINT_PTR id, DWORD ms)
{
    for (int i = 0; i < XTIME_NUM_TIMERS; i++)
    {
        struct XTimerSlot* slot = &G_XTIMERS[i];
        if (slot->id == id)
        {
            XTimer* t = slot->timer;
            t->callback(t->user_data);
            break;
        }
    }
}

void xtime_timer_start(XTimer* t, uint32_t interval_ms, xtime_timer_cb cb, void* user_data)
{
    for (int i = 0; i < XTIME_NUM_TIMERS; i++)
    {
        struct XTimerSlot* slot = &G_XTIMERS[i];
        if (slot->timer == NULL)
        {
            t->callback  = cb;
            t->user_data = user_data;
            // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-settimer
            t->_platform = (void*)SetTimer(NULL, 0, interval_ms, (TIMERPROC)_xtime_timer_proc);

            slot->id    = (UINT_PTR)t->_platform;
            slot->timer = t;
            break;
        }
    }
}

void xtime_timer_stop(XTimer* t)
{
    for (int i = 0; i < XTIME_NUM_TIMERS; i++)
    {
        struct XTimerSlot* slot = &G_XTIMERS[i];
        if (slot->timer == t)
        {
            // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-killtimer
            KillTimer(NULL, (UINT_PTR)t->_platform);
            memset(t, 0, sizeof(*t));
            memset(slot, 0, sizeof(*slot));
            break;
        }
    }
}

#elif defined(__APPLE__) // endif _WIN32
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <unistd.h>

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

void xtime_sleep_ms(uint32_t ms) { usleep((useconds_t)ms * 1000); }

static void _xtime_timer_proc(CFRunLoopTimerRef ref, void* info)
{
    XTimer* t = (XTimer*)info;
    t->callback(t->user_data);
}

void xtime_timer_start(XTimer* t, uint32_t interval_ms, xtime_timer_cb cb, void* user_data)
{
    t->callback                        = cb;
    t->user_data                       = user_data;
    double                interval_sec = (double)interval_ms / 1000.0;
    CFRunLoopTimerContext ctx          = {0, t, NULL, NULL, NULL};

    t->_platform = CFRunLoopTimerCreate(
        NULL,
        CFAbsoluteTimeGetCurrent() + interval_sec,
        interval_sec,
        0,
        0,
        _xtime_timer_proc,
        &ctx);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), (CFRunLoopTimerRef)t->_platform, kCFRunLoopCommonModes);
}

void xtime_timer_stop(XTimer* t)
{
    CFRunLoopTimerInvalidate((CFRunLoopTimerRef)t->_platform);
    CFRelease((CFRunLoopTimerRef)t->_platform);
    memset(t, 0, sizeof(*t));
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