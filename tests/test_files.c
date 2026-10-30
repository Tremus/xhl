// Tests xhl/files.h
//
// Creates a uniquely named folder in the OS temp directory, and moves it to the Trash/Recycle Bin when finished
//
// Not tested: xfiles_open_file_explorer & xfiles_select_in_file_explorer, because they open windows. Try them by hand.
// See files_watch.c for a much more thorough test of xfiles_watch_*
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define XHL_FILES_IMPL
#include "../include/xhl/files.h"

#include "xtest.h"

static void sleep_ms(unsigned ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

static void join_path(char* out, size_t outlen, const char* dir, const char* name)
{
    snprintf(out, outlen, "%s%s%s", dir, XFILES_DIR_STR, name);
}

static bool ends_with(const char* str, const char* suffix)
{
    size_t len        = strlen(str);
    size_t suffix_len = strlen(suffix);
    return len >= suffix_len && strcmp(str + len - suffix_len, suffix) == 0;
}

typedef struct ListResult
{
    int num_files;
    int num_dirs;
    int num_txt;
} ListResult;

static void on_list_item(void* data, const xfiles_list_item_t* item)
{
    ListResult* res  = data;
    const char* name = item->path + item->name_idx;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return;
    if (item->is_dir)
        res->num_dirs++;
    else
        res->num_files++;
    if (strcmp(item->path + item->ext_idx, ".txt") == 0)
        res->num_txt++;
}

typedef struct WatchResult
{
    bool created;
    bool modified;
    bool deleted;
} WatchResult;

static void on_watch_event(const XFilesWatchEvent* event, void* udata)
{
    WatchResult* res = udata;
    fprintf(stderr, "  watch event %d: %s\n", (int)event->type, event->path);
    if (!ends_with(event->path, "watched.txt"))
        return;
    if (event->type == XFILES_WATCH_CREATED)
        res->created = true;
    if (event->type == XFILES_WATCH_MODIFIED)
        res->modified = true;
    if (event->type == XFILES_WATCH_DELETED)
        res->deleted = true;
}

// Events are delivered asynchronously on some platforms, so keep flushing until one arrives or 3 seconds pass
static void flush_until(XFilesWatchContext* ctx, const bool* flag)
{
    for (int i = 0; i < 300 && !*flag; i++)
    {
        xfiles_watch_flush(ctx);
        if (!*flag)
            sleep_ms(10);
    }
}

int main(void)
{
    char root[512];
    char path_a[640];
    char path_b[640];
    char path_c[640];
    char subdir[640];
    if (!xtest_temp_path(root, sizeof(root), "xhl_test_files"))
    {
        fprintf(stderr, "Couldn't find the temp directory\n");
        return 1;
    }
    join_path(path_a, sizeof(path_a), root, "a.txt");
    join_path(path_b, sizeof(path_b), root, "b.txt");
    join_path(subdir, sizeof(subdir), root, "sub");
    join_path(path_c, sizeof(path_c), subdir, "c.bin");

    fprintf(stderr, "Testing in: %s\n", root);
    XTEST_ASSERT(!xfiles_exists(root)); // Left over from a crashed run? Delete it and run again

    // Names. Platform independent, but cheap to check here
    XTEST_ASSERT(strcmp(xfiles_get_name(path_a), "a.txt") == 0);
    XTEST_ASSERT(strcmp(xfiles_get_extension("a.txt"), ".txt") == 0);
    XTEST_ASSERT(xfiles_get_extension("noext") == NULL);

    // Directories
    {
        XTEST_ASSERT(xfiles_create_directory(root));
        XTEST_ASSERT(xfiles_exists(root));
        XTEST_ASSERT(xfiles_is_directory(root));
        XTEST_ASSERT(xfiles_create_directory(root)); // Already exists, still true

        char deep[700];
        join_path(deep, sizeof(deep), root, "x" XFILES_DIR_STR "y" XFILES_DIR_STR "z");
        XTEST_ASSERT(xfiles_create_directory_recursive(deep));
        XTEST_ASSERT(xfiles_is_directory(deep));

        XTEST_ASSERT(xfiles_create_directory(subdir));
    }

    // Write, append, read
    {
        static const char hello[] = "Hello";
        static const char world[] = " World!";
        XTEST_ASSERT(xfiles_write(path_a, hello, strlen(hello)));
        XTEST_ASSERT(xfiles_exists(path_a));
        XTEST_ASSERT(!xfiles_is_directory(path_a));
        XTEST_ASSERT(xfiles_append(path_a, world, strlen(world)));

        void*  data    = NULL;
        size_t datalen = 0;
        XTEST_ASSERT(xfiles_read(path_a, &data, &datalen));
        XTEST_ASSERT(datalen == strlen(hello) + strlen(world));
        XTEST_ASSERT(data != NULL && memcmp(data, "Hello World!", datalen) == 0);
        xfiles_free(data);

        // Write replaces old contents
        XTEST_ASSERT(xfiles_write(path_a, "Hi", 2));
        XTEST_ASSERT(xfiles_read(path_a, &data, &datalen));
        XTEST_ASSERT(datalen == 2 && memcmp(data, "Hi", 2) == 0);
        xfiles_free(data);

        XTEST_ASSERT(xfiles_write(path_c, "0123456789", 10));
    }

    // Streams
    {
        XFilesStream s = xfiles_stream_open(path_c);
        XTEST_ASSERT(s.is_open);
        XTEST_ASSERT(s.size == 10);

        char buf[16] = {0};
        XTEST_ASSERT(xfiles_stream_read(&s, buf, 4) == 4);
        XTEST_ASSERT(memcmp(buf, "0123", 4) == 0);

        XTEST_ASSERT(xfiles_stream_seek(&s, -3, XFILES_SEEK_END));
        XTEST_ASSERT(s.pos == 7);
        XTEST_ASSERT(xfiles_stream_read(&s, buf, sizeof(buf)) == 3);
        XTEST_ASSERT(memcmp(buf, "789", 3) == 0);
        XTEST_ASSERT(xfiles_stream_read(&s, buf, sizeof(buf)) == 0); // EOF

        XTEST_ASSERT(xfiles_stream_seek(&s, 2, XFILES_SEEK_BEGIN));
        XTEST_ASSERT(xfiles_stream_seek(&s, 3, XFILES_SEEK_CURRENT));
        XTEST_ASSERT(xfiles_stream_read(&s, buf, 1) == 1 && buf[0] == '5');

        xfiles_stream_close(&s);
    }

    // Move
    {
        XTEST_ASSERT(xfiles_move(path_a, path_b, XFILES_MOVE_DEFAULT));
        XTEST_ASSERT(!xfiles_exists(path_a));
        XTEST_ASSERT(xfiles_exists(path_b));

        // Replace an existing file
        XTEST_ASSERT(xfiles_write(path_a, "new", 3));
        XTEST_ASSERT(xfiles_move(path_a, path_b, XFILES_MOVE_OVERWRITE));
        void*  data    = NULL;
        size_t datalen = 0;
        XTEST_ASSERT(xfiles_read(path_b, &data, &datalen));
        XTEST_ASSERT(datalen == 3 && memcmp(data, "new", 3) == 0);
        xfiles_free(data);

        XTEST_ASSERT(xfiles_write(path_a, "a", 1));
    }

    // Metadata
    {
        XFilesMetadata m = xfiles_get_metadata(path_c);
        XTEST_ASSERT(m.last_error == 0);
        XTEST_ASSERT(m.type == XFILES_METADATA_TYPE_FILE);
        XTEST_ASSERT(m.size_bytes == 10);
        XTEST_ASSERT(m.modification_time_ns > 0);
        XTEST_ASSERT(m.creation_time_ns > 0);
        XTEST_ASSERT(!m.is_symlink);

        XFilesMetadata d = xfiles_get_metadata(subdir);
        XTEST_ASSERT(d.type == XFILES_METADATA_TYPE_DIRECTORY);
    }

    // List. root contains a.txt, b.txt, sub/ and x/
    {
        ListResult res = {0};
        xfiles_list(root, &res, on_list_item);
        XTEST_ASSERT(res.num_files == 2);
        XTEST_ASSERT(res.num_dirs == 2);
        XTEST_ASSERT(res.num_txt == 2);
    }

    // Delete
    {
        XTEST_ASSERT(xfiles_delete(path_a));
        XTEST_ASSERT(!xfiles_exists(path_a));
    }

    // Watch
    {
        char watched[640];
        join_path(watched, sizeof(watched), root, "watched.txt");

        WatchResult         res = {0};
        XFilesWatchContext* ctx = xfiles_watch_create(root, &res, on_watch_event);
        XTEST_ASSERT(ctx != NULL);
        if (ctx)
        {
            sleep_ms(100); // Let the watcher settle before making changes

            XTEST_ASSERT(xfiles_write(watched, "1", 1));
            flush_until(ctx, &res.created);
            XTEST_ASSERT(res.created);

            // Separate the write from the create, otherwise they may be coalesced into one event
            sleep_ms(100);
            XTEST_ASSERT(xfiles_append(watched, "2", 1));
            flush_until(ctx, &res.modified);
            XTEST_ASSERT(res.modified);

            XTEST_ASSERT(xfiles_delete(watched));
            flush_until(ctx, &res.deleted);
            XTEST_ASSERT(res.deleted);

            xfiles_watch_destroy(ctx);
        }
    }

    // User directories
    {
        char home[512];
        int  homelen = xfiles_get_user_directory(home, sizeof(home), XFILES_USER_DIRECTORY_HOME);
        XTEST_ASSERT(homelen > 0);
        XTEST_ASSERT(homelen == (int)strlen(home));
        XTEST_ASSERT(xfiles_is_directory(home));

        for (int i = 0; i < XFILES_USER_DIRECTORY_COUNT; i++)
        {
            char dir[512];
            int  len = xfiles_get_user_directory(dir, sizeof(dir), (XFilesUserDirectory)i);
            fprintf(stderr, "  user directory %d: %s\n", i, dir);
            XTEST_ASSERT(len > 0);
            // Not checking these exist. Minimal installs may not have eg. ~/Videos
        }
    }

    // Trash. Puts a single file in the bin, then the whole test folder
    {
        XTEST_ASSERT(xfiles_trash(path_b));
        XTEST_ASSERT(!xfiles_exists(path_b));

        XTEST_ASSERT(xfiles_trash(root));
        XTEST_ASSERT(!xfiles_exists(root));
    }

    return xtest_finish("test_files");
}
