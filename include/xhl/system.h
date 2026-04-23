#ifndef XHL_SYSTEM_H
#define XHL_SYSTEM_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int _xsys_break_helper = 0;

#ifdef _WIN32
#define xsys_debugbreak() __debugbreak()
#else
#define xsys_debugbreak() __builtin_debugtrap()
#endif

#ifdef NDEBUG
#define xsys_assert(...)
#else
#define xsys_assert(cond) (((cond) ? (void)0 : xsys_debugbreak()), _xsys_break_helper += 0)
#endif

#define XSYS_ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#if defined(_MSC_VER)
#define XSYS_ALIGN(a) __declspec(align(a))
#else
#define XSYS_ALIGN(a) __attribute__((aligned(a)))
#endif

typedef enum XSystemInfoBool
{
    XSYSTEM_INFO_BOOL_UNKOWN,
    XSYSTEM_INFO_BOOL_TRUE,
    XSYSTEM_INFO_BOOL_FALSE,
} XSystemInfoBool;

XSYS_ALIGN(8) typedef struct XSystemString
{
    char buffer[64];
} XSystemString;

typedef struct XSystemInfo
{
    XSystemInfoBool init;

    XSystemString os_name;
    int           os_version_id;

    XSystemString model_name; // eg. MacBookAir10,1

    int os_version_number_major;
    int os_version_number_minor;
    int os_version_number_patch;

    XSystemString cpu_name;
    uint64_t      num_cores;

    uint64_t num_core_perf_levels;
    uint64_t num_cores_for_perf_level[8];

    int num_monitors;
    struct XMonitorInfo
    {
        XSystemString name;

        // Number of actual pixels on the monitor
        int num_physical_pixels_x;
        int num_physical_pixels_y;

        // Total size of the desktop that gets rendered, before backingScaleFactor scaling.
        // The OS may resize this to fill the screen
        int num_logical_pixels_x;
        int num_logical_pixels_y;

        // macOS only. Note that external monitors may have backingScaleFactor >= 2
        double backingScaleFactor;
    } monitors[8];

    int num_gpus;
    int default_gpu_idx;
    struct XGPUInfo
    {
        XSystemString   name;
        uint64_t        vram_max_bytes;
        uint64_t        vram_used_bytes;
        XSystemInfoBool is_unified_memory;
    } gpus[8];

    int64_t ram_max_bytes;
    int64_t ram_used_bytes; // last known value
                            // used to quickly diagnose problems for when a user asks:
                            //     "why does this program crash immediately?"
                            // then after several back and forths you find out they are using 98% of their 8gigs of
                            // RAM and keep loads of chrome tabs open
} XSystemInfo;

extern XSystemInfo g_sysinfo;

void print_info(XSystemInfo* info);
void init_sysinfo(XSystemInfo* info);

#endif // XHL_SYSTEM_H

#ifdef XHL_SYSTEM_IMPL
#undef XHL_SYSTEM_IMPL

XSystemInfo g_sysinfo = {0};

#ifdef __APPLE__
#ifdef __OBJC__

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/graphics/IOGraphicsLib.h>
#import <Metal/Metal.h>
#import <mach/mach.h>
#import <sys/sysctl.h>
#import <sys/types.h>

static int sysctl_str(const char* name, char* out, size_t out_sz)
{
    // https://developer.apple.com/documentation/kernel/1387446-sysctlbyname
    size_t sz  = out_sz;
    int    ret = sysctlbyname(name, out, &sz, NULL, 0);
    if (ret == 0)
    {
        size_t last = sz < out_sz ? sz : (out_sz - 1);
        out[last]   = '\0';
    }
    return ret;
}

static int sysctl_u64(const char* name, uint64_t* out)
{
    size_t sz = sizeof(*out);
    return sysctlbyname(name, out, &sz, NULL, 0);
}

void init_sysinfo(XSystemInfo* info)
{
    if (info->init == XSYSTEM_INFO_BOOL_TRUE)
        return;

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    // OS Version
    {
        NSProcessInfo*           pi    = [NSProcessInfo processInfo];
        NSOperatingSystemVersion v     = [pi operatingSystemVersion];
        NSString*                name  = [pi operatingSystemVersionString];
        const char*              name2 = [name UTF8String];

        info->ram_max_bytes = pi.physicalMemory;

        info->os_version_number_major = v.majorVersion;
        info->os_version_number_major = v.minorVersion;
        info->os_version_number_major = v.patchVersion;

        double total_gb = (double)info->ram_max_bytes / (1024.0 * 1024.0 * 1024.0);

        snprintf(info->os_name.buffer, sizeof(info->os_name), "%s", name2);
    }

    sysctl_str("hw.model", info->model_name.buffer, sizeof(info->model_name));

    // CPU
    {
        sysctl_str("machdep.cpu.brand_string", info->cpu_name.buffer, sizeof(info->cpu_name));
        sysctl_u64("hw.ncpu", &info->num_cores);
        sysctl_u64("hw.nperflevels", &info->num_core_perf_levels);

        for (int i = 0; i < info->num_core_perf_levels && i < XSYS_ARRLEN(info->num_cores_for_perf_level); i++)
        {
            char key[32] = {0};
            snprintf(key, sizeof(key), "hw.perflevel%d.logicalcpu", i);

            uint64_t* dest = &info->num_cores_for_perf_level[i];
            sysctl_u64(key, dest);
        }
    }

    // RAM
    {
        mach_port_t port = mach_host_self();

        vm_size_t     page_size = 0;
        kern_return_t ret       = host_page_size(port, &page_size);
        xsys_assert(ret == KERN_SUCCESS);
        vm_statistics64_data_t vm    = {0};
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        ret                          = host_statistics64(port, HOST_VM_INFO64, (host_info64_t)&vm, &count);
        xsys_assert(ret == KERN_SUCCESS);
        // NOTE: calculations made here look different to what is seen in the Activity Monitor app, but its
        // reasonably close
        uint64_t available   = page_size * (vm.free_count + vm.inactive_count + vm.purgeable_count);
        info->ram_used_bytes = info->ram_max_bytes - available;
    }

    // GPU
    {
        const NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
        const NSUInteger              N       = devices.count;
        for (int i = 0; i < N && i < XSYS_ARRLEN(info->gpus); i++, info->num_gpus++)
        {
            const id<MTLDevice> device = [devices objectAtIndex:i];
            struct XGPUInfo*    ginfo  = &info->gpus[i];

            const char* name = [device.name UTF8String];

            if (name == NULL)
                name = "[Unkown]";

            snprintf(ginfo->name.buffer, sizeof(ginfo->name), "%s", name);

            if (@available(macOS 10.15, *))
            {
                BOOL hasUnifiedMemory    = [device hasUnifiedMemory];
                ginfo->is_unified_memory = hasUnifiedMemory ? XSYSTEM_INFO_BOOL_TRUE : XSYSTEM_INFO_BOOL_FALSE;
            }
            // Technically not the real max, but close enough for our purposes
            ginfo->vram_max_bytes = [device recommendedMaxWorkingSetSize];
            // Not sure if I trust these numbers...
            ginfo->vram_used_bytes = [device currentAllocatedSize];
        }
    }

    // Monitor
    {
        const NSArray*   screens = [NSScreen screens];
        const NSUInteger N       = [screens count];
        for (int i = 0; i < N && i < XSYS_ARRLEN(info->monitors); i++, info->num_monitors++)
        {
            const NSScreen*      s     = [screens objectAtIndex:i];
            struct XMonitorInfo* minfo = &info->monitors[i];

            if (@available(macOS 10.15, *))
            {
                NSString*   name  = [s localizedName];
                const char* name2 = [name UTF8String];
                if (name2)
                {
                    snprintf(minfo->name.buffer, sizeof(minfo->name), "%s", name2);
                }
            }

            minfo->backingScaleFactor = [s backingScaleFactor];

            // TODO: get minotor size
        }
    }

    [pool release];

    info->init = XSYSTEM_INFO_BOOL_TRUE;
}

void print_info(XSystemInfo* info)
{
    if (info->init != XSYSTEM_INFO_BOOL_TRUE)
        init_sysinfo(info);

    printf("OS               : %s\n", info->os_name.buffer);
    printf("Model            : %s\n", info->model_name.buffer);
    printf("CPU              : %s  (%llu cores)\n", info->cpu_name.buffer, info->num_cores);
    for (int i = 0; i < info->num_core_perf_levels; i++)
    {
        int count = info->num_cores_for_perf_level[i];
        printf("  - P%d Cores     : %d\n", i, count);
    }

    const double one_gb   = (double)(1 << 30); // 1 GB
    double       total_gb = (double)info->ram_max_bytes / one_gb;
    double       used_gb  = (double)info->ram_used_bytes / one_gb;
    printf("RAM              : %.3f / %.3f GB\n", used_gb, total_gb);

    for (int i = 0; i < info->num_gpus; i++)
    {
        struct XGPUInfo* ginfo = &info->gpus[i];
        printf("GPU (%d)          : %s\n", i, ginfo->name.buffer);

        const double one_mb   = (double)(1 << 20); // 1 MB
        double       total_mb = (double)ginfo->vram_max_bytes / one_mb;
        double       used_mb  = (double)ginfo->vram_used_bytes / one_mb;

        const char* memtype = ginfo->is_unified_memory == XSYSTEM_INFO_BOOL_TRUE    ? " (Unified)"
                              : ginfo->is_unified_memory == XSYSTEM_INFO_BOOL_FALSE ? " (Dedicated)"
                                                                                    : "";
        printf("  - VRAM         : %.3f/%.3f MB%s\n", used_mb, total_mb, memtype);
    }

    for (int i = 0; i < info->num_monitors; i++)
    {
        struct XMonitorInfo* minfo = &info->monitors[i];
        printf("Display (%d)      : %s\n", i, minfo->name.buffer);
        printf("  - Scale Factor : %.1f\n", minfo->backingScaleFactor);
    }
}

#endif // __OBJC__
#endif // __APPLE__
#ifdef _WIN32
#error "TODO"
#endif // _WIN32

#endif // XHL_SYSTEM_IMPL