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
    uint64_t      num_cores;

    uint64_t num_core_perf_levels;
    uint64_t num_cores_for_perf_level[8];

    int64_t ram_max_bytes;
    int64_t ram_used_bytes; // last known value
                            // used to quickly diagnose problems for when a user asks:
                            //     "why does this program crash immediately?"
                            // then after several back and forths you find out they are using 98% of their 8gigs of
                            // RAM and keep loads of chrome tabs open

    int num_gpus;
    int default_gpu_idx;
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

#endif // XHL_SYSTEM_H

#ifdef XHL_SYSTEM_IMPL
#undef XHL_SYSTEM_IMPL

#include <stdbool.h>
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

        if (FAILED(hr))
            continue;
        if (FAILED(pAdapter1->lpVtbl->GetDesc1(pAdapter1, &desc)))
            continue;
        // Skip "Microsoft Basic Render Driver" and similar
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

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

        // NOTE: some Intel CPU are prefixed with spaces
        // https://www.dungeon-master.com/forum/viewtopic.php?t=29636
        char* p = info->cpu_name.buffer;
        while (*p == ' ')
            p++;
        if (p > info->cpu_name.buffer)
        {
            ptrdiff_t diff = p - info->cpu_name.buffer;
            memmove(info->cpu_name.buffer, p, sizeof(info->cpu_name) - diff);
        }

        // Get E & P cores
        {
            char  buffer[2048];
            DWORD bufferSize = sizeof(buffer);

            // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-system_logical_processor_information_ex
            // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformationex
            // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-processor_relationship

            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pIterator = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer;

            BOOL ok = GetLogicalProcessorInformationEx(RelationProcessorCore, pIterator, &bufferSize);
            if (ok)
            {
                int   NumPCores = 0;
                int   NumECores = 0;
                DWORD offset    = 0;

                while (offset < bufferSize)
                {
                    xsys_assert(pIterator->Relationship == RelationProcessorCore);
                    if (pIterator->Processor.EfficiencyClass > 0)
                    {
                        NumPCores++;
                    }
                    else
                    {
                        // TODO: figure out how to detect "low power e-cores", distinct from regular e cores
                        NumECores++;
                    }

                    offset    += pIterator->Size;
                    pIterator  = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)((BYTE*)buffer + offset);
                }

                info->num_cores = NumPCores + NumECores;

                if (NumPCores && NumECores)
                {
                    info->num_core_perf_levels        = 2;
                    info->num_cores_for_perf_level[0] = NumECores;
                    info->num_cores_for_perf_level[1] = NumPCores;
                }
                else
                {
                    info->num_core_perf_levels        = 1;
                    info->num_cores_for_perf_level[0] = info->num_cores;
                }
            }
        }

        if (info->num_cores == 0) // fallback
        {
            // If the processor sypports hyperthreading, you will likely get the thread count, not the core count...
            // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsysteminfo
            // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-system_info
            SYSTEM_INFO si;
            ZeroMemory(&si, sizeof(si));
            GetSystemInfo(&si);
            info->num_cores = si.dwNumberOfProcessors;
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

void xsys_print(XSystemInfo* info)
{
    if (info->init != XSYSTEM_INFO_BOOL_TRUE)
        xsys_init(info);

    printf("OS               : %s\n", info->os_name.buffer);
    if (info->model_name.buffer[0])
        printf("Model            : %s\n", info->model_name.buffer);
    printf("CPU              : %s\n", info->cpu_name.buffer);
    printf("  - Total Cores  : %llu\n", info->num_cores);
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

        const char* memtype = _sysbool_to_string[ginfo->is_unified_memory];
        printf("  - VRAM         : %.3f/%.3f MB, Unified (%s)\n", used_mb, total_mb, memtype);
    }

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