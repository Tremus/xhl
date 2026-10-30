#ifndef XHL_SYSTEM_H
#define XHL_SYSTEM_H

/*
    Dead simple API to get (hopefully) all relevant hardware/OS info from users of your software

    You only need to call xsys_init() once and everything you need is in 'g_xsysinfo'
    It will intelligently detect multiple calls and skip double processing.
    To refresh the data structure, use:
        memset(&g_xsysinfo, 0, sizeof(g_xsysinfo));
        xsys_init(&g_xsysinfo);

    JUST TELL ME EVERYTHING ABOUT THE SYSTEM - no fine grained APIs for me to study, just give me data
*/
#if 0
// Example program
#define XHL_SYSTEM_IMPL
#include <xhl/system.h>
int main(void)
{
    xsys_print(&g_xsysinfo); // automatically calls xsys_init() :)
    return 0;
}
#endif

#include <stdint.h>

#if defined(_MSC_VER)
#define XSYS_ALIGN(a) __declspec(align(a))
#else
#define XSYS_ALIGN(a) __attribute__((aligned(a)))
#endif

typedef enum XSystemInfoBool
{
    XSYSTEM_INFO_BOOL_UNKNOWN,
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
    int           os_version_number_major;
    int           os_version_number_minor;
    int           os_version_number_patch;

    XSystemString model_name; // eg. MacBookAir10,1

    XSystemString cpu_name;

    // CPUs with hybrid cores (eg. Intel P & E cores, Apple Silicon) have multiple perf levels. Other CPUs have 1
    // Levels are ordered fastest first, so index 0 is always the highest performance cores (eg. P cores)
    // Physical cores count each core once. Logical cores count each hardware thread (eg. Hyper-Threading / SMT)
    // A physical count of 0 means it couldn't be detected
    // Use xsys_num_physical_cores() & xsys_num_logical_cores() for totals
    uint64_t num_core_perf_levels;
    uint64_t num_physical_cores_for_perf_level[8];
    uint64_t num_logical_cores_for_perf_level[8];

    int64_t ram_max_bytes;
    int64_t ram_used_bytes; // last known value
    // used to quickly diagnose problems for when a user asks:
    //     "why does this program crash immediately?"
    // then after several back and forths you find out they're using 98% of their 8gigs of RAM due to chrome tabs

    // TRUE if any GPUs were found. FALSE if the system was checked and definitely has none (eg. a headless server)
    // UNKNOWN if GPUs couldn't be checked (eg. WSL2, which hides them behind a virtual adapter)
    XSystemInfoBool has_gpu;
    int             num_gpus;
    int             default_gpu_idx;
    struct XGPUInfo
    {
        XSystemString   name;
        uint64_t        vram_max_bytes;
        uint64_t        vram_used_bytes;
        XSystemInfoBool is_unified_memory;
    } gpus[8];

    int num_monitors;
    struct XMonitorInfo
    {
        XSystemString name;

        // Number of actual pixels on the monitor. May not match users display settings, as many users with large
        // monitors will want to "zoom" in items on their desktop and make text larger through scaling
        // You probably don't want this value!
        int physical_pixels_x;
        int physical_pixels_y;

        // Total size of the desktop that gets rendered, BEFORE backingScaleFactor scaling is applied
        // This buffer may get stretched when it appears on the monitor
        // This is the more relavent dimension for programers worried about things like setting an ideal window size
        // Use this value!
        int software_pixels_x;
        int software_pixels_y;

        // macOS only. Note that external monitors may have backingScaleFactor >= 2
        double backingScaleFactor;

        double refresh_rate_hz; // eg. 60Hz

        // If true, your window will likely be here
        XSystemInfoBool is_primary;
    } monitors[8];
} XSystemInfo;

extern XSystemInfo g_xsysinfo;

void xsys_init(XSystemInfo* info); // Expect this to take 1.5-20ms on Windows, with 1.5ms being the average case
void xsys_print(XSystemInfo* info);

// Sums of all perf levels
uint64_t xsys_num_physical_cores(const XSystemInfo* info);
uint64_t xsys_num_logical_cores(const XSystemInfo* info);

#endif // XHL_SYSTEM_H

#ifdef XHL_SYSTEM_IMPL
#undef XHL_SYSTEM_IMPL

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int  _xsys_break_helper   = 0;
const char* _sysbool_to_string[] = {"Unkown", "True", "False"};

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

XSystemInfo g_xsysinfo = {0};

#if defined(_WIN32) || defined(__linux__)
// Returns the perf level index for 'key', where a higher key means faster cores. Adds a new level if 'key' hasn't been
// seen before, keeping levels sorted fastest first. 'level_keys' holds the key of each existing level
static int xsys_get_perf_level(XSystemInfo* info, int64_t* level_keys, int64_t key)
{
    const int N = (int)info->num_core_perf_levels;
    int       i = 0;
    for (; i < N; i++)
    {
        if (level_keys[i] == key)
            return i;
        if (level_keys[i] < key)
            break;
    }
    if (N == (int)XSYS_ARRLEN(info->num_logical_cores_for_perf_level))
        return N - 1; // Out of space. Lump the rest in with the slowest level

    const size_t num_moved = N - i;
    memmove(level_keys + i + 1, level_keys + i, num_moved * sizeof(*level_keys));
    memmove(
        info->num_physical_cores_for_perf_level + i + 1,
        info->num_physical_cores_for_perf_level + i,
        num_moved * sizeof(uint64_t));
    memmove(
        info->num_logical_cores_for_perf_level + i + 1,
        info->num_logical_cores_for_perf_level + i,
        num_moved * sizeof(uint64_t));
    level_keys[i]                              = key;
    info->num_physical_cores_for_perf_level[i] = 0;
    info->num_logical_cores_for_perf_level[i]  = 0;
    info->num_core_perf_levels++;
    return i;
}

// Some Intel CPU brand strings are prefixed with spaces
// https://www.dungeon-master.com/forum/viewtopic.php?t=29636
static void xsys_trim_leading_spaces(XSystemString* str)
{
    char* p = str->buffer;
    while (*p == ' ')
        p++;
    if (p > str->buffer)
    {
        ptrdiff_t diff = p - str->buffer;
        memmove(str->buffer, p, sizeof(str->buffer) - diff);
    }
}
#endif

#ifdef _WIN32

#include <windows.h>

#include <D3dkmthk.h>
#include <dxgi1_3.h>
#include <intrin.h>
#include <winerror.h>
#include <winreg.h>

#ifdef _MSC_VER
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "gdi32.lib")
#endif

// D3DKMTQueryStatistics in particular is not always available in system headers,
// https://stackoverflow.com/questions/74239837/d3dkmtquerystatistics-not-found-in-d3dkmthk-h

// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtenumadapters
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtopenadapterfromluid
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtcloseadapter
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtquerystatistics
// https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/nf-d3dkmthk-d3dkmtqueryadapterinfo

// TODO: query VRAM through DXGI
// https://forums.unrealengine.com/t/how-to-get-vram-usage-via-c/218627/2

typedef LONG(APIENTRY* D3DKMTEnumAdaptersProc)(D3DKMT_ENUMADAPTERS*);
typedef LONG(APIENTRY* D3DKMTOpenAdapterFromLuidProc)(D3DKMT_OPENADAPTERFROMLUID*);
typedef LONG(APIENTRY* D3DKMTCloseAdapterProc)(const D3DKMT_CLOSEADAPTER*);
typedef LONG(APIENTRY* D3DKMTQueryStatisticsProc)(D3DKMT_QUERYSTATISTICS*);
typedef LONG(APIENTRY* D3DKMTQueryAdapterInfoProc)(D3DKMT_QUERYADAPTERINFO*);
static D3DKMTEnumAdaptersProc        pD3DKMTEnumAdapters        = NULL;
static D3DKMTOpenAdapterFromLuidProc pD3DKMTOpenAdapterFromLuid = NULL;
static D3DKMTCloseAdapterProc        pD3DKMTCloseAdapter        = NULL;
static D3DKMTQueryStatisticsProc     pD3DKMTQueryStatistics     = NULL;
static D3DKMTQueryAdapterInfoProc    pD3DKMTQueryAdapterInfo    = NULL;

HMODULE g_gdi32 = NULL;

struct XSystemQueriedAdapter
{
    D3DKMT_ADAPTERADDRESS   Address;
    D3DKMT_QUERY_DEVICE_IDS QueryIDs;
};
struct XSystemQueriedAdapterArray
{
    int                          NumAdapters;
    struct XSystemQueriedAdapter Adapters[8];
};

static NTSTATUS XSystemQueryAdapterInfo(D3DKMT_HANDLE hAdapter, KMTQUERYADAPTERINFOTYPE type, void* pData, UINT Size)
{
    if (!pD3DKMTQueryAdapterInfo)
        return 1;

    D3DKMT_QUERYADAPTERINFO info;
    memset(&info, 0, sizeof(info));
    memset(pData, 0, Size);
    info.hAdapter              = hAdapter;
    info.Type                  = type;
    info.pPrivateDriverData    = pData;
    info.PrivateDriverDataSize = Size;
    return pD3DKMTQueryAdapterInfo(&info);
}

// Source: https://github.com/a1ive/nwinfo/blob/dfab034ef89d2546b6c4e6cc7730d29fb9059079/libnw/gpu/d3d_gpu.c#L69
bool xsys_is_adapter_in_array(const struct XSystemQueriedAdapterArray* qaa, struct XSystemQueriedAdapter* a)
{
    const int N = qaa->NumAdapters;
    for (int i = 0; i < N; i++)
    {
        const struct XSystemQueriedAdapter* b = &qaa->Adapters[i];
        if (a->Address.DeviceNumber != 0 && b->Address.DeviceNumber != 0 &&
            a->Address.BusNumber == b->Address.BusNumber && a->Address.DeviceNumber == b->Address.DeviceNumber &&
            a->Address.FunctionNumber == b->Address.FunctionNumber)
            return true;
        if (a->QueryIDs.DeviceIds.VendorID == b->QueryIDs.DeviceIds.VendorID &&
            a->QueryIDs.DeviceIds.DeviceID == b->QueryIDs.DeviceIds.DeviceID &&
            a->QueryIDs.DeviceIds.SubSystemID == b->QueryIDs.DeviceIds.SubSystemID &&
            a->QueryIDs.DeviceIds.RevisionID == b->QueryIDs.DeviceIds.RevisionID)
            return true;
    }
    return false;
}

LONG xsys_read_registry(const char* pSubKey, const char* pValueName, const char* pOutBuf, SIZE_T OutSize)
{
    LONG hr;
    HKEY k;
    // TODO: use RegOpenKeyExW?
    hr = RegOpenKeyExA(HKEY_LOCAL_MACHINE, pSubKey, 0, KEY_READ, &k);
    if (hr == ERROR_SUCCESS)
    {
        DWORD sz = OutSize;
        hr       = RegQueryValueExA(k, pValueName, NULL, NULL, (LPBYTE)pOutBuf, &sz);
        RegCloseKey(k);
    }
    return hr;
}

void xsys_get_gpus_D3DKMT(XSystemInfo* info)
{
    HRESULT             hr = 0;
    D3DKMT_ENUMADAPTERS Adapters;

    struct XSystemQueriedAdapterArray QueriedAdapters;
    _STATIC_ASSERT(ARRAYSIZE(QueriedAdapters.Adapters) >= ARRAYSIZE(info->gpus));

    ZeroMemory(&Adapters, sizeof(Adapters));
    ZeroMemory(&QueriedAdapters, sizeof(QueriedAdapters));

    BOOL HaveD3DKMTProcs = FALSE;
    if (g_gdi32 == NULL)
        g_gdi32 = GetModuleHandleW(L"gdi32.dll");
    if (g_gdi32 == NULL)
        g_gdi32 = LoadLibraryW(L"gdi32.dll"); // ~6ms

    HaveD3DKMTProcs = !!pD3DKMTEnumAdapters && !!pD3DKMTOpenAdapterFromLuid && !!pD3DKMTCloseAdapter &&
                      !!pD3DKMTQueryStatistics && !!pD3DKMTQueryAdapterInfo;

    if (g_gdi32 != NULL && HaveD3DKMTProcs == FALSE)
    {
        pD3DKMTEnumAdapters = (D3DKMTEnumAdaptersProc)GetProcAddress(g_gdi32, "D3DKMTEnumAdapters");
        pD3DKMTOpenAdapterFromLuid =
            (D3DKMTOpenAdapterFromLuidProc)GetProcAddress(g_gdi32, "D3DKMTOpenAdapterFromLuid");
        pD3DKMTCloseAdapter     = (D3DKMTCloseAdapterProc)GetProcAddress(g_gdi32, "D3DKMTCloseAdapter");
        pD3DKMTQueryStatistics  = (D3DKMTQueryStatisticsProc)GetProcAddress(g_gdi32, "D3DKMTQueryStatistics");
        pD3DKMTQueryAdapterInfo = (D3DKMTQueryAdapterInfoProc)GetProcAddress(g_gdi32, "D3DKMTQueryAdapterInfo");

        HaveD3DKMTProcs = !!pD3DKMTEnumAdapters && !!pD3DKMTOpenAdapterFromLuid && !!pD3DKMTCloseAdapter &&
                          !!pD3DKMTQueryStatistics && !!pD3DKMTQueryAdapterInfo;
    }

    if (HaveD3DKMTProcs)
        hr = pD3DKMTEnumAdapters(&Adapters);
    xsys_assert(hr == S_OK);

    UINT AdapterIndex = 0;
    while (AdapterIndex < Adapters.NumAdapters)
    {
        struct XGPUInfo* ginfo = &info->gpus[info->num_gpus];

        const D3DKMT_ADAPTERINFO* pAdapter = &Adapters.Adapters[AdapterIndex++];

        // Good reference https://github.com/a1ive/nwinfo/blob/master/libnw/gpu/d3d_gpu.c

        D3DKMT_OPENADAPTERFROMLUID OpenAdapter;
        ZeroMemory(&OpenAdapter, sizeof(OpenAdapter));

        OpenAdapter.AdapterLuid = pAdapter->AdapterLuid;
        hr                      = pD3DKMTOpenAdapterFromLuid(&OpenAdapter);
        xsys_assert(hr == 0);

        // Skip adapters

        // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3dkmthk/ns-d3dkmthk-_d3dkmt_adaptertype
        if (hr == 0)
        {
            D3DKMT_ADAPTERTYPE AdapterType;
            const int          type = KMTQAITYPE_ADAPTERTYPE;
            hr = XSystemQueryAdapterInfo(OpenAdapter.hAdapter, type, &AdapterType, sizeof(AdapterType));
            xsys_assert(hr == 0);

            // Filter out virtual adapters like Parsec Remote Desktop which may show up in our results
            if (AdapterType.IndirectDisplayDevice)
            {
                // printf("[%d] Skipping IndirectDisplayDevice\n", AdapterIndex - 1);
                hr = -1;
            }
        }

        // Useful for identifying duplicate adapters.
        if (hr == 0)
        {
            // Gather identifiable info
            struct XSystemQueriedAdapter* xqa = &QueriedAdapters.Adapters[QueriedAdapters.NumAdapters];

            const int type1 = KMTQAITYPE_ADAPTERADDRESS;
            hr |= XSystemQueryAdapterInfo(OpenAdapter.hAdapter, type1, &xqa->Address, sizeof(xqa->Address));
            xsys_assert(hr == 0);

            // D3DKMT_QUERY_DEVICE_IDS QueryIDs;
            const int type2 = KMTQAITYPE_PHYSICALADAPTERDEVICEIDS;
            hr |= XSystemQueryAdapterInfo(OpenAdapter.hAdapter, type2, &xqa->QueryIDs, sizeof(xqa->QueryIDs));
            xsys_assert(hr == 0);

            // Skip duplicate adapters
            // If duplicate is detected, set hr to FAILED(hr). This will stop num_gpus from incrementing
            if (hr == 0)
            {
                if (xsys_is_adapter_in_array(&QueriedAdapters, xqa))
                {
                    hr = -1; // is duplicate adapter
                }
                else
                {
                    QueriedAdapters.NumAdapters++;
                }
            }
        }

        // Get name
        if (hr == 0)
        {
            D3DKMT_QUERY_ADAPTER_UNIQUE_GUID guid;
            const int                        type = KMTQAITYPE_QUERY_ADAPTER_UNIQUE_GUID;
            hr = XSystemQueryAdapterInfo(OpenAdapter.hAdapter, type, &guid, sizeof(guid));
            xsys_assert(hr == 0);
            if (hr == 0)
            {
                CHAR SubKey[MAX_PATH];
                snprintf(
                    SubKey,
                    MAX_PATH,
                    "SYSTEM\\CurrentControlSet\\Control\\Video\\%ls\\0000",
                    guid.AdapterUniqueGUID);

                // Junk values can end up in registry
                hr = xsys_read_registry(SubKey, "DriverDesc", ginfo->name.buffer, sizeof(ginfo->name));
            }
        }

        // Get max VRAM
        if (hr == 0)
        {
            D3DKMT_SEGMENTSIZEINFO SegSize;
            const int              type = KMTQAITYPE_GETSEGMENTSIZE;
            hr = XSystemQueryAdapterInfo(OpenAdapter.hAdapter, type, &SegSize, sizeof(SegSize));
            xsys_assert(hr == 0);

            if (hr == 0)
            {
                // NOTE: have not tested this on any hardware with unified memory
                ginfo->vram_max_bytes = SegSize.DedicatedVideoMemorySize;
                if (SegSize.DedicatedVideoMemorySize)
                    ginfo->is_unified_memory = XSYSTEM_INFO_BOOL_FALSE;
                if (SegSize.DedicatedVideoMemorySize == 0)
                    ginfo->is_unified_memory = XSYSTEM_INFO_BOOL_TRUE;
            }
        }

        // Find num segments
        UINT NbSegments = 0;
        if (hr == 0)
        {
            D3DKMT_QUERYSTATISTICS qs;
            ZeroMemory(&qs, sizeof(qs));
            qs.Type        = D3DKMT_QUERYSTATISTICS_ADAPTER;
            qs.AdapterLuid = pAdapter->AdapterLuid;
            hr             = pD3DKMTQueryStatistics(&qs);
            xsys_assert(hr == 0);
            if (hr == 0)
                NbSegments = qs.QueryResult.AdapterInformation.NbSegments;
        }

        for (UINT SegmentId = 0; SegmentId < NbSegments; SegmentId++)
        {
            D3DKMT_QUERYSTATISTICS qs;
            ZeroMemory(&qs, sizeof(qs));
            qs.Type                   = D3DKMT_QUERYSTATISTICS_SEGMENT;
            qs.AdapterLuid            = pAdapter->AdapterLuid;
            qs.QuerySegment.SegmentId = SegmentId;

            if (pD3DKMTQueryStatistics(&qs) != ERROR_SUCCESS)
                continue;

            // Is segment shared memory?
            BOOL IsSharedMemory = !!qs.QueryResult.SegmentInformation.Aperture;
            if (!IsSharedMemory)
            {
                ginfo->vram_used_bytes += qs.QueryResult.SegmentInformation.BytesResident;
            }
        }

        if (OpenAdapter.hAdapter)
        {
            D3DKMT_CLOSEADAPTER CloseAdapter;
            ZeroMemory(&CloseAdapter, sizeof(CloseAdapter));
            CloseAdapter.hAdapter = OpenAdapter.hAdapter;
            pD3DKMTCloseAdapter(&CloseAdapter);
        }

        if (hr == 0)
        {
            info->num_gpus++; // we trust this info is correct
        }
        else
        {
            memset(ginfo, 0, sizeof(*ginfo)); // reset info
        }
    }
}

void xsys_get_gpus_DXGI(XSystemInfo* info)
{
    HRESULT            hr           = 0;
    UINT               AdapterIndex = 0;
    IDXGIFactory1*     pFactory2    = NULL;
    IDXGIAdapter1*     pAdapter1    = NULL;
    DXGI_ADAPTER_DESC1 desc;

    DXGI_ADAPTER_DESC old_desc;

    // Used to tell "no GPU" apart from "couldn't check"
    UINT NumHardwareAdapters = 0;
    UINT NumFailedAdapters   = 0;

    struct XSystemQueriedAdapterArray QueriedAdapters;
    _STATIC_ASSERT(ARRAYSIZE(QueriedAdapters.Adapters) >= ARRAYSIZE(info->gpus));

    ZeroMemory(&desc, sizeof(desc));
    ZeroMemory(&QueriedAdapters, sizeof(QueriedAdapters));

    // Extremely slow. ~16-18ms
    hr = CreateDXGIFactory2(0, &IID_IDXGIFactory2, (void**)&pFactory2);
    xsys_assert(hr == S_OK);

    while (pFactory2 != NULL && info->num_gpus < XSYS_ARRLEN(info->gpus))
    {
        struct XGPUInfo* ginfo = &info->gpus[info->num_gpus];

        if (pAdapter1)
        {
            pAdapter1->lpVtbl->Release(pAdapter1);
            pAdapter1 = NULL;
        }

        hr = pFactory2->lpVtbl->EnumAdapters1(pFactory2, AdapterIndex, &pAdapter1);
        if (hr == DXGI_ERROR_NOT_FOUND)
            break; // end of enumeration
        AdapterIndex++;

        if (FAILED(hr) || FAILED(pAdapter1->lpVtbl->GetDesc1(pAdapter1, &desc)))
        {
            NumFailedAdapters++;
            continue;
        }
        // Skip "Microsoft Basic Render Driver" and similar
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
        NumHardwareAdapters++;

        // Unfortunately Windows don't do a good job at removing duplucates from
        // IDXGIFactory1::EnumAdapters1 Identify adapter and test for duplicates
        // If duplicate is detected, set hr to FAILED(hr). This will stop num_gpus from incrementing
        struct XSystemQueriedAdapter* xqa = &QueriedAdapters.Adapters[QueriedAdapters.NumAdapters];

        xqa->QueryIDs.DeviceIds.VendorID    = desc.VendorId;
        xqa->QueryIDs.DeviceIds.DeviceID    = desc.DeviceId;
        xqa->QueryIDs.DeviceIds.SubSystemID = desc.SubSysId;
        xqa->QueryIDs.DeviceIds.RevisionID  = desc.Revision;

        if (xsys_is_adapter_in_array(&QueriedAdapters, xqa))
        {
            hr = -1; // is duplicate adapter
        }
        else
        {
            QueriedAdapters.NumAdapters++; // new adapter
        }

        ginfo->vram_max_bytes = desc.DedicatedVideoMemory;

        WideCharToMultiByte(
            CP_UTF8,
            0,
            desc.Description,
            -1,
            ginfo->name.buffer,
            sizeof(ginfo->name.buffer) - 1,
            NULL,
            NULL);

        if (hr == 0)
        {
            info->num_gpus++; // we trust this info is correct
        }
        else
        {
            memset(ginfo, 0, sizeof(*ginfo)); // reset info
        }
    }
    xsys_assert(pAdapter1 == NULL);

    // Every adapter was checked, and they were all software renderers
    if (pFactory2 && NumHardwareAdapters == 0 && NumFailedAdapters == 0)
        info->has_gpu = XSYSTEM_INFO_BOOL_FALSE;

    if (pFactory2)
        pFactory2->lpVtbl->Release(pFactory2);
}

static BOOL CALLBACK xsys_monitor_enum_proc(HMONITOR hmon, HDC hdc, LPRECT pRect, LPARAM lparam)
{
    XSystemInfo* info = (XSystemInfo*)lparam;
    if (info->num_monitors >= XSYS_ARRLEN(info->monitors))
    {
        return FALSE; // end enumeration
    }

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getmonitorinfow
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-monitorinfoexw
    MONITORINFOEXW MonitorInfo;
    ZeroMemory(&MonitorInfo, sizeof(MonitorInfo));
    MonitorInfo.cbSize = sizeof(MonitorInfo);
    if (!GetMonitorInfoW(hmon, (LPMONITORINFO)&MonitorInfo))
    {
        return TRUE; // continue enumeraion
    }

    struct XMonitorInfo* m = &info->monitors[info->num_monitors++];

    // MONITORINFOEXW contains the same resolution users see if they go to
    // Start > Settings > Display > Display resolution
    // It may not be the real resolution of the monitor, but its the one that really matters!
    m->software_pixels_x = MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left;
    m->software_pixels_y = MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top;

    if (MonitorInfo.dwFlags & MONITORINFOF_PRIMARY)
        m->is_primary = XSYSTEM_INFO_BOOL_TRUE;
    else
        m->is_primary = XSYSTEM_INFO_BOOL_FALSE;

    // MONITORINFOEXW contains garbage system names like "\\.\DISPLAY1". We need to translate to a human readable one
    DISPLAYCONFIG_PATH_INFO paths[8];
    DISPLAYCONFIG_MODE_INFO modes[8];
    UINT                    NumPaths = XSYS_ARRLEN(paths);
    UINT                    NumModes = XSYS_ARRLEN(modes);

    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-querydisplayconfig
    LONG hr = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &NumPaths, paths, &NumModes, modes, NULL);
    if (hr == ERROR_SUCCESS)
    {
        // Get actual name
        for (UINT32 i = 0; i < NumPaths; i++)
        {
            DISPLAYCONFIG_PATH_INFO* p = &paths[i];

            // Match the source name
            DISPLAYCONFIG_SOURCE_DEVICE_NAME SourceName;
            ZeroMemory(&SourceName, sizeof(SourceName));
            SourceName.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            SourceName.header.size      = sizeof(SourceName);
            SourceName.header.adapterId = p->sourceInfo.adapterId;
            SourceName.header.id        = p->sourceInfo.id;

            // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-displayconfiggetdeviceinfo
            hr = DisplayConfigGetDeviceInfo(&SourceName.header);
            if (hr != 0)
                continue;

            BOOL IsMatch = wcscmp(MonitorInfo.szDevice, SourceName.viewGdiDeviceName) == 0;
            if (!IsMatch)
                continue;

            // Get target name
            DISPLAYCONFIG_TARGET_DEVICE_NAME TargetName;
            ZeroMemory(&TargetName, sizeof(TargetName));
            TargetName.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
            TargetName.header.size      = sizeof(TargetName);
            TargetName.header.adapterId = p->targetInfo.adapterId;
            TargetName.header.id        = p->targetInfo.id;

            hr = DisplayConfigGetDeviceInfo(&TargetName.header);
            if (hr != 0)
                continue;

            WCHAR* Name = TargetName.monitorFriendlyDeviceName;
            WideCharToMultiByte(CP_UTF8, 0, Name, -1, m->name.buffer, sizeof(m->name.buffer) - 1, NULL, NULL);
            break; // Found the match
        }

        // Get actual resolution

        // NOTE: It's possible to use EnumDisplaySettingsW(0,i,DEVMODEW) to iterate through all supported resolutions,
        // and you could use that to estimate the monitors resolution based on the highest value, but from my testing
        // that approach is quite slow (~2ms).
        // This feels like the more "correct" way, and its fast

        for (UINT32 i = 0; i < NumModes; i++)
        {
            DISPLAYCONFIG_MODE_INFO* mode = &modes[i];

            if (mode->infoType == DISPLAYCONFIG_MODE_INFO_TYPE_TARGET &&
                (m->physical_pixels_x == 0 && m->physical_pixels_y == 0))
            {
                DISPLAYCONFIG_VIDEO_SIGNAL_INFO* pInfo = &mode->targetMode.targetVideoSignalInfo;
                m->physical_pixels_x                   = pInfo->activeSize.cx;
                m->physical_pixels_y                   = pInfo->activeSize.cy;
                m->refresh_rate_hz = (double)pInfo->vSyncFreq.Numerator / (double)pInfo->vSyncFreq.Denominator;
                break;
            }
            // NOTE: DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE gets the software res
        }
    }

    return TRUE; // Continue enumeraion
}

void xsys_init(XSystemInfo* info)
{
    if (info->init == XSYSTEM_INFO_BOOL_TRUE)
        return;
    info->init = XSYSTEM_INFO_BOOL_TRUE;

    // OS Version. ~0.03ms
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-osversioninfow
        RTL_OSVERSIONINFOW vi;
        ZeroMemory(&vi, sizeof(vi));
        vi.dwOSVersionInfoSize = sizeof(vi);

        typedef LONG(WINAPI * RtlGetVersionProc)(PRTL_OSVERSIONINFOW);
        RtlGetVersionProc pRtlGetVersion = NULL;
        LONG              Status         = S_FALSE;

        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        if (ntdll)
        {
            pRtlGetVersion = (RtlGetVersionProc)GetProcAddress(ntdll, "RtlGetVersion");
            xsys_assert(pRtlGetVersion);
        }

        // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-rtlgetversion
        if (pRtlGetVersion)
            Status = pRtlGetVersion(&vi);

        if (Status == S_OK)
        {
            info->os_version_number_major = vi.dwMajorVersion;
            info->os_version_number_minor = vi.dwMinorVersion;
            info->os_version_number_patch = vi.dwBuildNumber;
        }

        // https://learn.microsoft.com/en-us/windows/win32/sysinfo/operating-system-version
        const char* name = "Windows";
        if (vi.dwMajorVersion == 10 && vi.dwBuildNumber >= 22000)
        {
            name = "Windows 11";
        }
        else if (vi.dwMajorVersion == 10)
        {
            name = "Windows 10";
        }
        else if (vi.dwMajorVersion == 6)
        {
            switch (vi.dwMinorVersion)
            {
            case 0:
                name = "Windows Vista";
                break;
            case 1:
                name = "Windows 7";
                break;
            case 2:
                name = "Windows 8";
                break;
            case 3:
                name = "Windows 8.1";
                break;
            }
        }
        snprintf(info->os_name.buffer, sizeof(info->os_name), "%s", name);
    }

    // CPU. ~0.02ms
    {
        int regs[4];
        __cpuid(regs, 0x80000000);
        if ((unsigned int)regs[0] >= 0x80000004u) // is brand string supported?
        {
            __cpuidex((int*)info->cpu_name.buffer + 0, 0x80000002, 0);
            __cpuidex((int*)info->cpu_name.buffer + 4, 0x80000003, 0);
            __cpuidex((int*)info->cpu_name.buffer + 8, 0x80000004, 0);
            _Static_assert(sizeof(info->cpu_name) > 48, "Increase buffer capacity");
        }
        else // Fallback
        {
            const char* pSubKey = "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
            xsys_read_registry(pSubKey, "ProcessorNameString", info->cpu_name.buffer, sizeof(info->cpu_name));
        }

        // Simulate padding
        // memmove(info->cpu_name.buffer + 8, info->cpu_name.buffer, 48);
        // memset(info->cpu_name.buffer, ' ', 8);

        xsys_trim_leading_spaces(&info->cpu_name);

        // Get physical & logical cores for each perf level (eg. P & E cores)
        {
            // Each entry is ~48 bytes, so this fits ~170 physical cores
            char  buffer[8192];
            DWORD bufferSize = sizeof(buffer);

            // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_logical_processor_information_ex
            // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformationex
            // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-processor_relationship

            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pIterator = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer;

            BOOL ok = GetLogicalProcessorInformationEx(RelationProcessorCore, pIterator, &bufferSize);
            if (ok)
            {
                int64_t level_keys[XSYS_ARRLEN(info->num_logical_cores_for_perf_level)];
                DWORD   offset = 0;

                while (offset < bufferSize)
                {
                    xsys_assert(pIterator->Relationship == RelationProcessorCore);
                    const PROCESSOR_RELATIONSHIP* core = &pIterator->Processor;

                    // Each set bit in the group masks is a logical processor on this core
                    uint64_t NumThreads = 0;
                    for (WORD g = 0; g < core->GroupCount; g++)
                        for (KAFFINITY mask = core->GroupMask[g].Mask; mask; mask &= mask - 1)
                            NumThreads++;

                    // Higher EfficiencyClass means higher performance. Non hybrid CPUs report 0 for every core
                    // TODO: figure out how to detect "low power e-cores", distinct from regular e cores
                    int level = xsys_get_perf_level(info, level_keys, core->EfficiencyClass);
                    info->num_physical_cores_for_perf_level[level] += 1;
                    info->num_logical_cores_for_perf_level[level]  += NumThreads;

                    offset    += pIterator->Size;
                    pIterator  = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)((BYTE*)buffer + offset);
                }
            }
        }

        if (info->num_core_perf_levels == 0) // fallback
        {
            // Only gives the logical processor count
            // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsysteminfo
            // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info
            SYSTEM_INFO si;
            ZeroMemory(&si, sizeof(si));
            GetSystemInfo(&si);
            info->num_core_perf_levels                = 1;
            info->num_logical_cores_for_perf_level[0] = si.dwNumberOfProcessors;
        }
    }

    // RAM. ~0.006ms
    {
        MEMORYSTATUSEX ms;
        ZeroMemory(&ms, sizeof(ms));
        ms.dwLength = sizeof(ms);
        BOOL ok     = GlobalMemoryStatusEx(&ms);
        xsys_assert(ok);
        if (ok)
        {
            info->ram_max_bytes  = ms.ullTotalPhys;
            info->ram_used_bytes = ms.ullTotalPhys - ms.ullAvailPhys;
        }
    }

    // GPU
    xsys_get_gpus_D3DKMT(info); // ~1.5ms
    if (info->num_gpus == 0)    // fallback, ~18ms
    {
        xsys_get_gpus_DXGI(info);
    }
    if (info->num_gpus > 0)
        info->has_gpu = XSYSTEM_INFO_BOOL_TRUE;

    // Monitors. ~0.12ms
    EnumDisplayMonitors(NULL, NULL, xsys_monitor_enum_proc, (LPARAM)info);
}

#endif // _WIN32

#ifdef __APPLE__
#if !defined(__OBJC__)
#error "Compile this as objective C"
#endif

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

void xsys_init(XSystemInfo* info)
{
    if (info->init == XSYSTEM_INFO_BOOL_TRUE)
        return;
    info->init = XSYSTEM_INFO_BOOL_TRUE;

    // OS Version ~3-4ms
    {
        NSProcessInfo*           pinfo = [NSProcessInfo processInfo];
        NSOperatingSystemVersion v     = [pinfo operatingSystemVersion];
        NSString*                name  = [pinfo operatingSystemVersionString];
        const char*              name2 = [name UTF8String];

        info->ram_max_bytes = pinfo.physicalMemory;

        info->os_version_number_major = v.majorVersion;
        info->os_version_number_minor = v.minorVersion;
        info->os_version_number_patch = v.patchVersion;

        double total_gb = (double)info->ram_max_bytes / (1024.0 * 1024.0 * 1024.0);

        if (name2)
        {
            snprintf(info->os_name.buffer, sizeof(info->os_name), "%s", name2);
        }

        [name release];
    }

    sysctl_str("hw.model", info->model_name.buffer, sizeof(info->model_name));

    // CPU ~0.010ms
    {
        sysctl_str("machdep.cpu.brand_string", info->cpu_name.buffer, sizeof(info->cpu_name));

        // hw.nperflevels requires macOS 12+. perflevel0 is the fastest cores, which matches our ordering
        if (sysctl_u64("hw.nperflevels", &info->num_core_perf_levels) == 0)
        {
            if (info->num_core_perf_levels > XSYS_ARRLEN(info->num_logical_cores_for_perf_level))
                info->num_core_perf_levels = XSYS_ARRLEN(info->num_logical_cores_for_perf_level);

            for (int i = 0; i < info->num_core_perf_levels; i++)
            {
                char key[32] = {0};
                snprintf(key, sizeof(key), "hw.perflevel%d.physicalcpu", i);
                sysctl_u64(key, &info->num_physical_cores_for_perf_level[i]);
                snprintf(key, sizeof(key), "hw.perflevel%d.logicalcpu", i);
                sysctl_u64(key, &info->num_logical_cores_for_perf_level[i]);
            }
        }
        else // fallback
        {
            info->num_core_perf_levels = 1;
            sysctl_u64("hw.physicalcpu", &info->num_physical_cores_for_perf_level[0]);
            sysctl_u64("hw.logicalcpu", &info->num_logical_cores_for_perf_level[0]);
        }
    }

    // RAM ~0.003ms
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

    // GPU ~3ms?
    {
        NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
        const NSUInteger        N       = devices.count;
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

        // An empty list means Metal found no GPUs (eg. some VMs). A nil list means we couldn't check
        if (info->num_gpus > 0)
            info->has_gpu = XSYSTEM_INFO_BOOL_TRUE;
        else if (devices != nil)
            info->has_gpu = XSYSTEM_INFO_BOOL_FALSE;

        [devices release];
    }

    // Monitor ~50ms
    {
        // https://stackoverflow.com/questions/13859109/how-to-programmatically-determine-native-pixel-resolution-of-retina-macbook-pro

        NSArray*         screens = [NSScreen screens];
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

            NSDictionary<NSDeviceDescriptionKey, id>* desc = [s deviceDescription];
            if (desc)
            {
                NSNumber*         screenID = [desc objectForKey:@"NSScreenNumber"];
                CGDirectDisplayID ID       = [screenID unsignedIntValue];

                minfo->is_primary = CGDisplayIsMain(ID) ? XSYSTEM_INFO_BOOL_TRUE : XSYSTEM_INFO_BOOL_FALSE;

                {
                    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(ID);
                    if (mode)
                    {
                        minfo->refresh_rate_hz = CGDisplayModeGetRefreshRate(mode);
                        CFRelease(mode);
                    }
                }

                CGRect rectLogicalPixels = CGDisplayBounds(ID);
                minfo->software_pixels_x = rectLogicalPixels.size.width;
                minfo->software_pixels_y = rectLogicalPixels.size.height;

                CFArrayRef ms = CGDisplayCopyAllDisplayModes(ID, NULL);
                if (ms)
                {
                    CFIndex numModes = CFArrayGetCount(ms);

                    for (int j = 0; j < numModes; ++j)
                    {
                        CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(ms, j);

                        size_t pixelWidth  = CGDisplayModeGetPixelWidth(m);
                        size_t pixelHeight = CGDisplayModeGetPixelHeight(m);

                        // size_t width       = CGDisplayModeGetWidth(m);
                        // size_t height      = CGDisplayModeGetHeight(m);

                        // printf("dm #%d: %zu:%zu %zu:%zu\n", i, pixelWidth, pixelHeight, width, height);
                        // CGDisplayModeGetIOFlags(m) & kDisplayModeNativeFlag
                        // if (pixelWidth == width && pixelHeight == height)
                        // {
                        if (pixelWidth > minfo->physical_pixels_x)
                            minfo->physical_pixels_x = pixelWidth;
                        if (pixelHeight > minfo->physical_pixels_y)
                            minfo->physical_pixels_y = pixelHeight;
                        // }
                    }

                    CFRelease(ms);
                }
            }
        }

        // From my understanding of Obj-C reference counting, we should be releasing this here. For some reason however
        // it causes scaling issues in a live GUI
        // [screens release];
    }
}

#endif // __APPLE__

#ifdef __linux__

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif

// Reads a small text file (eg. from /proc or /sys) into 'out' and strips trailing whitespace
// Returns the length, or -1 on failure
static int xsys_read_file(const char* path, char* out, size_t out_sz)
{
    out[0] = '\0';
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;
    ssize_t n = read(fd, out, out_sz - 1);
    close(fd);
    if (n < 0)
        return -1;
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == ' ' || out[n - 1] == '\0'))
        n--;
    out[n] = '\0';
    return (int)n;
}

// Reads a number from a file. Handles hex values prefixed with "0x", eg. PCI IDs
static int64_t xsys_read_int(const char* path, int64_t fallback)
{
    char buf[32];
    if (xsys_read_file(path, buf, sizeof(buf)) <= 0)
        return fallback;
    return strtoll(buf, NULL, 0);
}

// Finds the line starting with 'key' and returns the text after 'key', or NULL
static const char* xsys_find_line(const char* text, const char* key)
{
    const size_t len = strlen(key);
    for (const char* line = text; line != NULL;)
    {
        if (strncmp(line, key, len) == 0)
            return line + len;
        line = strchr(line, '\n');
        if (line)
            line++;
    }
    return NULL;
}

// Returns true if 'cpu' is in a kernel CPU list, eg. "0-11,16,18-19"
static bool xsys_cpulist_contains(const char* list, int cpu)
{
    const char* p = list;
    while (*p)
    {
        char* end;
        long  first = strtol(p, &end, 10);
        if (end == p)
            break;
        long last = first;
        if (*end == '-')
        {
            p    = end + 1;
            last = strtol(p, &end, 10);
        }
        if (cpu >= first && cpu <= last)
            return true;
        if (*end != ',')
            break;
        p = end + 1;
    }
    return false;
}

static const char* xsys_pci_vendor_name(int64_t vendor)
{
    switch (vendor)
    {
    case 0x1002:
        return "AMD";
    case 0x10de:
        return "NVIDIA";
    case 0x8086:
        return "Intel";
    }
    return NULL;
}

// Looks up a PCI device name in the pci.ids database, eg. "Navi 21 [Radeon RX 6800/6800 XT / 6900 XT]"
// It's usually installed, but not always. The file is ~1.5MB, so this takes a few ms
static bool xsys_pci_device_name(int64_t vendor, int64_t device, char* out, size_t out_sz)
{
    static const char* paths[] = {"/usr/share/hwdata/pci.ids", "/usr/share/misc/pci.ids"};

    FILE* f = NULL;
    for (int i = 0; i < (int)XSYS_ARRLEN(paths) && f == NULL; i++)
        f = fopen(paths[i], "r");
    if (f == NULL)
        return false;

    // Format:
    // 1002  Advanced Micro Devices, Inc. [AMD/ATI]
    // \t73bf  Navi 21 [Radeon RX 6800/6800 XT / 6900 XT]
    // \t\t1002 0e3a  Radeon Pro W6900X   <- subsystem, ignored
    char line[512];
    bool in_vendor = false;
    bool found     = false;
    while (!found && fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        if (line[0] != '\t')
        {
            if (in_vendor)
                break; // Passed the end of our vendor's devices
            in_vendor = strtoll(line, NULL, 16) == vendor;
            continue;
        }
        if (!in_vendor || line[1] == '\t')
            continue;

        char*   name = NULL;
        int64_t id   = strtoll(line + 1, &name, 16);
        if (id == device)
        {
            while (*name == ' ')
                name++;
            snprintf(out, out_sz, "%.*s", (int)strcspn(name, "\n"), name);
            found = true;
        }
    }
    fclose(f);
    return found;
}

// Gets the monitor name and native resolution from the 128 byte EDID base block
// https://en.wikipedia.org/wiki/Extended_Display_Identification_Data
static void xsys_parse_edid(const unsigned char* edid, struct XMonitorInfo* m)
{
    // 4x 18 byte descriptors
    for (int i = 0; i < 4; i++)
    {
        const unsigned char* d           = edid + 54 + i * 18;
        const int            pixel_clock = d[0] | (d[1] << 8);
        if (pixel_clock != 0)
        {
            // Detailed timing. The first one is the preferred mode, which is the monitors native resolution
            if (m->physical_pixels_x == 0)
            {
                m->physical_pixels_x = d[2] | ((d[4] & 0xf0) << 4);
                m->physical_pixels_y = d[5] | ((d[7] & 0xf0) << 4);
            }
        }
        else if (d[3] == 0xfc)
        {
            // Display name. Up to 13 chars, terminated by '\n' and padded with spaces
            char name[14] = {0};
            memcpy(name, d + 5, 13);
            size_t len = strcspn(name, "\n");
            while (len > 0 && name[len - 1] == ' ')
                len--;
            if (len > 0)
                snprintf(m->name.buffer, sizeof(m->name), "%.*s", (int)len, name);
        }
    }
}

void xsys_init(XSystemInfo* info)
{
    if (info->init == XSYSTEM_INFO_BOOL_TRUE)
        return;
    info->init = XSYSTEM_INFO_BOOL_TRUE;

    // OS. eg. "Ubuntu 24.04.1 LTS". The version numbers are the kernel version, as distro versions aren't always
    // numeric (Arch has none)
    {
        char        text[2048];
        const char* name = NULL;
        if (xsys_read_file("/etc/os-release", text, sizeof(text)) > 0 ||
            xsys_read_file("/usr/lib/os-release", text, sizeof(text)) > 0)
        {
            name = xsys_find_line(text, "PRETTY_NAME=");
        }

        struct utsname u;
        const bool     has_uname = uname(&u) == 0;

        if (name)
        {
            if (*name == '"' || *name == '\'')
                name++;
            snprintf(info->os_name.buffer, sizeof(info->os_name), "%.*s", (int)strcspn(name, "\"'\n"), name);
        }
        else if (has_uname)
        {
            snprintf(info->os_name.buffer, sizeof(info->os_name), "%s", u.sysname);
        }

        if (has_uname)
        {
            sscanf(
                u.release,
                "%d.%d.%d",
                &info->os_version_number_major,
                &info->os_version_number_minor,
                &info->os_version_number_patch);
        }
    }

    // Missing in WSL and some VMs
    xsys_read_file("/sys/class/dmi/id/product_name", info->model_name.buffer, sizeof(info->model_name));

    // CPU
    {
#if defined(__x86_64__) || defined(__i386__)
        unsigned int  max_leaf = 0, unused;
        unsigned int* brand    = (unsigned int*)info->cpu_name.buffer;
        _Static_assert(sizeof(info->cpu_name) > 48, "Increase buffer capacity");
        if (__get_cpuid(0x80000000, &max_leaf, &unused, &unused, &unused) && max_leaf >= 0x80000004)
        {
            __get_cpuid(0x80000002, brand + 0, brand + 1, brand + 2, brand + 3);
            __get_cpuid(0x80000003, brand + 4, brand + 5, brand + 6, brand + 7);
            __get_cpuid(0x80000004, brand + 8, brand + 9, brand + 10, brand + 11);
        }
#endif
        if (info->cpu_name.buffer[0] == '\0') // fallback
        {
            // The first CPU's entry is well within the first 4KB
            char        text[4096];
            const char* name = NULL;
            if (xsys_read_file("/proc/cpuinfo", text, sizeof(text)) > 0)
                name = xsys_find_line(text, "model name");
            if (name)
            {
                name += strspn(name, "\t :");
                snprintf(info->cpu_name.buffer, sizeof(info->cpu_name), "%.*s", (int)strcspn(name, "\n"), name);
            }
        }
        xsys_trim_leading_spaces(&info->cpu_name);

        // Intel hybrid CPUs list their P & E cores under separate PMUs. Hidden in WSL and most VMs
        int64_t    level_keys[XSYS_ARRLEN(info->num_logical_cores_for_perf_level)];
        char       p_cores[256], e_cores[256];
        const bool is_intel_hybrid = xsys_read_file("/sys/devices/cpu_core/cpus", p_cores, sizeof(p_cores)) > 0 &&
                                     xsys_read_file("/sys/devices/cpu_atom/cpus", e_cores, sizeof(e_cores)) > 0;

        const long num_cpus = sysconf(_SC_NPROCESSORS_CONF);
        for (int cpu = 0; cpu < num_cpus; cpu++)
        {
            char path[128];

            // cpu0 usually can't be taken offline, so has no 'online' file
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", cpu);
            if (xsys_read_int(path, 1) == 0)
                continue;

            int64_t key = 0;
            if (is_intel_hybrid)
            {
                key = xsys_cpulist_contains(p_cores, cpu) ? 1 : 0;
            }
            else
            {
#if !defined(__x86_64__) && !defined(__i386__)
                // ARM big.LITTLE reports relative performance (max 1024). On x86 this is either missing, or on AMD
                // it varies slightly between cores of the same type, so we don't use it there
                snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpu_capacity", cpu);
                key = xsys_read_int(path, 1024);
#endif
            }

            int level = xsys_get_perf_level(info, level_keys, key);
            info->num_logical_cores_for_perf_level[level]++;

            // Count each physical core once, by its first hardware thread
            char siblings[256];
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", cpu);
            if (xsys_read_file(path, siblings, sizeof(siblings)) > 0 && strtol(siblings, NULL, 10) == cpu)
                info->num_physical_cores_for_perf_level[level]++;
        }

        if (info->num_core_perf_levels == 0) // fallback. /sys isn't mounted, eg. in some sandboxes
        {
            info->num_core_perf_levels                = 1;
            info->num_logical_cores_for_perf_level[0] = sysconf(_SC_NPROCESSORS_ONLN);
        }
    }

    // RAM
    {
        char text[4096];
        if (xsys_read_file("/proc/meminfo", text, sizeof(text)) > 0)
        {
            // Values are in kB. MemAvailable counts reclaimable page cache as free
            const char* total     = xsys_find_line(text, "MemTotal:");
            const char* available = xsys_find_line(text, "MemAvailable:");
            if (total && available)
            {
                info->ram_max_bytes  = strtoll(total, NULL, 10) * 1024;
                info->ram_used_bytes = info->ram_max_bytes - strtoll(available, NULL, 10) * 1024;
            }
        }
        if (info->ram_max_bytes == 0) // fallback
        {
            struct sysinfo si;
            if (sysinfo(&si) == 0)
            {
                info->ram_max_bytes  = (int64_t)si.totalram * si.mem_unit;
                info->ram_used_bytes = (int64_t)(si.totalram - si.freeram - si.bufferram) * si.mem_unit;
            }
        }
    }

    // GPUs. Listed from the PCI bus rather than /sys/class/drm, so GPUs without a loaded driver are still found, and
    // we can be sure there are none if the bus has no display controllers
    DIR*           pci_dir = opendir("/sys/bus/pci/devices");
    struct dirent* pci_entry;
    while (pci_dir != NULL && info->num_gpus < (int)XSYS_ARRLEN(info->gpus) && (pci_entry = readdir(pci_dir)) != NULL)
    {
        if (pci_entry->d_name[0] == '.')
            continue;

        char        path[PATH_MAX];
        char        device_path[PATH_MAX];
        const char* pci_addr = pci_entry->d_name; // eg. "0000:01:00.0"
        snprintf(device_path, sizeof(device_path), "/sys/bus/pci/devices/%s", pci_addr);

        // Class 0x03 is a display controller, eg. 0x030000 VGA, or 0x030200 3D (eg. the dGPU in some laptops)
        snprintf(path, sizeof(path), "%s/class", device_path);
        if ((xsys_read_int(path, 0) >> 16) != 0x03)
            continue;

        snprintf(path, sizeof(path), "%s/vendor", device_path);
        const int64_t vendor = xsys_read_int(path, 0);
        snprintf(path, sizeof(path), "%s/device", device_path);
        const int64_t device = xsys_read_int(path, 0);

        // Microsoft's Hyper-V GPU adapters (eg. in WSL2) hide the real GPU, and include the software renderer
        if (vendor == 0 || vendor == 0x1414)
            continue;

        struct XGPUInfo* ginfo = &info->gpus[info->num_gpus];

        // The NVIDIA proprietary driver has the marketing name, eg. "NVIDIA GeForce RTX 3080"
        char        nvidia_info[1024];
        const char* model = NULL;
        snprintf(path, sizeof(path), "/proc/driver/nvidia/gpus/%s/information", pci_addr);
        if (xsys_read_file(path, nvidia_info, sizeof(nvidia_info)) > 0)
            model = xsys_find_line(nvidia_info, "Model:");

        const char* vendor_name = xsys_pci_vendor_name(vendor);
        char        device_name[sizeof(ginfo->name)];
        if (model)
        {
            model += strspn(model, " \t");
            snprintf(ginfo->name.buffer, sizeof(ginfo->name), "%.*s", (int)strcspn(model, "\n"), model);
        }
        else if (xsys_pci_device_name(vendor, device, device_name, sizeof(device_name)))
        {
            if (vendor_name)
                snprintf(ginfo->name.buffer, sizeof(ginfo->name), "%s %s", vendor_name, device_name);
            else
                snprintf(ginfo->name.buffer, sizeof(ginfo->name), "%s", device_name);
        }
        else if (vendor_name)
        {
            snprintf(ginfo->name.buffer, sizeof(ginfo->name), "%s 0x%04llx", vendor_name, (unsigned long long)device);
        }
        else
        {
            snprintf(
                ginfo->name.buffer,
                sizeof(ginfo->name),
                "0x%04llx 0x%04llx",
                (unsigned long long)vendor,
                (unsigned long long)device);
        }

        // Only amdgpu reports VRAM. NVIDIA requires NVML
        snprintf(path, sizeof(path), "%s/mem_info_vram_total", device_path);
        ginfo->vram_max_bytes = xsys_read_int(path, 0);
        snprintf(path, sizeof(path), "%s/mem_info_vram_used", device_path);
        ginfo->vram_used_bytes = xsys_read_int(path, 0);

        // Heuristic. Intel iGPUs sit on PCI bus 0, while discrete Intel Arc cards don't. NVIDIA GPUs on PCI are
        // discrete. AMD APUs and discrete cards look the same here
        unsigned int bus = 0;
        sscanf(pci_addr, "%*x:%x:", &bus);
        if (vendor == 0x8086)
            ginfo->is_unified_memory = bus == 0 ? XSYSTEM_INFO_BOOL_TRUE : XSYSTEM_INFO_BOOL_FALSE;
        else if (vendor == 0x10de)
            ginfo->is_unified_memory = XSYSTEM_INFO_BOOL_FALSE;

        // The GPU the firmware booted with
        snprintf(path, sizeof(path), "%s/boot_vga", device_path);
        if (xsys_read_int(path, 0) == 1)
            info->default_gpu_idx = info->num_gpus;

        info->num_gpus++;
    }
    if (pci_dir)
        closedir(pci_dir);

    // WSL2 passes GPUs through /dev/dxg instead of PCI, so finding none doesn't mean there are none
    if (info->num_gpus > 0)
        info->has_gpu = XSYSTEM_INFO_BOOL_TRUE;
    else if (pci_dir != NULL && access("/dev/dxg", F_OK) != 0)
        info->has_gpu = XSYSTEM_INFO_BOOL_FALSE;

    // Monitors. Read straight from the kernel, so no X11/Wayland connection is needed, but this means we only get
    // the name and native resolution. Empty in WSL, and with NVIDIA's driver unless nvidia-drm.modeset=1
    {
        DIR*           dir = opendir("/sys/class/drm");
        struct dirent* entry;
        while (dir != NULL && info->num_monitors < (int)XSYS_ARRLEN(info->monitors) && (entry = readdir(dir)) != NULL)
        {
            // Connectors are named like "card0-DP-1"
            const char* connector = strchr(entry->d_name, '-');
            if (strncmp(entry->d_name, "card", 4) != 0 || connector == NULL)
                continue;

            char path[PATH_MAX];
            char status[32];
            snprintf(path, sizeof(path), "/sys/class/drm/%s/status", entry->d_name);
            xsys_read_file(path, status, sizeof(status));
            if (strcmp(status, "connected") != 0)
                continue;

            struct XMonitorInfo* m = &info->monitors[info->num_monitors++];
            snprintf(m->name.buffer, sizeof(m->name), "%s", connector + 1); // Replaced by the EDID name if found

            unsigned char edid[128];
            ssize_t       n = -1;
            snprintf(path, sizeof(path), "/sys/class/drm/%s/edid", entry->d_name);
            int fd = open(path, O_RDONLY | O_CLOEXEC);
            if (fd >= 0)
            {
                n = read(fd, edid, sizeof(edid));
                close(fd);
            }
            static const unsigned char edid_header[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
            if (n == sizeof(edid) && memcmp(edid, edid_header, sizeof(edid_header)) == 0)
                xsys_parse_edid(edid, m);
        }
        if (dir)
            closedir(dir);
    }
}

#endif // __linux__

uint64_t xsys_num_physical_cores(const XSystemInfo* info)
{
    uint64_t n = 0;
    for (uint64_t i = 0; i < info->num_core_perf_levels; i++)
        n += info->num_physical_cores_for_perf_level[i];
    return n;
}

uint64_t xsys_num_logical_cores(const XSystemInfo* info)
{
    uint64_t n = 0;
    for (uint64_t i = 0; i < info->num_core_perf_levels; i++)
        n += info->num_logical_cores_for_perf_level[i];
    return n;
}

void xsys_print(XSystemInfo* info)
{
    if (info->init != XSYSTEM_INFO_BOOL_TRUE)
        xsys_init(info);

    printf("OS               : %s\n", info->os_name.buffer);
    if (info->model_name.buffer[0])
        printf("Model            : %s\n", info->model_name.buffer);
    printf("CPU              : %s\n", info->cpu_name.buffer);
    printf(
        "  - Cores        : %llu physical, %llu logical\n",
        (unsigned long long)xsys_num_physical_cores(info),
        (unsigned long long)xsys_num_logical_cores(info));
    // Fastest cores first
    for (int i = 0; info->num_core_perf_levels > 1 && i < (int)info->num_core_perf_levels; i++)
    {
        printf(
            "  - Perf level %d : %llu physical, %llu logical\n",
            i,
            (unsigned long long)info->num_physical_cores_for_perf_level[i],
            (unsigned long long)info->num_logical_cores_for_perf_level[i]);
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

        const char* memtype = _sysbool_to_string[ginfo->is_unified_memory];
        printf("  - VRAM         : %.3f/%.3f MB, Unified (%s)\n", used_mb, total_mb, memtype);
    }
    if (info->has_gpu == XSYSTEM_INFO_BOOL_FALSE)
        printf("GPU              : None\n");
    else if (info->num_gpus == 0)
        printf("GPU              : Unknown (failed to detect)\n");

    for (int i = 0; i < info->num_monitors; i++)
    {
        struct XMonitorInfo* minfo = &info->monitors[i];
        printf("Display (%d)      : %s\n", i, minfo->name.buffer);
        if (minfo->is_primary)
            printf("  - Primary      : %s\n", _sysbool_to_string[minfo->is_primary]);
        if (minfo->physical_pixels_x && minfo->physical_pixels_y)
            printf("  - Physical Res : %dx%d\n", minfo->physical_pixels_x, minfo->physical_pixels_y);
        if (minfo->software_pixels_x && minfo->software_pixels_y)
            printf("  - Software Res : %dx%d\n", minfo->software_pixels_x, minfo->software_pixels_y);
        if (minfo->refresh_rate_hz)
            printf("  - Refresh Rate : %.1fHz\n", minfo->refresh_rate_hz);
        if (minfo->backingScaleFactor)
            printf("  - Scale Factor : %.1f\n", minfo->backingScaleFactor);
    }

    fflush(stdout);
}

#endif // XHL_SYSTEM_IMPL