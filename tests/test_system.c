// Tests xhl/system.h. On Linux this is blocked by system.h having no Linux implementation of xsys_init()
//
// Only checks things every desktop has. GPUs and monitors are printed but not checked, so this can still pass on
// headless CI machines
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdbool.h>
#include <stdint.h>

#define XHL_SYSTEM_IMPL
#include "../include/xhl/system.h"

#include "xtest.h"

int main(void)
{
    XSystemInfo* info = &g_xsysinfo;
    xsys_init(info);
    xsys_print(info);

    XTEST_ASSERT(info->init == XSYSTEM_INFO_BOOL_TRUE);

    XTEST_ASSERT(info->os_name.buffer[0] != '\0');
    XTEST_ASSERT(info->os_version_number_major > 0);

    XTEST_ASSERT(info->cpu_name.buffer[0] != '\0');
    XTEST_ASSERT(info->num_cores > 0);

    // Between 512MB and 64TB
    XTEST_ASSERT(info->ram_max_bytes > (512ll << 20));
    XTEST_ASSERT(info->ram_max_bytes < (64ll << 40));
    XTEST_ASSERT(info->ram_used_bytes > 0);
    XTEST_ASSERT(info->ram_used_bytes <= info->ram_max_bytes);

    XTEST_ASSERT(info->num_gpus >= 0 && info->num_gpus <= 8);
    XTEST_ASSERT(info->num_monitors >= 0 && info->num_monitors <= 8);
    for (int i = 0; i < info->num_monitors; i++)
    {
        XTEST_ASSERT(info->monitors[i].software_pixels_x > 0);
        XTEST_ASSERT(info->monitors[i].software_pixels_y > 0);
    }

    // Calling again is a no-op
    xsys_init(info);
    XTEST_ASSERT(info->init == XSYSTEM_INFO_BOOL_TRUE);

    return xtest_finish("test_system");
}
