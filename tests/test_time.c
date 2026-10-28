// Tests xhl/time.h. On Linux this is blocked by time.h having no Linux branch
//
// Not tested: xtime_timer_start/stop. They fire from the platform's main thread event loop (CFRunLoop / Win32 message
// loop), which this program doesn't run
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stdint.h>

#define XHL_TIME_IMPL
#include "../include/xhl/time.h"

#include "xtest.h"

int main(void)
{
    xtime_init();

    // Monotonic clock
    {
        uint64_t a = xtime_now_ns();
        uint64_t b = xtime_now_ns();
        XTEST_ASSERT(b >= a);

        // Generous upper bound so a busy CI machine doesn't fail this
        uint64_t before = xtime_now_ns();
        xtime_sleep_ms(50);
        double slept_ms = xtime_convert_ns_to_ms(xtime_now_ns() - before);
        fprintf(stderr, "xtime_sleep_ms(50) took %.2fms\n", slept_ms);
        XTEST_ASSERT(slept_ms >= 45.0);
        XTEST_ASSERT(slept_ms < 1000.0);
    }

    XTEST_ASSERT(xtime_convert_ns_to_ms(1500000) == 1.5);
    XTEST_ASSERT(xtime_convert_ns_to_sec(2500000000ull) == 2.5);

    // Unix time. Should be between 2024-01-01 and 2100-01-01
    {
        uint64_t now = xtime_unix_ms();
        fprintf(stderr, "xtime_unix_ms() = %llu\n", (unsigned long long)now);
        XTEST_ASSERT(now > 1704067200000ull);
        XTEST_ASSERT(now < 4102444800000ull);

        XDate d = xtime_get_date(now);
        fprintf(
            stderr,
            "Today (GMT): %04d-%02d-%02d %02d:%02d:%02d.%03d\n",
            d.year,
            d.month,
            d.day,
            d.hour,
            d.minute,
            d.second,
            d.millisecond);
        XTEST_ASSERT(d.year >= 2024 && d.year < 2100);
    }

    // Date conversion. Platform independent, but cheap to check here
    {
        XDate epoch = xtime_get_date(0);
        XTEST_ASSERT(epoch.year == 1970 && epoch.month == 1 && epoch.day == 1);
        XTEST_ASSERT(epoch.hour == 0 && epoch.minute == 0 && epoch.second == 0 && epoch.millisecond == 0);

        // 2024-02-29 12:34:56.789 GMT, a leap day
        XDate leap = xtime_get_date(1709210096789ull);
        XTEST_ASSERT(leap.year == 2024 && leap.month == 2 && leap.day == 29);
        XTEST_ASSERT(leap.hour == 12 && leap.minute == 34 && leap.second == 56 && leap.millisecond == 789);
    }

    return xtest_finish("test_time");
}
