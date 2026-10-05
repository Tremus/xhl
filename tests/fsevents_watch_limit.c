// Tests the macOS (FSEvents) implementation of xfiles_watch_* in xhl/files.h
//
// Build: clang fsevents_watch_limit.c -framework CoreServices -o fsevents_watch_limit
// Run it from a folder you don't mind it creating ./temp and ./temp_outside in. Both are deleted when it finishes
//
// FSEvents uses one stream for the whole tree instead of one file descriptor per entry, so the number of open fds
// should stay flat no matter how many files are watched.
//
// This program:
//   1. Creates ./temp and watches it with xfiles_watch_create()
//   2. Runs a set of scenarios (create, modify, rename, move, atomic save, move in/out, delete, folders) and checks
//      each one produces the expected events. Renames and moves are expected as DELETED + CREATED
//   3. Each round, creates temp/dir_NNNN, then 64 files inside it, then appends to the last file. The append must be
//      reported as MODIFIED, not CREATED
//   4. After each step it polls xfiles_watch_flush() until the callbacks it expected fire, or a timeout passes.
//      FSEvents delivers asynchronously with a 50ms latency, so a single flush straight after the change isn't enough
//   5. Stops after ~10000 entries, or one round after the first round where the watcher misses something
//
// Usage: fsevents_watch_limit [soft_fd_limit]
//   Pass 256 to simulate an app launched from Finder/Dock (launchd's default soft limit, see `launchctl limit
//   maxfiles`). With no argument the limit inherited from your shell is used.

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

static int  g_assert_failures = 0;
static void on_xfiles_assert(const char* cond, int line)
{
    if (g_assert_failures < 5)
        fprintf(stderr, "    xfiles assert failed: line %d (%s) errno=%d (%s)\n", line, cond, errno, strerror(errno));
    else if (g_assert_failures == 5)
        fprintf(stderr, "    ... further xfiles assert failures suppressed\n");
    g_assert_failures++;
}
#define XFILES_ASSERT(cond) ((cond) ? (void)0 : on_xfiles_assert(#cond, __LINE__))

#define XHL_FILES_IMPL
#include "../include/xhl/files.h"

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
    bool                   is_dir;
    char                   path[PATH_MAX];
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
    return "?";
}

static int popcount64(uint64_t x) { return __builtin_popcountll(x); }

static void on_watch(const XFilesWatchEvent* event, void* udata)
{
    (void)udata;
    enum XFilesWatchType type   = event->type;
    const char*            path   = event->path;
    size_t                 dirlen = strlen(g_round_dir);

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
            strncmp(path, g_round_dir, dirlen) == 0 && sscanf(path + dirlen, "/file_%d.txt", &idx) == 1 && idx >= 0 &&
            idx < FILES_PER_DIR)
            g_created_files_mask |= 1ull << idx;
    }
}

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static bool cond_dir(void) { return g_created_dir > 0; }
static bool cond_files(void) { return popcount64(g_created_files_mask) == FILES_PER_DIR; }
static bool cond_modified(void) { return g_modified > 0; }

typedef struct Expect
{
    enum XFilesWatchType type;
    const char*            path;
    bool                   is_dir;
} Expect;
static const Expect* g_expect;
static int           g_expect_len;

static bool event_matches(const LoggedEvent* e, const Expect* x)
{
    return e->type == x->type && e->is_dir == x->is_dir && strcmp(e->path, x->path) == 0;
}

static bool expect_found(const Expect* x)
{
    for (int i = 0; i < g_log_len; i++)
        if (event_matches(&g_log[i], x))
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
static void flush_until(XFilesWatchContext ctx, bool (*done)(void))
{
    uint64_t start = now_ms();
    for (;;)
    {
        xfiles_watch_flush(ctx);
        if (done())
            break;
        if (now_ms() - start > TIMEOUT_MS)
            break;
        usleep(5000);
    }
}

static void flush_for(XFilesWatchContext ctx, int ms)
{
    uint64_t start = now_ms();
    while (now_ms() - start < (uint64_t)ms)
    {
        xfiles_watch_flush(ctx);
        usleep(5000);
    }
}

static int count_open_fds(int limit)
{
    int n = 0;
    for (int fd = 0; fd < limit; fd++)
        if (fcntl(fd, F_GETFD) != -1)
            n++;
    return n;
}

static void remove_temp(void)
{
    char cmd[2 * PATH_MAX + 32];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s' '%s'", g_root, g_outside);
    system(cmd);
}

// Runs 'setup' (not checked), lets its events drain, then runs 'action' and checks it produced every expected event.
// Also fails if an expected path got an event of any other type, eg. MODIFIED when we expected CREATED
static bool run_scenario(
    XFilesWatchContext ctx,
    const char*            name,
    const char*            setup,
    const char*            action,
    const Expect*          expect,
    int                    expect_len)
{
    if (setup)
        system(setup);
    flush_for(ctx, 300);

    g_expect     = expect;
    g_expect_len = expect_len;
    g_log_len    = 0;
    g_logging    = true;
    system(action);
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
                same_path = true;
                same_type = same_type || g_log[i].type == expect[j].type;
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
#define RUN_SCENARIO(name, setup, action, ...)                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        const Expect expect_[] = {__VA_ARGS__};                                                                        \
        failures += !run_scenario(ctx, name, setup, action, expect_, (int)(sizeof(expect_) / sizeof(expect_[0])));     \
        count++;                                                                                                       \
    }                                                                                                                  \
    while (0)

static int run_scenarios(XFilesWatchContext ctx)
{
    char S[PATH_MAX], O[PATH_MAX];
    snprintf(S, sizeof(S), "%s/scenarios", g_root);
    snprintf(O, sizeof(O), "%s", g_outside);

    char a[PATH_MAX], b[PATH_MAX], sub[PATH_MAX], subb[PATH_MAX], tmp[PATH_MAX], c[PATH_MAX], d[PATH_MAX],
        e[PATH_MAX], pre[PATH_MAX];
    snprintf(a, sizeof(a), "%s/a.txt", S);
    snprintf(b, sizeof(b), "%s/b.txt", S);
    snprintf(sub, sizeof(sub), "%s/sub", S);
    snprintf(subb, sizeof(subb), "%s/sub/b.txt", S);
    snprintf(tmp, sizeof(tmp), "%s/sub/.b.txt.tmp", S);
    snprintf(c, sizeof(c), "%s/c.txt", S);
    snprintf(d, sizeof(d), "%s/d", S);
    snprintf(e, sizeof(e), "%s/e", S);
    snprintf(pre, sizeof(pre), "%s/pre_existing.txt", g_root);

    char setup[4 * PATH_MAX], action[4 * PATH_MAX];
    int  failures = 0;
    int  count    = 0;

    fprintf(stderr, "Scenarios:\n");

    snprintf(action, sizeof(action), "echo more >> '%s'", pre);
    RUN_SCENARIO("modify a file that existed before the watch started", NULL, action, {XFILES_WATCH_MODIFIED, pre, false});

    snprintf(setup, sizeof(setup), "mkdir -p '%s'", sub);
    snprintf(action, sizeof(action), "echo hi > '%s'", a);
    RUN_SCENARIO("create file", setup, action, {XFILES_WATCH_CREATED, a, false});

    snprintf(action, sizeof(action), "echo more >> '%s'", a);
    RUN_SCENARIO("append to file", NULL, action, {XFILES_WATCH_MODIFIED, a, false});

    snprintf(action, sizeof(action), "mv '%s' '%s'", a, b);
    RUN_SCENARIO("rename file", NULL, action, {XFILES_WATCH_DELETED, a, false}, {XFILES_WATCH_CREATED, b, false});

    snprintf(action, sizeof(action), "echo more >> '%s'", b);
    RUN_SCENARIO("append to renamed file", NULL, action, {XFILES_WATCH_MODIFIED, b, false});

    snprintf(action, sizeof(action), "mv '%s' '%s'", b, subb);
    RUN_SCENARIO(
        "move file to another folder",
        NULL,
        action,
        {XFILES_WATCH_DELETED, b, false},
        {XFILES_WATCH_CREATED, subb, false});

    // The temp file may or may not be reported as CREATED before it's renamed, so only the saved file is checked
    snprintf(action, sizeof(action), "echo saved > '%s' && mv '%s' '%s'", tmp, tmp, subb);
    RUN_SCENARIO("atomic save (write temp file, rename over original)", NULL, action, {XFILES_WATCH_MODIFIED, subb, false});

    snprintf(setup, sizeof(setup), "mkdir -p '%s'", O);
    snprintf(action, sizeof(action), "mv '%s' '%s/b.txt'", subb, O);
    RUN_SCENARIO("move file out of the watched folder", setup, action, {XFILES_WATCH_DELETED, subb, false});

    snprintf(setup, sizeof(setup), "echo x > '%s/c.txt'", O);
    snprintf(action, sizeof(action), "mv '%s/c.txt' '%s'", O, c);
    RUN_SCENARIO("move file into the watched folder", setup, action, {XFILES_WATCH_CREATED, c, false});

    snprintf(action, sizeof(action), "echo more >> '%s'", c);
    RUN_SCENARIO("append to moved in file", NULL, action, {XFILES_WATCH_MODIFIED, c, false});

    snprintf(action, sizeof(action), "rm '%s'", c);
    RUN_SCENARIO("delete file", NULL, action, {XFILES_WATCH_DELETED, c, false});

    snprintf(action, sizeof(action), "mkdir '%s'", d);
    RUN_SCENARIO("create folder", NULL, action, {XFILES_WATCH_CREATED, d, true});

    snprintf(action, sizeof(action), "mv '%s' '%s'", d, e);
    RUN_SCENARIO("rename folder", NULL, action, {XFILES_WATCH_DELETED, d, true}, {XFILES_WATCH_CREATED, e, true});

    snprintf(action, sizeof(action), "rmdir '%s'", e);
    RUN_SCENARIO("delete folder", NULL, action, {XFILES_WATCH_DELETED, e, true});

    snprintf(action, sizeof(action), "rm -rf '%s'", S);
    flush_for(ctx, 300);
    system(action);
    flush_for(ctx, 300);

    fprintf(stderr, "%d/%d scenarios passed\n\n", count - failures, count);
    return failures;
}

int main(int argc, char** argv)
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

    int fd_limit = maxfilesperproc;
    if (rl.rlim_cur < (rlim_t)fd_limit)
        fd_limit = (int)rl.rlim_cur;

    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd)))
        return 1;
    snprintf(g_root, sizeof(g_root), "%s/temp", cwd);
    snprintf(g_outside, sizeof(g_outside), "%s/temp_outside", cwd);

    remove_temp();
    if (mkdir(g_root, 0755) != 0)
    {
        fprintf(stderr, "mkdir(%s) failed: %s\n", g_root, strerror(errno));
        return 1;
    }
    // FSEvents reports canonical paths, eg. /tmp/... arrives as /private/tmp/...
    char canonical[PATH_MAX];
    if (!realpath(g_root, canonical))
        return 1;
    memcpy(g_root, canonical, sizeof(g_root));
    snprintf(g_outside, sizeof(g_outside), "%s_outside", g_root);

    // Created before the watch starts, to check modifying it isn't reported as CREATED
    {
        char pre[PATH_MAX];
        snprintf(pre, sizeof(pre), "%s/pre_existing.txt", g_root);
        int fd = open(pre, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd != -1)
            close(fd);
        sleep(1); // Make sure the file is older than the watch, and its events have passed
    }

    fprintf(stderr, "RLIMIT_NOFILE soft limit: %llu\n", (unsigned long long)rl.rlim_cur);
    fprintf(stderr, "kern.maxfilesperproc:     %d\n", maxfilesperproc);
    fprintf(stderr, "Effective fd limit:       %d\n", fd_limit);
    fprintf(stderr, "Watching:                 %s\n\n", g_root);

    XFilesWatchContext ctx = xfiles_watch_create(g_root, NULL, on_watch);
    if (!ctx)
    {
        fprintf(stderr, "xfiles_watch_create() failed\n");
        remove_temp();
        return 1;
    }
    xfiles_watch_flush(ctx);

    int scenario_failures = run_scenarios(ctx);

    int baseline_fds  = count_open_fds(fd_limit);
    int broken_rounds = 0;
    int max_fds       = baseline_fds;
    int round;
    fprintf(stderr, "Open fds after xfiles_watch_create(): %d\n\n", baseline_fds);

    for (round = 0; round < MAX_ROUNDS && broken_rounds < 2; round++)
    {
        g_created_dir         = 0;
        g_created_files_mask  = 0;
        g_modified            = 0;
        g_modified_as_created = 0;
        g_modify_target[0]    = 0;
        int asserts_before    = g_assert_failures;
        int overflows_before  = g_overflows;
        int test_open_fails   = 0;

        snprintf(g_round_dir, sizeof(g_round_dir), "%s/dir_%04d", g_root, round);
        if (mkdir(g_round_dir, 0755) != 0)
        {
            fprintf(stderr, "mkdir(%s) failed: %s\n", g_round_dir, strerror(errno));
            break;
        }
        flush_until(ctx, cond_dir);

        for (int i = 0; i < FILES_PER_DIR; i++)
        {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/file_%02d.txt", g_round_dir, i);
            int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1)
            {
                if (test_open_fails == 0)
                    fprintf(stderr, "    test program open(%s) failed: %s\n", path, strerror(errno));
                test_open_fails++;
                continue;
            }
            write(fd, "hello\n", 6);
            close(fd);
        }
        flush_until(ctx, cond_files);

        // Modify a file the watcher should now be listening to
        snprintf(g_modify_target, sizeof(g_modify_target), "%s/file_%02d.txt", g_round_dir, FILES_PER_DIR - 1);
        int fd = open(g_modify_target, O_WRONLY | O_APPEND);
        if (fd != -1)
        {
            write(fd, "world\n", 6);
            close(fd);
        }
        else
        {
            if (test_open_fails == 0)
                fprintf(stderr, "    test program open(%s) failed: %s\n", g_modify_target, strerror(errno));
            test_open_fails++;
        }
        flush_until(ctx, cond_modified);

        int open_fds      = count_open_fds(fd_limit);
        int new_asserts   = g_assert_failures - asserts_before;
        int new_overflows = g_overflows - overflows_before;
        int created_files = popcount64(g_created_files_mask);
        if (open_fds > max_fds)
            max_fds = open_fds;

        bool broken = g_created_dir == 0 || created_files != FILES_PER_DIR || !cond_modified() ||
                      g_modified_as_created != 0 || new_asserts != 0 ||
                      new_overflows != 0 || test_open_fails != 0;

        fprintf(
            stderr,
            "Round %4d: %6d entries on disk | open fds %5d/%d | dir created: %d | files created: %2d/%d | "
            "modified: %d (as created: %d) | %s\n",
            round,
            (round + 1) * (FILES_PER_DIR + 1),
            open_fds,
            fd_limit,
            g_created_dir,
            created_files,
            FILES_PER_DIR,
            g_modified,
            g_modified_as_created,
            broken ? "BROKEN" : "ok");

        if (broken)
        {
            if (broken_rounds++ == 0)
                fprintf(stderr, "\nThe FSEvents watcher broke after %d entries\n", round * (FILES_PER_DIR + 1));
            else
                fprintf(stderr, "\nThe next round:\n");
            if (new_asserts)
                fprintf(stderr, "  - %d asserts failed inside the watcher\n", new_asserts);
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
            if (test_open_fails)
                fprintf(stderr, "  - The rest of the process can no longer open files (%d failures)\n", test_open_fails);
            fprintf(stderr, "\n");
        }
    }

    if (broken_rounds == 0)
        fprintf(
            stderr,
            "\nDid not break after %d rounds (%d entries). Peak open fds: %d\n",
            round,
            round * (FILES_PER_DIR + 1),
            max_fds);

    xfiles_watch_destroy(ctx);
    fprintf(stderr, "Open fds after xfiles_watch_destroy(): %d\n", count_open_fds(fd_limit));

    remove_temp();
    return broken_rounds || scenario_failures ? 1 : 0; // Success means the watcher kept up the whole way
}
