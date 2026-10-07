// Tests xfiles_watch_* in xhl/files.h. FSEvents on macOS, ReadDirectoryChangesW on Windows
//
// Build (macOS):   clang -x objective-c files_watch.c -framework AppKit -o files_watch
// Build (Windows): clang files_watch.c -o files_watch.exe
// Run it from a folder you don't mind it creating ./temp and ./temp_outside in. Both are moved to the Trash/Recycle Bin
// when it finishes
//
// All file operations go through xfiles, so this also tests xfiles_write, xfiles_append, xfiles_move, xfiles_delete
// and xfiles_trash
//
// macOS: FSEvents uses one stream for the whole tree instead of one file descriptor per entry, so the number of open
// fds should stay flat no matter how many files are watched.
// Windows: ReadDirectoryChangesW uses one directory handle, so the number of open handles should stay flat.
//
// This program:
//   1. Creates ./temp and watches it with xfiles_watch_create()
//   2. Runs a set of scenarios (create, modify, rename, move, atomic save, move in/out, replace, delete, trash,
//      folders) and checks each one produces the expected events. Renames and moves are expected as DELETED + CREATED
//   3. Each round, creates temp/dir_NNNN, then 64 files inside it, then appends to the last file. The append must be
//      reported as MODIFIED, not CREATED
//   4. After each step it polls xfiles_watch_flush() until the callbacks it expected fire, or a timeout passes.
//      FSEvents delivers asynchronously with a 50ms latency, so a single flush straight after the change isn't enough
//   5. Stops after ~10000 entries, or one round after the first round where the watcher misses something
//
// Usage: files_watch [soft_fd_limit]
//   macOS only. Pass 256 to simulate an app launched from Finder/Dock (launchd's default soft limit, see `launchctl
//   limit maxfiles`). With no argument the limit inherited from your shell is used.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#define PATH_MAX 260
#pragma comment(lib, "shell32.lib") // xfiles_trash()
#pragma comment(lib, "user32.lib")  // xtime_timer_*()
#else
#include <fcntl.h>
#include <limits.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

static int  g_assert_failures = 0;
static void on_xfiles_assert(const char* cond, int line)
{
    if (g_assert_failures < 5)
    {
#ifdef _WIN32
        fprintf(stderr, "    xfiles assert failed: line %d (%s) GetLastError=%lu\n", line, cond, GetLastError());
#else
        fprintf(stderr, "    xfiles assert failed: line %d (%s) errno=%d (%s)\n", line, cond, errno, strerror(errno));
#endif
    }
    else if (g_assert_failures == 5)
        fprintf(stderr, "    ... further xfiles assert failures suppressed\n");
    g_assert_failures++;
}
#define XFILES_ASSERT(cond) ((cond) ? (void)0 : on_xfiles_assert(#cond, __LINE__))

#define XHL_FILES_IMPL
#include "../include/xhl/files.h"
#define XHL_TIME_IMPL
#include "../include/xhl/time.h"

#ifdef _WIN32
// Windows can't inspect a path that no longer exists, so is_dir is always false for DELETED events
#define DELETED_IS_DIR false
// ReadDirectoryChangesW reports writing to a new file as CREATED followed by MODIFIED
#define MODIFIED_AFTER_CREATED_OK true
#define HANDLE_NAME               "handles"
#else
#define DELETED_IS_DIR            true
#define MODIFIED_AFTER_CREATED_OK false
#define HANDLE_NAME               "fds"
#endif

// Not a real event type. Passes if the last event for the path was CREATED or MODIFIED, and allows any other events
// for that path before it. Used where the docs only promise "some mix" of events
#define EXPECT_CHANGED ((enum XFilesWatchType)100)

// A file replaced by moving another file over it. macOS reports MODIFIED. Windows reports some mix of DELETED, CREATED
// and MODIFIED
#ifdef _WIN32
#define REPLACED_TYPE EXPECT_CHANGED
#else
#define REPLACED_TYPE XFILES_WATCH_MODIFIED
#endif

enum
{
    FILES_PER_DIR = 64,
    MAX_ROUNDS    = 154, // 154 * 65 = 10010 entries
    TIMEOUT_MS    = 2000,
};

static char g_root[PATH_MAX];
static char g_outside[PATH_MAX]; // A folder next to the watched one, for moving files in and out
static char g_round_dir[PATH_MAX];
static char g_modify_target[PATH_MAX];

static int      g_created_dir;
static uint64_t g_created_files_mask; // One bit per file_NN.txt. FSEvents may report CREATED more than once
static int      g_modified;
static int      g_modified_as_created;
static int      g_overflows;

// Every event received while a scenario is running
typedef struct LoggedEvent
{
    enum XFilesWatchType type;
    bool                 is_dir;
    char                 path[PATH_MAX];
} LoggedEvent;
static LoggedEvent g_log[64];
static int         g_log_len;
static bool        g_logging;

static const char* type_name(enum XFilesWatchType type)
{
    switch (type)
    {
    case XFILES_WATCH_UNKNOWN:
        return "UNKNOWN";
    case XFILES_WATCH_CREATED:
        return "CREATED";
    case XFILES_WATCH_DELETED:
        return "DELETED";
    case XFILES_WATCH_MODIFIED:
        return "MODIFIED";
    case XFILES_WATCH_OVERFLOW:
        return "OVERFLOW";
    }
    if (type == EXPECT_CHANGED)
        return "CREATED or MODIFIED (last)";
    return "?";
}

static int popcount64(uint64_t x) { return __builtin_popcountll(x); }

static void on_watch(const XFilesWatchEvent* event, void* udata)
{
    (void)udata;
    enum XFilesWatchType type   = event->type;
    const char*          path   = event->path;
    size_t               dirlen = strlen(g_round_dir);

    if (g_logging && g_log_len < (int)(sizeof(g_log) / sizeof(g_log[0])))
    {
        LoggedEvent* e = &g_log[g_log_len++];
        e->type        = type;
        e->is_dir      = event->is_dir;
        snprintf(e->path, sizeof(e->path), "%s", path);
    }

    if (type == XFILES_WATCH_OVERFLOW)
    {
        g_overflows++;
        return;
    }

    if (strcmp(path, g_modify_target) == 0 && g_modify_target[0])
    {
        if (type == XFILES_WATCH_MODIFIED)
            g_modified++;
        else if (type == XFILES_WATCH_CREATED)
            g_modified_as_created++;
        return;
    }

    if (type == XFILES_WATCH_CREATED)
    {
        int idx;
        if (strcmp(path, g_round_dir) == 0)
            g_created_dir++;
        else if (
            strncmp(path, g_round_dir, dirlen) == 0 &&
            sscanf(path + dirlen, XFILES_DIR_STR "file_%d.txt", &idx) == 1 && idx >= 0 && idx < FILES_PER_DIR)
            g_created_files_mask |= 1ull << idx;
    }
}

static uint64_t now_ms(void) { return xtime_now_ns() / 1000000; }

static bool cond_dir(void) { return g_created_dir > 0; }
static bool cond_files(void) { return popcount64(g_created_files_mask) == FILES_PER_DIR; }
static bool cond_modified(void) { return g_modified > 0; }

typedef struct Expect
{
    enum XFilesWatchType type;
    const char*          path;
    bool                 is_dir;
} Expect;
static const Expect* g_expect;
static int           g_expect_len;

static bool expect_found(const Expect* x)
{
    if (x->type == EXPECT_CHANGED)
    {
        for (int i = g_log_len - 1; i >= 0; i--)
            if (strcmp(g_log[i].path, x->path) == 0)
                return (g_log[i].type == XFILES_WATCH_CREATED || g_log[i].type == XFILES_WATCH_MODIFIED) &&
                       g_log[i].is_dir == x->is_dir;
        return false;
    }
    for (int i = 0; i < g_log_len; i++)
        if (g_log[i].type == x->type && g_log[i].is_dir == x->is_dir && strcmp(g_log[i].path, x->path) == 0)
            return true;
    return false;
}

static bool cond_expected(void)
{
    for (int i = 0; i < g_expect_len; i++)
        if (!expect_found(&g_expect[i]))
            return false;
    return true;
}

// Flush until 'done' returns true or the timeout passes
static void flush_until(XFilesWatchContext* ctx, bool (*done)(void))
{
    uint64_t start = now_ms();
    for (;;)
    {
        xfiles_watch_flush(ctx);
        if (done())
            break;
        if (now_ms() - start > TIMEOUT_MS)
            break;
        xtime_sleep_ms(5);
    }
}

static void flush_for(XFilesWatchContext* ctx, int ms)
{
    uint64_t start = now_ms();
    while (now_ms() - start < (uint64_t)ms)
    {
        xfiles_watch_flush(ctx);
        xtime_sleep_ms(5);
    }
}

// Open fds on macOS, open handles on Windows
static int count_open_handles(int limit)
{
#ifdef _WIN32
    (void)limit;
    DWORD n = 0;
    GetProcessHandleCount(GetCurrentProcess(), &n);
    return (int)n;
#else
    int n = 0;
    for (int fd = 0; fd < limit; fd++)
        if (fcntl(fd, F_GETFD) != -1)
            n++;
    return n;
#endif
}

static void remove_temp(void)
{
    if (xfiles_exists(g_root))
        xfiles_trash(g_root);
    if (xfiles_exists(g_outside))
        xfiles_trash(g_outside);
}

// Lets the events of any setup drain, then starts logging. Do the action after this, then call scenario_end()
static void scenario_begin(XFilesWatchContext* ctx)
{
    flush_for(ctx, 300);
    g_log_len = 0;
    g_logging = true;
}

// Checks the action produced every expected event. Also fails if an expected path got an event of any other type, eg.
// MODIFIED when we expected CREATED
static bool scenario_end(XFilesWatchContext* ctx, const char* name, const Expect* expect, int expect_len)
{
    g_expect     = expect;
    g_expect_len = expect_len;
    flush_until(ctx, cond_expected);
    flush_for(ctx, 200); // Catch any extra events
    g_logging = false;

    bool ok = cond_expected();
    for (int i = 0; i < g_log_len; i++)
    {
        bool same_path = false, same_type = false;
        for (int j = 0; j < expect_len; j++)
        {
            if (strcmp(g_log[i].path, expect[j].path) == 0)
            {
                same_path  = true;
                same_type |= g_log[i].type == expect[j].type || expect[j].type == EXPECT_CHANGED;
                same_type |= MODIFIED_AFTER_CREATED_OK && g_log[i].type == XFILES_WATCH_MODIFIED &&
                             expect[j].type == XFILES_WATCH_CREATED;
            }
        }
        if (same_path && !same_type)
            ok = false;
    }

    fprintf(stderr, "%-4s %s\n", ok ? "ok" : "FAIL", name);
    if (!ok)
        for (int i = 0; i < expect_len; i++)
            fprintf(
                stderr,
                "     expected: %s%s %s\n",
                type_name(expect[i].type),
                expect[i].is_dir ? " [dir]" : "",
                expect[i].path);
    for (int i = 0; i < g_log_len; i++)
        fprintf(stderr, "     got:      %s%s %s\n", type_name(g_log[i].type), g_log[i].is_dir ? " [dir]" : "", g_log[i].path);
    return ok;
}

// Expected events are passed as {type, path, is_dir} initialisers
#define SCENARIO_END(name, ...)                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        const Expect expect_[] = {__VA_ARGS__};                                                                        \
        failures += !scenario_end(ctx, name, expect_, (int)(sizeof(expect_) / sizeof(expect_[0])));                    \
        count++;                                                                                                       \
    }                                                                                                                  \
    while (0)

#define STR(s) s, (sizeof(s) - 1)

static int run_scenarios(XFilesWatchContext* ctx)
{
    char S[PATH_MAX], O[PATH_MAX];
    snprintf(S, sizeof(S), "%s" XFILES_DIR_STR "scenarios", g_root);
    snprintf(O, sizeof(O), "%s", g_outside);

    char a[PATH_MAX], b[PATH_MAX], sub[PATH_MAX], subb[PATH_MAX], tmp[PATH_MAX], c[PATH_MAX], d[PATH_MAX],
        e[PATH_MAX], f[PATH_MAX], pre[PATH_MAX], ob[PATH_MAX], oc[PATH_MAX];
    snprintf(a, sizeof(a), "%s" XFILES_DIR_STR "a.txt", S);
    snprintf(b, sizeof(b), "%s" XFILES_DIR_STR "b.txt", S);
    snprintf(sub, sizeof(sub), "%s" XFILES_DIR_STR "sub", S);
    snprintf(subb, sizeof(subb), "%s" XFILES_DIR_STR "b.txt", sub);
    snprintf(tmp, sizeof(tmp), "%s" XFILES_DIR_STR ".b.txt.tmp", sub);
    snprintf(c, sizeof(c), "%s" XFILES_DIR_STR "c.txt", S);
    snprintf(d, sizeof(d), "%s" XFILES_DIR_STR "d", S);
    snprintf(e, sizeof(e), "%s" XFILES_DIR_STR "e", S);
    snprintf(f, sizeof(f), "%s" XFILES_DIR_STR "f.txt", S);
    snprintf(pre, sizeof(pre), "%s" XFILES_DIR_STR "pre_existing.txt", g_root);
    snprintf(ob, sizeof(ob), "%s" XFILES_DIR_STR "b.txt", O);
    snprintf(oc, sizeof(oc), "%s" XFILES_DIR_STR "c.txt", O);

    int failures = 0;
    int count    = 0;

    fprintf(stderr, "Scenarios:\n");

    {
        scenario_begin(ctx);
        xfiles_append(pre, STR("more\n"));
        SCENARIO_END("modify a file that existed before the watch started", {XFILES_WATCH_MODIFIED, pre, false});
    }

    {
        xfiles_create_directory_recursive(sub);
        scenario_begin(ctx);
        xfiles_write(a, STR("hi\n"));
        SCENARIO_END("create file", {XFILES_WATCH_CREATED, a, false});
    }

    {
        scenario_begin(ctx);
        xfiles_append(a, STR("more\n"));
        SCENARIO_END("append to file", {XFILES_WATCH_MODIFIED, a, false});
    }

    {
        scenario_begin(ctx);
        xfiles_move(a, b, XFILES_MOVE_DEFAULT);
        SCENARIO_END("rename file", {XFILES_WATCH_DELETED, a, false}, {XFILES_WATCH_CREATED, b, false});
    }

    {
        scenario_begin(ctx);
        xfiles_append(b, STR("more\n"));
        SCENARIO_END("append to renamed file", {XFILES_WATCH_MODIFIED, b, false});
    }

    {
        scenario_begin(ctx);
        xfiles_move(b, subb, XFILES_MOVE_DEFAULT);
        SCENARIO_END(
            "move file to another folder",
            {XFILES_WATCH_DELETED, b, false},
            {XFILES_WATCH_CREATED, subb, false});
    }

    {
        // How many apps (eg. text editors) save files. The path never stops existing, but it's a different file
        // underneath afterwards. The temp file may or may not be reported as CREATED before it's renamed, so only the
        // saved file is checked
        scenario_begin(ctx);
        xfiles_write(tmp, STR("saved\n"));
        xfiles_move(tmp, subb, XFILES_MOVE_OVERWRITE);
        SCENARIO_END("atomic save (write temp file, rename over original)", {REPLACED_TYPE, subb, false});
    }

    {
        // Checks the watcher didn't lose track of the file when it was replaced
        scenario_begin(ctx);
        xfiles_append(subb, STR("more\n"));
        SCENARIO_END("append to atomically saved file", {XFILES_WATCH_MODIFIED, subb, false});
    }

    {
        xfiles_create_directory(O);
        scenario_begin(ctx);
        xfiles_move(subb, ob, XFILES_MOVE_DEFAULT);
        SCENARIO_END("move file out of the watched folder", {XFILES_WATCH_DELETED, subb, false});
    }

    {
        xfiles_write(oc, STR("x\n"));
        scenario_begin(ctx);
        xfiles_move(oc, c, XFILES_MOVE_DEFAULT);
        SCENARIO_END("move file into the watched folder", {XFILES_WATCH_CREATED, c, false});
    }

    {
        scenario_begin(ctx);
        xfiles_append(c, STR("more\n"));
        SCENARIO_END("append to moved in file", {XFILES_WATCH_MODIFIED, c, false});
    }

    {
        // Eg. dragging a file in from another folder and choosing "Replace"
        xfiles_write(oc, STR("replacement\n"));
        scenario_begin(ctx);
        xfiles_move(oc, c, XFILES_MOVE_OVERWRITE);
        SCENARIO_END("move file into the watched folder, replacing an existing file", {REPLACED_TYPE, c, false});
    }

    {
        scenario_begin(ctx);
        xfiles_append(c, STR("more\n"));
        SCENARIO_END("append to replaced file", {XFILES_WATCH_MODIFIED, c, false});
    }

    {
        scenario_begin(ctx);
        xfiles_delete(c);
        SCENARIO_END("delete file", {XFILES_WATCH_DELETED, c, false});
    }

    {
        xfiles_write(f, STR("x\n"));
        scenario_begin(ctx);
        xfiles_trash(f);
        SCENARIO_END("trash file", {XFILES_WATCH_DELETED, f, false});
    }

    {
        scenario_begin(ctx);
        xfiles_create_directory(d);
        SCENARIO_END("create folder", {XFILES_WATCH_CREATED, d, true});
    }

    {
        scenario_begin(ctx);
        xfiles_move(d, e, XFILES_MOVE_DEFAULT);
        SCENARIO_END("rename folder", {XFILES_WATCH_DELETED, d, DELETED_IS_DIR}, {XFILES_WATCH_CREATED, e, true});
    }

    {
        scenario_begin(ctx);
        xfiles_trash(e);
        SCENARIO_END("trash folder", {XFILES_WATCH_DELETED, e, DELETED_IS_DIR});
    }

    flush_for(ctx, 300);
    xfiles_trash(S);
    flush_for(ctx, 300);

    fprintf(stderr, "%d/%d scenarios passed\n\n", count - failures, count);
    return failures;
}

int main(int argc, char** argv)
{
    xtime_init();

    // Find the fd limit. Windows has no equivalent, the handle limit is in the millions
    int fd_limit = 0;
#ifndef _WIN32
    {
        struct rlimit rl;
        getrlimit(RLIMIT_NOFILE, &rl);
        if (argc > 1)
        {
            rl.rlim_cur = strtoul(argv[1], NULL, 10);
            if (setrlimit(RLIMIT_NOFILE, &rl) != 0)
            {
                fprintf(stderr, "setrlimit(%s) failed: %s\n", argv[1], strerror(errno));
                return 1;
            }
        }
        getrlimit(RLIMIT_NOFILE, &rl);

        int    maxfilesperproc = 0;
        size_t sz              = sizeof(maxfilesperproc);
        sysctlbyname("kern.maxfilesperproc", &maxfilesperproc, &sz, NULL, 0);

        fd_limit = maxfilesperproc;
        if (rl.rlim_cur < (rlim_t)fd_limit)
            fd_limit = (int)rl.rlim_cur;

        fprintf(stderr, "RLIMIT_NOFILE soft limit: %llu\n", (unsigned long long)rl.rlim_cur);
        fprintf(stderr, "kern.maxfilesperproc:     %d\n", maxfilesperproc);
        fprintf(stderr, "Effective fd limit:       %d\n", fd_limit);
    }
#else
    (void)argc;
    (void)argv;
#endif

    // Set up paths
    {
        char cwd[PATH_MAX];
#ifdef _WIN32
        if (!GetCurrentDirectoryA(sizeof(cwd), cwd))
            return 1;
#else
        if (!getcwd(cwd, sizeof(cwd)))
            return 1;
#endif
        snprintf(g_root, sizeof(g_root), "%s" XFILES_DIR_STR "temp", cwd);
        snprintf(g_outside, sizeof(g_outside), "%s_outside", g_root);

        remove_temp();
        if (!xfiles_create_directory(g_root))
        {
            fprintf(stderr, "xfiles_create_directory(%s) failed\n", g_root);
            return 1;
        }
#ifndef _WIN32
        // FSEvents reports canonical paths, eg. /tmp/... arrives as /private/tmp/...
        char canonical[PATH_MAX];
        if (!realpath(g_root, canonical))
            return 1;
        memcpy(g_root, canonical, sizeof(g_root));
        snprintf(g_outside, sizeof(g_outside), "%s_outside", g_root);
#endif
    }

    // Created before the watch starts, to check modifying it isn't reported as CREATED
    {
        char pre[PATH_MAX];
        snprintf(pre, sizeof(pre), "%s" XFILES_DIR_STR "pre_existing.txt", g_root);
        xfiles_write(pre, "", 0);
        xtime_sleep_ms(1000); // Make sure the file is older than the watch, and its events have passed
    }

    fprintf(stderr, "Watching:                 %s\n\n", g_root);

    XFilesWatchContext* ctx = xfiles_watch_create(g_root, NULL, on_watch);
    if (!ctx)
    {
        fprintf(stderr, "xfiles_watch_create() failed\n");
        remove_temp();
        return 1;
    }
    xfiles_watch_flush(ctx);

    int scenario_failures = run_scenarios(ctx);

    int baseline_handles = count_open_handles(fd_limit);
    int broken_rounds    = 0;
    int max_handles      = baseline_handles;
    int round;
    fprintf(stderr, "Open " HANDLE_NAME " after xfiles_watch_create(): %d\n\n", baseline_handles);

    for (round = 0; round < MAX_ROUNDS && broken_rounds < 2; round++)
    {
        g_created_dir         = 0;
        g_created_files_mask  = 0;
        g_modified            = 0;
        g_modified_as_created = 0;
        g_modify_target[0]    = 0;
        int asserts_before    = g_assert_failures;
        int overflows_before  = g_overflows;
        int write_fails       = 0;

        snprintf(g_round_dir, sizeof(g_round_dir), "%s" XFILES_DIR_STR "dir_%04d", g_root, round);
        if (!xfiles_create_directory(g_round_dir))
        {
            fprintf(stderr, "xfiles_create_directory(%s) failed\n", g_round_dir);
            break;
        }
        flush_until(ctx, cond_dir);

        for (int i = 0; i < FILES_PER_DIR; i++)
        {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s" XFILES_DIR_STR "file_%02d.txt", g_round_dir, i);
            if (!xfiles_write(path, STR("hello\n")))
            {
                if (write_fails == 0)
                    fprintf(stderr, "    test program xfiles_write(%s) failed\n", path);
                write_fails++;
            }
        }
        flush_until(ctx, cond_files);

        // Modify a file the watcher should now be listening to
        snprintf(g_modify_target, sizeof(g_modify_target), "%s" XFILES_DIR_STR "file_%02d.txt", g_round_dir, FILES_PER_DIR - 1);
        if (!xfiles_append(g_modify_target, STR("world\n")))
        {
            if (write_fails == 0)
                fprintf(stderr, "    test program xfiles_append(%s) failed\n", g_modify_target);
            write_fails++;
        }
        flush_until(ctx, cond_modified);

        int open_handles  = count_open_handles(fd_limit);
        int new_asserts   = g_assert_failures - asserts_before;
        int new_overflows = g_overflows - overflows_before;
        int created_files = popcount64(g_created_files_mask);
        if (open_handles > max_handles)
            max_handles = open_handles;

        bool broken = g_created_dir == 0 || created_files != FILES_PER_DIR || !cond_modified() ||
                      g_modified_as_created != 0 || new_asserts != 0 || new_overflows != 0 || write_fails != 0;

        fprintf(
            stderr,
            "Round %4d: %6d entries on disk | open " HANDLE_NAME " %5d | dir created: %d | files created: %2d/%d | "
            "modified: %d (as created: %d) | %s\n",
            round,
            (round + 1) * (FILES_PER_DIR + 1),
            open_handles,
            g_created_dir,
            created_files,
            FILES_PER_DIR,
            g_modified,
            g_modified_as_created,
            broken ? "BROKEN" : "ok");

        if (broken)
        {
            if (broken_rounds++ == 0)
                fprintf(stderr, "\nThe watcher broke after %d entries\n", round * (FILES_PER_DIR + 1));
            else
                fprintf(stderr, "\nThe next round:\n");
            if (new_asserts)
                fprintf(stderr, "  - %d asserts failed inside xfiles\n", new_asserts);
            if (new_overflows)
                fprintf(stderr, "  - XFILES_WATCH_OVERFLOW was fired %d times\n", new_overflows);
            if (g_created_dir == 0)
                fprintf(stderr, "  - XFILES_WATCH_CREATED was never fired for %s\n", g_round_dir);
            if (created_files != FILES_PER_DIR)
                fprintf(
                    stderr,
                    "  - Only %d/%d XFILES_WATCH_CREATED events for files in the new folder\n",
                    created_files,
                    FILES_PER_DIR);
            if (!cond_modified())
                fprintf(stderr, "  - No event was fired for the modification of %s\n", g_modify_target);
            if (g_modified_as_created)
                fprintf(stderr, "  - The modification of %s was reported as CREATED\n", g_modify_target);
            if (write_fails)
                fprintf(stderr, "  - The rest of the process can no longer write files (%d failures)\n", write_fails);
            fprintf(stderr, "\n");
        }
    }

    if (broken_rounds == 0)
        fprintf(
            stderr,
            "\nDid not break after %d rounds (%d entries). Peak open " HANDLE_NAME ": %d\n",
            round,
            round * (FILES_PER_DIR + 1),
            max_handles);

    xfiles_watch_destroy(ctx);
    fprintf(stderr, "Open " HANDLE_NAME " after xfiles_watch_destroy(): %d\n", count_open_handles(fd_limit));

    remove_temp();
    return broken_rounds || scenario_failures ? 1 : 0; // Success means the watcher kept up the whole way
}
