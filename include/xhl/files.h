/*
 * Released into the public domain by Tré Dudman - 2024
 * For licensing and more info see https://github.com/Tremus/xhl
 *
 * stdio.h FILE* replacement for Windows, macOS & Linux
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
 * Linux: Requires the GNU/default extensions of glibc (-std=gnu99, or define _DEFAULT_SOURCE). Plain -std=c99 hides
 * syscall(), realpath() and friends
 *
 * LINKING:
 * Windows: Kernel32 Shell32 Shlwapi
 * macOS: -framework AppKit (or -framework CoreServices if you don't need the Objective-C functions)
 * Linux: Nothing :D
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
#define XFILES_DIR_STR    "/"
#define XFILES_DIR_CHAR   '/'
#define XFILES_TRASH_NAME "Trash"
#ifdef __APPLE__
#define XFILES_BROWSER_NAME "Finder"
#else
#define XFILES_BROWSER_NAME "File Manager"
#endif
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
// OSX: Trash /
// Linux: freedesktop.org Trash. ~/.local/share/Trash, or .Trash-$uid at the top of the drive for other drives
bool xfiles_trash(const char* path);
// No bins. File is deleted and is likely unrecoverable
bool xfiles_delete(const char* path);
// Opens OS file or folder as if it was opened with
// Win: File Explorer /
// OSX: Finder /
// Linux: xdg-open. Returns true if xdg-open was started, even if it then fails
// NOTE: this is a blocking call
bool xfiles_open_file_explorer(const char* path);
// Opens OS file browsing app with path selected
// Win: File Explorer /
// OSX: Finder /
// Linux: org.freedesktop.FileManager1.ShowItems over D-Bus. If the file manager doesn't support it, opens the parent
// folder with xdg-open
// NOTE: this is a blocking call
bool xfiles_select_in_file_explorer(const char* path);

typedef enum XFilesUserDirectory
{
    // Windows: {letter}:\\Users\\{username}
    // macOS: /Users/{username}
    // Linux: $HOME, usually /home/{username}
    XFILES_USER_DIRECTORY_HOME,
    // Windows: [HOME]\\AppData\\Roaming
    // macOS: [HOME]/Library/Application\ Support
    // Linux: $XDG_DATA_HOME, default [HOME]/.local/share
    XFILES_USER_DIRECTORY_APPDATA,
    // Linux: The rest come from $XDG_CONFIG_HOME/user-dirs.dirs, so they may be translated, eg. [HOME]/Bureau.
    // Defaults to [HOME]/Desktop etc. Not checked to exist
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
    int                last_error; // errno on macOS & Linux, GetLastError() on Windows
    XFilesMetadataType type;

    uint64_t size_bytes;

    // Convenience
    bool is_readonly;
    bool is_hidden;     // Linux: name starts with '.'
    bool is_system;     // Always false on Linux
    bool is_archive;    // Windows only, its for backing up files
    bool is_compressed; // (UF_COMPRESSED, FILE_ATTRIBUTE_COMPRESSED). Not to be confused with zip/7z/tar/gz/rar
    bool is_encrypted;
    bool is_executable; // +x user permissions on posix. Lazy extension checking on Windwos
    bool is_symlink;

    // Unix epoch times in nanoseconds
    uint64_t creation_time_ns; // File creation time (birthtime / CreationTime). 0 if the filesystem doesn't store it
    uint64_t modification_time_ns;  // Last write time (mtime / LastWriteTime)
    uint64_t access_time_ns;        // Last access time (atime / LastAccessTime)
    uint64_t status_change_time_ns; // POSIX ctime (0 on Windows)

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
    uint32_t _st_flags;   // User-defined flags (macOS BSD flags). 0 on Linux
    uint32_t _st_gen;     // File generation number (macOS). 0 on Linux
} XFilesMetadata;

XFilesMetadata xfiles_get_metadata(const char* path);

typedef struct xfiles_list_item_t
{
    char path[1024];

    unsigned path_len;
    unsigned name_idx;
    unsigned ext_idx;
    bool     is_dir;

    // Unix epoch time in nanoseconds. Free on Windows. On macOS & Linux it costs a stat per file, and is 0 for
    // directories
    uint64_t last_modified_ns;
} xfiles_list_item_t;

typedef void(xfiles_list_callback_t)(void* data, const xfiles_list_item_t* item);
// Calls the above callback for each item in a directory
void xfiles_list(const char* path, void* data, xfiles_list_callback_t* cb);

// NOTE: Treat these events as hints, not a perfect log. All OSes coalesce and occasionally reorder events, so you may
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
    // DELETED, CREATED and MODIFIED for the saved file. On Linux they arrive as CREATED for the saved file, with
    // DELETED for the temp file
    // On Linux this fires when a file opened for writing is closed, so you won't see it for files that are kept open
    // or written through mmap
    XFILES_WATCH_MODIFIED,
    // Events were lost and you should rescan 'path'. On Windows this happens when more than 64kb of events arrive
    // between flushes. On macOS this happens when FSEvents drops events, the watched folder itself is moved or
    // deleted, or more events arrive between flushes than fit in our buffer. On Linux this happens when the kernel's
    // queue fills up (/proc/sys/fs/inotify/max_queued_events, usually 16384), or the watched folder itself is moved or
    // deleted. Folders past the /proc/sys/fs/inotify/max_user_watches limit go unwatched without any warning
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

        // Convert FILETIME (100-ns intervals since 1601-01-01 UTC) to ns since 1970-01-01 UTC
        {
            const uint64_t EPOCH_DIFF_100NS = 116444736000000000ULL;

            ULARGE_INTEGER m;
            m.LowPart             = FindData.ftLastWriteTime.dwLowDateTime;
            m.HighPart            = FindData.ftLastWriteTime.dwHighDateTime;
            Item.last_modified_ns = m.QuadPart > EPOCH_DIFF_100NS ? (m.QuadPart - EPOCH_DIFF_100NS) * 100ULL : 0;
        }
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

#if defined(__APPLE__) || defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint64_t _xfiles_timespec_ns(struct timespec ts)
{
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static XFilesMetadataType _xfiles_metadata_type(mode_t mode)
{
    if (S_ISREG(mode))
        return XFILES_METADATA_TYPE_FILE;
    if (S_ISDIR(mode))
        return XFILES_METADATA_TYPE_DIRECTORY;
    if (S_ISLNK(mode))
        return XFILES_METADATA_TYPE_SYMLINK;
    if (S_ISBLK(mode))
        return XFILES_METADATA_TYPE_BLOCK_DEVICE;
    if (S_ISCHR(mode))
        return XFILES_METADATA_TYPE_CHAR_DEVICE;
    if (S_ISFIFO(mode))
        return XFILES_METADATA_TYPE_FIFO;
    if (S_ISSOCK(mode))
        return XFILES_METADATA_TYPE_SOCKET;
    return XFILES_METADATA_TYPE_UNKNOWN;
}

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

#ifdef __APPLE__
// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/rename.2.html
bool xfiles_move(const char* from, const char* to, XFilesMoveFlags flags)
{
    unsigned int rename_flags = (flags & XFILES_MOVE_OVERWRITE) ? 0 : RENAME_EXCL;
    return 0 == renamex_np(from, to, rename_flags);
}
#endif // __APPLE__

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/unlink.2.html
bool xfiles_delete(const char* path) { return unlink(path) == 0; }

#ifdef __APPLE__
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
    meta.size_bytes            = (uint64_t)st.st_size;
    meta.creation_time_ns      = _xfiles_timespec_ns(st.st_birthtimespec);
    meta.modification_time_ns  = _xfiles_timespec_ns(st.st_mtimespec);
    meta.access_time_ns        = _xfiles_timespec_ns(st.st_atimespec);
    meta.status_change_time_ns = _xfiles_timespec_ns(st.st_ctimespec);
    meta.file_index            = st.st_ino;
    meta.num_links             = st.st_nlink;
    meta.volume_serial_number  = st.st_dev;

    meta._st_mode    = st.st_mode;
    meta._st_blksize = st.st_blksize;
    meta._st_blocks  = st.st_blocks;
    meta._st_flags   = st.st_flags;
    meta._st_gen     = st.st_gen;

    meta.type = _xfiles_metadata_type(st.st_mode);

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
#endif // __APPLE__

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
        size_t namelen = strlen(entry->d_name);
        item.path_len  = item.name_idx + namelen;

        XFILES_ASSERT(item.path_len < sizeof(item.path));
        if (item.path_len >= sizeof(item.path)) // Guard overflow
            goto iterate;

        memcpy(&item.path[item.name_idx], entry->d_name, namelen);
        item.path[item.path_len] = 0;

        item.ext_idx = item.name_idx;
        for (uint32_t i = item.name_idx; i < item.path_len; i++)
            if (item.path[i] == '.')
                item.ext_idx = i;
        if (item.ext_idx == item.name_idx) // Failed to find extension
            item.ext_idx = item.path_len;
        item.is_dir = entry->d_type == DT_DIR;

        item.last_modified_ns = 0;
        if (!item.is_dir)
        {
            struct stat st;
            if (fstatat(dirfd(dir), entry->d_name, &st, AT_SYMLINK_NOFOLLOW) == 0)
            {
                // Some filesystems on Linux don't fill in d_type
                if (entry->d_type == DT_UNKNOWN)
                    item.is_dir = S_ISDIR(st.st_mode);
                if (!item.is_dir)
#ifdef __APPLE__
                    item.last_modified_ns = _xfiles_timespec_ns(st.st_mtimespec);
#else
                    item.last_modified_ns = _xfiles_timespec_ns(st.st_mtim);
#endif
            }
        }

        XFILES_ASSERT(strlen(item.path) == item.path_len);
        callback(data, &item);

    iterate:
        // readdir returns NULL if no more files, breaking the loop
        entry = readdir(dir);
    }

    if (dir)
        closedir(dir);
}
#endif // __APPLE__ || __linux__

#ifdef __APPLE__
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

    // The watched root
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

    ctx->pathlen = snprintf(ctx->path, sizeof(ctx->path), "%s", path);
    if (ctx->pathlen < 0 || ctx->pathlen > sizeof(ctx->path))
    {
        xfiles_watch_destroy(ctx);
        return NULL;
    }

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

#ifdef __linux__
#include <linux/stat.h> // struct statx. glibc only declares it with _GNU_SOURCE
#include <pwd.h>
#include <spawn.h>
#include <sys/inotify.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/wait.h>
#include <time.h>

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif

extern char** environ;

// https://man7.org/linux/man-pages/man2/rename.2.html
bool xfiles_move(const char* from, const char* to, XFilesMoveFlags flags)
{
    if (flags & XFILES_MOVE_OVERWRITE)
        return rename(from, to) == 0;

    // renameat2() is only declared with _GNU_SOURCE, so call the syscall. Requires Linux 3.15+
    if (syscall(SYS_renameat2, AT_FDCWD, from, AT_FDCWD, to, RENAME_NOREPLACE) == 0)
        return true;
    // Some filesystems (eg. some network & FUSE filesystems) don't support RENAME_NOREPLACE. Fall back to a check
    // followed by a rename, which isn't atomic
    if (errno != EINVAL && errno != ENOSYS)
        return false;
    struct stat st;
    if (lstat(to, &st) == 0)
    {
        errno = EEXIST;
        return false;
    }
    return rename(from, to) == 0;
}

XFilesMetadata xfiles_get_metadata(const char* path)
{
    XFilesMetadata meta = {0};

    if (path == NULL || path[0] == '\0')
    {
        meta.last_error = -1;
        return meta;
    }

    // statx is the only way to get the creation time. Requires Linux 4.11+, and some sandboxes block it, so fall back
    // to lstat. AT_SYMLINK_NOFOLLOW so symlinks are reported as symlinks
    // https://man7.org/linux/man-pages/man2/statx.2.html
    struct statx stx = {0};
    if (syscall(SYS_statx, AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, STATX_BASIC_STATS | STATX_BTIME, &stx) != 0)
    {
        struct stat st;
        if ((errno != ENOSYS && errno != EPERM) || lstat(path, &st) != 0)
        {
            meta.last_error = (int32_t)errno;
            return meta;
        }
        stx.stx_mode          = st.st_mode;
        stx.stx_size          = st.st_size;
        stx.stx_mtime.tv_sec  = st.st_mtim.tv_sec;
        stx.stx_mtime.tv_nsec = st.st_mtim.tv_nsec;
        stx.stx_atime.tv_sec  = st.st_atim.tv_sec;
        stx.stx_atime.tv_nsec = st.st_atim.tv_nsec;
        stx.stx_ctime.tv_sec  = st.st_ctim.tv_sec;
        stx.stx_ctime.tv_nsec = st.st_ctim.tv_nsec;
        stx.stx_ino           = st.st_ino;
        stx.stx_nlink         = st.st_nlink;
        stx.stx_dev_major     = major(st.st_dev);
        stx.stx_dev_minor     = minor(st.st_dev);
        stx.stx_blksize       = st.st_blksize;
        stx.stx_blocks        = st.st_blocks;
    }

    meta.size_bytes = stx.stx_size;
    // 0 if the filesystem doesn't record it (eg. some network filesystems)
    if (stx.stx_mask & STATX_BTIME)
        meta.creation_time_ns = (uint64_t)stx.stx_btime.tv_sec * 1000000000ULL + stx.stx_btime.tv_nsec;
    meta.modification_time_ns  = (uint64_t)stx.stx_mtime.tv_sec * 1000000000ULL + stx.stx_mtime.tv_nsec;
    meta.access_time_ns        = (uint64_t)stx.stx_atime.tv_sec * 1000000000ULL + stx.stx_atime.tv_nsec;
    meta.status_change_time_ns = (uint64_t)stx.stx_ctime.tv_sec * 1000000000ULL + stx.stx_ctime.tv_nsec;
    meta.file_index            = stx.stx_ino;
    meta.num_links             = stx.stx_nlink;
    meta.volume_serial_number  = (uint32_t)makedev(stx.stx_dev_major, stx.stx_dev_minor);

    meta._st_mode    = stx.stx_mode;
    meta._st_blksize = stx.stx_blksize;
    meta._st_blocks  = stx.stx_blocks;

    meta.type = _xfiles_metadata_type(stx.stx_mode);

    const char* name   = xfiles_get_name(path);
    meta.is_readonly   = (stx.stx_mode & 0222) == 0 || (stx.stx_attributes & STATX_ATTR_IMMUTABLE);
    meta.is_hidden     = (name ? name : path)[0] == '.'; // Dotfiles are hidden by convention
    meta.is_system     = false;
    meta.is_archive    = false;
    meta.is_compressed = (stx.stx_attributes & STATX_ATTR_COMPRESSED) != 0;
    meta.is_encrypted  = (stx.stx_attributes & STATX_ATTR_ENCRYPTED) != 0;
    meta.is_executable = (stx.stx_mode & 0111) != 0;
    meta.is_symlink    = S_ISLNK(stx.stx_mode) ? true : false;

    return meta;
}

// Writes 'a' followed by 'b' to 'out'. Returns the length excluding the null terminating byte, or 0 if it doesn't fit
static int _xfiles_concat(char* out, size_t outlen, const char* a, size_t alen, const char* b, size_t blen)
{
    if (alen + blen + 1 > outlen)
        return 0;
    memmove(out, a, alen);
    memcpy(out + alen, b, blen);
    out[alen + blen] = '\0';
    return (int)(alen + blen);
}

// Absolute path without following the last component, so a symlink is trashed rather than its target
static bool _xfiles_absolute_path(const char* path, char out[PATH_MAX])
{
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/')
        len--;
    size_t name_idx = len;
    while (name_idx > 0 && path[name_idx - 1] != '/')
        name_idx--;

    const char* name    = path + name_idx;
    size_t      namelen = len - name_idx;
    if (namelen == 0 || (namelen == 1 && name[0] == '.') || (namelen == 2 && name[0] == '.' && name[1] == '.'))
        return false;

    char parent[PATH_MAX];
    if (name_idx == 0)
        snprintf(parent, sizeof(parent), ".");
    else if (name_idx == 1)
        snprintf(parent, sizeof(parent), "/");
    else
        snprintf(parent, sizeof(parent), "%.*s", (int)(name_idx - 1), path);

    if (!realpath(parent, out))
        return false;
    size_t parentlen = strlen(out);
    if (out[parentlen - 1] != '/')
        out[parentlen++] = '/';
    return _xfiles_concat(out, PATH_MAX, out, parentlen, name, namelen) != 0;
}

// Percent encodes everything except unreserved characters and '/', for file:// URIs and .trashinfo files
static bool _xfiles_uri_encode(const char* in, char* out, size_t outlen)
{
    static const char HEX[] = "0123456789ABCDEF";
    size_t            n     = 0;
    for (const unsigned char* c = (const unsigned char*)in; *c; c++)
    {
        bool keep = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-' ||
                    *c == '_' || *c == '.' || *c == '~' || *c == '/';
        if (n + (keep ? 1 : 3) >= outlen)
            return false;
        if (keep)
        {
            out[n++] = *c;
        }
        else
        {
            out[n++] = '%';
            out[n++] = HEX[*c >> 4];
            out[n++] = HEX[*c & 15];
        }
    }
    out[n] = '\0';
    return true;
}

static bool _xfiles_mkdir_private(const char* path) { return mkdir(path, 0700) == 0 || errno == EEXIST; }

// Finds or creates the trash folder for a file on device 'dev'
// Sets 'topdir' to the mount point the trash is in, or an empty string for the home trash
// https://specifications.freedesktop.org/trash-spec/latest/
static bool _xfiles_find_trash(const char* abspath, dev_t dev, char trash[PATH_MAX], char topdir[PATH_MAX])
{
    struct stat st;
    topdir[0] = '\0';

    // Home trash: $XDG_DATA_HOME/Trash
    {
        char        datahome[PATH_MAX];
        const char* xdg = getenv("XDG_DATA_HOME");
        if (xdg && xdg[0] == '/')
            snprintf(datahome, sizeof(datahome), "%s", xdg);
        else if (xfiles_get_user_directory(datahome, sizeof(datahome), XFILES_USER_DIRECTORY_HOME))
            snprintf(datahome + strlen(datahome), sizeof(datahome) - strlen(datahome), "/.local/share");
        else
            datahome[0] = '\0';

        if (datahome[0] && xfiles_create_directory_recursive(datahome))
        {
            snprintf(trash, PATH_MAX, "%s/Trash", datahome);
            if (_xfiles_mkdir_private(trash) && stat(trash, &st) == 0 && st.st_dev == dev)
                return true;
        }
    }

    // The file is on another device. Use a trash at the top of its mount point. Moving it to the home trash would copy
    // it, which could take a long time, and the spec doesn't require it
    // Walk up from the file until the parent is on a different device
    snprintf(topdir, PATH_MAX, "%s", abspath);
    for (;;)
    {
        char* slash = strrchr(topdir, '/');
        if (slash == topdir)
        {
            if (stat("/", &st) == 0 && st.st_dev == dev)
                topdir[1] = '\0';
            break;
        }
        *slash = '\0';
        if (stat(topdir, &st) != 0)
            return false;
        if (st.st_dev != dev)
        {
            *slash = '/';
            break;
        }
    }
    const char* sep = strcmp(topdir, "/") == 0 ? "" : "/";

    // $topdir/.Trash/$uid, but only if an admin created $topdir/.Trash as a real folder with the sticky bit set
    snprintf(trash, PATH_MAX, "%s%s.Trash", topdir, sep);
    if (lstat(trash, &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_ISVTX))
    {
        snprintf(trash, PATH_MAX, "%s%s.Trash/%u", topdir, sep, (unsigned)getuid());
        if (_xfiles_mkdir_private(trash) && lstat(trash, &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == getuid())
            return true;
    }

    // $topdir/.Trash-$uid
    snprintf(trash, PATH_MAX, "%s%s.Trash-%u", topdir, sep, (unsigned)getuid());
    return _xfiles_mkdir_private(trash) && lstat(trash, &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == getuid();
}

// Moves the file to the freedesktop.org Trash, so the file manager can show it and put it back
// https://specifications.freedesktop.org/trash-spec/latest/
bool xfiles_trash(const char* path)
{
    char        abspath[PATH_MAX];
    char        trash[PATH_MAX];
    char        topdir[PATH_MAX];
    struct stat st;

    if (!_xfiles_absolute_path(path, abspath) || lstat(abspath, &st) != 0)
        return false;
    if (!_xfiles_find_trash(abspath, st.st_dev, trash, topdir))
        return false;

    char files_dir[PATH_MAX];
    char info_dir[PATH_MAX];
    snprintf(files_dir, sizeof(files_dir), "%s/files", trash);
    snprintf(info_dir, sizeof(info_dir), "%s/info", trash);
    if (!_xfiles_mkdir_private(files_dir) || !_xfiles_mkdir_private(info_dir))
        return false;

    // Path= is relative to the mount point for trashes on other devices, so it still works if it's mounted elsewhere
    const char* original = abspath;
    if (topdir[0])
        original += strcmp(topdir, "/") == 0 ? 1 : strlen(topdir) + 1;

    char info[PATH_MAX * 3 + 128];
    int  infolen = snprintf(info, sizeof(info), "[Trash Info]\nPath=");
    if (!_xfiles_uri_encode(original, info + infolen, sizeof(info) - infolen))
        return false;
    infolen += strlen(info + infolen);
    {
        time_t    now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        infolen += strftime(info + infolen, sizeof(info) - infolen, "\nDeletionDate=%Y-%m-%dT%H:%M:%S\n", &tm);
    }

    // On a name collision try "name.2", "name.3", etc. Creating the .trashinfo file with O_EXCL reserves the name
    const char* name = xfiles_get_name(abspath);
    for (int i = 1; i < 1000; i++)
    {
        char trashname[NAME_MAX + 16];
        char info_path[PATH_MAX];
        char dest[PATH_MAX];
        if (i == 1)
            snprintf(trashname, sizeof(trashname), "%s", name);
        else
            snprintf(trashname, sizeof(trashname), "%s.%d", name, i);
        if (snprintf(info_path, sizeof(info_path), "%s/%s.trashinfo", info_dir, trashname) >= (int)sizeof(info_path))
            return false;
        if (snprintf(dest, sizeof(dest), "%s/%s", files_dir, trashname) >= (int)sizeof(dest))
            return false;

        int fd = open(info_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd == -1)
        {
            if (errno == EEXIST)
                continue;
            return false;
        }
        bool written = write(fd, info, infolen) == infolen;
        close(fd);

        if (written && xfiles_move(abspath, dest, XFILES_MOVE_DEFAULT))
            return true;

        int err = errno;
        unlink(info_path);
        if (!written || err != EEXIST)
            return false;
    }
    return false;
}

// Runs a program and waits for it to exit. Returns true if it exited with 0
static bool _xfiles_run(char* const argv[])
{
    // posix_spawn rather than fork, which is safer in a multithreaded host and doesn't copy its page tables
    // https://man7.org/linux/man-pages/man3/posix_spawn.3.html
    pid_t                      pid;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    int err = posix_spawnp(&pid, argv[0], &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (err != 0)
        return false;

    int status = 0;
    while (waitpid(pid, &status, 0) == -1)
        if (errno != EINTR)
            return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool xfiles_open_file_explorer(const char* path)
{
    // Some xdg-open backends wait for the app they open to exit, so run it in the background with sh. sh exits as soon
    // as xdg-open has started, and xdg-open is reparented to init so it doesn't become a zombie of the host
    char* argv[] = {
        (char*)"sh",
        (char*)"-c",
        (char*)"command -v xdg-open >/dev/null && { xdg-open \"$1\" >/dev/null 2>&1 & }",
        (char*)"sh",
        (char*)path,
        NULL};
    bool ok = _xfiles_run(argv);
    XFILES_ASSERT(ok);
    return ok;
}

bool xfiles_select_in_file_explorer(const char* path)
{
    // Most file managers (Nautilus, Dolphin, Nemo, Caja, Thunar, ...) implement org.freedesktop.FileManager1
    // https://www.freedesktop.org/wiki/Specifications/file-manager-interface/
    char abspath[PATH_MAX];
    char arg[PATH_MAX * 3 + 32];
    if (!_xfiles_absolute_path(path, abspath))
        return false;

    int n = snprintf(arg, sizeof(arg), "array:string:file://");
    if (_xfiles_uri_encode(abspath, arg + n, sizeof(arg) - n))
    {
        char* argv[] = {
            (char*)"dbus-send",
            (char*)"--session",
            (char*)"--print-reply", // Waits for the reply, so the exit code tells us if it worked
            (char*)"--reply-timeout=5000",
            (char*)"--dest=org.freedesktop.FileManager1",
            (char*)"/org/freedesktop/FileManager1",
            (char*)"org.freedesktop.FileManager1.ShowItems",
            arg,
            (char*)"string:",
            NULL};
        if (_xfiles_run(argv))
            return true;
    }

    // No file manager implements the interface, or dbus-send isn't installed. Open the parent folder instead
    *strrchr(abspath, '/') = '\0';
    return xfiles_open_file_explorer(abspath[0] ? abspath : "/");
}

// Reads the value of 'key' from user-dirs.dirs, eg. XDG_DESKTOP_DIR="$HOME/Desktop"
// Returns the length of the value written to 'out', or 0 if it wasn't found
// https://www.freedesktop.org/wiki/Software/xdg-user-dirs/
static int _xfiles_read_user_dir(
    const char* config_dir,
    const char* key,
    const char* home,
    size_t      homelen,
    char*       out,
    size_t      outlen)
{
    char path[PATH_MAX];
    char buf[4096];
    snprintf(path, sizeof(path), "%s/user-dirs.dirs", config_dir);

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd == -1)
        return 0;
    ssize_t nread = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (nread <= 0)
        return 0;
    buf[nread] = '\0';

    size_t keylen = strlen(key);
    for (char* line = buf; line; line = strchr(line, '\n'))
    {
        while (*line == '\n' || *line == ' ' || *line == '\t')
            line++;
        if (strncmp(line, key, keylen) != 0 || line[keylen] != '=' || line[keylen + 1] != '"')
            continue;

        // Values are either "$HOME/..." or an absolute path. Characters may be escaped with '\'
        const char* c = line + keylen + 2;
        size_t      n = 0;
        if (strncmp(c, "$HOME", 5) == 0)
        {
            if (homelen >= outlen)
                return 0;
            memcpy(out, home, homelen);
            n  = homelen;
            c += 5;
        }
        else if (*c != '/')
        {
            return 0;
        }
        for (; *c && *c != '"' && *c != '\n'; c++)
        {
            if (*c == '\\' && c[1])
                c++;
            if (n + 1 >= outlen)
                return 0;
            out[n++] = *c;
        }
        out[n] = '\0';
        return (int)n;
    }
    return 0;
}

int xfiles_get_user_directory(char* out, size_t outlen, XFilesUserDirectory loc)
{
    // Keys in user-dirs.dirs, and the defaults when they're missing
    static const char* XDG_KEYS[] = {
        NULL,                // XFILES_USER_DIRECTORY_HOME
        NULL,                // XFILES_USER_DIRECTORY_APPDATA
        "XDG_DESKTOP_DIR",   // XFILES_USER_DIRECTORY_DESKTOP
        "XDG_DOCUMENTS_DIR", // XFILES_USER_DIRECTORY_DOCUMENTS
        "XDG_DOWNLOAD_DIR",  // XFILES_USER_DIRECTORY_DOWNLOADS
        "XDG_MUSIC_DIR",     // XFILES_USER_DIRECTORY_MUSIC
        "XDG_PICTURES_DIR",  // XFILES_USER_DIRECTORY_PICTURES
        "XDG_VIDEOS_DIR",    // XFILES_USER_DIRECTORY_VIDEOS
    };
    static const char* PATHS[] = {
        "/",             // XFILES_USER_DIRECTORY_HOME. Trimmed below, unless HOME=/
        "/.local/share", // XFILES_USER_DIRECTORY_APPDATA
        "/Desktop",      // XFILES_USER_DIRECTORY_DESKTOP
        "/Documents",    // XFILES_USER_DIRECTORY_DOCUMENTS
        "/Downloads",    // XFILES_USER_DIRECTORY_DOWNLOADS
        "/Music",        // XFILES_USER_DIRECTORY_MUSIC
        "/Pictures",     // XFILES_USER_DIRECTORY_PICTURES
        "/Videos",       // XFILES_USER_DIRECTORY_VIDEOS
    };
    _Static_assert(XFILES_ARRLEN(XDG_KEYS) == XFILES_USER_DIRECTORY_COUNT, "");
    _Static_assert(XFILES_ARRLEN(PATHS) == XFILES_USER_DIRECTORY_COUNT, "");

    if (loc < 0)
        loc = (XFilesUserDirectory)0;
    if (loc >= XFILES_USER_DIRECTORY_COUNT)
        loc = (XFilesUserDirectory)(XFILES_USER_DIRECTORY_COUNT - 1);

    // $HOME, or the password database if it isn't set
    char        pwbuf[4096];
    const char* home = getenv("HOME");
    if (home == NULL || home[0] != '/')
    {
        struct passwd  pw;
        struct passwd* result = NULL;
        if (getpwuid_r(getuid(), &pw, pwbuf, sizeof(pwbuf), &result) != 0 || result == NULL)
            return 0;
        home = pw.pw_dir;
    }
    size_t homelen = strlen(home);
    // HOME=/home/u/ is legal. Trim it so it doesn't become /home/u//Desktop
    // HOME=/ trims to "", so joins become /Desktop rather than //Desktop
    while (homelen > 0 && home[homelen - 1] == '/')
        homelen--;

    // $XDG_CONFIG_HOME, default ~/.config
    char        config_dir[PATH_MAX];
    const char* xdg_config = getenv("XDG_CONFIG_HOME");
    if (xdg_config && xdg_config[0] == '/')
        snprintf(config_dir, sizeof(config_dir), "%s", xdg_config);
    else
        snprintf(config_dir, sizeof(config_dir), "%.*s/.config", (int)homelen, home);

    int num = 0;
    if (loc == XFILES_USER_DIRECTORY_APPDATA)
    {
        // $XDG_DATA_HOME, default ~/.local/share
        const char* xdg_data = getenv("XDG_DATA_HOME");
        if (xdg_data && xdg_data[0] == '/')
            num = _xfiles_concat(out, outlen, xdg_data, strlen(xdg_data), "", 0);
    }
    else if (XDG_KEYS[loc])
        num = _xfiles_read_user_dir(config_dir, XDG_KEYS[loc], home, homelen, out, outlen);
    if (num == 0)
        num = _xfiles_concat(out, outlen, home, homelen, PATHS[loc], strlen(PATHS[loc]));

    // Users can "disable" their own folders by setting it to "$HOME/" in user-dirs.dirs, which xdg-user-dirs-update
    // We remove the trailing slash because every folder path we return lacks a trailing slash. This means users of this
    // function can safely append "/mysubdir" with a leading "/" to whatever path we give them, and never get "//"
    while (num > 1 && out[num - 1] == '/')
        out[--num] = '\0';

    XFILES_ASSERT(num > 0);
    return num;
}

// File watching using inotify
//
// inotify only watches a single folder, so each folder in the tree gets its own watch. 'dirs' maps watch descriptors
// to folders. Each folder stores its parent and its own name, so renaming a folder only changes one entry, and paths
// are rebuilt by walking up to the root. Names are stored in 'names'. Removing or renaming a folder leaves its old name
// behind as garbage, which is compacted when 'names' would otherwise grow.
//
// New folders are scanned after they're watched, and anything already inside them is reported as CREATED. Without this
// we'd miss files created before the watch was added, eg. 'mkdir -p a/b/c' or extracting an archive. This can cause
// duplicate CREATED events.
//
// Renames and moves:
// The kernel sends IN_MOVED_FROM for the old path and IN_MOVED_TO for the new path, with matching cookies. When a
// watched folder is moved, its watch follows it, so we remember the folder from IN_MOVED_FROM and update its parent and
// name on the matching IN_MOVED_TO. If the IN_MOVED_TO never arrives, the folder was moved out of the tree, so its
// watches are removed.
#define XFILES_WATCH_MASK                                                                                              \
    (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF |            \
     IN_ONLYDIR | IN_DONT_FOLLOW)

typedef struct XFilesWatchDir
{
    int      wd;       // -1 for a free slot
    int      parent;   // Index of the parent folder, -1 for the root. For a free slot, the next free slot
    uint32_t name_off; // Offset into 'names'. The root's name is the full path to the watched folder
    uint32_t name_len;
} XFilesWatchDir;

struct XFilesWatchContext
{
    int fd;

    // dirs[0] is always the root
    XFilesWatchDir* dirs;
    int             num_dirs;
    int             cap_dirs;
    int             free_dir; // Head of the free list, -1 if empty

    char*    names;
    uint32_t names_len;
    uint32_t names_cap;

    // A folder was moved from somewhere in the tree, and we're waiting to see where it went
    uint32_t moved_cookie; // 0 if none
    int      moved_dir;    // -1 if the folder wasn't watched

    void*               udata;
    XFilesWatchCallback callback;

    // Large enough for ~1000 events with short names. read() returns as many whole events as fit
    char buf[1024 * 64] __attribute__((aligned(__alignof__(struct inotify_event))));
};

static void _xfiles_watch_send(struct XFilesWatchContext* ctx, enum XFilesWatchType type, const char* path, bool is_dir)
{
    XFilesWatchEvent event;
    event.type   = type;
    event.path   = path;
    event.is_dir = is_dir;
    ctx->callback(&event, ctx->udata);
}

static size_t _xfiles_watch_dir_path(const struct XFilesWatchContext* ctx, int idx, char* out, size_t outlen);

static void _xfiles_watch_send_overflow(struct XFilesWatchContext* ctx)
{
    char root[PATH_MAX];
    if (_xfiles_watch_dir_path(ctx, 0, root, sizeof(root)))
        _xfiles_watch_send(ctx, XFILES_WATCH_OVERFLOW, root, true);
}

// Linear search. Events for the same folder tend to arrive together, but if this shows up in a profile with very large
// trees, try caching the last result or a hash map
static int _xfiles_watch_find_wd(const struct XFilesWatchContext* ctx, int wd)
{
    for (int i = 0; i < ctx->num_dirs; i++)
        if (ctx->dirs[i].wd == wd)
            return i;
    return -1;
}

static int _xfiles_watch_find_child(const struct XFilesWatchContext* ctx, int parent, const char* name)
{
    size_t namelen = strlen(name);
    for (int i = 0; i < ctx->num_dirs; i++)
    {
        const XFilesWatchDir* d = &ctx->dirs[i];
        if (d->wd >= 0 && d->parent == parent && d->name_len == namelen &&
            memcmp(ctx->names + d->name_off, name, namelen) == 0)
            return i;
    }
    return -1;
}

// Returns false if out of memory
static bool _xfiles_watch_set_name(struct XFilesWatchContext* ctx, int idx, const char* name, size_t namelen)
{
    if (ctx->names_len + namelen > ctx->names_cap)
    {
        // Count the names still in use. If at least half of 'names' is garbage, compact it instead of growing it
        uint32_t live = 0;
        for (int i = 0; i < ctx->num_dirs; i++)
            if (i != idx && (i == 0 || ctx->dirs[i].wd >= 0))
                live += ctx->dirs[i].name_len;

        uint32_t cap = ctx->names_cap ? ctx->names_cap : 4096;
        while (cap < 2 * (live + namelen))
            cap *= 2;

        char* names = (char*)XFILES_MALLOC(cap);
        XFILES_ASSERT(names != NULL);
        if (names == NULL)
            return false;

        uint32_t len = 0;
        for (int i = 0; i < ctx->num_dirs; i++)
        {
            XFilesWatchDir* d = &ctx->dirs[i];
            if (i != idx && (i == 0 || d->wd >= 0))
            {
                memcpy(names + len, ctx->names + d->name_off, d->name_len);
                d->name_off  = len;
                len         += d->name_len;
            }
        }
        XFILES_FREE(ctx->names);
        ctx->names     = names;
        ctx->names_len = len;
        ctx->names_cap = cap;
    }

    memcpy(ctx->names + ctx->names_len, name, namelen);
    ctx->dirs[idx].name_off  = ctx->names_len;
    ctx->dirs[idx].name_len  = (uint32_t)namelen;
    ctx->names_len          += (uint32_t)namelen;
    return true;
}

// Returns the length of the path, or 0 if it doesn't fit in 'out'
static size_t _xfiles_watch_dir_path(const struct XFilesWatchContext* ctx, int idx, char* out, size_t outlen)
{
    size_t len = 0;
    for (int i = idx; i != -1; i = ctx->dirs[i].parent)
        len += ctx->dirs[i].name_len + (ctx->dirs[i].parent != -1);
    if (len >= outlen)
        return 0;

    // Fill in backwards, from the folder up to the root
    size_t end = len;
    out[end]   = '\0';
    for (int i = idx; i != -1; i = ctx->dirs[i].parent)
    {
        end -= ctx->dirs[i].name_len;
        memcpy(out + end, ctx->names + ctx->dirs[i].name_off, ctx->dirs[i].name_len);
        if (ctx->dirs[i].parent != -1)
            out[--end] = '/';
    }
    return len;
}

// Stops watching a folder and everything below it
static void _xfiles_watch_remove_tree(struct XFilesWatchContext* ctx, int idx)
{
    // Two passes, because finding descendants walks up through 'parent', which a freed slot reuses. Folders being
    // removed are marked with wd = -2 in the first pass
    for (int i = 0; i < ctx->num_dirs; i++)
    {
        if (ctx->dirs[i].wd < 0)
            continue;
        int p = i;
        while (p != -1 && p != idx)
            p = ctx->dirs[p].parent;
        if (p == idx)
        {
            // Fails harmlessly if the kernel already removed the watch, eg. the folder was deleted
            inotify_rm_watch(ctx->fd, ctx->dirs[i].wd);
            ctx->dirs[i].wd = -2;
        }
    }
    for (int i = 0; i < ctx->num_dirs; i++)
    {
        if (ctx->dirs[i].wd != -2)
            continue;
        ctx->dirs[i].wd = -1;
        // The root slot is never reused, so its path stays valid for OVERFLOW events
        if (i != 0)
        {
            ctx->dirs[i].parent = ctx->free_dir;
            ctx->free_dir       = i;
        }
    }
    if (ctx->moved_dir != -1 && ctx->dirs[ctx->moved_dir].wd < 0)
        ctx->moved_dir = -1;
}

// Watches the folder at 'path' and everything below it. 'path' must be a PATH_MAX sized buffer, which is used for the
// paths of children. If 'send_children' is true, sends CREATED for everything found inside
static void _xfiles_watch_add_tree(
    struct XFilesWatchContext* ctx,
    char*                      path,
    size_t                     pathlen,
    int                        parent,
    const char*                name,
    size_t                     namelen,
    bool                       send_children)
{
    // https://man7.org/linux/man-pages/man2/inotify_add_watch.2.html
    int wd = inotify_add_watch(ctx->fd, path, XFILES_WATCH_MASK);
    // ENOENT: deleted before we got to it. ENOTDIR: replaced by a file. EACCES: no permission. ENOSPC: hit the
    // max_user_watches limit in /proc/sys/fs/inotify
    XFILES_ASSERT(wd != -1 || errno == ENOENT || errno == ENOTDIR || errno == EACCES);
    if (wd == -1)
        return;
    // Already watching it, through a bind mount
    if (_xfiles_watch_find_wd(ctx, wd) != -1)
        return;

    int idx = ctx->free_dir;
    if (idx != -1)
    {
        ctx->free_dir = ctx->dirs[idx].parent;
    }
    else
    {
        if (ctx->num_dirs == ctx->cap_dirs)
        {
            int             cap  = ctx->cap_dirs ? ctx->cap_dirs * 2 : 64;
            XFilesWatchDir* dirs = (XFilesWatchDir*)XFILES_REALLOC(ctx->dirs, cap * sizeof(*dirs));
            XFILES_ASSERT(dirs != NULL);
            if (dirs == NULL)
            {
                inotify_rm_watch(ctx->fd, wd);
                return;
            }
            ctx->dirs     = dirs;
            ctx->cap_dirs = cap;
        }
        idx = ctx->num_dirs++;
    }
    ctx->dirs[idx].wd     = wd;
    ctx->dirs[idx].parent = parent;
    if (!_xfiles_watch_set_name(ctx, idx, name, namelen))
    {
        _xfiles_watch_remove_tree(ctx, idx);
        return;
    }

    DIR* dir = opendir(path);
    if (dir == NULL)
        return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        const char* child    = entry->d_name;
        size_t      childlen = strlen(child);
        if (strcmp(child, ".") == 0 || strcmp(child, "..") == 0)
            continue;

        bool is_dir = entry->d_type == DT_DIR;
        if (entry->d_type == DT_UNKNOWN)
        {
            struct stat st;
            is_dir = fstatat(dirfd(dir), child, &st, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(st.st_mode);
        }
        if (!is_dir && !send_children)
            continue;
        if (pathlen + 1 + childlen >= PATH_MAX)
            continue;

        path[pathlen] = '/';
        memcpy(path + pathlen + 1, child, childlen + 1);
        if (send_children)
            _xfiles_watch_send(ctx, XFILES_WATCH_CREATED, path, is_dir);
        if (is_dir)
            _xfiles_watch_add_tree(ctx, path, pathlen + 1 + childlen, idx, child, childlen, send_children);
        path[pathlen] = '\0';
    }
    closedir(dir);
}

XFilesWatchContext* xfiles_watch_create(const char* path, void* udata, XFilesWatchCallback cb)
{
    XFILES_ASSERT(path != NULL); // What path should be watched?
    XFILES_ASSERT(cb != NULL);   // Did you forget to write a callback?

    struct XFilesWatchContext* ctx = (struct XFilesWatchContext*)XFILES_MALLOC(sizeof(*ctx));
    if (ctx == NULL)
        return NULL;
    memset(ctx, 0, sizeof(*ctx));
    ctx->free_dir  = -1;
    ctx->moved_dir = -1;
    ctx->udata     = udata;
    ctx->callback  = cb;

    // Non blocking, so flush can read until the queue is empty
    // https://man7.org/linux/man-pages/man2/inotify_init1.2.html
    ctx->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    XFILES_ASSERT(ctx->fd != -1);
    if (ctx->fd == -1)
    {
        XFILES_FREE(ctx);
        return NULL;
    }

    char   root[PATH_MAX];
    size_t rootlen = snprintf(root, sizeof(root), "%s", path);
    while (rootlen > 1 && root[rootlen - 1] == '/')
        root[--rootlen] = '\0';

    if (rootlen < sizeof(root))
        _xfiles_watch_add_tree(ctx, root, rootlen, -1, root, rootlen, false);
    if (ctx->num_dirs == 0 || ctx->dirs[0].wd < 0)
    {
        xfiles_watch_destroy(ctx);
        return NULL;
    }
    return ctx;
}

// Called when we know a folder from IN_MOVED_FROM won't get a matching IN_MOVED_TO
static void _xfiles_watch_resolve_move(struct XFilesWatchContext* ctx)
{
    if (ctx->moved_dir != -1)
        _xfiles_watch_remove_tree(ctx, ctx->moved_dir);
    ctx->moved_dir    = -1;
    ctx->moved_cookie = 0;
}

static void _xfiles_watch_handle(struct XFilesWatchContext* ctx, const struct inotify_event* ev)
{
    // https://man7.org/linux/man-pages/man7/inotify.7.html
    if (ev->mask & IN_Q_OVERFLOW)
    {
        _xfiles_watch_resolve_move(ctx);
        _xfiles_watch_send_overflow(ctx);
        return;
    }

    bool moved_within = (ev->mask & IN_MOVED_TO) && ctx->moved_cookie != 0 && ctx->moved_cookie == ev->cookie;
    if (!moved_within)
        _xfiles_watch_resolve_move(ctx);

    int idx = _xfiles_watch_find_wd(ctx, ev->wd);
    if (idx == -1)
        return;

    // The watch was removed because the folder was deleted, moved out of the tree, or its filesystem was unmounted
    if (ev->mask & IN_IGNORED)
    {
        _xfiles_watch_remove_tree(ctx, idx);
        return;
    }
    // The root itself was moved or deleted
    if (ev->mask & (IN_DELETE_SELF | IN_MOVE_SELF))
    {
        if (idx == 0)
            _xfiles_watch_send_overflow(ctx);
        return;
    }
    if (ev->len == 0)
        return;

    char   path[PATH_MAX];
    size_t dirlen  = _xfiles_watch_dir_path(ctx, idx, path, sizeof(path));
    size_t namelen = strlen(ev->name);
    if (dirlen == 0 || dirlen + 1 + namelen >= sizeof(path))
        return;
    path[dirlen] = '/';
    memcpy(path + dirlen + 1, ev->name, namelen + 1);
    size_t pathlen = dirlen + 1 + namelen;
    bool   is_dir  = (ev->mask & IN_ISDIR) != 0;

    if (ev->mask & IN_CREATE)
    {
        _xfiles_watch_send(ctx, XFILES_WATCH_CREATED, path, is_dir);
        if (is_dir)
            _xfiles_watch_add_tree(ctx, path, pathlen, idx, ev->name, namelen, true);
    }
    else if (ev->mask & IN_MOVED_TO)
    {
        _xfiles_watch_send(ctx, XFILES_WATCH_CREATED, path, is_dir);
        if (is_dir && moved_within && ctx->moved_dir != -1)
        {
            // Renamed or moved within the tree. Its watches came with it
            ctx->dirs[ctx->moved_dir].parent = idx;
            if (!_xfiles_watch_set_name(ctx, ctx->moved_dir, ev->name, namelen))
                _xfiles_watch_remove_tree(ctx, ctx->moved_dir);
        }
        else if (is_dir)
        {
            // Moved in from outside the tree. Or moved within the tree before we got to watch it, eg. 'mkdir a && mv a
            // b && touch b/c', in which case nothing inside has been reported yet
            _xfiles_watch_add_tree(ctx, path, pathlen, idx, ev->name, namelen, moved_within);
        }
        ctx->moved_dir    = -1;
        ctx->moved_cookie = 0;
    }
    else if (ev->mask & IN_MOVED_FROM)
    {
        if (is_dir)
        {
            ctx->moved_dir    = _xfiles_watch_find_child(ctx, idx, ev->name);
            ctx->moved_cookie = ev->cookie;
        }
        _xfiles_watch_send(ctx, XFILES_WATCH_DELETED, path, is_dir);
    }
    else if (ev->mask & IN_DELETE)
    {
        // The folder's own watch gets IN_IGNORED, which removes it
        _xfiles_watch_send(ctx, XFILES_WATCH_DELETED, path, is_dir);
    }
    else if (ev->mask & IN_CLOSE_WRITE)
    {
        // A file opened for writing was closed. Unlike IN_MODIFY this fires once per save rather than once per
        // write(), and the file is no longer half written. It misses writes through mmap and files that are kept open
        _xfiles_watch_send(ctx, XFILES_WATCH_MODIFIED, path, false);
    }
}

void xfiles_watch_flush(XFilesWatchContext* _ctx)
{
    XFILES_ASSERT(_ctx != NULL);
    struct XFilesWatchContext* ctx = (struct XFilesWatchContext*)_ctx;

    for (;;)
    {
        // -1 with EAGAIN when the queue is empty
        ssize_t nread = read(ctx->fd, ctx->buf, sizeof(ctx->buf));
        if (nread <= 0)
            break;

        // Writing a new file arrives as IN_CREATE followed by IN_CLOSE_WRITE. Skip the IN_CLOSE_WRITE so it's only
        // reported as CREATED. If the two arrive in different reads you'll get CREATED then MODIFIED
        const struct inotify_event* last_created = NULL;
        for (ssize_t off = 0; off < nread;)
        {
            const struct inotify_event* ev  = (const struct inotify_event*)(ctx->buf + off);
            off                            += sizeof(*ev) + ev->len;

            bool skip = (ev->mask & IN_CLOSE_WRITE) && last_created && last_created->wd == ev->wd &&
                        strcmp(last_created->name, ev->name) == 0;
            if (!skip)
                _xfiles_watch_handle(ctx, ev);
            last_created = (ev->mask & IN_CREATE) ? ev : NULL;
        }
    }

    // The kernel queues IN_MOVED_FROM and IN_MOVED_TO together, so by now we've seen both if there was going to be one
    _xfiles_watch_resolve_move(ctx);
}

void xfiles_watch_destroy(XFilesWatchContext* _ctx)
{
    XFILES_ASSERT(_ctx != NULL);
    struct XFilesWatchContext* ctx = (struct XFilesWatchContext*)_ctx;
    // Closing the inotify instance removes all of its watches
    if (ctx->fd != -1)
        close(ctx->fd);
    XFILES_FREE(ctx->dirs);
    XFILES_FREE(ctx->names);
    XFILES_FREE(ctx);
}

#endif // __linux__

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
