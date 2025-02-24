/* Refactor of the brilliant thread.h library by mattiasgustavsson.
 * Atomic ops have been pulled from the also brilliant c89atomic lib by David Reid
 * Comments are removed for readability, an understanding of how threads work is assumed.
 * Alignment of data is also assumed, so many unions have been removed
 * Additional 8, 16, 64bit atomic operations have been added + bitwise.
 */

// clang-format off
#ifndef XHL_THREAD_H
#define XHL_THREAD_H

#define XTHREAD_STACK_SIZE_DEFAULT (0)
#define XTHREAD_SIGNAL_WAIT_INFINITE (-1)
#define XTHREAD_QUEUE_WAIT_INFINITE (-1)

#ifdef _MSC_VER
#define XTHREAD_LOCAL __declspec(thread)
#else
#define XTHREAD_LOCAL _Thread_local
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef void* xt_thread_ptr_t;
typedef void* xt_tls_t;

typedef volatile unsigned char      xt_atomic_uint8_t;
typedef volatile unsigned short     xt_atomic_uint16_t;
typedef volatile unsigned int       xt_atomic_uint32_t;
typedef volatile unsigned long long xt_atomic_uint64_t;

typedef volatile char      xt_atomic_int8_t;
typedef volatile short     xt_atomic_int16_t;
typedef volatile int       xt_atomic_int32_t;
typedef volatile long long xt_atomic_int64_t;
typedef volatile void*     xt_atomic_ptr_t;
typedef volatile unsigned  xt_atomic_float;

typedef union  xt_mutex_t  xt_mutex_t;
typedef union  xt_signal_t xt_signal_t;
typedef union  xt_timer_t  xt_timer_t;
typedef struct xt_queue_t  xt_queue_t;

typedef volatile unsigned char xt_spinlock_t;

xt_thread_ptr_t xthread_current(void);
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

// TODO add explicit memory order to atomic ops
enum xt_memory_order
{
    xt_memory_order_relaxed, // __ATOMIC_RELAXED
    xt_memory_order_consume, // __ATOMIC_CONSUME
    xt_memory_order_acquire, // __ATOMIC_ACQUIRE
    xt_memory_order_release, // __ATOMIC_RELEASE
    xt_memory_order_acq_rel, // __ATOMIC_ACQ_REL
    xt_memory_order_seq_cst, // __ATOMIC_SEQ_CST
};

uint8_t  xt_atomic_load_u8 (const xt_atomic_uint8_t*);
uint16_t xt_atomic_load_u16(const xt_atomic_uint16_t*);
uint32_t xt_atomic_load_u32(const xt_atomic_uint32_t*);
uint64_t xt_atomic_load_u64(const xt_atomic_uint64_t*);
int8_t   xt_atomic_load_i8 (const xt_atomic_int8_t*);
int16_t  xt_atomic_load_i16(const xt_atomic_int16_t*);
int32_t  xt_atomic_load_i32(const xt_atomic_int32_t*);
int64_t  xt_atomic_load_i64(const xt_atomic_int64_t*);

void xt_atomic_store_u8 (xt_atomic_uint8_t*,  uint8_t);
void xt_atomic_store_u16(xt_atomic_uint16_t*, uint16_t);
void xt_atomic_store_u32(xt_atomic_uint32_t*, uint32_t);
void xt_atomic_store_u64(xt_atomic_uint64_t*, uint64_t);
void xt_atomic_store_i8 (xt_atomic_int8_t*,   int8_t);
void xt_atomic_store_i16(xt_atomic_int16_t*,  int16_t);
void xt_atomic_store_i32(xt_atomic_int32_t*,  int32_t);
void xt_atomic_store_i64(xt_atomic_int64_t*,  int64_t);

uint8_t  xt_atomic_exchange_u8 (xt_atomic_uint8_t*,     uint8_t);
uint16_t xt_atomic_exchange_u16(xt_atomic_uint16_t*ptr, uint16_t);
uint32_t xt_atomic_exchange_u32(xt_atomic_uint32_t*ptr, uint32_t);
uint64_t xt_atomic_exchange_u64(xt_atomic_uint64_t*ptr, uint64_t);
int8_t   xt_atomic_exchange_i8 (xt_atomic_int8_t*ptr,   int8_t);
int16_t  xt_atomic_exchange_i16(xt_atomic_int16_t*ptr,  int16_t);
int32_t  xt_atomic_exchange_i32(xt_atomic_int32_t*ptr,  int32_t);
int64_t  xt_atomic_exchange_i64(xt_atomic_int64_t*ptr,  int64_t);

uint8_t xt_atomic_fetch_add_u8(xt_atomic_uint8_t*, uint8_t);
uint16_t xt_atomic_fetch_add_u16(xt_atomic_uint16_t*ptr, uint16_t);
uint32_t xt_atomic_fetch_add_u32(xt_atomic_uint32_t*ptr, uint32_t);
uint64_t xt_atomic_fetch_add_u64(xt_atomic_uint64_t*ptr, uint64_t);
int8_t xt_atomic_fetch_add_i8(xt_atomic_int8_t*ptr, int8_t);
int16_t xt_atomic_fetch_add_i16(xt_atomic_int16_t*ptr, int16_t);
int32_t xt_atomic_fetch_add_i32(xt_atomic_int32_t*ptr, int32_t);
int64_t xt_atomic_fetch_add_i64(xt_atomic_int64_t*ptr, int64_t);

uint8_t xt_atomic_fetch_sub_u8(xt_atomic_uint8_t*, uint8_t);
uint16_t xt_atomic_fetch_sub_u16(xt_atomic_uint16_t*ptr, uint16_t);
uint32_t xt_atomic_fetch_sub_u32(xt_atomic_uint32_t*ptr, uint32_t);
uint64_t xt_atomic_fetch_sub_u64(xt_atomic_uint64_t*ptr, uint64_t);
int8_t xt_atomic_fetch_sub_i8(xt_atomic_int8_t*ptr, int8_t);
int16_t xt_atomic_fetch_sub_i16(xt_atomic_int16_t*ptr, int16_t);
int32_t xt_atomic_fetch_sub_i32(xt_atomic_int32_t*ptr, int32_t);
int64_t xt_atomic_fetch_sub_i64(xt_atomic_int64_t*ptr, int64_t);

uint8_t xt_atomic_fetch_and_u8(xt_atomic_uint8_t*, uint8_t);
uint16_t xt_atomic_fetch_and_u16(xt_atomic_uint16_t*ptr, uint16_t);
uint32_t xt_atomic_fetch_and_u32(xt_atomic_uint32_t*ptr, uint32_t);
uint64_t xt_atomic_fetch_and_u64(xt_atomic_uint64_t*ptr, uint64_t);
int8_t xt_atomic_fetch_and_i8(xt_atomic_int8_t*ptr, int8_t);
int16_t xt_atomic_fetch_and_i16(xt_atomic_int16_t*ptr, int16_t);
int32_t xt_atomic_fetch_and_i32(xt_atomic_int32_t*ptr, int32_t);
int64_t xt_atomic_fetch_and_i64(xt_atomic_int64_t*ptr, int64_t);

// GCC has __atomic_fetch_nand, but MSVC doesn't. I don't really need it so it's not inluded here

uint8_t xt_atomic_fetch_nand_u8(xt_atomic_uint8_t*, uint8_t);
uint16_t xt_atomic_fetch_nand_u16(xt_atomic_uint16_t*ptr, uint16_t);
uint32_t xt_atomic_fetch_nand_u32(xt_atomic_uint32_t*ptr, uint32_t);
uint64_t xt_atomic_fetch_nand_u64(xt_atomic_uint64_t*ptr, uint64_t);
int8_t xt_atomic_fetch_nand_i8(xt_atomic_int8_t*ptr, int8_t);
int16_t xt_atomic_fetch_nand_i16(xt_atomic_int16_t*ptr, int16_t);
int32_t xt_atomic_fetch_nand_i32(xt_atomic_int32_t*ptr, int32_t);
int64_t xt_atomic_fetch_nand_i64(xt_atomic_int64_t*ptr, int64_t);

uint8_t xt_atomic_fetch_or_u8(xt_atomic_uint8_t*, uint8_t);
uint16_t xt_atomic_fetch_or_u16(xt_atomic_uint16_t*ptr, uint16_t);
uint32_t xt_atomic_fetch_or_u32(xt_atomic_uint32_t*ptr, uint32_t);
uint64_t xt_atomic_fetch_or_u64(xt_atomic_uint64_t*ptr, uint64_t);
int8_t xt_atomic_fetch_or_i8(xt_atomic_int8_t*ptr, int8_t);
int16_t xt_atomic_fetch_or_i16(xt_atomic_int16_t*ptr, int16_t);
int32_t xt_atomic_fetch_or_i32(xt_atomic_int32_t*ptr, int32_t);
int64_t xt_atomic_fetch_or_i64(xt_atomic_int64_t*ptr, int64_t);

uint8_t xt_atomic_fetch_xor_u8(xt_atomic_uint8_t*, uint8_t);
uint16_t xt_atomic_fetch_xor_u16(xt_atomic_uint16_t*ptr, uint16_t);
uint32_t xt_atomic_fetch_xor_u32(xt_atomic_uint32_t*ptr, uint32_t);
uint64_t xt_atomic_fetch_xor_u64(xt_atomic_uint64_t*ptr, uint64_t);
int8_t xt_atomic_fetch_xor_i8(xt_atomic_int8_t*ptr, int8_t);
int16_t xt_atomic_fetch_xor_i16(xt_atomic_int16_t*ptr, int16_t);
int32_t xt_atomic_fetch_xor_i32(xt_atomic_int32_t*ptr, int32_t);
int64_t xt_atomic_fetch_xor_i64(xt_atomic_int64_t*ptr, int64_t);

uint8_t  xt_atomic_compare_exchange_u8 (xt_atomic_uint8_t*  ptr, uint8_t  expected, uint8_t  desired);
uint16_t xt_atomic_compare_exchange_u16(xt_atomic_uint16_t* ptr, uint16_t expected, uint16_t desired);
uint32_t xt_atomic_compare_exchange_u32(xt_atomic_uint32_t* ptr, uint32_t expected, uint32_t desired);
uint64_t xt_atomic_compare_exchange_u64(xt_atomic_uint64_t* ptr, uint64_t expected, uint64_t desired);
int8_t   xt_atomic_compare_exchange_i8 (xt_atomic_int8_t*   ptr, int8_t   expected, int8_t   desired);
int16_t  xt_atomic_compare_exchange_i16(xt_atomic_int16_t*  ptr, int16_t  expected, int16_t  desired);
int32_t  xt_atomic_compare_exchange_i32(xt_atomic_int32_t*  ptr, int32_t  expected, int32_t  desired);
int64_t  xt_atomic_compare_exchange_i64(xt_atomic_int64_t*  ptr, int64_t  expected, int64_t  desired);

static inline void* xt_atomic_load_ptr(const xt_atomic_ptr_t* ptr) { return (void*)xt_atomic_load_u64((uint64_t*)ptr); }
static inline void  xt_atomic_store_ptr(xt_atomic_ptr_t* ptr, void* v) { xt_atomic_store_u64((uint64_t*)ptr, (uint64_t)v); }
static inline void* xt_atomic_exchange_ptr(xt_atomic_ptr_t* ptr, void* v) { return (void*)xt_atomic_exchange_u64((uint64_t*)ptr, (uint64_t)v); }
static inline bool  xt_atomic_compare_exchange_strong_ptr(xt_atomic_ptr_t* ptr, void* expected, void* desired) { return xt_atomic_compare_exchange_u64((xt_atomic_uint64_t*)ptr, (uint64_t)expected, (uint64_t)desired); }

union xt_uif {unsigned u; int i; float f;};

static inline float xt_atomic_load_f32(const xt_atomic_float* ptr)
{
    union xt_uif v  = {.u = xt_atomic_load_u32(ptr)};
    return v.f;
}

static inline void xt_atomic_store_f32(xt_atomic_float* ptr, float v)
{
    union xt_uif a = {.f = v};
    xt_atomic_store_u32(ptr, a.u);
}

static inline float xt_atomic_exchange_f32(xt_atomic_float* ptr, float v)
{
    union xt_uif a = {.f = v};
    union xt_uif b = {.u = xt_atomic_exchange_u32(ptr, a.u)};
    return b.f;
}


void xthread_timer_init(xt_timer_t* timer);
void xthread_timer_term(xt_timer_t* timer);
void xthread_timer_wait(xt_timer_t* timer, uint64_t nanoseconds);

xt_tls_t xthread_tls_create(void);

void  xthread_tls_destroy(xt_tls_t tls);
void  xthread_tls_set(xt_tls_t tls, void* value);
void* xthread_tls_get(xt_tls_t tls);

void  xthread_queue_init(xt_queue_t* queue, int size, void** values, int count);
void  xthread_queue_term(xt_queue_t* queue);
int   xthread_queue_produce(xt_queue_t* queue, void* value, int timeout_ms);
void* xthread_queue_consume(xt_queue_t* queue, int timeout_ms);
int   xthread_queue_count(xt_queue_t* queue);

// Progressive backoff spinlock based on Timur Doumler's ADC 2020 talk
// https://www.youtube.com/watch?v=zrWYJ6FdOFQ
void xt_spinlock_lock(xt_spinlock_t* ptr);
bool xt_spinlock_trylock(xt_spinlock_t* ptr);
void xt_spinlock_unlock(xt_spinlock_t* ptr);

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
    int               size;
    void**            values;
#ifndef NDEBUG
    xt_atomic_int32_t id_produce_is_set;
    xt_atomic_int32_t id_consume_is_set;
    xt_thread_ptr_t   id_produce;
    xt_thread_ptr_t   id_consume;
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* XHL_THREAD_H */

#ifdef XHL_THREAD_IMPL
#undef XHL_THREAD_IMPL

#if __SSE2__ || __AVX__
// Used for _mm_pause()
// https://www.intel.com/content/www/us/en/docs/cpp-compiler/developer-guide-reference/2021-10/pause-intrinsic.html
#include <emmintrin.h>
#elif __arm64__
// Used for __wfe()
// https://developer.arm.com/documentation/dui0375/g/Compiler-specific-Features/--wfe-intrinsic
#include <arm_acle.h>
#else
#error "Microarchitecture not supported!"
#endif

#include <assert.h>
#define XTHREAD_ASSERT(expression, message) assert((expression) && (message))

#if defined(_WIN32)

#pragma comment(lib, "winmm.lib")

#if ! defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0600
#error "Requires Windows Vista or greater"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCKAPI_
#include <windows.h>

#include <timeapi.h>

xt_thread_ptr_t xthread_current(void) { return (void*)(uintptr_t)GetCurrentThreadId(); }
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

xt_tls_t xthread_tls_create(void)
{
    DWORD tls = TlsAlloc();
    if (tls == TLS_OUT_OF_INDEXES)
        return NULL;
    else
        return (xt_tls_t)(uintptr_t)tls;
}
void  xthread_tls_destroy(xt_tls_t tls)              { TlsFree((DWORD)(uintptr_t)tls); }
void  xthread_tls_set    (xt_tls_t tls, void* value) { TlsSetValue((DWORD)(uintptr_t)tls, value); }
void* xthread_tls_get    (xt_tls_t tls)              { return TlsGetValue((DWORD)(uintptr_t)tls); }

#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

xt_thread_ptr_t xthread_current(void) { return (void*)pthread_self(); }

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

_Static_assert(sizeof(xt_mutex_t) >= sizeof(pthread_mutex_t), "Mutex too smol...");
void xthread_mutex_init(xt_mutex_t* mutex) { pthread_mutex_init((pthread_mutex_t*)mutex, NULL); }
void xthread_mutex_term(xt_mutex_t* mutex) { pthread_mutex_destroy((pthread_mutex_t*)mutex); }
void xthread_mutex_lock(xt_mutex_t* mutex) { pthread_mutex_lock((pthread_mutex_t*)mutex); }
void xthread_mutex_unlock(xt_mutex_t* mutex) { pthread_mutex_unlock((pthread_mutex_t*)mutex); }

xt_tls_t xthread_tls_create(void)
{
    pthread_key_t tls;
    if (pthread_key_create(&tls, NULL) == 0)
        return (xt_tls_t)tls;
    else
        return NULL;
}
void  xthread_tls_destroy(xt_tls_t tls)          { pthread_key_delete((pthread_key_t)(uintptr_t)tls); }
void  xthread_tls_set(xt_tls_t tls, void* value) { pthread_setspecific((pthread_key_t)(uintptr_t)tls, value); }
void* xthread_tls_get(xt_tls_t tls)              { return pthread_getspecific((pthread_key_t)(uintptr_t)tls); }

#else
#error Unknown platform.
#endif

#if defined(_MSC_VER) && !(__clang__)

uint8_t  xt_atomic_load_u8 (const xt_atomic_uint8_t*  ptr) { return _InterlockedCompareExchange8 ((xt_atomic_int8_t*)ptr, 0, 0); }
uint16_t xt_atomic_load_u16(const xt_atomic_uint16_t* ptr) { return _InterlockedCompareExchange16((xt_atomic_int16_t*)ptr, 0, 0); }
uint32_t xt_atomic_load_u32(const xt_atomic_uint32_t* ptr) { return _InterlockedCompareExchange((xt_atomic_int32_t*)ptr, 0, 0); }
uint64_t xt_atomic_load_u64(const xt_atomic_uint64_t* ptr) { return _InterlockedCompareExchange64(ptr, 0, 0); }
int8_t   xt_atomic_load_i8 (const xt_atomic_int8_t*   ptr) { return _InterlockedCompareExchange8 (ptr, 0, 0); }
int16_t  xt_atomic_load_i16(const xt_atomic_int16_t*  ptr) { return _InterlockedCompareExchange16(ptr, 0, 0); }
int32_t  xt_atomic_load_i32(const xt_atomic_int32_t*  ptr) { return _InterlockedCompareExchange(ptr, 0, 0); }
int64_t  xt_atomic_load_i64(const xt_atomic_int64_t*  ptr) { return _InterlockedCompareExchange64(ptr, 0, 0); }

void xt_atomic_store_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { _InterlockedExchange8(ptr, v); }
void xt_atomic_store_u16(xt_atomic_uint16_t* ptr, uint16_t v) { _InterlockedExchange16(ptr, v); }
void xt_atomic_store_u32(xt_atomic_uint32_t* ptr, uint32_t v) { _InterlockedExchange(ptr, v); }
void xt_atomic_store_u64(xt_atomic_uint64_t* ptr, uint64_t v) { _InterlockedExchange64(ptr, v); }
void xt_atomic_store_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { _InterlockedExchange8(ptr, v); }
void xt_atomic_store_i16(xt_atomic_int16_t*  ptr, int16_t  v) { _InterlockedExchange16(ptr, v); }
void xt_atomic_store_i32(xt_atomic_int32_t*  ptr, int32_t  v) { _InterlockedExchange(ptr, v); }
void xt_atomic_store_i64(xt_atomic_int64_t*  ptr, int64_t  v) { _InterlockedExchange64(ptr, v); }

uint8_t  xt_atomic_exchange_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return _InterlockedExchange8(ptr, v); }
uint16_t xt_atomic_exchange_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return _InterlockedExchange16(ptr, v); }
uint32_t xt_atomic_exchange_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return _InterlockedExchange(ptr, v); }
uint64_t xt_atomic_exchange_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return _InterlockedExchange64(ptr, v); }
int8_t   xt_atomic_exchange_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return _InterlockedExchange8(ptr, v); }
int16_t  xt_atomic_exchange_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return _InterlockedExchange16(ptr, v); }
int32_t  xt_atomic_exchange_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return _InterlockedExchange(ptr, v); }
int64_t  xt_atomic_exchange_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return _InterlockedExchange64(ptr, v); }

uint8_t  xt_atomic_fetch_add_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return _InterlockedExchangeAdd8(ptr, v); }
uint16_t xt_atomic_fetch_add_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return _InterlockedExchangeAdd16(ptr, v); }
uint32_t xt_atomic_fetch_add_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return _InterlockedExchangeAdd(ptr, v); }
uint64_t xt_atomic_fetch_add_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return _InterlockedExchangeAdd64(ptr, v); }
int8_t   xt_atomic_fetch_add_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return _InterlockedExchangeAdd8(ptr, v); }
int16_t  xt_atomic_fetch_add_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return _InterlockedExchangeAdd16(ptr, v); }
int32_t  xt_atomic_fetch_add_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return _InterlockedExchangeAdd(ptr, v); }
int64_t  xt_atomic_fetch_add_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return _InterlockedExchangeAdd64(ptr, v); }

uint8_t  xt_atomic_fetch_sub_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return xt_atomic_fetch_add_u8 (ptr, -v); }
uint16_t xt_atomic_fetch_sub_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return xt_atomic_fetch_add_u16(ptr, -v); }
uint32_t xt_atomic_fetch_sub_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return xt_atomic_fetch_add_u32(ptr, -v); }
uint64_t xt_atomic_fetch_sub_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return xt_atomic_fetch_add_u64(ptr, -v); }
int8_t   xt_atomic_fetch_sub_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return xt_atomic_fetch_add_i8 (ptr, -v); }
int16_t  xt_atomic_fetch_sub_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return xt_atomic_fetch_add_i16(ptr, -v); }
int32_t  xt_atomic_fetch_sub_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return xt_atomic_fetch_add_i32(ptr, -v); }
int64_t  xt_atomic_fetch_sub_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return xt_atomic_fetch_add_i64(ptr, -v); }

uint8_t  xt_atomic_fetch_and_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return _InterlockedAnd8(ptr, v); }
uint16_t xt_atomic_fetch_and_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return _InterlockedAnd16(ptr, v); }
uint32_t xt_atomic_fetch_and_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return _InterlockedAnd(ptr, v); }
uint64_t xt_atomic_fetch_and_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return _InterlockedAnd64(ptr, v); }
int8_t   xt_atomic_fetch_and_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return _InterlockedAnd8(ptr, v); }
int16_t  xt_atomic_fetch_and_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return _InterlockedAnd16(ptr, v); }
int32_t  xt_atomic_fetch_and_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return _InterlockedAnd(ptr, v); }
int64_t  xt_atomic_fetch_and_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return _InterlockedAnd64(ptr, v); }

uint8_t  xt_atomic_fetch_or_u8 (xt_atomic_uint8_t* ptr, uint8_t  v) { return _InterlockedOr8(ptr, v); }
uint16_t xt_atomic_fetch_or_u16(xt_atomic_uint16_t*ptr, uint16_t v) { return _InterlockedOr16(ptr, v); }
uint32_t xt_atomic_fetch_or_u32(xt_atomic_uint32_t*ptr, uint32_t v) { return _InterlockedOr(ptr, v); }
uint64_t xt_atomic_fetch_or_u64(xt_atomic_uint64_t*ptr, uint64_t v) { return _InterlockedOr64(ptr, v); }
int8_t   xt_atomic_fetch_or_i8 (xt_atomic_int8_t*ptr,   int8_t   v) { return _InterlockedOr8(ptr, v); }
int16_t  xt_atomic_fetch_or_i16(xt_atomic_int16_t*ptr,  int16_t  v) { return _InterlockedOr16(ptr, v); }
int32_t  xt_atomic_fetch_or_i32(xt_atomic_int32_t*ptr,  int32_t  v) { return _InterlockedOr(ptr, v); }
int64_t  xt_atomic_fetch_or_i64(xt_atomic_int64_t*ptr,  int64_t  v) { return _InterlockedOr64(ptr, v); }

uint8_t  xt_atomic_fetch_xor_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return _InterlockedXor8(ptr, v); }
uint16_t xt_atomic_fetch_xor_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return _InterlockedXor16(ptr, v); }
uint32_t xt_atomic_fetch_xor_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return _InterlockedXor(ptr, v); }
uint64_t xt_atomic_fetch_xor_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return _InterlockedXor64(ptr, v); }
int8_t   xt_atomic_fetch_xor_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return _InterlockedXor8(ptr, v); }
int16_t  xt_atomic_fetch_xor_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return _InterlockedXor16(ptr, v); }
int32_t  xt_atomic_fetch_xor_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return _InterlockedXor(ptr, v); }
int64_t  xt_atomic_fetch_xor_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return _InterlockedXor64(ptr, v); }

uint8_t  xt_atomic_compare_exchange_u8 (xt_atomic_uint8_t*  ptr, uint8_t  expected, uint8_t  desired) { return _InterlockedCompareExchange8 (ptr, desired, expected); }
uint16_t xt_atomic_compare_exchange_u16(xt_atomic_uint16_t* ptr, uint16_t expected, uint16_t desired) { return _InterlockedCompareExchange16(ptr, desired, expected); }
uint32_t xt_atomic_compare_exchange_u32(xt_atomic_uint32_t* ptr, uint32_t expected, uint32_t desired) { return _InterlockedCompareExchange  (ptr, desired, expected); }
uint64_t xt_atomic_compare_exchange_u64(xt_atomic_uint64_t* ptr, uint64_t expected, uint64_t desired) { return _InterlockedCompareExchange64(ptr, desired, expected); }
int8_t   xt_atomic_compare_exchange_i8 (xt_atomic_int8_t*   ptr, int8_t   expected, int8_t   desired) { return _InterlockedCompareExchange8 (ptr, desired, expected); }
int16_t  xt_atomic_compare_exchange_i16(xt_atomic_int16_t*  ptr, int16_t  expected, int16_t  desired) { return _InterlockedCompareExchange16(ptr, desired, expected); }
int32_t  xt_atomic_compare_exchange_i32(xt_atomic_int32_t*  ptr, int32_t  expected, int32_t  desired) { return _InterlockedCompareExchange  (ptr, desired, expected); }
int64_t  xt_atomic_compare_exchange_i64(xt_atomic_int64_t*  ptr, int64_t  expected, int64_t  desired) { return _InterlockedCompareExchange64(ptr, desired, expected); }

bool xt_spinlock_trylock(xt_spinlock_t*  ptr) { return _InterlockedCompareExchange8(ptr, 1, 0); }
void xt_spinlock_unlock( xt_spinlock_t*  ptr) { return _InterlockedExchange8(ptr, 0); }

#else

uint8_t  xt_atomic_load_u8 (const xt_atomic_uint8_t*  ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
uint16_t xt_atomic_load_u16(const xt_atomic_uint16_t* ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
uint32_t xt_atomic_load_u32(const xt_atomic_uint32_t* ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
uint64_t xt_atomic_load_u64(const xt_atomic_uint64_t* ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
int8_t   xt_atomic_load_i8 (const xt_atomic_int8_t*   ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
int16_t  xt_atomic_load_i16(const xt_atomic_int16_t*  ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
int32_t  xt_atomic_load_i32(const xt_atomic_int32_t*  ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
int64_t  xt_atomic_load_i64(const xt_atomic_int64_t*  ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }

void xt_atomic_store_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
void xt_atomic_store_u16(xt_atomic_uint16_t* ptr, uint16_t v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
void xt_atomic_store_u32(xt_atomic_uint32_t* ptr, uint32_t v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
void xt_atomic_store_u64(xt_atomic_uint64_t* ptr, uint64_t v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
void xt_atomic_store_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
void xt_atomic_store_i16(xt_atomic_int16_t*  ptr, int16_t  v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
void xt_atomic_store_i32(xt_atomic_int32_t*  ptr, int32_t  v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
void xt_atomic_store_i64(xt_atomic_int64_t*  ptr, int64_t  v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }

uint8_t  xt_atomic_exchange_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
uint16_t xt_atomic_exchange_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
uint32_t xt_atomic_exchange_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
uint64_t xt_atomic_exchange_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
int8_t   xt_atomic_exchange_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
int16_t  xt_atomic_exchange_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
int32_t  xt_atomic_exchange_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
int64_t  xt_atomic_exchange_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }

uint8_t  xt_atomic_fetch_add_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
uint16_t xt_atomic_fetch_add_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
uint32_t xt_atomic_fetch_add_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
uint64_t xt_atomic_fetch_add_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
int8_t   xt_atomic_fetch_add_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
int16_t  xt_atomic_fetch_add_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
int32_t  xt_atomic_fetch_add_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
int64_t  xt_atomic_fetch_add_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }

uint8_t  xt_atomic_fetch_sub_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
uint16_t xt_atomic_fetch_sub_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
uint32_t xt_atomic_fetch_sub_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
uint64_t xt_atomic_fetch_sub_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
int8_t   xt_atomic_fetch_sub_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
int16_t  xt_atomic_fetch_sub_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
int32_t  xt_atomic_fetch_sub_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
int64_t  xt_atomic_fetch_sub_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }

uint8_t  xt_atomic_fetch_and_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
uint16_t xt_atomic_fetch_and_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
uint32_t xt_atomic_fetch_and_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
uint64_t xt_atomic_fetch_and_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
int8_t   xt_atomic_fetch_and_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
int16_t  xt_atomic_fetch_and_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
int32_t  xt_atomic_fetch_and_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
int64_t  xt_atomic_fetch_and_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }

uint8_t  xt_atomic_fetch_or_u8 (xt_atomic_uint8_t* ptr, uint8_t  v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
uint16_t xt_atomic_fetch_or_u16(xt_atomic_uint16_t*ptr, uint16_t v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
uint32_t xt_atomic_fetch_or_u32(xt_atomic_uint32_t*ptr, uint32_t v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
uint64_t xt_atomic_fetch_or_u64(xt_atomic_uint64_t*ptr, uint64_t v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
int8_t   xt_atomic_fetch_or_i8 (xt_atomic_int8_t*ptr,   int8_t   v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
int16_t  xt_atomic_fetch_or_i16(xt_atomic_int16_t*ptr,  int16_t  v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
int32_t  xt_atomic_fetch_or_i32(xt_atomic_int32_t*ptr,  int32_t  v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
int64_t  xt_atomic_fetch_or_i64(xt_atomic_int64_t*ptr,  int64_t  v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }

uint8_t  xt_atomic_fetch_xor_u8 (xt_atomic_uint8_t*  ptr, uint8_t  v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
uint16_t xt_atomic_fetch_xor_u16(xt_atomic_uint16_t* ptr, uint16_t v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
uint32_t xt_atomic_fetch_xor_u32(xt_atomic_uint32_t* ptr, uint32_t v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
uint64_t xt_atomic_fetch_xor_u64(xt_atomic_uint64_t* ptr, uint64_t v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
int8_t   xt_atomic_fetch_xor_i8 (xt_atomic_int8_t*   ptr, int8_t   v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
int16_t  xt_atomic_fetch_xor_i16(xt_atomic_int16_t*  ptr, int16_t  v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
int32_t  xt_atomic_fetch_xor_i32(xt_atomic_int32_t*  ptr, int32_t  v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
int64_t  xt_atomic_fetch_xor_i64(xt_atomic_int64_t*  ptr, int64_t  v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }

uint8_t  xt_atomic_compare_exchange_u8 (xt_atomic_uint8_t*  ptr, uint8_t  expected, uint8_t  desired) { return __sync_val_compare_and_swap(           ptr, expected, desired); }
uint16_t xt_atomic_compare_exchange_u16(xt_atomic_uint16_t* ptr, uint16_t expected, uint16_t desired) { return __sync_val_compare_and_swap(           ptr, expected, desired); }
uint32_t xt_atomic_compare_exchange_u32(xt_atomic_uint32_t* ptr, uint32_t expected, uint32_t desired) { return __sync_val_compare_and_swap(           ptr, expected, desired); }
uint64_t xt_atomic_compare_exchange_u64(xt_atomic_uint64_t* ptr, uint64_t expected, uint64_t desired) { return __sync_val_compare_and_swap(           ptr, expected, desired); }
int8_t   xt_atomic_compare_exchange_i8 (xt_atomic_int8_t*   ptr, int8_t   expected, int8_t   desired) { return __sync_val_compare_and_swap((uint8_t*) ptr, expected, desired); }
int16_t  xt_atomic_compare_exchange_i16(xt_atomic_int16_t*  ptr, int16_t  expected, int16_t  desired) { return __sync_val_compare_and_swap((uint16_t*)ptr, expected, desired); }
int32_t  xt_atomic_compare_exchange_i32(xt_atomic_int32_t*  ptr, int32_t  expected, int32_t  desired) { return __sync_val_compare_and_swap((uint32_t*)ptr, expected, desired); }
int64_t  xt_atomic_compare_exchange_i64(xt_atomic_int64_t*  ptr, int64_t  expected, int64_t  desired) { return __sync_val_compare_and_swap((uint64_t*)ptr, expected, desired); }

bool xt_spinlock_trylock(xt_spinlock_t* ptr) { return __atomic_exchange_n(ptr, 1, __ATOMIC_ACQUIRE); }
void xt_spinlock_unlock(xt_spinlock_t* ptr)  { __atomic_store_n(ptr, 0, __ATOMIC_RELEASE); }

#endif

struct xthread_internal_signal_t
{
#if defined(_WIN32)
    CRITICAL_SECTION   mutex;
    CONDITION_VARIABLE condition;
    int                value;
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    pthread_mutex_t mutex;
    pthread_cond_t  condition;
    int             value;
#endif
};
_Static_assert(sizeof(xt_signal_t) >= sizeof(struct xthread_internal_signal_t), "too smol");

void xthread_signal_init(xt_signal_t* signal)
{
    struct xthread_internal_signal_t* internal = (struct xthread_internal_signal_t*)signal;

#if defined(_WIN32)
    InitializeCriticalSectionAndSpinCount(&internal->mutex, 32);
    InitializeConditionVariable(&internal->condition);
    internal->value = 0;
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
    DeleteCriticalSection(&internal->mutex);
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__)
    pthread_mutex_destroy(&internal->mutex);
    pthread_cond_destroy(&internal->condition);
#endif
}

void xthread_signal_raise(xt_signal_t* signal)
{
    struct xthread_internal_signal_t* internal = (struct xthread_internal_signal_t*)signal;

#if defined(_WIN32)
    EnterCriticalSection(&internal->mutex);
    internal->value = 1;
    LeaveCriticalSection(&internal->mutex);
    WakeConditionVariable(&internal->condition);
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

void xthread_timer_wait(xt_timer_t* timer, uint64_t nanoseconds)
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
    xt_atomic_store_i32(&queue->head, 0);
    xt_atomic_store_i32(&queue->tail, count > size ? size : count);
    xt_atomic_store_i32(&queue->count, count > size ? size : count);
    queue->size = size;
#ifndef NDEBUG
    xt_atomic_store_i32(&queue->id_produce_is_set, 0);
    xt_atomic_store_i32(&queue->id_consume_is_set, 0);
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
    if (xt_atomic_compare_exchange_i32(&queue->id_produce_is_set, 0, 1) == 0)
        queue->id_produce = xthread_current();
    XTHREAD_ASSERT(
        xthread_current() == queue->id_produce,
        "thread_queue_produce called from multiple threads");
#endif
    while (xt_atomic_load_i32(&queue->count) ==
           queue->size) // TODO: fix signal so that this can be an "if" instead of "while"
    {
        if (timeout_ms == 0)
            return 0;
        if (xthread_signal_wait(
                &queue->space_open,
                timeout_ms == XTHREAD_QUEUE_WAIT_INFINITE ? XTHREAD_SIGNAL_WAIT_INFINITE : timeout_ms) == 0)
            return 0;
    }
    int tail                          = xt_atomic_fetch_add_i32(&queue->tail, 1);
    queue->values[tail % queue->size] = value;
    if (xt_atomic_fetch_add_i32(&queue->count, 1) == 0)
        xthread_signal_raise(&queue->data_ready);
    return 1;
}

void* xthread_queue_consume(xt_queue_t* queue, int timeout_ms)
{
#ifndef NDEBUG
    if (xt_atomic_compare_exchange_i32(&queue->id_consume_is_set, 0, 1) == 0)
        queue->id_consume = xthread_current();
    XTHREAD_ASSERT(
        xthread_current() == queue->id_consume,
        "thread_queue_consume called from multiple threads");
#endif
    while (xt_atomic_load_i32(&queue->count) ==
           0) // TODO: fix signal so that this can be an "if" instead of "while"
    {
        if (timeout_ms == 0)
            return NULL;
        if (xthread_signal_wait(
                &queue->data_ready,
                timeout_ms == XTHREAD_QUEUE_WAIT_INFINITE ? XTHREAD_SIGNAL_WAIT_INFINITE : timeout_ms) == 0)
            return NULL;
    }
    int   head   = xt_atomic_fetch_add_i32(&queue->head, 1);
    void* retval = queue->values[head % queue->size];
    if (xt_atomic_fetch_sub_i32(&queue->count, 1) == queue->size)
        xthread_signal_raise(&queue->space_open);
    return retval;
}

int xthread_queue_count(xt_queue_t* queue) { return xt_atomic_load_i32(&queue->count); }

void xt_spinlock_lock(xt_atomic_uint8_t* ptr)
{
#ifdef __SSE2__

    // Stage 1: ~25ns
    for (int i = 0; i < 5; i++)
        if (xt_spinlock_trylock(ptr))
            return;

    // Stage 2: ~400ns
    for (int i = 0; i < 10; i++)
    {
        if (xt_spinlock_trylock(ptr))
            return;

        _mm_pause();
    }

    // Stage 3: ~400ns
    for (int i = 0; i < 3000; i++)
    {
        if (xt_spinlock_trylock(ptr))
            return;

        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();
        _mm_pause();

        xthread_yield();
    }

#elif defined(__arm64__)

    // Stage 1: ~20ns
    for (int i = 0; i < 5; i++)
        if (xt_spinlock_trylock(ptr))
            return;

    // Stage 2: ~1ms
    for (int i = 0; i < 750; i++)
    {
        if (xt_spinlock_trylock(ptr))
            return;

        __wfe(); // ~1300ns

        xthread_yield();
    }
#endif
/*
NOTE: only this function is under the Boost license
https://github.com/crill-dev/crill

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/
}

#endif /* XHL_THREAD_IMPL */
// clang-format on