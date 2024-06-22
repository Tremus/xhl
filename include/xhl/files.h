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
 * MAX_PATH (260) wide characters using stack memory to avoid using malloc. If you need longer paths than this, you will
 * have to change this yourself!
 * https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file#maximum-path-length-limitation
 *
 * BUILDING:
 * Simply #define XHL_FILES_IMPL in one of your build targets before including this header
 * MacOS: Requires Objective-C/Objective-C++ for some functions
 *
 * LINKING:
 * Windows: Kernel32 Shlwapi
 * macOS: -framework AppKit
 */
#ifndef XHL_FILES_H
#define XHL_FILES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
#define XFILES_DIR_STR  "\\"
#define XFILES_DIR_CHAR '\\'
#else
#define XFILES_DIR_STR  "/"
#define XFILES_DIR_CHAR '/'
#endif

#define XFILES_ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

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

// Returns only true on success and sets 'outbuffer' and 'outbufferlen' with file contents and size
// Must release 'outbuffer' with free()
bool xfiles_read(const char* path, void** outbuffer, size_t* outbufferlen);
// Creates file if it doesn't exist with default access permissions.
// If file already exists, it overwrites all contents.
bool xfiles_write(const char* path, const void* buffer, size_t bufferlen);

// Moves the file to:
// Win: Recycle Bin /
// OSX: Trash
bool xfiles_delete_safely(const char* path);
// No bins. File is deleted and is likely unrecoverable
bool xfiles_delete_permanently(const char* path);
// Opens OS file browsing app with path selected
// Win: File Explorer /
// OSX: Finder
bool xfiles_open_file_explorer(const char* path);

#ifdef __cplusplus
}
#endif

#ifdef XHL_FILES_IMPL

#if ! defined(XFILES_MALLOC) || ! defined(XFILES_FREE)
#include <stdlib.h>
#define XFILES_MALLOC(size) malloc(size)
#define XFILES_FREE(ptr)    free(ptr)
#endif

#ifdef _WIN32
#include <Windows.h>
#include <shlwapi.h>

bool xfiles_exists(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-pathfileexistsw
    // https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar

    WCHAR pathunicode[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, pathunicode, XFILES_ARRLEN(pathunicode)))
        return PathFileExistsW(pathunicode);
    return false;
}

bool xfiles_create_directory(const char* path)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createdirectoryw
    WCHAR DirPath[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, DirPath, XFILES_ARRLEN(DirPath)))
        return CreateDirectoryW(DirPath, 0);
    return false;
}

bool xfiles_read(const char* path, void** outbuffer, size_t* outbufferlen)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfilesizeex
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile

    UINT8*        data     = NULL;
    HANDLE        hFile    = NULL;
    LARGE_INTEGER FileSize = {0};
    BOOL          ok       = FALSE;

    WCHAR FileName[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FileName, XFILES_ARRLEN(FileName)))
    {
        hFile = CreateFileW(
            FileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            NULL);

        if (hFile != INVALID_HANDLE_VALUE)
        {
            ok = GetFileSizeEx(hFile, &FileSize);
            if (ok)
            {
                data = XFILES_MALLOC(FileSize.QuadPart);
                ok   = ReadFile(hFile, data, FileSize.QuadPart, NULL, NULL);

                if (ok)
                {
                    *outdata    = data;
                    *outdatalen = FileSize.QuadPart;
                }
                else
                {
                    XFILES_FREE(data);
                    data              = NULL;
                    FileSize.QuadPart = 0;
                }
            }
            CloseHandle(hFile);
        }
    }

    return ok;
}

bool xfiles_write(const char* path, const void* buffer, size_t bufferlen)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile

    WCHAR  FilePath[MAX_PATH];
    HANDLE hFile         = NULL;
    BOOL   ok            = FALSE;
    DWORD  nBytesWritten = 0;

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, FilePath, XFILES_ARRLEN(FilePath)))
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
            ok = WriteFile(hFile, data, datalen, &nBytesWritten, NULL);
            XFILES_ASSERT(ok);
            CloseHandle(hFile);
        }
    }

    return ok;
}

// TODO
bool xfiles_delete_safely(const char* path);
bool xfiles_delete_permanently(const char* path);
bool xfiles_open_file_explorer(const char* path);

#endif

#ifdef __APPLE__
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef XFILES_ASSERT
#ifdef NDEBUG
// clang-format off
#define XFILES_ASSERT(cond) do { (void)(cond); } while (0)
// clang-format on
#else
#include <assert.h>
#define XFILES_ASSERT(cond) (cond) ? (void)0 : __builtin_debugtrap()
#endif // NDEBUG
#endif // XFILES_ASSERT

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/access.2.html
bool xfiles_exists(const char* path) { return access(path, F_OK) == 0; }
// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/mkdir.2.html
bool xfiles_create_directory(const char* path) { return mkdir(path, 0777) == 0; }

bool xfiles_read(const char* path, void** outbuffer, size_t* outbufferlen)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/open.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fstat.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/read.2.html
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/close.2.html

    int         fd        = -1;
    int         ret       = -1;
    struct stat info      = {0};
    void*       data      = NULL;
    ssize_t     readBytes = -1;

    fd = open(path, O_RDONLY);
    if (fd != -1)
    {
        ret = fstat(fd, &info);
        if (ret != -1)
        {
            data      = XFILES_MALLOC(info.st_size);
            readBytes = read(fd, data, info.st_size);

            if (readBytes == -1)
            {
                XFILES_FREE(data);
                data = NULL;
            }
            else
            {
                *outbuffer    = data;
                *outbufferlen = info.st_size;
            }
        }
        close(fd);
    }

    return readBytes != -1;
}

bool xfiles_write(const char* path, const void* buffer, size_t bufferlen)
{
    // https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/write.2.html#//apple_ref/doc/man/2/write
    int     fd;
    ssize_t nwritten = -1;

    fd = open(path, O_WRONLY | O_CREAT, 0777);
    if (fd != -1)
    {
        nwritten = write(fd, buffer, bufferlen);
        close(fd);
    }
    return nwritten != -1;
}

// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/unlink.2.html
bool xfiles_delete_permanently(const char* path) { return unlink(path) == 0; }

#ifdef __OBJC__
#import <AppKit/NSWorkspace.h>

bool xfiles_delete_safely(const char* path)
{
    NSString* str     = [[NSString alloc] initWithString:@(path)];
    NSURL*    itemUrl = [[NSURL fileURLWithPath:str isDirectory:FALSE] retain];

    const NSFileManager* fm           = [NSFileManager defaultManager]; // strong
    NSURL*               resultingURL = NULL;
    NSError*             error        = NULL;

    BOOL ok = NO;

    // Apparently a long standing bug in macOS is that this function will not give you the give 'Put Back' feature
    // inside Trash that you would normally see if you deleted the file using the Finder app. Such a shame!
    // https://openradar.appspot.com/radar?id=5063396789583872
    ok = [fm trashItemAtURL:itemUrl resultingItemURL:&resultingURL error:&error];

    [fm release];
    [itemUrl release];
    [str release];
    return ok;
}

bool xfiles_open_file_explorer(const char* path)
{
    BOOL ok = [[NSWorkspace sharedWorkspace] openFile:@(path) withApplication:@"Finder"];
    return ok;
}
#endif // __OBJC__
#endif // __APPLE__

const char* xfiles_get_name(const char* path)
{
    const char* name = path;
    for (const char* c = path; *c != '\0'; c++)
        if (*c == XFILES_DIR_CHAR)
            name = c + 1;
    return name != path ? NULL : name;
}

const char* xfiles_get_extension(const char* name)
{
    const char* extension = name;
    for (const char* c = name; *c != '\0'; c++)
        if (*c == '.')
            extension = c;
    return extension != name ? NULL : extension;
}

bool xfiles_create_directory_recursive(const char* path)
{
    if (! xfiles_exists(path))
    {
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
                if (! xfiles_exists(nextpath))
                    xfiles_create_directory(nextpath);
                XFILES_ASSERT(xfiles_exists(nextpath));
                nextpath[i] = XFILES_DIR_CHAR;
            }
        }
        bool ok = xfiles_create_directory(path);
        XFILES_ASSERT(ok);
        return ok;
    }
    return true;
}

#endif // XHL_FILES_IMPL
#endif // XHL_FILES_H