/*
 * Released into the public domain by Tré Dudman - 2024
 * For licensing and more info see https://github.com/Tremus/xhl
 *
 * stdio.h FILE* replacement for Windows & macOS
 * Contains a few extra nice to have features not offered by libc
 * All strings are assumed to be NULL terminated with UTF8 encoding
 *
 * WHY NOT JUST USE LIBC?
 * Windows C/C++ runtime can break when using Cyrillic characters for example in your file paths.
 * Presumably Microsofts implementation of libc interprets strings as ANSI rather than UTF8, which is a bummer for
 * anyone using non-latin characters in their Windows username.
 *
 * This problem alone was enough of a reason to create this library. Under the hood, your UTF8 paths are converted to
 * UTF16 on Windows which eliminates the above problem. Enjoy!
 *
 * LIMITATIONS:
 * Windows support file paths with names up to 32,767 wide characters. This library lazily supports a maximum of
 * MAX_PATH (260) wide characters using stack memory to avoid using malloc or similar. If you need longer paths, you
 * will have to add this yourself!
 * From what I've read around the internet, Windows File Explorer still doesn't support long paths. This may change in
 * future.
 * https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file#maximum-path-length-limitation
 *
 * BUILDING:
 * Simply #define XHL_FILES_IMPL in one of your build targets before including this header
 * MacOS: Requires Objective-C/Objective-C++ for some functions. Supports both ARC and no ARC
 *
 * LINKING:
 * Windows: Kernel32 Shell32 Shlwapi
 * macOS: -framework AppKit
 *
 * CREDITS:
 * Randy Gaul. 'xfiles_list' is a modified version of 'cf_scan' from the cute_files library
 * https://github.com/RandyGaul/cute_headers_deprecated/blob/master/cute_files.h
 */
#ifndef XHL_FILES_H
#define XHL_FILES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#define XFILES_DIR_STR      "\\"
#define XFILES_DIR_CHAR     '\\'
#define XFILES_BROWSER_NAME "File Explorer"
#define XFILES_TRASH_NAME   "Recycle Bin"
#else
#define XFILES_DIR_STR      "/"
#define XFILES_DIR_CHAR     '/'
#define XFILES_BROWSER_NAME "Finder"
#define XFILES_TRASH_NAME   "Trash"
#endif

#define XFILES_ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

#if !defined(XFILES_MALLOC) || !defined(XFILES_REALLOC) || !defined(XFILES_FREE)
#include <stdlib.h>
#define XFILES_MALLOC(size)       malloc(size)
#define XFILES_REALLOC(ptr, size) realloc(ptr, size)
#define XFILES_FREE(ptr)          free(ptr)
#endif

#ifndef XFILES_ASSERT
#ifdef NDEBUG
// clang-format off
#define XFILES_ASSERT(cond) do { (void)(cond); } while (0)
// clang-format on
#else
#ifdef _WIN32
#define XFILES_ASSERT(cond) (cond) ? (void)0 : __debugbreak()
#else // #if __APPLE__
#define XFILES_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif // _WIN32
#endif // NDEBUG
#endif // XFILES_ASSERT

#ifdef __cplusplus
extern "C" {
#endif

// On success, returns file or directory name, else returns NULL
const char* xfiles_get_name(const char* path);
// On success, returns file extension, else returns NULL
const char* xfiles_get_extension(const char* name);

// Returns true if directory or file exists
bool xfiles_exists(const char* path);
// Returns true if directory was created
bool xfiles_create_directory(const char* path);
// Creates directory and any required parent directories, then returns true if the exists or was craeted.
bool xfiles_create_directory_recursive(const char* path);

// Returns only true on success and sets 'out' and 'outlen' with file contents and size
// Must release 'out' with free()
bool xfiles_read(const char* path, void** out, size_t* outlen);
// Creates file if it doesn't exist with default access permissions.
// If file already exists, it overwrites all contents.
bool xfiles_write(const char* path, const void* in, size_t inlen);
// Creates file if it doesn't exist with default access permissions.
// Writes data starting from the end of a file, leaving old contents intact
bool xfiles_append(const char* path, const char* in, size_t inlen);
// Renames / moves file or folder
// You are advised to check for path collisions using xfiles_exists() beforehand
bool xfiles_move(const char* from, const char* to);

// Moves the file to:
// Win: Recycle Bin /
// OSX: Trash
bool xfiles_trash(const char* path);
// No bins. File is deleted and is likely unrecoverable
bool xfiles_delete(const char* path);
// Opens OS file or folder as if it was opened with
// Win: File Explorer /
// OSX: Finder
bool xfiles_open_file_explorer(const char* path);
// Opens OS file browsing app with path selected
// Win: File Explorer /
// OSX: Finder
bool xfiles_select_in_file_explorer(const char* path);

enum XFILES_USER_DIRECTORY
{
    // Windows: {letter}:\\Users\\{username}
    // macOS: /Users/{username}
    XFILES_USER_DIRECTORY_HOME,
    // Windows: [HOME]\\AppData\Roaming
    // macOS: [HOME]/Library/Application\ Support
    XFILES_USER_DIRECTORY_APPDATA,
    XFILES_USER_DIRECTORY_DESKTOP,
    XFILES_USER_DIRECTORY_DOCUMENTS,
    XFILES_USER_DIRECTORY_DOWNLOADS,
    XFILES_USER_DIRECTORY_MUSIC,
    XFILES_USER_DIRECTORY_PICTURES,
    XFILES_USER_DIRECTORY_VIDEOS,
    XFILES_USER_DIRECTORY_COUNT,
};

// Returns number of bytes written to 'char* out', excluding the null terminating byte
int xfiles_get_user_directory(char* out, size_t outlen, enum XFILES_USER_DIRECTORY loc);

typedef struct xfiles_list_item_t
{
    char path[1024];

    unsigned path_len;
    unsigned name_idx;
    unsigned ext_idx;
    bool     is_dir;
} xfiles_list_item_t;

typedef void(xfiles_list_callback_t)(void* data, const xfiles_list_item_t* item);
// Calls the above callback for each item in a directory
void xfiles_list(const char* path, void* data, xfiles_list_callback_t* cb);

enum XFILES_WATCH_TYPE
{
    XFILES_WATCH_CONTINUE,
    XFILES_WATCH_CREATED,
    XFILES_WATCH_DELETED,
    XFILES_WATCH_MODIFIED,
};
// If type == CONTINUE, return zero to exit from xfiles_watch()
typedef int (*xfiles_watch_callback_t)(enum XFILES_WATCH_TYPE type, const char* path, void* udata);
// Watch a directory and all subdirs for file & folder changes.
// Blocking call. Doesn't create any threads.
// macOS implementation will allocate memory. Windows implementation doesn't.
// cb_frequency_ms = approx wait time in milliseconds between polling for file events. eg. 50 ms
void xfiles_watch(const char* path, unsigned cb_frequency_ms, void* udata, xfiles_watch_callback_t cb);

#ifdef __cplusplus
}
#endif

#ifdef XHL_FILES_IMPL

#ifdef _WIN32
#include <Windows.h>

#include <Shlobj.h>
#include <Shlwapi.h>
#include <assert.h>
#include <shellapi.h>
#include <stdio.h>

#pragma comment(lib, "Shlwapi.lib")

bool xfiles_exists(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-pathfileexistsw
    // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar

    WCHAR pathunicode[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, pathunicode, XFILES_ARRLEN(pathunicode));
    XFILES_ASSERT(num);
    if (num)
        return PathFileExistsW(pathunicode);
    return false;
}

bool xfiles_create_directory(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectoryw
    WCHAR DirPath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, DirPath, XFILES_ARRLEN(DirPath));
    XFILES_ASSERT(num);
    if (num)
    {
        BOOL ok = CreateDirectoryW(DirPath, 0);
        XFILES_ASSERT(ok);
        return ok;
    }
    return false;
}

bool xfiles_read(const char* path, void** out, size_t* outlen)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfilesizeex
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile

    void*         data     = NULL;
    HANDLE        hFile    = NULL;
    LARGE_INTEGER FileSize = {0};
    BOOL          ok       = FALSE;

    WCHAR FileName[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FileName, XFILES_ARRLEN(FileName));
    XFILES_ASSERT(num);
    if (num)
    {
        hFile = CreateFileW(
            FileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);
        XFILES_ASSERT(hFile != INVALID_HANDLE_VALUE);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ok = GetFileSizeEx(hFile, &FileSize);
            XFILES_ASSERT(ok);
            if (ok)
            {
                data = XFILES_MALLOC(FileSize.QuadPart);
                ok   = ReadFile(hFile, data, FileSize.QuadPart, NULL, NULL);
                XFILES_ASSERT(ok);
                if (ok)
                {
                    *out    = data;
                    *outlen = FileSize.QuadPart;
                }
                else
                {
                    XFILES_FREE(data);
                    data              = NULL;
                    FileSize.QuadPart = 0;
                }
            }
            ok = ok && CloseHandle(hFile);
            XFILES_ASSERT(ok);
        }
    }

    return ok;
}

bool xfiles_write(const char* path, const void* in, size_t inlen)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile

    WCHAR  FilePath[MAX_PATH];
    HANDLE hFile         = NULL;
    BOOL   ok            = FALSE;
    DWORD  nBytesWritten = 0;
    int    num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        hFile = CreateFileW(
            FilePath,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);
        XFILES_ASSERT(hFile != INVALID_HANDLE_VALUE);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ok = WriteFile(hFile, in, inlen, &nBytesWritten, NULL);
            XFILES_ASSERT(ok);
            ok = ok && CloseHandle(hFile);
            XFILES_ASSERT(ok);
        }
    }

    return ok;
}

bool xfiles_append(const char* path, const char* in, size_t inlen)
{
    WCHAR  FilePath[MAX_PATH];
    HANDLE hFile         = NULL;
    BOOL   ok            = FALSE;
    DWORD  nBytesWritten = 0;
    int    num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        hFile =
            CreateFileW(FilePath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        XFILES_ASSERT(hFile != INVALID_HANDLE_VALUE);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ok = WriteFile(hFile, in, inlen, &nBytesWritten, NULL);
            XFILES_ASSERT(ok);
            ok = ok && CloseHandle(hFile);
            XFILES_ASSERT(ok);
        }
    }

    return ok;
}

bool xfiles_move(const char* from, const char* to)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefilew
    WCHAR FromPath[MAX_PATH];
    WCHAR ToPath[MAX_PATH];
    int   num1 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, from, -1, FromPath, XFILES_ARRLEN(FromPath));
    int   num2 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, to, -1, ToPath, XFILES_ARRLEN(ToPath));
    if (num1 && num2)
        return MoveFileW(FromPath, ToPath);
    return false;
}

bool xfiles_trash(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shfileoperationw
    // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shfileopstructw
    WCHAR PathList[MAX_PATH + 8] = {0};

    int num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, PathList, XFILES_ARRLEN(PathList));
    XFILES_ASSERT(num);
    if (num)
    {
        SHFILEOPSTRUCTW FileOp = {0};

        FileOp.wFunc  = FO_DELETE;
        FileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR | FOF_NOERRORUI | FOF_NORECURSION |
                        FOF_RENAMEONCOLLISION | FOF_SILENT;
        FileOp.pFrom = PathList;

        int ret = SHFileOperationW(&FileOp);
        XFILES_ASSERT(ret == 0);
        return ret == 0;
    }
    return false;
}

bool xfiles_delete(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-deletefilew
    WCHAR FilePath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        BOOL ok = DeleteFileW(FilePath);
        XFILES_ASSERT(ok);
        return ok;
    }
    return false;
}

bool xfiles_open_file_explorer(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew
    WCHAR FilePath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        INT_PTR ret = (INT_PTR)ShellExecuteW(NULL, L"open", FilePath, NULL, NULL, SW_SHOWDEFAULT);
        XFILES_ASSERT(ret > 32);
        return ret > 32;

        // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shopenfolderandselectitems
        LPITEMIDLIST pItemList = ILCreateFromPathW(FilePath);
        if (pItemList)
        {
            SHOpenFolderAndSelectItems(pItemList, 0, 0, 0);
            ILFree(pItemList);
        }
    }
    return false;
}

bool xfiles_select_in_file_explorer(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shopenfolderandselectitems
    // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-ilcreatefrompathw
    WCHAR FilePath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        LPITEMIDLIST pItemList = ILCreateFromPathW(FilePath);
        if (pItemList)
        {
            CoInitialize(NULL);
            HRESULT hres = SHOpenFolderAndSelectItems(pItemList, 0, 0, 0);
            XFILES_ASSERT(hres == S_OK);
            ILFree(pItemList);
            CoUninitialize();

            return hres = S_OK;
        }
    }
    return false;
}

int xfiles_get_user_directory(char* out, size_t outlen, enum XFILES_USER_DIRECTORY loc)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
    // https://learn.microsoft.com/en-us/windows/win32/shell/knownfolderid

    static const KNOWNFOLDERID* FOLDER_IDS[] = {
        &FOLDERID_Profile,        // XFILES_USER_DIRECTORY_HOME
        &FOLDERID_RoamingAppData, // XFILES_USER_DIRECTORY_APPDATA
        &FOLDERID_Desktop,        // XFILES_USER_DIRECTORY_DESKTOP
        &FOLDERID_Documents,      // XFILES_USER_DIRECTORY_DOCUMENTS
        &FOLDERID_Downloads,      // XFILES_USER_DIRECTORY_DOWNLOADS
        &FOLDERID_Music,          // XFILES_USER_DIRECTORY_MUSIC
        &FOLDERID_Pictures,       // XFILES_USER_DIRECTORY_PICTURES
        &FOLDERID_Videos,         // XFILES_USER_DIRECTORY_VIDEOS
    };
#ifdef _MSC_VER
    static_assert(XFILES_ARRLEN(FOLDER_IDS) == XFILES_USER_DIRECTORY_COUNT, "");
#else
    _Static_assert(XFILES_ARRLEN(FOLDER_IDS) == XFILES_USER_DIRECTORY_COUNT, "");
#endif

#ifdef __cplusplus
#define XFILES_REF(ptr) *ptr
#else
#define XFILES_REF(ptr) ptr
#endif

    int   num  = 0;
    PWSTR Path = NULL;
    if (loc < 0)
        loc = (enum XFILES_USER_DIRECTORY)0;
    if (loc >= XFILES_USER_DIRECTORY_COUNT)
        loc = (enum XFILES_USER_DIRECTORY)(ARRAYSIZE(FOLDER_IDS) - 1);
    if (S_OK == SHGetKnownFolderPath(XFILES_REF(FOLDER_IDS[loc]), 0, NULL, &Path))
    {
        num = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, Path, -1, out, outlen, NULL, NULL);
        if (num)
            num--; // remove count of null byte
        XFILES_ASSERT(num == strlen(out));
        CoTaskMemFree(Path);
    }

    return num;
}

void xfiles_list(const char* path, void* data, xfiles_list_callback_t* callback)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilew
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findnextfilew
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findclose

    HANDLE             hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW   FindData;
    xfiles_list_item_t Item;
    bool               HasNext = 0;

    {
        WCHAR PathUnicode[MAX_PATH] = {0};
        int   n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, PathUnicode, XFILES_ARRLEN(PathUnicode));
        if (n)
        {
            PathUnicode[n] = 0;
            wcsncat(PathUnicode, L"\\*", XFILES_ARRLEN(PathUnicode) - n - 1);
            hFind = FindFirstFileW(PathUnicode, &FindData);
        }
    }

    HasNext = hFind != INVALID_HANDLE_VALUE;
    XFILES_ASSERT(hFind != INVALID_HANDLE_VALUE);

    Item.name_idx = snprintf(Item.path, sizeof(Item.path), "%s\\", path);

    while (HasNext)
    {
        int NameLen = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            FindData.cFileName,
            -1,
            Item.path + Item.name_idx,
            sizeof(Item.path) - Item.name_idx,
            NULL,
            NULL);
        XFILES_ASSERT(NameLen);
        if (NameLen == 0) // Failed conversion
            goto iterate;

        Item.path_len = Item.name_idx + NameLen - 1;

        Item.ext_idx = Item.name_idx;
        for (unsigned i = Item.name_idx; i < Item.path_len; i++)
            if (Item.path[i] == '.')
                Item.ext_idx = i;
        if (Item.ext_idx == Item.name_idx) // Failed to find extension
            Item.ext_idx = Item.path_len;

        XFILES_ASSERT(strlen(Item.path) == Item.path_len);
        Item.is_dir = FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
        callback(data, &Item);

    iterate:
        HasNext = FindNextFileW(hFind, &FindData);
    }

    if (hFind != INVALID_HANDLE_VALUE)
        FindClose(hFind);
}

void xfiles_watch(const char* path, unsigned cb_frequency_ms, void* udata, xfiles_watch_callback_t cb)
{
    HANDLE hDirectory;
    // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped
    OVERLAPPED overlapped;

    int pathlen = strlen(path);
    // Remove backslash
    if (pathlen > 0 && (path[pathlen - 1] == '\\' || path[pathlen - 1] == '/'))
        pathlen--;

    {
        WCHAR wPath[MAX_PATH];
        int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, pathlen, wPath, XFILES_ARRLEN(wPath));
        XFILES_ASSERT(num);

        hDirectory = CreateFileW(
            wPath,
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL);
        XFILES_ASSERT(hDirectory != INVALID_HANDLE_VALUE);
        if (hDirectory == INVALID_HANDLE_VALUE)
            return;
    }

    overlapped.hEvent = CreateEventW(NULL, FALSE, 0, NULL);
    XFILES_ASSERT(overlapped.hEvent);

    int loop = 0;
    do
    {
        BYTE buffer[1024 * 4];
        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw
        BOOL success = ReadDirectoryChangesW(
            hDirectory,
            buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                // FILE_NOTIFY_CHANGE_ATTRIBUTES |
                FILE_NOTIFY_CHANGE_LAST_WRITE,
            NULL,
            &overlapped,
            NULL);
        XFILES_ASSERT(success);
        if (!success)
            goto exit;

        DWORD result = WaitForSingleObject(overlapped.hEvent, cb_frequency_ms);
        if (result == WAIT_OBJECT_0)
        {
            DWORD dwNumberOfBytesTransferred = 0;
            GetOverlappedResult(hDirectory, &overlapped, &dwNumberOfBytesTransferred, TRUE);

            DWORD dwOffset = 0;
            while (dwOffset < dwNumberOfBytesTransferred)
            {
                FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)(buffer + dwOffset);

                enum XFILES_WATCH_TYPE type = 0;
                if (pNotify->Action == FILE_ACTION_ADDED)
                    type = XFILES_WATCH_CREATED;
                if (pNotify->Action == FILE_ACTION_REMOVED)
                    type = XFILES_WATCH_DELETED;
                if (pNotify->Action == FILE_ACTION_MODIFIED)
                    type = XFILES_WATCH_MODIFIED;

                if (type)
                {
                    char fp[MAX_PATH];
                    int  NameLength = pNotify->FileNameLength / sizeof(wchar_t);

                    int fplen  = snprintf(fp, sizeof(fp), "%.*s" XFILES_DIR_STR, pathlen, path);
                    fplen     += WideCharToMultiByte(
                        CP_UTF8,
                        WC_ERR_INVALID_CHARS,
                        pNotify->FileName,
                        NameLength,
                        fp + fplen,
                        sizeof(fp) - fplen,
                        NULL,
                        NULL);

                    cb(type, fp, udata);
                }

                dwOffset += pNotify->NextEntryOffset;
                if (pNotify->NextEntryOffset == 0)
                    break;
            }
        }

        loop = cb(XFILES_WATCH_CONTINUE, "", udata);
    }
    while (loop);

exit:
    CloseHandle(overlapped.hEvent);
    CloseHandle(hDirectory);
}

#endif // _WIN32

#ifdef __APPLE__
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <unistd.h>

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/access.2.html
bool xfiles_exists(const char* path) { return access(path, F_OK) == 0; }
// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/mkdir.2.html
bool xfiles_create_directory(const char* path) { return mkdir(path, 0777) == 0; }

bool xfiles_read(const char* path, void** out, size_t* outlen)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/open.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fstat.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/read.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/close.2.html

    int         fd    = -1;
    int         ret   = -1;
    struct stat info  = {0};
    void*       data  = NULL;
    ssize_t     nread = -1;

    fd = open(path, O_RDONLY);
    XFILES_ASSERT(fd != -1);
    if (fd != -1)
    {
        ret = fstat(fd, &info);
        XFILES_ASSERT(ret != -1);
        if (ret != -1)
        {
            data  = XFILES_MALLOC(info.st_size);
            nread = read(fd, data, info.st_size);
            XFILES_ASSERT(nread != -1);
            if (nread == -1)
            {
                XFILES_FREE(data);
                data = NULL;
            }
            else
            {
                *out    = data;
                *outlen = info.st_size;
            }
        }
        close(fd);
    }

    return nread != -1;
}

bool xfiles_write(const char* path, const void* in, size_t inlen)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/write.2.html#//apple_ref/doc/man/2/write
    int     fd;
    ssize_t nwritten = -1;

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    XFILES_ASSERT(fd != -1);
    if (fd != -1)
    {
        nwritten = write(fd, in, inlen);
        XFILES_ASSERT(nwritten != -1);
        close(fd);
    }
    return nwritten != -1;
}

bool xfiles_append(const char* path, const char* in, size_t inlen)
{
    int     fd;
    ssize_t nwritten = -1;

    fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0777);
    XFILES_ASSERT(fd != -1);
    if (fd != -1)
    {
        nwritten = write(fd, in, inlen);
        XFILES_ASSERT(nwritten != -1);
        close(fd);
    }
    return nwritten != -1;
}

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/rename.2.html
bool xfiles_move(const char* from, const char* to) { return 0 == rename(from, to); }

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/unlink.2.html
bool xfiles_delete(const char* path) { return unlink(path) == 0; }

void xfiles_list(const char* path, void* data, xfiles_list_callback_t* callback)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/opendir.3.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/readdir.3.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/closedir.3.html

    DIR*               dir   = NULL;
    struct dirent*     entry = NULL;
    xfiles_list_item_t item;

    dir = opendir(path);
    if (dir)
        entry = readdir(dir);

    item.name_idx = snprintf(item.path, sizeof(item.path), "%s/", path);

    while (entry)
    {
        item.path_len = item.name_idx + entry->d_namlen;

        XFILES_ASSERT(item.path_len < sizeof(item.path));
        if (item.path_len >= sizeof(item.path)) // Guard overflow
            goto iterate;

        memcpy(&item.path[item.name_idx], entry->d_name, entry->d_namlen);
        item.path[item.path_len] = 0;

        item.ext_idx = item.name_idx;
        for (uint32_t i = item.name_idx; i < item.path_len; i++)
            if (item.path[i] == '.')
                item.ext_idx = i;
        if (item.ext_idx == item.name_idx) // Failed to find extension
            item.ext_idx = item.path_len;
        item.is_dir = DT_DIR & entry->d_type;

        XFILES_ASSERT(strlen(item.path) == item.path_len);
        callback(data, &item);

    iterate:
        // readdir returns NULL if no more files, breaking the loop
        entry = readdir(dir);
    }

    if (dir)
        closedir(dir);
}

struct XFWatchHandles
{
    int  fd;
    bool is_dir;
    // path is stored in stringpool
    size_t stringpool_offset;
};

struct XFWatchContext
{
    struct kevent*         events; // xarray
    size_t                 events_len;
    size_t                 events_cap;
    struct XFWatchHandles* handles; // xarray
    size_t                 handles_len;
    size_t                 handles_cap;
    char*                  stringpool; // xarray
    size_t                 stringpool_len;
    size_t                 stringpool_cap;

    void*                   udata;
    xfiles_watch_callback_t callback;
};

static size_t _xfiles_watch_push_string(struct XFWatchContext* ctx, const char* str, size_t len)
{
    size_t offset = ctx->stringpool_len;
    XFILES_ASSERT((offset & 15) == 0);

    size_t nextlen = offset + len + 1;       // +1 for '\0' byte
    nextlen        = (nextlen + 0xf) & ~0xf; // Round to 16 byte boundary
    XFILES_ASSERT((nextlen & 15) == 0);

    if (nextlen > ctx->stringpool_cap)
    {
        size_t nextcap = nextlen * 2;
        if (nextcap < 4096) // min cap
            nextcap = 4096;
        ctx->stringpool     = XFILES_REALLOC(ctx->stringpool, nextcap);
        ctx->stringpool_cap = nextcap;
    }
    ctx->stringpool_len = nextlen;

    memcpy(ctx->stringpool + offset, str, len);
    ctx->stringpool[offset + len] = '\0';

    return offset;
}

void _xfiles_watch_add_listener(struct XFWatchContext* ctx, const char* path_, bool is_dir)
{
    struct kevent         event;
    struct XFWatchHandles wh;

    wh.fd = open(path_, O_EVTONLY);
    XFILES_ASSERT(wh.fd > 0);
    if (wh.fd <= 0)
        return;

    wh.is_dir            = is_dir;
    wh.stringpool_offset = _xfiles_watch_push_string(ctx, path_, strlen(path_));
    char* path           = ctx->stringpool + wh.stringpool_offset;
    XFILES_ASSERT(path >= ctx->stringpool);

    unsigned short flags = EV_ADD | EV_CLEAR;
    unsigned int fflags  = NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_RENAME | NOTE_REVOKE;
    event.ident          = wh.fd;
    event.filter         = EVFILT_VNODE;
    event.flags          = flags;
    event.fflags         = fflags;
    event.data           = 0;
    // Supposedly you're able to set a ptr here to whatever you want. It appears to be occasionally overwritten by
    // something in kevent(), so we can't trust it... Unless I'm doing a buffer overrun elsewhere, this is a bug
    event.udata = path;

    XFILES_ASSERT(ctx->events_len == ctx->handles_len);
    XFILES_ASSERT(ctx->events_cap == ctx->handles_cap);
    if (ctx->events_len + 1 > ctx->events_cap)
    {
        size_t nextcap = 2 * (ctx->events_len + 1);
        if (nextcap < 64) // min cap
            nextcap = 64;
        ctx->events      = XFILES_REALLOC(ctx->events, sizeof(*ctx->events) * nextcap);
        ctx->handles     = XFILES_REALLOC(ctx->handles, sizeof(*ctx->handles) * nextcap);
        ctx->events_cap  = nextcap;
        ctx->handles_cap = nextcap;
    }
    ctx->events[ctx->events_len++]   = event;
    ctx->handles[ctx->handles_len++] = wh;

    size_t nevents  = ctx->events_len;
    size_t nfolders = ctx->handles_len;
    XFILES_ASSERT(nevents == nfolders);
}

int _xfiles_watch_find_listener_by_string(struct XFWatchContext* ctx, const char* path)
{
    int    i;
    size_t nevents  = ctx->events_len;
    size_t nfolders = ctx->handles_len;
    XFILES_ASSERT(nevents == nfolders);
    for (i = 0; i < nfolders; i++)
    {
        struct XFWatchHandles* wh          = ctx->handles + i;
        const char*            cached_path = ctx->stringpool + wh->stringpool_offset;
        int                    match       = strcmp(path, cached_path);
        if (match == 0)
            return i;
    }
    return -1;
}

int _xfiles_watch_find_listener_by_fd(struct XFWatchContext* ctx, int fd)
{
    int    i;
    size_t nevents  = ctx->events_len;
    size_t nfolders = ctx->handles_len;
    XFILES_ASSERT(nevents == nfolders);
    for (i = 0; i < nfolders; i++)
    {
        struct kevent* e = ctx->events + i;
        if (e->ident == fd)
            return i;
    }
    return -1;
}

void _xfiles_watch_remove_listener_at_index(struct XFWatchContext* ctx, int i)
{
    XFILES_ASSERT(ctx->events_len > 0 && ctx->handles_len > 0);
    if (i != -1)
    {
        struct XFWatchHandles wh   = ctx->handles[i]; // copy data onto stack before deleting
        const char*           path = ctx->stringpool + wh.stringpool_offset;
        close(wh.fd);

        XFILES_ASSERT(ctx->events_len == ctx->handles_len);
        size_t remaining = ctx->events_len - i - 1;
        if (remaining)
        {
            memmove(ctx->events + i, ctx->events + i + 1, sizeof(*ctx->events) * remaining);
            memmove(ctx->handles + i, ctx->handles + i + 1, sizeof(*ctx->handles) * remaining);
        }
        ctx->events_len--;
        ctx->handles_len--;
    }
}

void _xfiles_watch_cb_on_direntry(void* data, const xfiles_list_item_t* item)
{
    const char* name    = item->path + item->name_idx;
    int         match_1 = strcmp(name, ".");
    int         match_2 = strcmp(name, "..");
    if (match_1 == 0 || match_2 == 0)
        return;

    struct XFWatchContext* ctx = data;
    _xfiles_watch_add_listener(ctx, item->path, item->is_dir);

    if (item->is_dir)
    {
        xfiles_list(item->path, ctx, _xfiles_watch_cb_on_direntry);
    }
}

void _xfiles_watch_cb_poll_for_new_entries(void* data, const xfiles_list_item_t* item)
{
    const char* name    = item->path + item->name_idx;
    int         match_1 = strcmp(name, ".");
    int         match_2 = strcmp(name, "..");
    if (match_1 == 0 || match_2 == 0)
        return;

    struct XFWatchContext* ctx = data;
    int                    idx = _xfiles_watch_find_listener_by_string(ctx, item->path);
    if (idx == -1)
    {
        _xfiles_watch_add_listener(ctx, item->path, item->is_dir);
        ctx->callback(XFILES_WATCH_CREATED, item->path, ctx->udata);

        if (item->is_dir)
        {
            xfiles_list(item->path, ctx, _xfiles_watch_cb_poll_for_new_entries);
        }
    }
}

void xfiles_watch(const char* path, unsigned cb_frequency_ms, void* udata, xfiles_watch_callback_t cb)
{
    struct XFWatchContext ctx = {0};
    int                   kq  = 0;

    ctx.udata    = udata;
    ctx.callback = cb;

    _xfiles_watch_add_listener(&ctx, path, true);
    xfiles_list(path, &ctx, _xfiles_watch_cb_on_direntry);

    // https://man.freebsd.org/cgi/man.cgi?kqueue
    // https://www.ipnom.com/FreeBSD-Man-Pages/kqueue.2.html
    kq = kqueue();

    int loop = 0;
    do
    {
        struct timespec timeout;
        timeout.tv_sec  = 0;
        timeout.tv_nsec = cb_frequency_ms * 1000000;

        struct kevent event_data[32] = {0};
        size_t        nevents        = ctx.events_len;
        // https://www.ipnom.com/FreeBSD-Man-Pages/kevent.2.html
        int event_count = kevent(kq, ctx.events, nevents, event_data, XFILES_ARRLEN(event_data), &timeout);

        if ((event_count < 0) || (event_data[0].flags == EV_ERROR))
            break;

        if (event_count)
        {
            for (int i = 0; i < event_count; i++)
            {
                // Helpful table pulled from here:
                // https://github.com/segmentio/fs/blob/main/notify_darwin.go
                // | Condition                               | Events                   |
                // | --------------------------------------- | ------------------------ |
                // | creating a file in a directory          | NOTE_WRITE               |
                // | creating a directory in a directory     | NOTE_WRITE NOTE_LINK     |
                // | creating a link in a directory          | NOTE_WRITE               |
                // | creating a symlink in a directory       | NOTE_WRITE               |
                // | removing a file from a directory        | NOTE_WRITE               |
                // | removing a directory from a directory   | NOTE_WRITE NOTE_LINK     |
                // | renaming a file within a directory      | NOTE_WRITE               |
                // | renaming a directory within a directory | NOTE_WRITE               |
                // | moving a file out of a directory        | NOTE_WRITE               |
                // | moving a directory out of a directory   | NOTE_WRITE NOTE_LINK     |
                // | writing to a file                       | NOTE_WRITE NOTE_EXTEND   |
                // | truncating a file                       | NOTE_ATTRIB              |
                // | overwriting a symlink                   | NOTE_DELETE, NOTE_RENAME |

                // NOTE: The control over the event loop with kevent is nice, but the info returned absolutely sucks.
                // There is no simple "file created" or "file deleted" event to respond to
                // If you create a folder in Finder, you get a NOTE_WRITE|NOTE_LINK event
                // If you move a folder to the Trash, you get a NOTE_WRITE|NOTE_LINK event...?
                // If you `rm -R` a folder, you get a NOTE_WRITE|NOTE_LINK event, not a NOTE_DELETE event???
                // kevents are effectively triggers for doing your own polling
                struct kevent* e = event_data + i;
                XFILES_ASSERT(e->filter == EVFILT_VNODE);

                int cached_path_idx = _xfiles_watch_find_listener_by_fd(&ctx, e->ident);
                XFILES_ASSERT(cached_path_idx != -1);

                const struct XFWatchHandles* wh      = ctx.handles + cached_path_idx;
                const char*                  ev_path = ctx.stringpool + wh->stringpool_offset;

                bool previously_existed = cached_path_idx != -1;
                bool currently_exists   = xfiles_exists(ev_path);

                // struct XFWatchHandles(*view_folders)[512] = (void*)ctx.handles;

                if (previously_existed && !currently_exists)
                {
                    XFILES_ASSERT(_xfiles_watch_find_listener_by_fd(&ctx, e->ident) != -1);

                    // Remove watch listeners for every item in directory & subdirs
                    if (wh->is_dir)
                    {
                        // We don't get "rename" events for deleted/renamed directories contents, so we have to remove
                        // them from our data structure ourselves
                        int N = ctx.handles_len;
                        for (int j = N; j-- > 0;)
                        {
                            // if a (directory) is substring of b (subdir or file), remove b
                            const char* candidate_path = ctx.stringpool + ctx.handles[j].stringpool_offset;
                            const char* a              = ev_path;
                            const char* b              = candidate_path;
                            while (*a == *b && *a != 0 && *b != 0)
                            {
                                a++;
                                b++;
                            }

                            bool is_substring = *a == 0 && *b != 0;
                            if (is_substring) // must be child directory or file
                            {
                                XFILES_ASSERT(!xfiles_exists(candidate_path));
                                _xfiles_watch_remove_listener_at_index(&ctx, j);
                                ctx.callback(XFILES_WATCH_DELETED, candidate_path, ctx.udata);
                            }
                        }
                    } // !is_dir
                    _xfiles_watch_remove_listener_at_index(&ctx, cached_path_idx);
                    ctx.callback(XFILES_WATCH_DELETED, ev_path, ctx.udata);
                }

                if (previously_existed && currently_exists)
                {
                    if (e->fflags & NOTE_WRITE)
                    {
                        ctx.callback(XFILES_WATCH_MODIFIED, ev_path, ctx.udata);

                        // Dir was modified. A new file may have been added
                        if (wh->is_dir)
                            xfiles_list(ev_path, &ctx, _xfiles_watch_cb_poll_for_new_entries);
                    }
                    // if (e->fflags & NOTE_ATTRIB) // do we need an attrib event?
                    //     ctx.callback(XFILES_WATCH_MODIFIED, ev_path, ctx.udata);
                }
            }
        }

        loop = ctx.callback(XFILES_WATCH_CONTINUE, "", ctx.udata);
    }
    while (loop);

    close(kq);

    size_t nevents  = ctx.events_len;
    size_t nfolders = ctx.handles_len;
    XFILES_ASSERT(nevents == nfolders);
    for (int i = nevents; i-- > 0;)
    {
        const char* path = ctx.stringpool + ctx.handles[i].stringpool_offset;
        _xfiles_watch_remove_listener_at_index(&ctx, i);
    }
    XFILES_FREE(ctx.events);
    XFILES_FREE(ctx.handles);
    XFILES_FREE(ctx.stringpool);
}

#ifdef __OBJC__
#import <AppKit/NSWorkspace.h>

bool xfiles_trash(const char* path)
{
    NSString* str     = [[NSString alloc] initWithUTF8String:path];
    NSURL*    itemUrl = [NSURL fileURLWithPath:str isDirectory:FALSE];

    const NSFileManager* fm           = [NSFileManager defaultManager]; // strong
    NSURL*               resultingURL = NULL;
    NSError*             error        = NULL;

    BOOL ok = NO;

    // Apparently a long standing bug in macOS is that this function will not give you the give 'Put Back' feature
    // inside Trash that you would normally see if you deleted the file using the Finder app. Such a shame!
    // https://openradar.appspot.com/radar?id=5063396789583872
    ok = [fm trashItemAtURL:itemUrl resultingItemURL:&resultingURL error:&error];

#if !__has_feature(objc_arc)
    [fm release];
    [itemUrl release];
    [str release];
#endif
    return ok;
}

bool xfiles_open_file_explorer(const char* path)
{
    BOOL ok = [[NSWorkspace sharedWorkspace] openFile:@(path) withApplication:@"Finder"];
    XFILES_ASSERT(ok);
    return ok;
}

bool xfiles_select_in_file_explorer(const char* path)
{
    // https://developer.apple.com/documentation/appkit/nsworkspace/1524399-selectfile
    [[NSWorkspace sharedWorkspace] selectFile:@(path) inFileViewerRootedAtPath:@("")];
    return true;
}

int xfiles_get_user_directory(char* out, size_t outlen, enum XFILES_USER_DIRECTORY loc)
{
    static const char* PATHS[] = {
        "",                             // XFILES_USER_DIRECTORY_HOME,
        "/Library/Application Support", // XFILES_USER_DIRECTORY_APPDATA
        "/Desktop",                     // XFILES_USER_DIRECTORY_DESKTOP
        "/Documents",                   // XFILES_USER_DIRECTORY_DOCUMENTS
        "/Downloads",                   // XFILES_USER_DIRECTORY_DOWNLOADS
        "/Music",                       // XFILES_USER_DIRECTORY_MUSIC
        "/Pictures",                    // XFILES_USER_DIRECTORY_PICTURES
        "/Movies",                      // XFILES_USER_DIRECTORY_VIDEOS
    };
    _Static_assert(XFILES_ARRLEN(PATHS) == XFILES_USER_DIRECTORY_COUNT);

    // I don't think calling this function touches the reference count, but then I haven't looked at the binary
    // https://developer.apple.com/documentation/foundation/1413045-nshomedirectory
    NSString*   nsstr   = NSHomeDirectory();
    const char* homebuf = [nsstr UTF8String];
    size_t      homelen = strlen(homebuf);
    const char* subdir;
    size_t      subdirlen;

    if (loc < 0)
        loc = 0;
    if (loc >= XFILES_USER_DIRECTORY_COUNT)
        loc = XFILES_USER_DIRECTORY_COUNT - 1;

    subdir    = PATHS[loc];
    subdirlen = strlen(subdir);

    XFILES_ASSERT((homelen + subdirlen + 1) <= outlen);
    if ((homelen + subdirlen + 1) > outlen)
        return 0;

    memcpy(out, homebuf, homelen);
    memcpy(out + homelen, subdir, subdirlen);
    out[homelen + subdirlen] = '\0';

    return homelen + subdirlen;
}

#endif // __OBJC__
#endif // __APPLE__

const char* xfiles_get_name(const char* path)
{
    const char* name = path;
    for (const char* c = path; *c != '\0'; c++)
        if (*c == XFILES_DIR_CHAR)
            name = c + 1;
    return name == path ? NULL : name;
}

const char* xfiles_get_extension(const char* name)
{
    const char* extension = name;
    for (const char* c = name; *c != '\0'; c++)
        if (*c == '.')
            extension = c;
    return extension == name ? NULL : extension;
}

bool xfiles_create_directory_recursive(const char* path)
{
    if (xfiles_exists(path))
        return true;

    char nextpath[1024];
    int  i;
    nextpath[0] = path[0];
#ifdef _WIN32
    // eg: "C:\\Users\\username", start at "U"
    nextpath[1] = path[1];
    nextpath[2] = path[2];
    i           = 3;
#else
    // eg "/Users/username", start at "U"
    i = 1;
#endif
    for (; path[i] != '\0'; i++)
    {
        nextpath[i] = path[i];
        if (nextpath[i] == XFILES_DIR_CHAR)
        {
            nextpath[i] = '\0';
            if (!xfiles_exists(nextpath))
                xfiles_create_directory(nextpath);
            XFILES_ASSERT(xfiles_exists(nextpath));
            nextpath[i] = XFILES_DIR_CHAR;
        }
    }
    bool ok = xfiles_create_directory(path);
    XFILES_ASSERT(ok);
    return ok;
}

#endif // XHL_FILES_IMPL
#endif // XHL_FILES_H