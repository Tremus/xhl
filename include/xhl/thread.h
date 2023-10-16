/* Refactor of the brilliant thread.h library by mattiasgustavsson.
 * Comments are removed for readability, an understanding of how threads work is assumed.
 * Alignment of data is also assumed, so many unions have been removed
 * (TODO) Additional 8, 16, 64bit atomic operations have been added + bitwise.
 */

#ifndef XHL_THREAD_H
#define XHL_THREAD_H

#define XTHREAD_STACK_SIZE_DEFAULT (0)
#define XTHREAD_SIGNAL_WAIT_INFINITE (-1)
#define XTHREAD_QUEUE_WAIT_INFINITE (-1)

#ifdef __cplusplus
extern "C" {
#endif

typedef void*     xt_thread_id_t;
typedef void*     xt_thread_ptr_t;
typedef char      xt_atomic_int8_t;
typedef short     xt_atomic_int16_t;
typedef int       xt_atomic_int32_t;
typedef long long xt_atomic_int64_t;
typedef void*     xt_atomic_ptr_t;
typedef void*     xthread_tls_t;

typedef union xt_mutex_t  xt_mutex_t;
typedef union xt_signal_t xt_signal_t;
typedef union xt_timer_t  xt_timer_t;
typedef struct xt_queue_t xt_queue_t;

xt_thread_id_t  xthread_current_thread_id(void);
xt_thread_ptr_t xthread_create(int (*thread_proc)(void*), void* user_data, int stack_size);

void xthread_yield(void);
void xthread_set_high_priority(void);
void xthread_exit(int return_code);
void xthread_destroy(xt_thread_ptr_t thread);
int  xthread_join(xt_thread_ptr_t thread);
int  xthread_detach(xt_thread_ptr_t thread);

void xthread_mutex_init(xt_mutex_t* mutex);
void xthread_mutex_term(xt_mutex_t* mutex);
void xthread_mutex_lock(xt_mutex_t* mutex);
void xthread_mutex_unlock(xt_mutex_t* mutex);

void xthread_signal_init(xt_signal_t* signal);
void xthread_signal_term(xt_signal_t* signal);
void xthread_signal_raise(xt_signal_t* signal);
int  xthread_signal_wait(xt_signal_t* signal, int timeout_ms);

int  xthread_atomic_int32_load(xt_atomic_int32_t* atomic);
void xthread_atomic_int32_store(xt_atomic_int32_t* atomic, int desired);
int  xthread_atomic_int32_inc(xt_atomic_int32_t* atomic);
int  xthread_atomic_int32_dec(xt_atomic_int32_t* atomic);
int  xthread_atomic_int32_add(xt_atomic_int32_t* atomic, int value);
int  xthread_atomic_int32_sub(xt_atomic_int32_t* atomic, int value);
int  xthread_atomic_int32_swap(xt_atomic_int32_t* atomic, int desired);
int  xthread_atomic_int32_compare_and_swap(xt_atomic_int32_t* atomic, int expected, int desired);

void* xthread_atomic_ptr_load(xt_atomic_ptr_t* atomic);
void  xthread_atomic_ptr_store(xt_atomic_ptr_t* atomic, void* desired);
void* xthread_atomic_ptr_swap(xt_atomic_ptr_t* atomic, void* desired);
void* xthread_atomic_ptr_compare_and_swap(xt_atomic_ptr_t* atomic, void* expected, void* desired);

void xthread_timer_init(xt_timer_t* timer);
void xthread_timer_term(xt_timer_t* timer);
void xthread_timer_wait(xt_timer_t* timer, unsigned long long nanoseconds);

xthread_tls_t xthread_tls_create(void);

void  xthread_tls_destroy(xthread_tls_t tls);
void  xthread_tls_set(xthread_tls_t tls, void* value);
void* xthread_tls_get(xthread_tls_t tls);

void  xthread_queue_init(xt_queue_t* queue, int size, void** values, int count);
void  xthread_queue_term(xt_queue_t* queue);
int   xthread_queue_produce(xt_queue_t* queue, void* value, int timeout_ms);
void* xthread_queue_consume(xt_queue_t* queue, int timeout_ms);
int   xthread_queue_count(xt_queue_t* queue);

union xt_mutex_t
{
    void* align;
    char  data[64];
};

union xt_signal_t
{
    void* align;
    char  data[116];
};

union xt_timer_t
{
    void* data;
    char  d[8];
};

struct xt_queue_t
{
    xt_signal_t       data_ready;
    xt_signal_t       space_open;
    xt_atomic_int32_t count;
    xt_atomic_int32_t head;
    xt_atomic_int32_t tail;
    void**            values;
    int               size;
#ifndef NDEBUG
    xt_atomic_int32_t id_produce_is_set;
    xt_thread_id_t    id_produce;
    xt_atomic_int32_t id_consume_is_set;
    xt_thread_id_t    id_consume;
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* XHL_THREAD_H */

#ifdef XHL_THREAD_IMPL
#undef XHL_THREAD_IMPL

#ifndef THREAD_ASSERT
#undef _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE
#undef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#define THREAD_ASSERT(expression, message) assert((expression) && (message))
#endif

#if defined(_WIN32)

#pragma comment(lib, "winmm.lib")

#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS

#if ! defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x501 // requires Windows XP minimum
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCKAPI_
#include <windows.h>

#include <timeapi.h>

xt_thread_id_t  xthread_current_thread_id(void) { return (void*)(uintptr_t)GetCurrentThreadId(); }
xt_thread_ptr_t xthread_create(int (*thread_proc)(void*), void* user_data, int stack_size)
{
    DWORD  thread_id;
    HANDLE handle = CreateThread(
        NULL,
        stack_size > 0 ? (size_t)stack_size : 0U,
        (LPTHREAD_START_ROUTINE)(uintptr_t)thread_proc,
        user_data,
        0,
        &thread_id);
    if (! handle)
        return NULL;
    return (xt_thread_ptr_t)handle;
}

void xthread_yield(void) { SwitchToThread(); }
void xthread_exit(int return_code) { ExitThread((DWORD)return_code); }
void xthread_destroy(xt_thread_ptr_t thread)
{
    WaitForSingleObject((HANDLE)thread, INFINITE);
    CloseHandle((HANDLE)thread);
}
int xthread_join(xt_thread_ptr_t thread)
{
    WaitForSingleObject((HANDLE)thread, INFINITE);
    DWORD retval;
    GetExitCodeThread((HANDLE)thread, &retval);
    return (int)retval;
}
int  xthread_detach(xt_thread_ptr_t thread) { return CloseHandle((HANDLE)thread) != 0; }
void xthread_set_high_priority(void) { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST); }

_Static_assert(sizeof(xt_mutex_t) >= sizeof(CRITICAL_SECTION), "Mutex too smol...");
void xthread_mutex_init(xt_mutex_t* mutex) { InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)mutex, 32); }
void xthread_mutex_term(xt_mutex_t* mutex) { DeleteCriticalSection((CRITICAL_SECTION*)mutex); }
void xthread_mutex_lock(xt_mutex_t* mutex) { EnterCriticalSection((CRITICAL_SECTION*)mutex); }
void xthread_mutex_unlock(xt_mutex_t* mutex) { LeaveCriticalSection((CRITICAL_SECTION*)mutex); }

// clang-format off
int  xthread_atomic_int32_load (xt_atomic_int32_t* atomic)                { return InterlockedCompareExchange((volatile long*)atomic, 0, 0); }
void xthread_atomic_int32_store(xt_atomic_int32_t* atomic, int desired)   { InterlockedExchange((volatile long*)atomic, desired); }
int  xthread_atomic_int32_inc  (xt_atomic_int32_t* atomic)                { return InterlockedIncrement((volatile long*)atomic) - 1; }
int  xthread_atomic_int32_dec  (xt_atomic_int32_t* atomic)                { return InterlockedDecrement((volatile long*)atomic) + 1; }
int  xthread_atomic_int32_add  (xt_atomic_int32_t* atomic, int value)     { return InterlockedExchangeAdd((volatile long*)atomic, value); }
int  xthread_atomic_int32_sub  (xt_atomic_int32_t* atomic, int value)     { return InterlockedExchangeAdd((volatile long*)atomic, -value); }
int  xthread_atomic_int32_swap (xt_atomic_int32_t* atomic, int desired)   { return InterlockedExchange((volatile long*)atomic, desired); }
int  xthread_atomic_int32_compare_and_swap(xt_atomic_int32_t* atomic, int expected, int desired) { return InterlockedCompareExchange((volatile long*)atomic, desired, expected); }

void* xthread_atomic_ptr_load (xt_atomic_ptr_t* atomic)                                           { return InterlockedCompareExchangePointer(atomic, 0, 0); }
void  xthread_atomic_ptr_store(xt_atomic_ptr_t* atomic, void* desired)                            { InterlockedExchangePointer(atomic, desired); }
void* xthread_atomic_ptr_swap (xt_atomic_ptr_t* atomic, void* desired)                            { return InterlockedExchangePointer(atomic, desired); }
void* xthread_atomic_ptr_compare_and_swap(xt_atomic_ptr_t* atomic, void* expected, void* desired) { return InterlockedCompareExchangePointer(atomic, desired, expected); }

xthread_tls_t xthread_tls_create(void)
{
    DWORD tls = TlsAlloc();
    if (tls == TLS_OUT_OF_INDEXES)
        return NULL;
    else
        return (xthread_tls_t)(uintptr_t)tls;
}
void  xthread_tls_destroy(xthread_tls_t tls)              { TlsFree((DWORD)(uintptr_t)tls); }
void  xthread_tls_set    (xthread_tls_t tls, void* value) { TlsSetValue((DWORD)(uintptr_t)tls, value); }
void* xthread_tls_get    (xthread_tls_t tls)              { return TlsGetValue((DWORD)(uintptr_t)tls); }

// clang-format on
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

xt_thread_id_t xthread_current_thread_id(void) { return (void*)pthread_self(); }

void xthread_yield(void) { sched_yield(); }

void xthread_exit(int return_code) { pthread_exit((void*)(uintptr_t)return_code); }

xt_thread_ptr_t xthread_create(int (*thread_proc)(void*), void* user_data, int stack_size)
{
    pthread_t thread;
    if (0 != pthread_create(&thread, NULL, (void* (*)(void*))thread_proc, user_data))
        return NULL;

    return (xt_thread_ptr_t)thread;
}
void xthread_destroy(xt_thread_ptr_t thread) { pthread_join((pthread_t)thread, NULL); }
int  xthread_join(xt_thread_ptr_t thread)
{
    void* retval;
    pthread_join((pthread_t)thread, &retval);
    return (int)(uintptr_t)retval;
}
int  xthread_detach(xt_thread_ptr_t thread) { return pthread_detach((pthread_t)thread) == 0; }
void xthread_set_high_priority(void)
{
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_min(SCHED_RR);
    pthread_setschedparam(pthread_self(), SCHED_RR, &sp);
}

// clang-format off
_Static_assert(sizeof(xt_mutex_t) < sizeof(pthread_mutex_t) ? 0 : 1, "Mutex too smol...");
void xthread_mutex_init(xt_mutex_t* mutex) { pthread_mutex_init((pthread_mutex_t*)mutex, NULL); }
void xthread_mutex_term(xt_mutex_t* mutex) { pthread_mutex_destroy((pthread_mutex_t*)mutex); }
void xthread_mutex_lock(xt_mutex_t* mutex) { pthread_mutex_lock((pthread_mutex_t*)mutex); }
void xthread_mutex_unlock(xt_mutex_t* mutex) { pthread_mutex_unlock((pthread_mutex_t*)mutex); }

int  xthread_atomic_int32_load(xt_atomic_int32_t* atomic) { return (int)__sync_fetch_and_add(atomic, 0); }
void xthread_atomic_int32_store(xt_atomic_int32_t* atomic, int desired)
{
    __sync_fetch_and_and(atomic, 0);
    __sync_fetch_and_or(atomic, desired);
}
int xthread_atomic_int32_inc(xt_atomic_int32_t* atomic) { return (int)__sync_fetch_and_add(atomic, 1); }
int xthread_atomic_int32_dec(xt_atomic_int32_t* atomic) { return (int)__sync_fetch_and_sub(atomic, 1); }
int xthread_atomic_int32_add(xt_atomic_int32_t* atomic, int value)
{
    return (int)__sync_fetch_and_add(atomic, value);
}
int xthread_atomic_int32_sub(xt_atomic_int32_t* atomic, int value)
{
    return (int)__sync_fetch_and_sub(atomic, value);
}
int xthread_atomic_int32_swap(xt_atomic_int32_t* atomic, int desired)
{
    int old = (int)__sync_lock_test_and_set(atomic, desired);
    __sync_lock_release(&atomic->i);
    return old;
}
int xthread_atomic_int32_compare_and_swap(xt_atomic_int32_t* atomic, int expected, int desired)
{
    return (int)__sync_val_compare_and_swap(atomic, expected, desired);
}
void* xthread_atomic_ptr_load(xt_atomic_ptr_t* atomic) { return __sync_fetch_and_add(atomic, 0); }
void  xthread_atomic_ptr_store(xt_atomic_ptr_t* atomic, void* desired)
{
    __sync_lock_test_and_set(atomic, desired);
    __sync_lock_release(atomic);
}
void* xthread_atomic_ptr_swap(xt_atomic_ptr_t* atomic, void* desired)
{
    void* old = __sync_lock_test_and_set(atomic, desired);
    __sync_lock_release(&atomic->ptr);
    return old;
}
void* xthread_atomic_ptr_compare_and_swap(xt_atomic_ptr_t* atomic, void* expected, void* desired) { return __sync_val_compare_and_swap(atomic, expected, desired); }

xthread_tls_t xthread_tls_create(void)
{
    pthread_key_t tls;
    if (pthread_key_create(&tls, NULL) == 0)
        return (xthread_tls_t)tls;
    else
        return NULL;
}
void  xthread_tls_destroy(xthread_tls_t tls)          { pthread_key_delete((pthread_key_t)(uintptr_t)tls); }
void  xthread_tls_set(xthread_tls_t tls, void* value) { pthread_setspecific((pthread_key_t)(uintptr_t)tls, value); }
void* xthread_tls_get(xthread_tls_t tls)              { return pthread_getspecific((pthread_key_t)(uintptr_t)tls); }

// clang-format on
#else
#error Unknown platform.
#endif

struct xthread_internal_signal_t
{
#if defined(_WIN32)
#if _WIN32_WINNT >= 0x0600
    CRITICAL_SECTION   mutex;
    CONDITION_VARIABLE condition;
    int                value;
#else
#pragma message("Warning: _WIN32_WINNT < 0x0600 - condition variables not available")
    HANDLE event;
#endif
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    pthread_mutex_t mutex;
    pthread_cond_t  condition;
    int             value;
#endif
};

void xthread_signal_init(xt_signal_t* signal)
{
    // Compile-time size check
    struct x
    {
        char thread_signal_type_too_small : (sizeof(xt_signal_t) < sizeof(struct xthread_internal_signal_t) ? 0 : 1);
    };

    struct xthread_internal_signal_t* internal = (struct xthread_internal_signal_t*)signal;

#if defined(_WIN32)

#if _WIN32_WINNT >= 0x0600
    InitializeCriticalSectionAndSpinCount(&internal->mutex, 32);
    InitializeConditionVariable(&internal->condition);
    internal->value = 0;
#else
    internal->event = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif

#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    pthread_mutex_init(&internal->mutex, NULL);
    pthread_cond_init(&internal->condition, NULL);
    internal->value = 0;

#endif
}

void xthread_signal_term(xt_signal_t* signal)
{
    struct xthread_internal_signal_t* internal = (struct xthread_internal_signal_t*)signal;

#if defined(_WIN32)

#if _WIN32_WINNT >= 0x0600
    DeleteCriticalSection(&internal->mutex);
#else
    CloseHandle(internal->event);
#endif

#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    pthread_mutex_destroy(&internal->mutex);
    pthread_cond_destroy(&internal->condition);
#endif
}

void xthread_signal_raise(xt_signal_t* signal)
{
    struct xthread_internal_signal_t* internal = (struct xthread_internal_signal_t*)signal;

#if defined(_WIN32)

#if _WIN32_WINNT >= 0x0600
    EnterCriticalSection(&internal->mutex);
    internal->value = 1;
    LeaveCriticalSection(&internal->mutex);
    WakeConditionVariable(&internal->condition);
#else
    SetEvent(internal->event);
#endif

#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    pthread_mutex_lock(&internal->mutex);
    internal->value = 1;
    pthread_mutex_unlock(&internal->mutex);
    pthread_cond_signal(&internal->condition);
#endif
}

int xthread_signal_wait(xt_signal_t* signal, int timeout_ms)
{
    struct xthread_internal_signal_t* internal = (struct xthread_internal_signal_t*)signal;

#if defined(_WIN32)

#if _WIN32_WINNT >= 0x0600
    int timed_out = 0;
    EnterCriticalSection(&internal->mutex);
    while (internal->value == 0)
    {
        BOOL res =
            SleepConditionVariableCS(&internal->condition, &internal->mutex, timeout_ms < 0 ? INFINITE : timeout_ms);
        if (! res && GetLastError() == ERROR_TIMEOUT)
        {
            timed_out = 1;
            break;
        }
    }
    internal->value = 0;
    LeaveCriticalSection(&internal->mutex);
    return ! timed_out;
#else
    int failed = WAIT_OBJECT_0 != WaitForSingleObject(internal->event, timeout_ms < 0 ? INFINITE : timeout_ms);
    return ! failed;
#endif

#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)

    struct timespec ts;
    if (timeout_ms >= 0)
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ts.tv_sec  = time(NULL) + timeout_ms / 1000;
        ts.tv_nsec = tv.tv_usec * 1000 + 1000 * 1000 * (timeout_ms % 1000);
        ts.tv_sec  += ts.tv_nsec / (1000 * 1000 * 1000);
        ts.tv_nsec %= (1000 * 1000 * 1000);
    }

    int timed_out = 0;
    pthread_mutex_lock(&internal->mutex);
    while (internal->value == 0)
    {
        if (timeout_ms < 0)
            pthread_cond_wait(&internal->condition, &internal->mutex);
        else if (pthread_cond_timedwait(&internal->condition, &internal->mutex, &ts) == ETIMEDOUT)
        {
            timed_out = 1;
            break;
        }
    }
    if (! timed_out)
        internal->value = 0;
    pthread_mutex_unlock(&internal->mutex);
    return ! timed_out;
#endif
}

void xthread_timer_init(xt_timer_t* timer)
{
#if defined(_WIN32)

    // Compile-time size check
    struct x
    {
        char thread_timer_type_too_small : (sizeof(xt_mutex_t) < sizeof(HANDLE) ? 0 : 1);
    };

    TIMECAPS tc;
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR)
        timeBeginPeriod(tc.wPeriodMin);

    *(HANDLE*)timer = CreateWaitableTimer(NULL, TRUE, NULL);
#endif // No init on MacOS/Linux/Android
}

void xthread_timer_term(xt_timer_t* timer)
{
#if defined(_WIN32)
    CloseHandle(*(HANDLE*)timer);
    TIMECAPS tc;
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR)
        timeEndPeriod(tc.wPeriodMin);
#endif // No deinit on MacOS/Linux/Android
}

void xthread_timer_wait(xt_timer_t* timer, unsigned long long nanoseconds)
{
#if defined(_WIN32)
    LARGE_INTEGER due_time;
    due_time.QuadPart = -(LONGLONG)(nanoseconds / 100);
    BOOL b            = SetWaitableTimer(*(HANDLE*)timer, &due_time, 0, 0, 0, FALSE);
    (void)b;
    WaitForSingleObject(*(HANDLE*)timer, INFINITE);
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    struct timespec rem;
    struct timespec req;
    req.tv_sec  = nanoseconds / 1000000000ULL;
    req.tv_nsec = nanoseconds - req.tv_sec * 1000000000ULL;
    while (nanosleep(&req, &rem))
        req = rem;
#endif
}

void xthread_queue_init(xt_queue_t* queue, int size, void** values, int count)
{
    queue->values = values;
    xthread_signal_init(&queue->data_ready);
    xthread_signal_init(&queue->space_open);
    xthread_atomic_int32_store(&queue->head, 0);
    xthread_atomic_int32_store(&queue->tail, count > size ? size : count);
    xthread_atomic_int32_store(&queue->count, count > size ? size : count);
    queue->size = size;
#ifndef NDEBUG
    xthread_atomic_int32_store(&queue->id_produce_is_set, 0);
    xthread_atomic_int32_store(&queue->id_consume_is_set, 0);
#endif
}

void xthread_queue_term(xt_queue_t* queue)
{
    xthread_signal_term(&queue->space_open);
    xthread_signal_term(&queue->data_ready);
}

int xthread_queue_produce(xt_queue_t* queue, void* value, int timeout_ms)
{
#ifndef NDEBUG
    if (xthread_atomic_int32_compare_and_swap(&queue->id_produce_is_set, 0, 1) == 0)
        queue->id_produce = xthread_current_thread_id();
    THREAD_ASSERT(
        xthread_current_thread_id() == queue->id_produce,
        "thread_queue_produce called from multiple threads");
#endif
    while (xthread_atomic_int32_load(&queue->count) ==
           queue->size) // TODO: fix signal so that this can be an "if" instead of "while"
    {
        if (timeout_ms == 0)
            return 0;
        if (xthread_signal_wait(
                &queue->space_open,
                timeout_ms == XTHREAD_QUEUE_WAIT_INFINITE ? XTHREAD_SIGNAL_WAIT_INFINITE : timeout_ms) == 0)
            return 0;
    }
    int tail                          = xthread_atomic_int32_inc(&queue->tail);
    queue->values[tail % queue->size] = value;
    if (xthread_atomic_int32_inc(&queue->count) == 0)
        xthread_signal_raise(&queue->data_ready);
    return 1;
}

void* xthread_queue_consume(xt_queue_t* queue, int timeout_ms)
{
#ifndef NDEBUG
    if (xthread_atomic_int32_compare_and_swap(&queue->id_consume_is_set, 0, 1) == 0)
        queue->id_consume = xthread_current_thread_id();
    THREAD_ASSERT(
        xthread_current_thread_id() == queue->id_consume,
        "thread_queue_consume called from multiple threads");
#endif
    while (xthread_atomic_int32_load(&queue->count) ==
           0) // TODO: fix signal so that this can be an "if" instead of "while"
    {
        if (timeout_ms == 0)
            return NULL;
        if (xthread_signal_wait(
                &queue->data_ready,
                timeout_ms == XTHREAD_QUEUE_WAIT_INFINITE ? XTHREAD_SIGNAL_WAIT_INFINITE : timeout_ms) == 0)
            return NULL;
    }
    int   head   = xthread_atomic_int32_inc(&queue->head);
    void* retval = queue->values[head % queue->size];
    if (xthread_atomic_int32_dec(&queue->count) == queue->size)
        xthread_signal_raise(&queue->space_open);
    return retval;
}

int xthread_queue_count(xt_queue_t* queue) { return xthread_atomic_int32_load(&queue->count); }

#endif /* XHL_THREAD_IMPL */