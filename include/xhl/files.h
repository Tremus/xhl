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
 * From what I've read around the internet, Windows File Explorer still doesn't support long paths. Users of your
 * software likely won't have or need any long paths. This may change in future.
 * https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file#maximum-path-length-limitation
 *
 * FEATURES:
 * - file read/write/append
 * - file/folder exists Y/N
 * - file/folder rename/move
 * - folder create
 * - file/folder move to bin
 * - file/folder delete
 * - open folders in Finder/Explorer (opt. with preselected file/folder)
 * - folder scan. Useful for custom search algorithms
 * - get platform specific paths eg. \AppData\, /Application Support/, {user}/Desktop, etc.
 * - file watching. Useful for devtools like recompiling code on change, or maintaining an accurate file tree
 *
 * BUILDING:
 * Simply #define XHL_FILES_IMPL in one of your build targets before including this header
 * MacOS: Requires Objective-C/Objective-C++ for some functions. Supports both ARC and no ARC
 *
 * LINKING:
 * Windows: Kernel32 Shell32 Shlwapi
 * macOS: -framework AppKit (or -framework CoreServices if you don't need the Objective-C functions)
 *
 * CREDITS:
 * Randy Gaul. 'xfiles_list' is a modified version of 'cf_scan' from the cute_files library
 * https://github.com/RandyGaul/cute_headers_deprecated/blob/master/cute_files.h
 */

// Example program 1: File watching
#if 0
#define XHL_FILES_IMPL

#include <signal.h>
#include <stdio.h>
#include <xhl/files.h>

int  g_running = 1;
void ctrl_c_callback(int code)
{
    fprintf(stderr, "Terminating\n");
    g_running = 0;
}

void cb_onfilechange(const XFilesWatchEvent* event, void* udata)
{
    const char* kind = event->is_dir ? "folder" : "file";
    switch (event->type)
    {
    case XFILES_WATCH_CREATED:
        fprintf(stderr, "Created %s %s\n", kind, event->path);
        break;
    case XFILES_WATCH_DELETED:
        fprintf(stderr, "Deleted %s %s\n", kind, event->path);
        break;
    case XFILES_WATCH_MODIFIED:
        fprintf(stderr, "Modified %s %s\n", kind, event->path);
        // Recompile program?
        // Recompile shader?
        // Note that if you are modifying files in an IDE with a linter for formatter, you will likely get multiple
        // 'modified' callbacks. If you're hoping to recompile code, you may want to write your own throttle for
        // whatever actions you make in response
        break;
    case XFILES_WATCH_OVERFLOW:
        fprintf(stderr, "Oh dear, we lost some events. Please rescan %s\n", event->path);
        break;
    case default:
        break;
    }
}

int main()
{
    fprintf(stderr, "Press Crtl+C to exit\n");
    g_running = 1;
    signal(SIGINT, ctrl_c_callback);
    // setup event queue
    XFilesWatchContext* ctx = xfiles_watch_create("/path/to/directory", NULL, cb_onfilechange);
    while (g_running)
    {
        xfiles_watch_flush(ctx); // poll for items in queue, trigger cb_onfilechange()
        Sleep(50); // Windows, sleep 50ms
        usleep(50000) // Unix, sleep 50ms
    }
    xfiles_watch_destroy(ctx); // cleanup

    return 0;
}
#endif // Example program 1: File watching

// Example program 2: Recursive file searching
#if 0
#define XHL_FILES_IMPL
#include <stdio.h>
#include <xhl/files.h>

void cb_onfile(void* data, const xfiles_list_item_t* item)
{
    const char* name = item->path + item->name_idx;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return;

    fprintf(stderr, "Entry: %s\n", item->path);
    if (item->is_dir)
    {
        xfiles_list(item->path, data, cb_onfile);
    }
    else
    {
        int* p_file_counter = data;
        (*p_file_counter)++;
    }
}

int main()
{
    int file_counter = 0;
    xfiles_list("/path/to/directory", &file_counter, cb_onfile);
    fprintf(stderr, "Found %d files\n", file_counter);
    return 0;
}
#endif // Example program 2: Recursive file searching

#ifndef XHL_FILES_H
#define XHL_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
// Returns true if directory, false in any other case
bool xfiles_is_directory(const char* path);
// Returns true if directory was created, or if it already exists
bool xfiles_create_directory(const char* path);
// Creates directory and any required parent directories, then returns true if the exists or was craeted.
bool xfiles_create_directory_recursive(const char* path);

// Returns only true on success and sets 'out' and 'outlen' with file contents and size
// Must release 'out' with xfiles_free()
bool xfiles_read(const char* path, void** out, size_t* outlen);
// Frees a file's contents allocated with xfiles_read()
void xfiles_free(void* data);

typedef struct XFilesStream
{
    union
    {
        void* _win32_HANDLE;
        int   _posix_fd;
    };
    uint64_t size;
    uint64_t pos;
    bool     is_open;
    int      error;
} XFilesStream;

typedef enum XFilesSeekOrigin
{
    XFILES_SEEK_BEGIN,   // relative to the start of the file
    XFILES_SEEK_CURRENT, // relative to the current position
    XFILES_SEEK_END,     // relative to the end of the file
} XFilesSeekOrigin;

// Opens a file for streamed reading. Returns NULL on failure. Release with xfiles_stream_close()
XFilesStream xfiles_stream_open(const char* path);
void         xfiles_stream_close(XFilesStream* stream);

// -1 on error. 0 on EOF, else number of bytes actually read. Blocking
int64_t xfiles_stream_read(XFilesStream* stream, void* out, size_t num_bytes);
bool    xfiles_stream_seek(XFilesStream* stream, int64_t offset, XFilesSeekOrigin origin);

// Creates file if it doesn't exist with default access permissions.
// If file already exists, it overwrites all contents.
bool xfiles_write(const char* path, const void* in, size_t inlen);
// Creates file if it doesn't exist with default access permissions.
// Writes data starting from the end of a file, leaving old contents intact
bool xfiles_append(const char* path, const char* in, size_t inlen);
typedef enum XFilesMoveFlags
{
    XFILES_MOVE_DEFAULT = 0, // Fails if 'to' already exists
    // Replaces 'to' if it's a file. Atomic when 'from' and 'to' are on the same volume
    // macOS will also replace 'to' if both are folders and 'to' is empty. Windows never replaces folders
    XFILES_MOVE_OVERWRITE = 1 << 0,
} XFilesMoveFlags;
// Renames / moves file or folder
// You are advised to check for path collisions using xfiles_exists() beforehand
bool xfiles_move(const char* from, const char* to, XFilesMoveFlags flags);

// Moves the file to:
// Win: Recycle Bin /
// OSX: Trash
bool xfiles_trash(const char* path);
// No bins. File is deleted and is likely unrecoverable
bool xfiles_delete(const char* path);
// Opens OS file or folder as if it was opened with
// Win: File Explorer /
// OSX: Finder
// NOTE: this is a blocking call
bool xfiles_open_file_explorer(const char* path);
// Opens OS file browsing app with path selected
// Win: File Explorer /
// OSX: Finder
// NOTE: this is a blocking call
bool xfiles_select_in_file_explorer(const char* path);

typedef enum XFilesUserDirectory
{
    // Windows: {letter}:\\Users\\{username}
    // macOS: /Users/{username}
    XFILES_USER_DIRECTORY_HOME,
    // Windows: [HOME]\\AppData\\Roaming
    // macOS: [HOME]/Library/Application\ Support
    XFILES_USER_DIRECTORY_APPDATA,
    XFILES_USER_DIRECTORY_DESKTOP,
    XFILES_USER_DIRECTORY_DOCUMENTS,
    XFILES_USER_DIRECTORY_DOWNLOADS,
    XFILES_USER_DIRECTORY_MUSIC,
    XFILES_USER_DIRECTORY_PICTURES,
    XFILES_USER_DIRECTORY_VIDEOS,
    XFILES_USER_DIRECTORY_COUNT,
} XFilesUserDirectory;

// Returns number of bytes written to 'char* out', excluding the null terminating byte
int xfiles_get_user_directory(char* out, size_t outlen, XFilesUserDirectory loc);

typedef enum XFilesMetadataType
{
    XFILES_METADATA_TYPE_UNKNOWN = 0,
    XFILES_METADATA_TYPE_FILE,
    XFILES_METADATA_TYPE_DIRECTORY,
    XFILES_METADATA_TYPE_SYMLINK,
    XFILES_METADATA_TYPE_BLOCK_DEVICE,
    XFILES_METADATA_TYPE_CHAR_DEVICE,
    XFILES_METADATA_TYPE_FIFO,
    XFILES_METADATA_TYPE_SOCKET
} XFilesMetadataType;

typedef struct XFilesMetadata
{
    int                last_error; // errno on macOS, GetLastError() on Windows
    XFilesMetadataType type;

    uint64_t size_bytes;

    // Convenience
    bool is_readonly;
    bool is_hidden;
    bool is_system;
    bool is_archive;    // Windows only, its for backing up files
    bool is_compressed; // (UF_COMPRESSED, FILE_ATTRIBUTE_COMPRESSED). Not to be confused with zip/7z/tar/gz/rar
    bool is_encrypted;
    bool is_executable; // +x user permissions on posix. Lazy extension checking on Windwos
    bool is_symlink;

    // Unix epoch times in nanoseconds
    uint64_t creation_time_ns;      // File creation time (birthtime / CreationTime)
    uint64_t modification_time_ns;  // Last write time (mtime / LastWriteTime)
    uint64_t access_time_ns;        // Last access time (atime / LastAccessTime)
    uint64_t status_change_time_ns; // POSIX ctime (macOS only, 0 on Windows)

    // Stuff you probably dont want

    // Might get rid of this. Can't imagine a use for it
    uint64_t file_index;           // (st_ino / (nFileIndexHigh << 32 | nFileSizeLow))
    uint32_t num_links;            // (st_nlink / nNumberOfLinks)
    uint32_t volume_serial_number; // (st_dev / dwVolumeSerialNumber)

    // Windows fields you might want
    uint32_t _dwFileAttributes; // BY_HANDLE_FILE_INFORMATION.dwFileAttributes
    uint32_t _dwReparseTag;     // FILE_ATTRIBUTE_TAG_INFO.ReparseTag

    // POSIX fields (stat64) you might want
    uint32_t _st_mode;    // File mode bits (POSIX)
    uint32_t _st_blksize; // Preferred I/O block size
    uint64_t _st_blocks;  // Number of 512-byte blocks allocated
    uint32_t _st_flags;   // User-defined flags (macOS BSD flags)
    uint32_t _st_gen;     // File generation number (macOS)
} XFilesMetadata;

XFilesMetadata xfiles_get_metadata(const char* path);

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

// NOTE: Treat these events as hints, not a perfect log. Both OSes coalesce and occasionally reorder events, so you may
// see a DELETED for a path you never saw created, or a MODIFIED for a file you have never seen before. If your app
// needs an exact file tree, rescan the affected folder when you receive an event for it.
enum XFilesWatchType
{
    XFILES_WATCH_UNKNOWN = 0,
    // Created, moved into the watched folder from somewhere else, or the new path of a rename
    // When a folder is renamed you get DELETED + CREATED for the folder only, not for each of its children. If you're
    // tracking a file tree, rescan the new folder
    XFILES_WATCH_CREATED,
    // Deleted, moved out of the watched folder (eg. to the Trash/Recycle Bin), or the old path of a rename
    XFILES_WATCH_DELETED,
    // File path stays the same, contents or metadata was modified in some way
    // Many editors save files 'atomically' by writing a temp file then renaming it over the original. On macOS these
    // saves arrive as MODIFIED for the saved file, with DELETED for the temp file. On Windows expect some mix of
    // DELETED, CREATED and MODIFIED for the saved file
    XFILES_WATCH_MODIFIED,
    // Events were lost and you should rescan 'path'. On Windows this happens when more than 64kb of events arrive
    // between flushes. On macOS this happens when FSEvents drops events, the watched folder itself is moved or
    // deleted, or more events arrive between flushes than fit in our buffer.
    XFILES_WATCH_OVERFLOW,
};
typedef struct XFilesWatchEvent
{
    enum XFilesWatchType type;
    const char*          path;
    // Windows can't inspect a path that no longer exists, so this is always false for DELETED events on Windows
    bool is_dir;
} XFilesWatchEvent;
// 'event' and its strings are only valid for the duration of the callback
typedef void (*XFilesWatchCallback)(const XFilesWatchEvent* event, void* udata);
typedef struct XFilesWatchContext XFilesWatchContext;

// Sets up an event queue for file change notifications
XFilesWatchContext* xfiles_watch_create(const char* path, void* udata, XFilesWatchCallback cb);
// Process all events in the queue
void xfiles_watch_flush(XFilesWatchContext* ctx);
void xfiles_watch_destroy(XFilesWatchContext* ctx);

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

bool xfiles_is_directory(const char* path)
{
    bool  is_directory = false;
    DWORD attr         = 0;
    WCHAR WPath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, WPath, ARRAYSIZE(WPath));
    if (num)
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileattributesw
        attr = GetFileAttributesW(WPath);
    }
    if ((attr & FILE_ATTRIBUTE_DIRECTORY) && attr != INVALID_FILE_ATTRIBUTES)
        is_directory = true;
    return is_directory;
}

bool xfiles_create_directory(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectoryw
    WCHAR DirPath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, DirPath, XFILES_ARRLEN(DirPath));
    XFILES_ASSERT(num);
    if (num)
    {
        BOOL ret = CreateDirectoryW(DirPath, 0);

        if (ret) // any non-zero is a success
        {
            return true;
        }
        else
        {
            DWORD err = GetLastError();
            if (err == ERROR_ALREADY_EXISTS)
                return true; // close enough to "created a directory"
            if (err == ERROR_PATH_NOT_FOUND)
                return false;
        }
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
        if (hFile == INVALID_HANDLE_VALUE)
        {
            int err = GetLastError();
            if (err == ERROR_ACCESS_DENIED)
            {
                // NOTE: you may get this error when trying to "read" a directory. Check if a directory already exists
                // at this path
            }
            XFILES_ASSERT(hFile != INVALID_HANDLE_VALUE);
        }
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
                    xfiles_free(data);
                    data              = NULL;
                    FileSize.QuadPart = 0;
                }
            }
            ok &= CloseHandle(hFile);
            XFILES_ASSERT(ok);
        }
    }

    return ok;
}

XFilesStream xfiles_stream_open(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfilesizeex

    XFilesStream stream = {._win32_HANDLE = INVALID_HANDLE_VALUE};

    WCHAR FileName[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FileName, XFILES_ARRLEN(FileName));
    XFILES_ASSERT(num);
    if (num)
    {
        DWORD dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
        stream._win32_HANDLE =
            CreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, dwFlagsAndAttributes, NULL);
        XFILES_ASSERT(stream._win32_HANDLE != INVALID_HANDLE_VALUE);
    }

    if (stream._win32_HANDLE != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize = {0};
        BOOL          ok       = GetFileSizeEx(stream._win32_HANDLE, &FileSize);
        XFILES_ASSERT(ok);
        if (ok)
        {
            stream.size = (uint64_t)FileSize.QuadPart;
        }
        else
        {
            ok = CloseHandle(stream._win32_HANDLE);
            XFILES_ASSERT(ok);
            stream._win32_HANDLE = INVALID_HANDLE_VALUE;
        }
    }
    else
    {
        stream.error = GetLastError();
    }
    stream.is_open = stream._win32_HANDLE != INVALID_HANDLE_VALUE;

    return stream;
}

void xfiles_stream_close(XFilesStream* stream)
{
    if (stream->is_open)
    {
        BOOL ok = CloseHandle(stream->_win32_HANDLE);
        XFILES_ASSERT(ok);
        if (ok)
        {
            stream->_win32_HANDLE = INVALID_HANDLE_VALUE;
            stream->is_open       = false;
        }
    }
}

int64_t xfiles_stream_read(XFilesStream* stream, void* out, size_t nbytes)
{
    if (!stream->is_open)
        return -1;

    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
    DWORD nread = 0;
    BOOL  ok    = ReadFile(stream->_win32_HANDLE, out, (DWORD)nbytes, &nread, NULL);
    XFILES_ASSERT(ok);
    if (!ok)
        return -1;

    stream->pos += nread;
    return (int64_t)nread;
}

bool xfiles_stream_seek(XFilesStream* stream, int64_t offset, XFilesSeekOrigin origin)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointerex
    _Static_assert(FILE_BEGIN == XFILES_SEEK_BEGIN, "");
    _Static_assert(FILE_CURRENT == XFILES_SEEK_CURRENT, "");
    _Static_assert(FILE_END == XFILES_SEEK_END, "");

    LARGE_INTEGER liOffset;
    LARGE_INTEGER liNewPos = {0};
    liOffset.QuadPart      = offset;

    BOOL ok = SetFilePointerEx(stream->_win32_HANDLE, liOffset, &liNewPos, origin);
    XFILES_ASSERT(ok);
    if (ok)
        stream->pos = (uint64_t)liNewPos.QuadPart;
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
            FILE_SHARE_READ,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            int err = GetLastError();
            if (err == ERROR_ACCESS_DENIED)
            {
                // NOTE: you may get this error when trying to "write" to a directory. Check if a directory already
                // exists at this path
            }
            XFILES_ASSERT(hFile != INVALID_HANDLE_VALUE);
        }
        if (hFile != INVALID_HANDLE_VALUE)
        {
            ok = WriteFile(hFile, in, inlen, &nBytesWritten, NULL);
            XFILES_ASSERT(ok);
            ok &= CloseHandle(hFile);
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
            ok &= CloseHandle(hFile);
            XFILES_ASSERT(ok);
        }
    }

    return ok;
}

bool xfiles_move(const char* from, const char* to, XFilesMoveFlags flags)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefilew
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw
    WCHAR FromPath[MAX_PATH];
    WCHAR ToPath[MAX_PATH];
    int   num1 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, from, -1, FromPath, XFILES_ARRLEN(FromPath));
    int   num2 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, to, -1, ToPath, XFILES_ARRLEN(ToPath));
    if (num1 && num2)
    {
        // In the case that the destination is on a different volume to the source, the file will be copied to the new
        // volume, and deleted afterwards. This is much safer, "cut & pasting" big files to/from an external drive on
        // Windows is an infamous way to lose important data
        DWORD dwFlags = MOVEFILE_COPY_ALLOWED;
        // Never replaces a folder, even with this flag
        if (flags & XFILES_MOVE_OVERWRITE)
            dwFlags |= MOVEFILE_REPLACE_EXISTING;
        return MoveFileExW(FromPath, ToPath, dwFlags);
    }
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
    WCHAR FilePath[MAX_PATH];
    int   num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath));
    XFILES_ASSERT(num);
    if (num)
    {
        // ShellExecuteW depends on Windows registry to find what application to use to with the verb "open"
        // It's possible for the registry to be poisened by 3rd party software or manual tweaking
        // This happened to me after downloading FilePilot, setting it as the default file browser, then uninstalling
        // it. After doing this ShellExecuteW would always return 'SE_ERR_NOASSOC'
        // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew
        INT_PTR ret = (INT_PTR)ShellExecuteW(NULL, L"open", FilePath, NULL, NULL, SW_SHOWDEFAULT);

        if (ret == SE_ERR_NOASSOC) // Oh dear, Windows can't figure out what application to use!?
        {
            // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw
            // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/ns-shellapi-shellexecuteinfow
            // https://learn.microsoft.com/en-us/windows/win32/shell/launch
            SHELLEXECUTEINFOW sei = {0};
            sei.cbSize            = sizeof(SHELLEXECUTEINFOW);
            sei.fMask             = 0;
            sei.lpVerb            = L"open";
            sei.lpFile            = L"explorer.exe"; // Explicitly use File Explorer
            sei.lpParameters      = FilePath;
            sei.nShow             = SW_SHOW;

            BOOL ok = ShellExecuteExW(&sei);

            ret = (INT_PTR)sei.hInstApp;
        }
        XFILES_ASSERT(ret > 32);

        return ret > 32;
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

            return hres == S_OK;
        }
    }
    return false;
}

int xfiles_get_user_directory(char* out, size_t outlen, XFilesUserDirectory loc)
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
        loc = (XFilesUserDirectory)0;
    if (loc >= XFILES_USER_DIRECTORY_COUNT)
        loc = (XFilesUserDirectory)(ARRAYSIZE(FOLDER_IDS) - 1);
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

// Mostly LLM generated. GLWTS
XFilesMetadata xfiles_get_metadata(const char* path)
{
    XFilesMetadata meta = {0};

    if (path == NULL || path[0] == '\0')
    {
        meta.last_error = -1;
        return meta;
    }

    // Open file
    HANDLE hFile = INVALID_HANDLE_VALUE;
    {
        WCHAR WPath[MAX_PATH] = {0};
        int   wlen            = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, WPath, ARRAYSIZE(WPath));
        if (wlen == 0)
        {
            meta.last_error = (int)GetLastError();
            return meta;
        }
        // Open with FILE_FLAG_BACKUP_SEMANTICS so directories also work,
        // and FILE_FLAG_OPEN_REPARSE_POINT so we stat the symlink itself.
        // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
        hFile = CreateFileW(
            WPath,
            FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
            NULL);
    }

    if (hFile == INVALID_HANDLE_VALUE)
    {
        meta.last_error = (int32_t)GetLastError();
        return meta;
    }

    BY_HANDLE_FILE_INFORMATION info;
    BOOL                       ok = GetFileInformationByHandle(hFile, &info);
    if (!ok)
    {
        meta.last_error = (int32_t)GetLastError();
        CloseHandle(hFile);
        return meta;
    }

    meta.size_bytes = ((uint64_t)info.nFileSizeHigh << 32) | (uint64_t)info.nFileSizeLow;

    // Convert FILETIME (100-ns intervals since 1601-01-01 UTC) to ns since 1970-01-01 UTC
    {
        // 116444736000000000 = number of 100ns ticks between 1601-01-01 and 1970-01-01
        const uint64_t EPOCH_DIFF_100NS = 116444736000000000ULL;

        ULARGE_INTEGER c, m, a;
        c.LowPart  = info.ftCreationTime.dwLowDateTime;
        c.HighPart = info.ftCreationTime.dwHighDateTime;
        m.LowPart  = info.ftLastWriteTime.dwLowDateTime;
        m.HighPart = info.ftLastWriteTime.dwHighDateTime;
        a.LowPart  = info.ftLastAccessTime.dwLowDateTime;
        a.HighPart = info.ftLastAccessTime.dwHighDateTime;

        meta.creation_time_ns      = (uint64_t)((c.QuadPart - EPOCH_DIFF_100NS) * 100ULL);
        meta.modification_time_ns  = (uint64_t)((m.QuadPart - EPOCH_DIFF_100NS) * 100ULL);
        meta.access_time_ns        = (uint64_t)((a.QuadPart - EPOCH_DIFF_100NS) * 100ULL);
        meta.status_change_time_ns = 0; // no direct POSIX ctime equivalent on Windows
    }

    meta.file_index           = ((uint64_t)info.nFileIndexHigh << 32) | (uint64_t)info.nFileIndexLow;
    meta.num_links            = info.nNumberOfLinks;
    meta.volume_serial_number = info.dwVolumeSerialNumber;
    meta._dwFileAttributes    = info.dwFileAttributes;

    // https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-points
    if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
    {
        // Get the reparse tag to decide if it is really a symlink
        // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getfileinformationbyhandleex
        // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ne-minwinbase-file_info_by_handle_class
        FILE_ATTRIBUTE_TAG_INFO tag_info;
        ok = GetFileInformationByHandleEx(hFile, FileAttributeTagInfo, &tag_info, sizeof(tag_info));
        if (ok)
        {
            meta._dwReparseTag = tag_info.ReparseTag;
            if (tag_info.ReparseTag == IO_REPARSE_TAG_SYMLINK || tag_info.ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
            {
                meta.type       = XFILES_METADATA_TYPE_SYMLINK;
                meta.is_symlink = true;
            }
            else
            {
                meta.type = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? XFILES_METADATA_TYPE_DIRECTORY
                                                                               : XFILES_METADATA_TYPE_FILE;
            }
        }
    }
    else if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        meta.type = XFILES_METADATA_TYPE_DIRECTORY;
    }
    else
    {
        meta.type = XFILES_METADATA_TYPE_FILE;
    }

    meta.is_readonly   = (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    meta.is_hidden     = (info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    meta.is_system     = (info.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) != 0;
    meta.is_archive    = (info.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0;
    meta.is_compressed = (info.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0;
    meta.is_encrypted  = (info.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) != 0;
    meta.is_executable = false; // Windows uses ACLs, not a simple bit; best-effort below
    {
        // Treat .exe/.bat/.cmd/.com as executable as a convenience heuristic
        const char* ext = xfiles_get_extension(path);
        int         c   = ext != NULL ? ext[1] : 0;
        if (c == 'e' || c == 'E' || c == 'b' || c == 'B' || c == 'c' || c == 'C')
        {
            ext++; // increment past "."
            if (_stricmp(ext, "exe") == 0 || _stricmp(ext, "bat") == 0 || _stricmp(ext, "cmd") == 0 ||
                _stricmp(ext, "com") == 0)
            {
                meta.is_executable = true;
            }
        }
    }

    CloseHandle(hFile);
    return meta;
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
        int   n =
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, PathUnicode, XFILES_ARRLEN(PathUnicode) - 3);
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

typedef struct XFilesWatchContext
{
    HANDLE hDirectory;
    // https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-overlapped
    OVERLAPPED overlapped;

    // 64KB is max size ReadDirectoryChangesW supports. This only reduces the likelihood of loosing file events. By
    // design it cannot be prevented on Windows (to my knowledge)
    BYTE buffer[1024 * 64];

    int  pathlen;
    char path[MAX_PATH];

    void*               udata;
    XFilesWatchCallback callback;
} XFilesWatchContext;

void _xfiles_watch_init_overlapped(XFilesWatchContext* ctx)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-readdirectorychangesw
    BOOL  bWatchSubtree  = TRUE;
    DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                           // FILE_NOTIFY_CHANGE_ATTRIBUTES |
                           FILE_NOTIFY_CHANGE_LAST_WRITE;
    BOOL ok = ReadDirectoryChangesW(
        ctx->hDirectory,
        ctx->buffer,
        sizeof(ctx->buffer),
        bWatchSubtree,
        dwNotifyFilter,
        NULL,
        &ctx->overlapped,
        NULL);
    XFILES_ASSERT(ok);
}

XFilesWatchContext* xfiles_watch_create(const char* path, void* udata, XFilesWatchCallback cb)
{
    XFilesWatchContext* ctx = (XFilesWatchContext*)XFILES_MALLOC(sizeof(*ctx));

    WCHAR wPath[MAX_PATH] = {0};
    int   num             = 0;

    memset(ctx, 0, sizeof(*ctx));

    ctx->hDirectory = INVALID_HANDLE_VALUE;
    ctx->pathlen    = strlen(path);
    ctx->udata      = udata;
    ctx->callback   = cb;

    // Remove backslash
    if (ctx->pathlen > 0 && (path[ctx->pathlen - 1] == '\\' || path[ctx->pathlen - 1] == '/'))
        ctx->pathlen--;
    snprintf(ctx->path, sizeof(ctx->path), "%.*s", ctx->pathlen, path);

    num = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ctx->path, ctx->pathlen, wPath, XFILES_ARRLEN(wPath));
    XFILES_ASSERT(num);

    ctx->hDirectory = CreateFileW(
        wPath,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL);
    XFILES_ASSERT(ctx->hDirectory != INVALID_HANDLE_VALUE);

    if (ctx->hDirectory != INVALID_HANDLE_VALUE)
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventw
        ctx->overlapped.hEvent = CreateEventW(NULL, FALSE, 0, NULL);
        XFILES_ASSERT(ctx->overlapped.hEvent);

        _xfiles_watch_init_overlapped(ctx);
    }

    // Handle failure
    if (ctx->hDirectory == INVALID_HANDLE_VALUE || ctx->overlapped.hEvent == NULL)
    {
        xfiles_watch_destroy(ctx);
        ctx = NULL;
    }

    return ctx;
}

void xfiles_watch_flush(XFilesWatchContext* _ctx)
{
    XFilesWatchContext* ctx = (XFilesWatchContext*)_ctx;

    if (ctx->overlapped.Internal == STATUS_PENDING)
        return;

    // https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
    DWORD result = WaitForSingleObject(ctx->overlapped.hEvent, 0);

    if (result == WAIT_OBJECT_0)
    {
        DWORD dwNumberOfBytesTransferred = 0;
        DWORD dwOffset                   = 0;

        // https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult
        BOOL bWait   = FALSE;
        BOOL success = GetOverlappedResult(ctx->hDirectory, &ctx->overlapped, &dwNumberOfBytesTransferred, bWait);

        // !success is how we detect we hit the buffer cap.
        // Windows docs tell us to do a full directory rescan when we do that
        // dwNumberOfBytesTransferred == 0 will probably never be hit, but it's a signal Windows may be acting up due to
        // "result == WAIT_OBJECT_0" above, which means we are expecting to recieve data.
        if (!success || dwNumberOfBytesTransferred == 0)
        {
            XFilesWatchEvent event = {0};
            event.type             = XFILES_WATCH_OVERFLOW;
            event.path             = ctx->path;
            event.is_dir           = true;
            ctx->callback(&event, ctx->udata);
        }
        else
        {
            while (dwOffset < dwNumberOfBytesTransferred)
            {
                FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)(ctx->buffer + dwOffset);

                XFilesWatchEvent event = {0};
                event.type             = ((enum XFilesWatchType)0xffffffff); // invalid
                if (pNotify->Action == FILE_ACTION_ADDED || pNotify->Action == FILE_ACTION_RENAMED_NEW_NAME)
                    event.type = XFILES_WATCH_CREATED;
                if (pNotify->Action == FILE_ACTION_REMOVED || pNotify->Action == FILE_ACTION_RENAMED_OLD_NAME)
                    event.type = XFILES_WATCH_DELETED;
                if (pNotify->Action == FILE_ACTION_MODIFIED)
                    event.type = XFILES_WATCH_MODIFIED;

                if (event.type != ((enum XFilesWatchType)0xffffffff))
                {
                    char fp[MAX_PATH] = {0};
                    int  NameLength   = pNotify->FileNameLength / sizeof(wchar_t);

                    int fplen  = snprintf(fp, sizeof(fp), "%.*s" XFILES_DIR_STR, ctx->pathlen, ctx->path);
                    fplen     += WideCharToMultiByte(
                        CP_UTF8,
                        WC_ERR_INVALID_CHARS,
                        pNotify->FileName,
                        NameLength,
                        fp + fplen,
                        sizeof(fp) - fplen,
                        NULL,
                        NULL);

                    event.path = fp;
                    // Deleted paths can't be inspected, so is_dir is always false for them
                    if (event.type != XFILES_WATCH_DELETED)
                        event.is_dir = xfiles_is_directory(fp);
                    ctx->callback(&event, ctx->udata);
                }

                dwOffset += pNotify->NextEntryOffset;
                if (pNotify->NextEntryOffset == 0)
                    break;
            }
        }

        // After events are flushed we need to call ReadDirectoryChangesW() to prepare our event queue again
        _xfiles_watch_init_overlapped(ctx);
    }
}

void xfiles_watch_destroy(XFilesWatchContext* _ctx)
{
    XFilesWatchContext* ctx = (XFilesWatchContext*)_ctx;
    if (ctx->hDirectory != INVALID_HANDLE_VALUE)
    {
        // The kernel may still write into ctx->buffer & ctx->overlapped while a read is pending. Cancel it and wait
        // for the cancellation to complete before freeing them
        if (ctx->overlapped.hEvent && CancelIoEx(ctx->hDirectory, &ctx->overlapped))
        {
            DWORD dwNumberOfBytesTransferred = 0;
            GetOverlappedResult(ctx->hDirectory, &ctx->overlapped, &dwNumberOfBytesTransferred, TRUE);
        }
        CloseHandle(ctx->hDirectory);
    }
    if (ctx->overlapped.hEvent)
        CloseHandle(ctx->overlapped.hEvent);
    XFILES_FREE(ctx);
}

#endif // _WIN32

#ifdef __APPLE__
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/access.2.html
bool xfiles_exists(const char* path) { return access(path, F_OK) == 0; }

bool xfiles_is_directory(const char* path)
{
    bool is_directory = false;

    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html
    struct stat st = {0};
    lstat(path, &st);
    if (S_ISDIR(st.st_mode))
        is_directory = true;
    return is_directory;
}

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/mkdir.2.html
bool xfiles_create_directory(const char* path)
{
    int ret = mkdir(path, 0777);
    if (ret != 0)
    {
        int err = errno;
        XFILES_ASSERT(err == EEXIST);
        if (err == EEXIST)
            return true;
    }
    return ret == 0;
}

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
                xfiles_free(data);
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

XFilesStream xfiles_stream_open(const char* path)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/open.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fstat.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/close.2.html

    XFilesStream stream = {._posix_fd = -1};

    stream._posix_fd = open(path, O_RDONLY);
    XFILES_ASSERT(stream._posix_fd != -1);
    if (stream._posix_fd != -1)
    {
        struct stat info = {0};
        int         ret  = fstat(stream._posix_fd, &info);
        XFILES_ASSERT(ret != -1);
        if (ret != -1)
        {
            stream.size = (uint64_t)info.st_size;
        }
        else
        {
            close(stream._posix_fd);
            stream._posix_fd = -1;
        }
    }
    else
    {
        stream.error = errno;
    }
    stream.is_open = stream._posix_fd >= 0;

    return stream;
}

void xfiles_stream_close(XFilesStream* stream)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/close.2.html
    if (stream->is_open)
    {
        int ret = close(stream->_posix_fd);
        if (ret != -1)
        {
            stream->_posix_fd = -1;
            stream->is_open   = false;
        }
        else
        {
            stream->error = errno;
        }
    }
}

int64_t xfiles_stream_read(XFilesStream* stream, void* out, size_t nbytes)
{
    if (!stream->is_open)
        return -1;
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/read.2.html
    ssize_t nread = read(stream->_posix_fd, out, nbytes);
    XFILES_ASSERT(nread != -1);
    if (nread == -1)
    {
        stream->error = errno;
        return -1;
    }

    stream->pos += (uint64_t)nread;
    return (int64_t)nread;
}

bool xfiles_stream_seek(XFilesStream* stream, int64_t offset, XFilesSeekOrigin origin)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/lseek.2.html
    // NOTE: SEEK_SET, SEEK_CUR & SEEK_END match XFilesSeekOrigin
    _Static_assert(SEEK_SET == XFILES_SEEK_BEGIN, "");
    _Static_assert(SEEK_CUR == XFILES_SEEK_CURRENT, "");
    _Static_assert(SEEK_END == XFILES_SEEK_END, "");

    off_t newpos = lseek(stream->_posix_fd, (off_t)offset, origin);
    XFILES_ASSERT(newpos != -1);
    if (newpos == -1)
        return false;

    stream->pos = (uint64_t)newpos;
    return true;
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
bool xfiles_move(const char* from, const char* to, XFilesMoveFlags flags)
{
    unsigned int rename_flags = (flags & XFILES_MOVE_OVERWRITE) ? 0 : RENAME_EXCL;
    return 0 == renamex_np(from, to, rename_flags);
}

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/unlink.2.html
bool xfiles_delete(const char* path) { return unlink(path) == 0; }

XFilesMetadata xfiles_get_metadata(const char* path)
{
    XFilesMetadata meta = {0};

    if (path == NULL || path[0] == '\0')
    {
        meta.last_error = -1;
        return meta;
    }

    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html
    struct stat st;
    // lstat so symlinks are reported as symlinks
    if (lstat(path, &st) != 0)
    {
        meta.last_error = (int32_t)errno;
        return meta;
    }

    // Unified fields
    meta.size_bytes       = (uint64_t)st.st_size;
    meta.creation_time_ns = (uint64_t)st.st_birthtimespec.tv_sec * 1000000000LL + (uint64_t)st.st_birthtimespec.tv_nsec;
    meta.modification_time_ns  = (uint64_t)st.st_mtimespec.tv_sec * 1000000000LL + (uint64_t)st.st_mtimespec.tv_nsec;
    meta.access_time_ns        = (uint64_t)st.st_atimespec.tv_sec * 1000000000LL + (uint64_t)st.st_atimespec.tv_nsec;
    meta.status_change_time_ns = (uint64_t)st.st_ctimespec.tv_sec * 1000000000LL + (uint64_t)st.st_ctimespec.tv_nsec;
    meta.file_index            = st.st_ino;
    meta.num_links             = st.st_nlink;
    meta.volume_serial_number  = st.st_dev;

    meta._st_mode    = st.st_mode;
    meta._st_blksize = st.st_blksize;
    meta._st_blocks  = st.st_blocks;
    meta._st_flags   = st.st_flags;
    meta._st_gen     = st.st_gen;

    // Determine type
    if (S_ISREG(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_FILE;
    }
    else if (S_ISDIR(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_DIRECTORY;
    }
    else if (S_ISLNK(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_SYMLINK;
    }
    else if (S_ISBLK(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_BLOCK_DEVICE;
    }
    else if (S_ISCHR(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_CHAR_DEVICE;
    }
    else if (S_ISFIFO(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_FIFO;
    }
    else if (S_ISSOCK(st.st_mode))
    {
        meta.type = XFILES_METADATA_TYPE_SOCKET;
    }

    // Convenience boolean flags (BSD-style)
    // st_flags on macOS: UF_HIDDEN, UF_IMMUTABLE, UF_COMPRESSED, etc.
    meta.is_readonly   = ((st.st_mode & 0222) == 0);       // no write bits for anyone
    meta.is_hidden     = (st.st_flags & 0x00008000u) != 0; // UF_HIDDEN
    meta.is_system     = (st.st_flags & 0x00080000u) != 0; // SF_RESTRICTED, rough analogue
    meta.is_archive    = false;                            // No direct analogue on macOS
    meta.is_compressed = (st.st_flags & 0x00000020u) != 0; // UF_COMPRESSED
    meta.is_encrypted  = false;                            // FileVault is volume-level; per-file is not stable here
    meta.is_executable = (st.st_mode & 0111) != 0;
    meta.is_symlink    = S_ISLNK(st.st_mode) ? true : false;

    return meta;
}

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

// File watching using FSEvents. Requires macOS 10.7+
//
// Threading:
// FSEvents delivers on a private serial dispatch queue (the producer). Events are copied into a lock free single
// producer single consumer byte ring. xfiles_watch_flush() is the consumer and fires your callback on the calling
// thread. Neither side ever waits on the other. Don't call flush on the same context from two threads at once.
//
// Ring layout:
// write_idx and read_idx increase forever and wrap naturally as uint32_t. Capacity is a power of two, so
// 'idx & (SIZE - 1)' gives the byte position and 'write_idx - read_idx' gives the bytes in use.
// Each record is [uint32_t flags][uint32_t pathlen][path bytes + '\0'], padded to 8 bytes. Paths are never split
// across the end of the ring: if a record doesn't fit in the tail, a skip record (pathlen == XFILES_WATCH_SKIP)
// fills the tail and the record starts at offset 0. Positions are multiples of 8, so the tail is always >= 8 bytes
// and a skip header always fits.
//
// Interpreting events:
// FSEvents flags are sticky. They describe everything that has happened to a file recently, not just the latest
// change, eg. every write to a new file still has ItemCreated set, and every write to a renamed file still has
// ItemRenamed set. So flags alone can't tell CREATED from MODIFIED. We keep a small cache of hashes of the paths we've
// reported as existing, and fall back to comparing the file's birth time with the time the watch started.
// A rename arrives as one event for the old path (which no longer exists, so DELETED) and one for the new path
// (which isn't in the cache yet, so CREATED).
#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <limits.h>
#include <time.h>

enum
{
    // Must be a power of two. When full, the rest of the incoming batch is dropped and XFILES_WATCH_OVERFLOW is
    // reported on the next flush
    XFILES_WATCH_RING_SIZE     = 1024 * 128,
    XFILES_WATCH_RECORD_HEADER = 8,
    // Apple Silicon cache lines are 128 bytes. Used as padding between producer and consumer owned fields
    XFILES_WATCH_CACHE_LINE = 128,
    // Must be a power of two. Hashes of paths we have reported as existing. Collisions overwrite older entries, which
    // costs us an occasional duplicate CREATED
    XFILES_WATCH_KNOWN_PATHS = 4096,
};
#define XFILES_WATCH_SKIP 0xffffffffu

_Static_assert((XFILES_WATCH_RING_SIZE & (XFILES_WATCH_RING_SIZE - 1)) == 0, "Ring size must be a power of two");
_Static_assert(XFILES_WATCH_RING_SIZE >= 2 * (XFILES_WATCH_RECORD_HEADER + PATH_MAX + 8), "Ring too small");
_Static_assert((XFILES_WATCH_KNOWN_PATHS & (XFILES_WATCH_KNOWN_PATHS - 1)) == 0, "Must be a power of two");

struct XFilesWatchContext
{
    // Producer owned. Written by the FSEvents callback, read by flush
    // Padding (rather than alignment attributes) keeps these on separate cache lines from the consumer fields
    // without requiring XFILES_MALLOC to return 128 byte aligned memory
    uint8_t  _pad0[XFILES_WATCH_CACHE_LINE];
    uint32_t write_idx;
    uint32_t overflowed; // Set by the producer, cleared by the consumer with an atomic exchange
    uint8_t  _pad1[XFILES_WATCH_CACHE_LINE];

    // Consumer owned. Written by flush, read by the FSEvents callback
    uint32_t read_idx;
    uint8_t  _pad2[XFILES_WATCH_CACHE_LINE];

    uint8_t ring[XFILES_WATCH_RING_SIZE];

    // Consumer only. 0 is an empty slot
    uint32_t known_paths[XFILES_WATCH_KNOWN_PATHS];

    // Everything below is written once in create and read only afterwards
    FSEventStreamRef stream;
    // dispatch_queue_t. Stored as void* so this struct stays plain C when compiled with ARC
    void*           queue;
    bool            started;
    struct timespec start_time;

    // realpath() of the watched root. FSEvents reports canonical paths, eg. /var/... arrives as /private/var/...
    int  pathlen;
    char path[PATH_MAX];

    void*               udata;
    XFilesWatchCallback callback;
};

typedef struct XFWatchRecord
{
    uint32_t    flags;
    uint32_t    pathlen;
    const char* path; // Points into the ring
    uint32_t    next; // read_idx of the following record
} XFWatchRecord;

static void _xfiles_watch_fsevents_cb(
    ConstFSEventStreamRef         stream,
    void*                         info,
    size_t                        num_events,
    void*                         event_paths,
    const FSEventStreamEventFlags event_flags[],
    const FSEventStreamEventId    event_ids[])
{
    // https://developer.apple.com/documentation/coreservices/fseventstreamcallback
    struct XFilesWatchContext* ctx = (struct XFilesWatchContext*)info;
    // Plain C strings because we didn't pass kFSEventStreamCreateFlagUseCFTypes
    const char** paths = (const char**)event_paths;
    (void)stream;
    (void)event_ids;

    // We are the only writer of write_idx, so a relaxed load is enough
    // Acquire on read_idx: the consumer must be finished with those bytes before we overwrite them
    uint32_t w = __atomic_load_n(&ctx->write_idx, __ATOMIC_RELAXED);
    uint32_t r = __atomic_load_n(&ctx->read_idx, __ATOMIC_ACQUIRE);

    for (size_t i = 0; i < num_events; i++)
    {
        uint32_t pathlen = (uint32_t)strlen(paths[i]);
        uint32_t reclen  = (XFILES_WATCH_RECORD_HEADER + pathlen + 1 + 7) & ~7u;
        uint32_t pos     = w & (XFILES_WATCH_RING_SIZE - 1);
        uint32_t tail    = XFILES_WATCH_RING_SIZE - pos;
        uint32_t need    = reclen > tail ? tail + reclen : reclen;

        // Check for space. If it looks full, reload read_idx once in case the consumer has caught up
        if (need > XFILES_WATCH_RING_SIZE - (w - r))
        {
            r = __atomic_load_n(&ctx->read_idx, __ATOMIC_ACQUIRE);
            if (need > XFILES_WATCH_RING_SIZE - (w - r))
            {
                __atomic_store_n(&ctx->overflowed, 1, __ATOMIC_RELAXED);
                break;
            }
        }

        // Record doesn't fit before the end of the ring. Fill the tail with a skip record and wrap to 0
        if (reclen > tail)
        {
            uint32_t skip = XFILES_WATCH_SKIP;
            memcpy(ctx->ring + pos + 4, &skip, 4);
            w   += tail;
            pos  = 0;
        }

        {
            uint8_t* rec   = ctx->ring + pos;
            uint32_t flags = (uint32_t)event_flags[i];
            memcpy(rec, &flags, 4);
            memcpy(rec + 4, &pathlen, 4);
            memcpy(rec + XFILES_WATCH_RECORD_HEADER, paths[i], pathlen + 1);
            w += reclen;
        }
    }

    // Publish the whole batch at once. Release: record bytes become visible before the new write_idx
    __atomic_store_n(&ctx->write_idx, w, __ATOMIC_RELEASE);
}

// Reads the record at 'r', stepping over a skip record if there is one. Returns false when there are no more records
static bool _xfiles_watch_read_record(const struct XFilesWatchContext* ctx, uint32_t r, uint32_t w, XFWatchRecord* rec)
{
    if (r == w)
        return false;

    uint32_t       pos = r & (XFILES_WATCH_RING_SIZE - 1);
    const uint8_t* p   = ctx->ring + pos;
    memcpy(&rec->pathlen, p + 4, 4);
    if (rec->pathlen == XFILES_WATCH_SKIP)
    {
        // The producer always writes a record straight after a skip record
        r += XFILES_WATCH_RING_SIZE - pos;
        XFILES_ASSERT(r != w);
        p = ctx->ring;
        memcpy(&rec->pathlen, p + 4, 4);
    }
    memcpy(&rec->flags, p, 4);
    rec->path = (const char*)p + XFILES_WATCH_RECORD_HEADER;
    rec->next = r + ((XFILES_WATCH_RECORD_HEADER + rec->pathlen + 1 + 7) & ~7u);
    return true;
}

// 32-bit FNV-1a. Never returns 0, which marks an empty slot in known_paths
static uint32_t _xfiles_watch_hash_path(const char* str)
{
    uint32_t hash = 2166136261U; // 32-bit FNV offset basis
    while (*str)
    {
        hash ^= (uint8_t)*str++;
        hash *= 16777619U; // 32-bit FNV prime
    }
    return hash ? hash : 1;
}

static void _xfiles_watch_send(struct XFilesWatchContext* ctx, enum XFilesWatchType type, const char* path, bool is_dir)
{
    XFilesWatchEvent event;
    event.type   = type;
    event.path   = path;
    event.is_dir = is_dir;
    ctx->callback(&event, ctx->udata);
}

static void _xfiles_watch_noop(void* p) { (void)p; }

XFilesWatchContext* xfiles_watch_create(const char* path, void* udata, XFilesWatchCallback cb)
{
    XFILES_ASSERT(path != NULL); // What path should be watched?
    XFILES_ASSERT(cb != NULL);   // Did you forget to write a callback?

    struct XFilesWatchContext* ctx = (struct XFilesWatchContext*)XFILES_MALLOC(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    ctx->udata    = udata;
    ctx->callback = cb;

    if (realpath(path, ctx->path) == NULL)
    {
        XFILES_FREE(ctx);
        return NULL;
    }
    ctx->pathlen = (int)strlen(ctx->path);

    // Create stream
    {
        // https://developer.apple.com/documentation/coreservices/1443980-fseventstreamcreate
        CFStringRef cfpath  = CFStringCreateWithCString(kCFAllocatorDefault, ctx->path, kCFStringEncodingUTF8);
        CFArrayRef  cfpaths = CFArrayCreate(kCFAllocatorDefault, (const void**)&cfpath, 1, &kCFTypeArrayCallBacks);

        FSEventStreamContext stream_ctx = {0};
        stream_ctx.info                 = ctx;

        // FileEvents:  per file events instead of per directory
        // NoDefer:     deliver the first event of a burst immediately, then batch for 'latency' seconds
        // WatchRoot:   tell us if the root itself gets moved or deleted
        // Consider kFSEventStreamCreateFlagIgnoreSelf if you don't want events caused by your own process
        FSEventStreamCreateFlags flags =
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagWatchRoot;
        CFTimeInterval latency = 0.05;

        ctx->stream = FSEventStreamCreate(
            kCFAllocatorDefault,
            _xfiles_watch_fsevents_cb,
            &stream_ctx,
            cfpaths,
            kFSEventStreamEventIdSinceNow,
            latency,
            flags);

        CFRelease(cfpaths);
        CFRelease(cfpath);
    }
    XFILES_ASSERT(ctx->stream != NULL);
    if (ctx->stream == NULL)
    {
        xfiles_watch_destroy(ctx);
        return NULL;
    }

    // Schedule & start
    {
        clock_gettime(CLOCK_REALTIME, &ctx->start_time);

        dispatch_queue_t queue = dispatch_queue_create("xhl.files.watch", DISPATCH_QUEUE_SERIAL);
#if __has_feature(objc_arc)
        ctx->queue = (__bridge_retained void*)queue;
#else
        ctx->queue = queue;
#endif
        FSEventStreamSetDispatchQueue(ctx->stream, queue);
        ctx->started = FSEventStreamStart(ctx->stream);
        XFILES_ASSERT(ctx->started);
        if (!ctx->started)
        {
            xfiles_watch_destroy(ctx);
            return NULL;
        }
    }

    return ctx;
}

void xfiles_watch_flush(XFilesWatchContext* _ctx)
{
    XFILES_ASSERT(_ctx != NULL);
    struct XFilesWatchContext* ctx = (struct XFilesWatchContext*)_ctx;

    // Load before exchanging so we don't dirty the producer's cache line on every flush
    if (__atomic_load_n(&ctx->overflowed, __ATOMIC_RELAXED) &&
        __atomic_exchange_n(&ctx->overflowed, 0, __ATOMIC_RELAXED))
        _xfiles_watch_send(ctx, XFILES_WATCH_OVERFLOW, ctx->path, true);

    // We are the only writer of read_idx, so a relaxed load is enough
    // Acquire on write_idx: the record bytes must be visible before we read them
    // write_idx is loaded once, so events arriving during this flush wait for the next one. This bounds the time
    // spent in flush even if the producer is busy
    uint32_t r = __atomic_load_n(&ctx->read_idx, __ATOMIC_RELAXED);
    uint32_t w = __atomic_load_n(&ctx->write_idx, __ATOMIC_ACQUIRE);

    XFWatchRecord rec;
    while (_xfiles_watch_read_record(ctx, r, w, &rec))
    {
        const FSEventStreamEventFlags modified_flags =
            kFSEventStreamEventFlagItemCreated | kFSEventStreamEventFlagItemModified |
            kFSEventStreamEventFlagItemInodeMetaMod | kFSEventStreamEventFlagItemChangeOwner |
            kFSEventStreamEventFlagItemXattrMod | kFSEventStreamEventFlagItemFinderInfoMod |
            kFSEventStreamEventFlagItemRenamed;

        bool        is_dir   = (rec.flags & kFSEventStreamEventFlagItemIsDir) != 0;
        bool        renamed  = (rec.flags & kFSEventStreamEventFlagItemRenamed) != 0;
        bool        removed  = (rec.flags & kFSEventStreamEventFlagItemRemoved) != 0;
        uint32_t    hash     = _xfiles_watch_hash_path(rec.path);
        uint32_t*   known    = &ctx->known_paths[hash & (XFILES_WATCH_KNOWN_PATHS - 1)];
        bool        is_known = *known == hash;
        struct stat st;
        bool        exists = lstat(rec.path, &st) == 0;

        // FSEvents dropped events (MustScanSubDirs is also set with UserDropped/KernelDropped), or the root was
        // moved/deleted. 'path' is the directory you should rescan
        if (rec.flags & (kFSEventStreamEventFlagMustScanSubDirs | kFSEventStreamEventFlagRootChanged))
        {
            _xfiles_watch_send(ctx, XFILES_WATCH_OVERFLOW, rec.path, true);
        }
        else if (rec.flags & kFSEventStreamEventFlagHistoryDone)
        {
        }
        else if (!exists)
        {
            // Deleted, or renamed/moved away from this path
            if (removed || renamed)
            {
                if (is_known)
                    *known = 0;
                _xfiles_watch_send(ctx, XFILES_WATCH_DELETED, rec.path, is_dir);
            }
        }
        else
        {
            bool born_after_start = st.st_birthtimespec.tv_sec > ctx->start_time.tv_sec ||
                                    (st.st_birthtimespec.tv_sec == ctx->start_time.tv_sec &&
                                     st.st_birthtimespec.tv_nsec >= ctx->start_time.tv_nsec);

            if (!is_known && renamed)
            {
                // The new path of a rename, or moved in from outside the watched folder. Its birth time is from
                // whenever it was first created, so we can't use that here
                _xfiles_watch_send(ctx, XFILES_WATCH_CREATED, rec.path, is_dir);
            }
            else if (!is_known && (rec.flags & kFSEventStreamEventFlagItemCreated) && born_after_start)
            {
                _xfiles_watch_send(ctx, XFILES_WATCH_CREATED, rec.path, is_dir);
            }
            else if (rec.flags & modified_flags)
            {
                // Includes a file renamed over an existing path, eg. an atomic save. The path existed before and
                // still does, so it was modified
                _xfiles_watch_send(ctx, XFILES_WATCH_MODIFIED, rec.path, is_dir);
            }
            *known = hash;
        }

        // 'path' points into the ring, so only release the record after the user callback has returned
        // Release: we're done reading these bytes before the producer may overwrite them
        r = rec.next;
        __atomic_store_n(&ctx->read_idx, r, __ATOMIC_RELEASE);
    }
}

void xfiles_watch_destroy(XFilesWatchContext* _ctx)
{
    XFILES_ASSERT(_ctx != NULL);
    struct XFilesWatchContext* ctx = (struct XFilesWatchContext*)_ctx;

    if (ctx->stream)
    {
        if (ctx->started)
            FSEventStreamStop(ctx->stream);
        FSEventStreamInvalidate(ctx->stream);
    }
    if (ctx->queue)
    {
#if __has_feature(objc_arc)
        dispatch_queue_t queue = (__bridge_transfer dispatch_queue_t)ctx->queue;
#else
        dispatch_queue_t queue = (dispatch_queue_t)ctx->queue;
#endif
        // A callback may already be running on the queue. Wait for it before freeing the ring
        dispatch_sync_f(queue, NULL, _xfiles_watch_noop);
#if !__has_feature(objc_arc)
        dispatch_release(queue);
#endif
    }
    if (ctx->stream)
        FSEventStreamRelease(ctx->stream);

    XFILES_FREE(ctx);
}

#ifdef __OBJC__
#import <AppKit/NSWorkspace.h>
#import <Foundation/Foundation.h>

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

int xfiles_get_user_directory(char* out, size_t outlen, XFilesUserDirectory loc)
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
    _Static_assert(XFILES_ARRLEN(PATHS) == XFILES_USER_DIRECTORY_COUNT, "");

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

void xfiles_free(void* data) { XFILES_FREE(data); }

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
