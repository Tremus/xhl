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

#ifdef _WIN32
#define xt_debugbreak() __debugbreak()
#else
#define xt_debugbreak() __builtin_debugtrap()
#endif

#ifdef NDEBUG
#define xt_assert(...)
#else
static int _xt_break_helper = 0;
#define xt_assert(cond) (((cond) ? (void)0 : xt_debugbreak()), _xt_break_helper += 0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef void* xt_thread_ptr_t;

typedef volatile unsigned char      xatomic_u8;
typedef volatile unsigned short     xatomic_u16;
typedef volatile unsigned int       xatomic_u32;
typedef volatile uint64_t           xatomic_u64;

typedef volatile char      xatomic_int8_t;
typedef volatile short     xatomic_int16_t;
typedef volatile int       xatomic_i32;
typedef volatile int64_t   xatomic_i64;
typedef volatile void*     xatomic_ptr;
typedef volatile unsigned  xatomic_float;

typedef union  xt_mutex_t  xt_mutex_t;
typedef union  xt_signal_t xt_signal_t;
typedef union  xt_timer_t  xt_timer_t;
typedef struct xt_queue_t  xt_queue_t;

xt_thread_ptr_t xt_thread_current(void);
xt_thread_ptr_t xt_thread_create(int (*thread_proc)(void*), void* user_data, int stack_size);

void xt_thread_yield(void);
void xt_thread_set_high_priority(void);
void xt_thread_exit(int return_code);
void xt_thread_destroy(xt_thread_ptr_t thread);
int  xt_thread_join(xt_thread_ptr_t thread);
int  xt_thread_detach(xt_thread_ptr_t thread);

void xt_mutex_init(xt_mutex_t* mutex);
void xt_mutex_term(xt_mutex_t* mutex);
void xt_mutex_lock(xt_mutex_t* mutex);
void xt_mutex_unlock(xt_mutex_t* mutex);

void xt_signal_init(xt_signal_t* signal);
void xt_signal_term(xt_signal_t* signal);
void xt_signal_raise(xt_signal_t* signal);
int  xt_signal_wait(xt_signal_t* signal, int timeout_ms);

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


#if defined(_MSC_VER) && !(__clang__)

#if !defined(_M_X64) && !defined(_M_ARM64)
#error "xhl/thread.h atomics assume 64 bit (aligned 8 byte plain loads/stores are atomic)"
#endif

#include <intrin.h>

// x64 is TSO: plain loads already acquire and plain stores already release, so only the compiler needs fencing.
// ARM64 needs a real barrier. __iso_volatile_* are plain accesses regardless of /volatile:ms or /volatile:iso
#if defined(_M_ARM64)
#define XT_BARRIER() __dmb(_ARM64_BARRIER_ISH)
#else
#define XT_BARRIER() _ReadWriteBarrier()
#endif

// seq_cst loads: a plain load plus barrier is enough because every seq_cst store below is an interlocked op (full fence)
static uint8_t  xatomic_load_u8 (const xatomic_u8*  ptr) { uint8_t  v = (uint8_t) __iso_volatile_load8 ((const volatile __int8*) ptr); XT_BARRIER(); return v; }
static uint16_t xatomic_load_u16(const xatomic_u16* ptr) { uint16_t v = (uint16_t)__iso_volatile_load16((const volatile __int16*)ptr); XT_BARRIER(); return v; }
static uint32_t xatomic_load_u32(const xatomic_u32* ptr) { uint32_t v = (uint32_t)__iso_volatile_load32((const volatile __int32*)ptr); XT_BARRIER(); return v; }
static uint64_t xatomic_load_u64(const xatomic_u64* ptr) { uint64_t v = (uint64_t)__iso_volatile_load64((const volatile __int64*)ptr); XT_BARRIER(); return v; }
static int8_t   xatomic_load_i8 (const xatomic_int8_t*   ptr) { int8_t  v = (int8_t) __iso_volatile_load8 ((const volatile __int8*) ptr); XT_BARRIER(); return v; }
static int16_t  xatomic_load_i16(const xatomic_int16_t*  ptr) { int16_t v = (int16_t)__iso_volatile_load16((const volatile __int16*)ptr); XT_BARRIER(); return v; }
static int32_t  xatomic_load_i32(const xatomic_i32*  ptr) { int32_t v = (int32_t)__iso_volatile_load32((const volatile __int32*)ptr); XT_BARRIER(); return v; }
static int64_t  xatomic_load_i64(const xatomic_i64*  ptr) { int64_t v = (int64_t)__iso_volatile_load64((const volatile __int64*)ptr); XT_BARRIER(); return v; }

static void xatomic_store_u8 (xatomic_u8*  ptr, uint8_t  v) { _InterlockedExchange8(ptr, v); }
static void xatomic_store_u16(xatomic_u16* ptr, uint16_t v) { _InterlockedExchange16(ptr, v); }
static void xatomic_store_u32(xatomic_u32* ptr, uint32_t v) { _InterlockedExchange(ptr, v); }
static void xatomic_store_u64(xatomic_u64* ptr, uint64_t v) { _InterlockedExchange64(ptr, v); }
static void xatomic_store_i8 (xatomic_int8_t*   ptr, int8_t   v) { _InterlockedExchange8(ptr, v); }
static void xatomic_store_i16(xatomic_int16_t*  ptr, int16_t  v) { _InterlockedExchange16(ptr, v); }
static void xatomic_store_i32(xatomic_i32*  ptr, int32_t  v) { _InterlockedExchange(ptr, v); }
static void xatomic_store_i64(xatomic_i64*  ptr, int64_t  v) { _InterlockedExchange64(ptr, v); }

static uint8_t  xatomic_exchange_u8 (xatomic_u8*  ptr, uint8_t  v) { return _InterlockedExchange8(ptr, v); }
static uint16_t xatomic_exchange_u16(xatomic_u16* ptr, uint16_t v) { return _InterlockedExchange16(ptr, v); }
static uint32_t xatomic_exchange_u32(xatomic_u32* ptr, uint32_t v) { return _InterlockedExchange(ptr, v); }
static uint64_t xatomic_exchange_u64(xatomic_u64* ptr, uint64_t v) { return _InterlockedExchange64(ptr, v); }
static int8_t   xatomic_exchange_i8 (xatomic_int8_t*   ptr, int8_t   v) { return _InterlockedExchange8(ptr, v); }
static int16_t  xatomic_exchange_i16(xatomic_int16_t*  ptr, int16_t  v) { return _InterlockedExchange16(ptr, v); }
static int32_t  xatomic_exchange_i32(xatomic_i32*  ptr, int32_t  v) { return _InterlockedExchange(ptr, v); }
static int64_t  xatomic_exchange_i64(xatomic_i64*  ptr, int64_t  v) { return _InterlockedExchange64(ptr, v); }

static uint8_t  xatomic_fetch_add_u8 (xatomic_u8*  ptr, uint8_t  v) { return _InterlockedExchangeAdd8(ptr, v); }
static uint16_t xatomic_fetch_add_u16(xatomic_u16* ptr, uint16_t v) { return _InterlockedExchangeAdd16(ptr, v); }
static uint32_t xatomic_fetch_add_u32(xatomic_u32* ptr, uint32_t v) { return _InterlockedExchangeAdd(ptr, v); }
static uint64_t xatomic_fetch_add_u64(xatomic_u64* ptr, uint64_t v) { return _InterlockedExchangeAdd64(ptr, v); }
static int8_t   xatomic_fetch_add_i8 (xatomic_int8_t*   ptr, int8_t   v) { return _InterlockedExchangeAdd8(ptr, v); }
static int16_t  xatomic_fetch_add_i16(xatomic_int16_t*  ptr, int16_t  v) { return _InterlockedExchangeAdd16(ptr, v); }
static int32_t  xatomic_fetch_add_i32(xatomic_i32*  ptr, int32_t  v) { return _InterlockedExchangeAdd(ptr, v); }
static int64_t  xatomic_fetch_add_i64(xatomic_i64*  ptr, int64_t  v) { return _InterlockedExchangeAdd64(ptr, v); }

static uint8_t  xatomic_fetch_sub_u8 (xatomic_u8*  ptr, uint8_t  v) { return xatomic_fetch_add_u8 (ptr, -v); }
static uint16_t xatomic_fetch_sub_u16(xatomic_u16* ptr, uint16_t v) { return xatomic_fetch_add_u16(ptr, -v); }
static uint32_t xatomic_fetch_sub_u32(xatomic_u32* ptr, uint32_t v) { return xatomic_fetch_add_u32(ptr, -v); }
static uint64_t xatomic_fetch_sub_u64(xatomic_u64* ptr, uint64_t v) { return xatomic_fetch_add_u64(ptr, -v); }
static int8_t   xatomic_fetch_sub_i8 (xatomic_int8_t*   ptr, int8_t   v) { return xatomic_fetch_add_i8 (ptr, -v); }
static int16_t  xatomic_fetch_sub_i16(xatomic_int16_t*  ptr, int16_t  v) { return xatomic_fetch_add_i16(ptr, -v); }
static int32_t  xatomic_fetch_sub_i32(xatomic_i32*  ptr, int32_t  v) { return xatomic_fetch_add_i32(ptr, -v); }
static int64_t  xatomic_fetch_sub_i64(xatomic_i64*  ptr, int64_t  v) { return xatomic_fetch_add_i64(ptr, -v); }

static uint8_t  xatomic_fetch_and_u8 (xatomic_u8*  ptr, uint8_t  v) { return _InterlockedAnd8(ptr, v); }
static uint16_t xatomic_fetch_and_u16(xatomic_u16* ptr, uint16_t v) { return _InterlockedAnd16(ptr, v); }
static uint32_t xatomic_fetch_and_u32(xatomic_u32* ptr, uint32_t v) { return _InterlockedAnd(ptr, v); }
static uint64_t xatomic_fetch_and_u64(xatomic_u64* ptr, uint64_t v) { return _InterlockedAnd64(ptr, v); }
static int8_t   xatomic_fetch_and_i8 (xatomic_int8_t*   ptr, int8_t   v) { return _InterlockedAnd8(ptr, v); }
static int16_t  xatomic_fetch_and_i16(xatomic_int16_t*  ptr, int16_t  v) { return _InterlockedAnd16(ptr, v); }
static int32_t  xatomic_fetch_and_i32(xatomic_i32*  ptr, int32_t  v) { return _InterlockedAnd(ptr, v); }
static int64_t  xatomic_fetch_and_i64(xatomic_i64*  ptr, int64_t  v) { return _InterlockedAnd64(ptr, v); }

static uint8_t  xatomic_fetch_or_u8 (xatomic_u8* ptr, uint8_t  v) { return _InterlockedOr8(ptr, v); }
static uint16_t xatomic_fetch_or_u16(xatomic_u16*ptr, uint16_t v) { return _InterlockedOr16(ptr, v); }
static uint32_t xatomic_fetch_or_u32(xatomic_u32*ptr, uint32_t v) { return _InterlockedOr(ptr, v); }
static uint64_t xatomic_fetch_or_u64(xatomic_u64*ptr, uint64_t v) { return _InterlockedOr64(ptr, v); }
static int8_t   xatomic_fetch_or_i8 (xatomic_int8_t*ptr,   int8_t   v) { return _InterlockedOr8(ptr, v); }
static int16_t  xatomic_fetch_or_i16(xatomic_int16_t*ptr,  int16_t  v) { return _InterlockedOr16(ptr, v); }
static int32_t  xatomic_fetch_or_i32(xatomic_i32*ptr,  int32_t  v) { return _InterlockedOr(ptr, v); }
static int64_t  xatomic_fetch_or_i64(xatomic_i64*ptr,  int64_t  v) { return _InterlockedOr64(ptr, v); }

static uint8_t  xatomic_fetch_xor_u8 (xatomic_u8*  ptr, uint8_t  v) { return _InterlockedXor8(ptr, v); }
static uint16_t xatomic_fetch_xor_u16(xatomic_u16* ptr, uint16_t v) { return _InterlockedXor16(ptr, v); }
static uint32_t xatomic_fetch_xor_u32(xatomic_u32* ptr, uint32_t v) { return _InterlockedXor(ptr, v); }
static uint64_t xatomic_fetch_xor_u64(xatomic_u64* ptr, uint64_t v) { return _InterlockedXor64(ptr, v); }
static int8_t   xatomic_fetch_xor_i8 (xatomic_int8_t*   ptr, int8_t   v) { return _InterlockedXor8(ptr, v); }
static int16_t  xatomic_fetch_xor_i16(xatomic_int16_t*  ptr, int16_t  v) { return _InterlockedXor16(ptr, v); }
static int32_t  xatomic_fetch_xor_i32(xatomic_i32*  ptr, int32_t  v) { return _InterlockedXor(ptr, v); }
static int64_t  xatomic_fetch_xor_i64(xatomic_i64*  ptr, int64_t  v) { return _InterlockedXor64(ptr, v); }

static uint8_t  xatomic_compare_exchange_u8 (xatomic_u8*  ptr, uint8_t  expected, uint8_t  desired) { return _InterlockedCompareExchange8 (ptr, desired, expected); }
static uint16_t xatomic_compare_exchange_u16(xatomic_u16* ptr, uint16_t expected, uint16_t desired) { return _InterlockedCompareExchange16(ptr, desired, expected); }
static uint32_t xatomic_compare_exchange_u32(xatomic_u32* ptr, uint32_t expected, uint32_t desired) { return _InterlockedCompareExchange  (ptr, desired, expected); }
static uint64_t xatomic_compare_exchange_u64(xatomic_u64* ptr, uint64_t expected, uint64_t desired) { return _InterlockedCompareExchange64(ptr, desired, expected); }
static int8_t   xatomic_compare_exchange_i8 (xatomic_int8_t*   ptr, int8_t   expected, int8_t   desired) { return _InterlockedCompareExchange8 (ptr, desired, expected); }
static int16_t  xatomic_compare_exchange_i16(xatomic_int16_t*  ptr, int16_t  expected, int16_t  desired) { return _InterlockedCompareExchange16(ptr, desired, expected); }
static int32_t  xatomic_compare_exchange_i32(xatomic_i32*  ptr, int32_t  expected, int32_t  desired) { return _InterlockedCompareExchange  (ptr, desired, expected); }
static int64_t  xatomic_compare_exchange_i64(xatomic_i64*  ptr, int64_t  expected, int64_t  desired) { return _InterlockedCompareExchange64(ptr, desired, expected); }

#else

static uint8_t  xatomic_load_u8 (const xatomic_u8*  ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
static uint16_t xatomic_load_u16(const xatomic_u16* ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
static uint32_t xatomic_load_u32(const xatomic_u32* ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
static uint64_t xatomic_load_u64(const xatomic_u64* ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
static int8_t   xatomic_load_i8 (const xatomic_int8_t*   ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
static int16_t  xatomic_load_i16(const xatomic_int16_t*  ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
static int32_t  xatomic_load_i32(const xatomic_i32*  ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
static int64_t  xatomic_load_i64(const xatomic_i64*  ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }

static void xatomic_store_u8 (xatomic_u8*  ptr, uint8_t  v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
static void xatomic_store_u16(xatomic_u16* ptr, uint16_t v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
static void xatomic_store_u32(xatomic_u32* ptr, uint32_t v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
static void xatomic_store_u64(xatomic_u64* ptr, uint64_t v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
static void xatomic_store_i8 (xatomic_int8_t*   ptr, int8_t   v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
static void xatomic_store_i16(xatomic_int16_t*  ptr, int16_t  v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
static void xatomic_store_i32(xatomic_i32*  ptr, int32_t  v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }
static void xatomic_store_i64(xatomic_i64*  ptr, int64_t  v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }

static uint8_t  xatomic_exchange_u8 (xatomic_u8*  ptr, uint8_t  v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
static uint16_t xatomic_exchange_u16(xatomic_u16* ptr, uint16_t v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
static uint32_t xatomic_exchange_u32(xatomic_u32* ptr, uint32_t v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
static uint64_t xatomic_exchange_u64(xatomic_u64* ptr, uint64_t v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
static int8_t   xatomic_exchange_i8 (xatomic_int8_t*   ptr, int8_t   v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
static int16_t  xatomic_exchange_i16(xatomic_int16_t*  ptr, int16_t  v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
static int32_t  xatomic_exchange_i32(xatomic_i32*  ptr, int32_t  v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }
static int64_t  xatomic_exchange_i64(xatomic_i64*  ptr, int64_t  v) { return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST); }

static uint8_t  xatomic_fetch_add_u8 (xatomic_u8*  ptr, uint8_t  v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
static uint16_t xatomic_fetch_add_u16(xatomic_u16* ptr, uint16_t v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
static uint32_t xatomic_fetch_add_u32(xatomic_u32* ptr, uint32_t v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
static uint64_t xatomic_fetch_add_u64(xatomic_u64* ptr, uint64_t v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
static int8_t   xatomic_fetch_add_i8 (xatomic_int8_t*   ptr, int8_t   v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
static int16_t  xatomic_fetch_add_i16(xatomic_int16_t*  ptr, int16_t  v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
static int32_t  xatomic_fetch_add_i32(xatomic_i32*  ptr, int32_t  v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }
static int64_t  xatomic_fetch_add_i64(xatomic_i64*  ptr, int64_t  v) { return __atomic_fetch_add(ptr, v, __ATOMIC_SEQ_CST); }

static uint8_t  xatomic_fetch_sub_u8 (xatomic_u8*  ptr, uint8_t  v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
static uint16_t xatomic_fetch_sub_u16(xatomic_u16* ptr, uint16_t v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
static uint32_t xatomic_fetch_sub_u32(xatomic_u32* ptr, uint32_t v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
static uint64_t xatomic_fetch_sub_u64(xatomic_u64* ptr, uint64_t v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
static int8_t   xatomic_fetch_sub_i8 (xatomic_int8_t*   ptr, int8_t   v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
static int16_t  xatomic_fetch_sub_i16(xatomic_int16_t*  ptr, int16_t  v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
static int32_t  xatomic_fetch_sub_i32(xatomic_i32*  ptr, int32_t  v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }
static int64_t  xatomic_fetch_sub_i64(xatomic_i64*  ptr, int64_t  v) { return __atomic_fetch_sub(ptr, v, __ATOMIC_SEQ_CST); }

static uint8_t  xatomic_fetch_and_u8 (xatomic_u8*  ptr, uint8_t  v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
static uint16_t xatomic_fetch_and_u16(xatomic_u16* ptr, uint16_t v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
static uint32_t xatomic_fetch_and_u32(xatomic_u32* ptr, uint32_t v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
static uint64_t xatomic_fetch_and_u64(xatomic_u64* ptr, uint64_t v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
static int8_t   xatomic_fetch_and_i8 (xatomic_int8_t*   ptr, int8_t   v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
static int16_t  xatomic_fetch_and_i16(xatomic_int16_t*  ptr, int16_t  v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
static int32_t  xatomic_fetch_and_i32(xatomic_i32*  ptr, int32_t  v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }
static int64_t  xatomic_fetch_and_i64(xatomic_i64*  ptr, int64_t  v) { return __atomic_fetch_and(ptr, v, __ATOMIC_SEQ_CST); }

static uint8_t  xatomic_fetch_or_u8 (xatomic_u8* ptr, uint8_t  v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
static uint16_t xatomic_fetch_or_u16(xatomic_u16*ptr, uint16_t v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
static uint32_t xatomic_fetch_or_u32(xatomic_u32*ptr, uint32_t v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
static uint64_t xatomic_fetch_or_u64(xatomic_u64*ptr, uint64_t v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
static int8_t   xatomic_fetch_or_i8 (xatomic_int8_t*ptr,   int8_t   v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
static int16_t  xatomic_fetch_or_i16(xatomic_int16_t*ptr,  int16_t  v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
static int32_t  xatomic_fetch_or_i32(xatomic_i32*ptr,  int32_t  v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }
static int64_t  xatomic_fetch_or_i64(xatomic_i64*ptr,  int64_t  v) { return __atomic_fetch_or(ptr, v, __ATOMIC_SEQ_CST); }

static uint8_t  xatomic_fetch_xor_u8 (xatomic_u8*  ptr, uint8_t  v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
static uint16_t xatomic_fetch_xor_u16(xatomic_u16* ptr, uint16_t v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
static uint32_t xatomic_fetch_xor_u32(xatomic_u32* ptr, uint32_t v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
static uint64_t xatomic_fetch_xor_u64(xatomic_u64* ptr, uint64_t v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
static int8_t   xatomic_fetch_xor_i8 (xatomic_int8_t*   ptr, int8_t   v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
static int16_t  xatomic_fetch_xor_i16(xatomic_int16_t*  ptr, int16_t  v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
static int32_t  xatomic_fetch_xor_i32(xatomic_i32*  ptr, int32_t  v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }
static int64_t  xatomic_fetch_xor_i64(xatomic_i64*  ptr, int64_t  v) { return __atomic_fetch_xor(ptr, v, __ATOMIC_SEQ_CST); }

static uint8_t  xatomic_compare_exchange_u8 (xatomic_u8*  ptr, uint8_t  expected, uint8_t  desired) { return __sync_val_compare_and_swap(           ptr, expected, desired); }
static uint16_t xatomic_compare_exchange_u16(xatomic_u16* ptr, uint16_t expected, uint16_t desired) { return __sync_val_compare_and_swap(           ptr, expected, desired); }
static uint32_t xatomic_compare_exchange_u32(xatomic_u32* ptr, uint32_t expected, uint32_t desired) { return __sync_val_compare_and_swap(           ptr, expected, desired); }
static uint64_t xatomic_compare_exchange_u64(xatomic_u64* ptr, uint64_t expected, uint64_t desired) { return __sync_val_compare_and_swap(           ptr, expected, desired); }
static int8_t   xatomic_compare_exchange_i8 (xatomic_int8_t*   ptr, int8_t   expected, int8_t   desired) { return __sync_val_compare_and_swap((uint8_t*) ptr, expected, desired); }
static int16_t  xatomic_compare_exchange_i16(xatomic_int16_t*  ptr, int16_t  expected, int16_t  desired) { return __sync_val_compare_and_swap((uint16_t*)ptr, expected, desired); }
static int32_t  xatomic_compare_exchange_i32(xatomic_i32*  ptr, int32_t  expected, int32_t  desired) { return __sync_val_compare_and_swap((uint32_t*)ptr, expected, desired); }
static int64_t  xatomic_compare_exchange_i64(xatomic_i64*  ptr, int64_t  expected, int64_t  desired) { return __sync_val_compare_and_swap((uint64_t*)ptr, expected, desired); }

#endif

// Ordered variants: explicit memory order instead of the implicit seq_cst used above.
// On GCC/Clang these map straight onto __atomic_*. On MSVC loads/stores/fences honour the order
// (the branches fold away when order is a constant), while CAS is always an interlocked op (full fence).
#if defined(_MSC_VER) && !defined(__clang__)

// Loads: relaxed = plain load. acquire/consume/seq_cst = plain load followed by a barrier
static inline uint8_t  xatomic_load_u8_ordered (const xatomic_u8*  ptr, enum xt_memory_order order) { uint8_t  v = (uint8_t) __iso_volatile_load8 ((const volatile __int8*) ptr); if (order != xt_memory_order_relaxed) XT_BARRIER(); return v; }
static inline uint16_t xatomic_load_u16_ordered(const xatomic_u16* ptr, enum xt_memory_order order) { uint16_t v = (uint16_t)__iso_volatile_load16((const volatile __int16*)ptr); if (order != xt_memory_order_relaxed) XT_BARRIER(); return v; }
static inline uint32_t xatomic_load_u32_ordered(const xatomic_u32* ptr, enum xt_memory_order order) { uint32_t v = (uint32_t)__iso_volatile_load32((const volatile __int32*)ptr); if (order != xt_memory_order_relaxed) XT_BARRIER(); return v; }
static inline uint64_t xatomic_load_u64_ordered(const xatomic_u64* ptr, enum xt_memory_order order) { uint64_t v = (uint64_t)__iso_volatile_load64((const volatile __int64*)ptr); if (order != xt_memory_order_relaxed) XT_BARRIER(); return v; }
static inline int8_t   xatomic_load_i8_ordered (const xatomic_int8_t*   ptr, enum xt_memory_order order) { int8_t  v = (int8_t) __iso_volatile_load8 ((const volatile __int8*) ptr); if (order != xt_memory_order_relaxed) XT_BARRIER(); return v; }
static inline int16_t  xatomic_load_i16_ordered(const xatomic_int16_t*  ptr, enum xt_memory_order order) { int16_t v = (int16_t)__iso_volatile_load16((const volatile __int16*)ptr); if (order != xt_memory_order_relaxed) XT_BARRIER(); return v; }
static inline int32_t  xatomic_load_i32_ordered(const xatomic_i32*  ptr, enum xt_memory_order order) { int32_t v = (int32_t)__iso_volatile_load32((const volatile __int32*)ptr); if (order != xt_memory_order_relaxed) XT_BARRIER(); return v; }
static inline int64_t  xatomic_load_i64_ordered(const xatomic_i64*  ptr, enum xt_memory_order order) { int64_t v = (int64_t)__iso_volatile_load64((const volatile __int64*)ptr); if (order != xt_memory_order_relaxed) XT_BARRIER(); return v; }

// Stores: seq_cst = interlocked exchange. relaxed = plain store. release/acq_rel = barrier followed by a plain store
static inline void xatomic_store_u8_ordered (xatomic_u8*  ptr, uint8_t  v, enum xt_memory_order order) { if (order == xt_memory_order_seq_cst) { xatomic_store_u8 (ptr, v); return; } if (order != xt_memory_order_relaxed) XT_BARRIER(); __iso_volatile_store8 ((volatile __int8*) ptr, (__int8) v); }
static inline void xatomic_store_u16_ordered(xatomic_u16* ptr, uint16_t v, enum xt_memory_order order) { if (order == xt_memory_order_seq_cst) { xatomic_store_u16(ptr, v); return; } if (order != xt_memory_order_relaxed) XT_BARRIER(); __iso_volatile_store16((volatile __int16*)ptr, (__int16)v); }
static inline void xatomic_store_u32_ordered(xatomic_u32* ptr, uint32_t v, enum xt_memory_order order) { if (order == xt_memory_order_seq_cst) { xatomic_store_u32(ptr, v); return; } if (order != xt_memory_order_relaxed) XT_BARRIER(); __iso_volatile_store32((volatile __int32*)ptr, (__int32)v); }
static inline void xatomic_store_u64_ordered(xatomic_u64* ptr, uint64_t v, enum xt_memory_order order) { if (order == xt_memory_order_seq_cst) { xatomic_store_u64(ptr, v); return; } if (order != xt_memory_order_relaxed) XT_BARRIER(); __iso_volatile_store64((volatile __int64*)ptr, (__int64)v); }
static inline void xatomic_store_i8_ordered (xatomic_int8_t*   ptr, int8_t   v, enum xt_memory_order order) { if (order == xt_memory_order_seq_cst) { xatomic_store_i8 (ptr, v); return; } if (order != xt_memory_order_relaxed) XT_BARRIER(); __iso_volatile_store8 ((volatile __int8*) ptr, (__int8) v); }
static inline void xatomic_store_i16_ordered(xatomic_int16_t*  ptr, int16_t  v, enum xt_memory_order order) { if (order == xt_memory_order_seq_cst) { xatomic_store_i16(ptr, v); return; } if (order != xt_memory_order_relaxed) XT_BARRIER(); __iso_volatile_store16((volatile __int16*)ptr, (__int16)v); }
static inline void xatomic_store_i32_ordered(xatomic_i32*  ptr, int32_t  v, enum xt_memory_order order) { if (order == xt_memory_order_seq_cst) { xatomic_store_i32(ptr, v); return; } if (order != xt_memory_order_relaxed) XT_BARRIER(); __iso_volatile_store32((volatile __int32*)ptr, (__int32)v); }
static inline void xatomic_store_i64_ordered(xatomic_i64*  ptr, int64_t  v, enum xt_memory_order order) { if (order == xt_memory_order_seq_cst) { xatomic_store_i64(ptr, v); return; } if (order != xt_memory_order_relaxed) XT_BARRIER(); __iso_volatile_store64((volatile __int64*)ptr, (__int64)v); }

// CAS: every lock-prefixed op on x64 is a full fence, so there is nothing cheaper to pick.
// The unsuffixed _Interlocked* are also full fences on ARM64. If ARM64 becomes a target, switch on order
// to _InterlockedCompareExchange*_nf / _acq / _rel for cheaper relaxed/acquire/release CAS

static inline uint8_t  xatomic_compare_exchange_u8_ordered (xatomic_u8*  ptr, uint8_t  expected, uint8_t  desired, enum xt_memory_order order) { (void)order; return xatomic_compare_exchange_u8 (ptr, expected, desired); }
static inline uint16_t xatomic_compare_exchange_u16_ordered(xatomic_u16* ptr, uint16_t expected, uint16_t desired, enum xt_memory_order order) { (void)order; return xatomic_compare_exchange_u16(ptr, expected, desired); }
static inline uint32_t xatomic_compare_exchange_u32_ordered(xatomic_u32* ptr, uint32_t expected, uint32_t desired, enum xt_memory_order order) { (void)order; return xatomic_compare_exchange_u32(ptr, expected, desired); }
static inline uint64_t xatomic_compare_exchange_u64_ordered(xatomic_u64* ptr, uint64_t expected, uint64_t desired, enum xt_memory_order order) { (void)order; return xatomic_compare_exchange_u64(ptr, expected, desired); }
static inline int8_t   xatomic_compare_exchange_i8_ordered (xatomic_int8_t*   ptr, int8_t   expected, int8_t   desired, enum xt_memory_order order) { (void)order; return xatomic_compare_exchange_i8 (ptr, expected, desired); }
static inline int16_t  xatomic_compare_exchange_i16_ordered(xatomic_int16_t*  ptr, int16_t  expected, int16_t  desired, enum xt_memory_order order) { (void)order; return xatomic_compare_exchange_i16(ptr, expected, desired); }
static inline int32_t  xatomic_compare_exchange_i32_ordered(xatomic_i32*  ptr, int32_t  expected, int32_t  desired, enum xt_memory_order order) { (void)order; return xatomic_compare_exchange_i32(ptr, expected, desired); }
static inline int64_t  xatomic_compare_exchange_i64_ordered(xatomic_i64*  ptr, int64_t  expected, int64_t  desired, enum xt_memory_order order) { (void)order; return xatomic_compare_exchange_i64(ptr, expected, desired); }

// seq_cst needs a StoreLoad fence: an interlocked op on a local is one (and is usually cheaper than mfence).
// acquire/release/acq_rel only need XT_BARRIER (compiler-only on x64)
static inline void xatomic_thread_fence(enum xt_memory_order order)
{
    if (order == xt_memory_order_seq_cst)
    {
        xatomic_u32 barrier = 0;
        xatomic_exchange_u32(&barrier, 0);
    }
    else if (order != xt_memory_order_relaxed)
    {
        XT_BARRIER();
    }
}

#else

static inline uint8_t  xatomic_load_u8_ordered (const xatomic_u8*  ptr, enum xt_memory_order order) { return __atomic_load_n(ptr, order); }
static inline uint16_t xatomic_load_u16_ordered(const xatomic_u16* ptr, enum xt_memory_order order) { return __atomic_load_n(ptr, order); }
static inline uint32_t xatomic_load_u32_ordered(const xatomic_u32* ptr, enum xt_memory_order order) { return __atomic_load_n(ptr, order); }
static inline uint64_t xatomic_load_u64_ordered(const xatomic_u64* ptr, enum xt_memory_order order) { return __atomic_load_n(ptr, order); }
static inline int8_t   xatomic_load_i8_ordered (const xatomic_int8_t*   ptr, enum xt_memory_order order) { return (int8_t) __atomic_load_n((const xatomic_u8*)ptr,  order); }
static inline int16_t  xatomic_load_i16_ordered(const xatomic_int16_t*  ptr, enum xt_memory_order order) { return (int16_t)__atomic_load_n((const xatomic_u16*)ptr, order); }
static inline int32_t  xatomic_load_i32_ordered(const xatomic_i32*  ptr, enum xt_memory_order order) { return (int32_t)__atomic_load_n((const xatomic_u32*)ptr, order); }
static inline int64_t  xatomic_load_i64_ordered(const xatomic_i64*  ptr, enum xt_memory_order order) { return (int64_t)__atomic_load_n((const xatomic_u64*)ptr, order); }

static inline void xatomic_store_u8_ordered (xatomic_u8*  ptr, uint8_t  v, enum xt_memory_order order) { __atomic_store_n(ptr, v, order); }
static inline void xatomic_store_u16_ordered(xatomic_u16* ptr, uint16_t v, enum xt_memory_order order) { __atomic_store_n(ptr, v, order); }
static inline void xatomic_store_u32_ordered(xatomic_u32* ptr, uint32_t v, enum xt_memory_order order) { __atomic_store_n(ptr, v, order); }
static inline void xatomic_store_u64_ordered(xatomic_u64* ptr, uint64_t v, enum xt_memory_order order) { __atomic_store_n(ptr, v, order); }
static inline void xatomic_store_i8_ordered (xatomic_int8_t*   ptr, int8_t   v, enum xt_memory_order order) { __atomic_store_n((xatomic_u8*)ptr,  (uint8_t)v,  order); }
static inline void xatomic_store_i16_ordered(xatomic_int16_t*  ptr, int16_t  v, enum xt_memory_order order) { __atomic_store_n((xatomic_u16*)ptr, (uint16_t)v, order); }
static inline void xatomic_store_i32_ordered(xatomic_i32*  ptr, int32_t  v, enum xt_memory_order order) { __atomic_store_n((xatomic_u32*)ptr, (uint32_t)v, order); }
static inline void xatomic_store_i64_ordered(xatomic_i64*  ptr, int64_t  v, enum xt_memory_order order) { __atomic_store_n((xatomic_u64*)ptr, (uint64_t)v, order); }

// The failure path of a CAS is only a load, so it can't be release (same mapping as std::atomic)
static inline int xt_cas_failure_order(enum xt_memory_order order)
{
    int fail = order;
    if (order == xt_memory_order_release)
        fail = xt_memory_order_relaxed;
    if (order == xt_memory_order_acq_rel)
        fail = xt_memory_order_acquire;
    return fail;
}

static inline uint8_t xatomic_compare_exchange_u8_ordered(xatomic_u8* ptr, uint8_t expected, uint8_t desired, enum xt_memory_order order)
{
    int fail = xt_cas_failure_order(order);
    __atomic_compare_exchange_n(ptr, &expected, desired, 0, order, fail);
    return expected;
}
static inline uint16_t xatomic_compare_exchange_u16_ordered(xatomic_u16* ptr, uint16_t expected, uint16_t desired, enum xt_memory_order order)
{
    int fail = xt_cas_failure_order(order);
    __atomic_compare_exchange_n(ptr, &expected, desired, 0, order, fail);
    return expected;
}
static inline uint32_t xatomic_compare_exchange_u32_ordered(xatomic_u32* ptr, uint32_t expected, uint32_t desired, enum xt_memory_order order)
{
    int fail = xt_cas_failure_order(order);
    __atomic_compare_exchange_n(ptr, &expected, desired, 0, order, fail);
    return expected;
}
static inline uint64_t xatomic_compare_exchange_u64_ordered(xatomic_u64* ptr, uint64_t expected, uint64_t desired, enum xt_memory_order order)
{
    int fail = xt_cas_failure_order(order);
    __atomic_compare_exchange_n(ptr, &expected, desired, 0, order, fail);
    return expected;
}
static inline int8_t xatomic_compare_exchange_i8_ordered(xatomic_int8_t* ptr, int8_t expected, int8_t desired, enum xt_memory_order order)
{
    int fail = xt_cas_failure_order(order);
    uint8_t exp = (uint8_t)expected;
    __atomic_compare_exchange_n((xatomic_u8*)ptr, &exp, (uint8_t)desired, 0, order, fail);
    return (int8_t)exp;
}
static inline int16_t xatomic_compare_exchange_i16_ordered(xatomic_int16_t* ptr, int16_t expected, int16_t desired, enum xt_memory_order order)
{
    int fail = xt_cas_failure_order(order);
    uint16_t exp = (uint16_t)expected;
    __atomic_compare_exchange_n((xatomic_u16*)ptr, &exp, (uint16_t)desired, 0, order, fail);
    return (int16_t)exp;
}
static inline int32_t xatomic_compare_exchange_i32_ordered(xatomic_i32* ptr, int32_t expected, int32_t desired, enum xt_memory_order order)
{
    int fail = xt_cas_failure_order(order);
    uint32_t exp = (uint32_t)expected;
    __atomic_compare_exchange_n((xatomic_u32*)ptr, &exp, (uint32_t)desired, 0, order, fail);
    return (int32_t)exp;
}
static inline int64_t xatomic_compare_exchange_i64_ordered(xatomic_i64* ptr, int64_t expected, int64_t desired, enum xt_memory_order order)
{
    int fail = xt_cas_failure_order(order);
    uint64_t exp = (uint64_t)expected;
    __atomic_compare_exchange_n((xatomic_u64*)ptr, &exp, (uint64_t)desired, 0, order, fail);
    return (int64_t)exp;
}

static inline void xatomic_thread_fence(enum xt_memory_order order) { __atomic_thread_fence(order); }

#endif

// Pointers are 4 bytes on wasm32, so a 64 bit op would also touch the 4 bytes after the pointer
// compare_exchange_strong_ptr returns true if it swapped
#if UINTPTR_MAX == UINT64_MAX
static inline void* xatomic_load_ptr(const xatomic_ptr* ptr) { return (void*)xatomic_load_u64((const xatomic_u64*)ptr); }
static inline void  xatomic_store_ptr(xatomic_ptr* ptr, void* v) { xatomic_store_u64((xatomic_u64*)ptr, (uint64_t)v); }
static inline void* xatomic_exchange_ptr(xatomic_ptr* ptr, void* v) { return (void*)xatomic_exchange_u64((xatomic_u64*)ptr, (uint64_t)v); }
static inline bool  xatomic_compare_exchange_strong_ptr(xatomic_ptr* ptr, void* expected, void* desired) { return xatomic_compare_exchange_u64((xatomic_u64*)ptr, (uint64_t)expected, (uint64_t)desired) == (uint64_t)expected; }
#else
static inline void* xatomic_load_ptr(const xatomic_ptr* ptr) { return (void*)(uintptr_t)xatomic_load_u32((const xatomic_u32*)ptr); }
static inline void  xatomic_store_ptr(xatomic_ptr* ptr, void* v) { xatomic_store_u32((xatomic_u32*)ptr, (uint32_t)(uintptr_t)v); }
static inline void* xatomic_exchange_ptr(xatomic_ptr* ptr, void* v) { return (void*)(uintptr_t)xatomic_exchange_u32((xatomic_u32*)ptr, (uint32_t)(uintptr_t)v); }
static inline bool  xatomic_compare_exchange_strong_ptr(xatomic_ptr* ptr, void* expected, void* desired) { return xatomic_compare_exchange_u32((xatomic_u32*)ptr, (uint32_t)(uintptr_t)expected, (uint32_t)(uintptr_t)desired) == (uint32_t)(uintptr_t)expected; }
#endif

union xt_uif {unsigned u; int i; float f;};

static inline float xatomic_load_f32(const xatomic_float* ptr)
{
    union xt_uif v  = {.u = xatomic_load_u32(ptr)};
    return v.f;
}

static inline void xatomic_store_f32(xatomic_float* ptr, float v)
{
    union xt_uif a = {.f = v};
    xatomic_store_u32(ptr, a.u);
}

static inline float xatomic_exchange_f32(xatomic_float* ptr, float v)
{
    union xt_uif a = {.f = v};
    union xt_uif b = {.u = xatomic_exchange_u32(ptr, a.u)};
    return b.f;
}


void xt_timer_init(xt_timer_t* timer);
void xt_timer_term(xt_timer_t* timer);
void xt_timer_wait(xt_timer_t* timer, uint64_t nanoseconds);

void  xt_queue_init(xt_queue_t* queue, int size, void** values, int count);
void  xt_queue_term(xt_queue_t* queue);
int   xt_queue_produce(xt_queue_t* queue, void* value, int timeout_ms);
void* xt_queue_consume(xt_queue_t* queue, int timeout_ms);
int   xt_queue_count(xt_queue_t* queue);

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
    xatomic_i32 count;
    xatomic_i32 head;
    xatomic_i32 tail;
    int               size;
    void**            values;
#ifndef NDEBUG
    xatomic_i32 id_produce_is_set;
    xatomic_i32 id_consume_is_set;
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

#include <assert.h>
#define XTHREAD_ASSERT(expression, message) assert((expression) && (message))

#if defined(_WIN32)

#pragma comment(lib, "winmm.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _WINSOCKAPI_
#include <windows.h>

#include <timeapi.h>

xt_thread_ptr_t xt_thread_current(void) { return (void*)(uintptr_t)GetCurrentThreadId(); }
xt_thread_ptr_t xt_thread_create(int (*thread_proc)(void*), void* user_data, int stack_size)
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

void xt_thread_yield(void) { SwitchToThread(); }
void xt_thread_exit(int return_code) { ExitThread((DWORD)return_code); }
void xt_thread_destroy(xt_thread_ptr_t thread)
{
    WaitForSingleObject((HANDLE)thread, INFINITE);
    CloseHandle((HANDLE)thread);
}
int xt_thread_join(xt_thread_ptr_t thread)
{
    WaitForSingleObject((HANDLE)thread, INFINITE);
    DWORD retval;
    GetExitCodeThread((HANDLE)thread, &retval);
    return (int)retval;
}
int  xt_thread_detach(xt_thread_ptr_t thread) { return CloseHandle((HANDLE)thread) != 0; }
void xt_thread_set_high_priority(void) { SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST); }

_Static_assert(sizeof(xt_mutex_t) >= sizeof(CRITICAL_SECTION), "Mutex too smol...");
void xt_mutex_init(xt_mutex_t* mutex) { InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)mutex, 32); }
void xt_mutex_term(xt_mutex_t* mutex) { DeleteCriticalSection((CRITICAL_SECTION*)mutex); }
void xt_mutex_lock(xt_mutex_t* mutex) { EnterCriticalSection((CRITICAL_SECTION*)mutex); }
void xt_mutex_unlock(xt_mutex_t* mutex) { LeaveCriticalSection((CRITICAL_SECTION*)mutex); }

#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

xt_thread_ptr_t xt_thread_current(void) { return (void*)pthread_self(); }

void xt_thread_yield(void) { sched_yield(); }

void xt_thread_exit(int return_code) { pthread_exit((void*)(uintptr_t)return_code); }

xt_thread_ptr_t xt_thread_create(int (*thread_proc)(void*), void* user_data, int stack_size)
{
    pthread_t thread;
    if (0 != pthread_create(&thread, NULL, (void* (*)(void*))thread_proc, user_data))
        return NULL;

    return (xt_thread_ptr_t)thread;
}
void xt_thread_destroy(xt_thread_ptr_t thread) { pthread_join((pthread_t)thread, NULL); }
int  xt_thread_join(xt_thread_ptr_t thread)
{
    void* retval;
    pthread_join((pthread_t)thread, &retval);
    return (int)(uintptr_t)retval;
}
int  xt_thread_detach(xt_thread_ptr_t thread) { return pthread_detach((pthread_t)thread) == 0; }
void xt_thread_set_high_priority(void)
{
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_min(SCHED_RR);
    pthread_setschedparam(pthread_self(), SCHED_RR, &sp);
}

_Static_assert(sizeof(xt_mutex_t) >= sizeof(pthread_mutex_t), "Mutex too smol...");
void xt_mutex_init(xt_mutex_t* mutex) { pthread_mutex_init((pthread_mutex_t*)mutex, NULL); }
void xt_mutex_term(xt_mutex_t* mutex) { pthread_mutex_destroy((pthread_mutex_t*)mutex); }
void xt_mutex_lock(xt_mutex_t* mutex) { pthread_mutex_lock((pthread_mutex_t*)mutex); }
void xt_mutex_unlock(xt_mutex_t* mutex) { pthread_mutex_unlock((pthread_mutex_t*)mutex); }

#else
#error Unknown platform.
#endif

struct xt_internal_signal_t
{
#if defined(_WIN32)
    CRITICAL_SECTION   mutex;
    CONDITION_VARIABLE condition;
    int                value;
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    pthread_mutex_t mutex;
    pthread_cond_t  condition;
    int             value;
#endif
};
_Static_assert(sizeof(xt_signal_t) >= sizeof(struct xt_internal_signal_t), "too smol");

void xt_signal_init(xt_signal_t* signal)
{
    struct xt_internal_signal_t* internal = (struct xt_internal_signal_t*)signal;

#if defined(_WIN32)
    InitializeCriticalSectionAndSpinCount(&internal->mutex, 32);
    InitializeConditionVariable(&internal->condition);
    internal->value = 0;
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    pthread_mutex_init(&internal->mutex, NULL);
    pthread_cond_init(&internal->condition, NULL);
    internal->value = 0;
#endif
}

void xt_signal_term(xt_signal_t* signal)
{
    struct xt_internal_signal_t* internal = (struct xt_internal_signal_t*)signal;

#if defined(_WIN32)
    DeleteCriticalSection(&internal->mutex);
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    pthread_mutex_destroy(&internal->mutex);
    pthread_cond_destroy(&internal->condition);
#endif
}

void xt_signal_raise(xt_signal_t* signal)
{
    struct xt_internal_signal_t* internal = (struct xt_internal_signal_t*)signal;

#if defined(_WIN32)
    EnterCriticalSection(&internal->mutex);
    internal->value = 1;
    LeaveCriticalSection(&internal->mutex);
    WakeConditionVariable(&internal->condition);
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    pthread_mutex_lock(&internal->mutex);
    internal->value = 1;
    pthread_mutex_unlock(&internal->mutex);
    pthread_cond_signal(&internal->condition);
#endif
}

int xt_signal_wait(xt_signal_t* signal, int timeout_ms)
{
    struct xt_internal_signal_t* internal = (struct xt_internal_signal_t*)signal;

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

#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)

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

void xt_timer_init(xt_timer_t* timer)
{
#if defined(_WIN32)
    TIMECAPS tc;
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR)
        timeBeginPeriod(tc.wPeriodMin);

    *(HANDLE*)timer = CreateWaitableTimer(NULL, TRUE, NULL);
#endif // No init on MacOS/Linux/Android
}

void xt_timer_term(xt_timer_t* timer)
{
#if defined(_WIN32)
    CloseHandle(*(HANDLE*)timer);
    TIMECAPS tc;
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR)
        timeEndPeriod(tc.wPeriodMin);
#endif // No deinit on MacOS/Linux/Android
}

void xt_timer_wait(xt_timer_t* timer, uint64_t nanoseconds)
{
#if defined(_WIN32)
    LARGE_INTEGER due_time;
    due_time.QuadPart = -(LONGLONG)(nanoseconds / 100);
    BOOL b            = SetWaitableTimer(*(HANDLE*)timer, &due_time, 0, 0, 0, FALSE);
    (void)b;
    WaitForSingleObject(*(HANDLE*)timer, INFINITE);
#elif defined(__linux__) || defined(__APPLE__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
    struct timespec rem;
    struct timespec req;
    req.tv_sec  = nanoseconds / 1000000000ULL;
    req.tv_nsec = nanoseconds - req.tv_sec * 1000000000ULL;
    while (nanosleep(&req, &rem))
        req = rem;
#endif
}

void xt_queue_init(xt_queue_t* queue, int size, void** values, int count)
{
    queue->values = values;
    xt_signal_init(&queue->data_ready);
    xt_signal_init(&queue->space_open);
    xatomic_store_i32(&queue->head, 0);
    xatomic_store_i32(&queue->tail, count > size ? size : count);
    xatomic_store_i32(&queue->count, count > size ? size : count);
    queue->size = size;
#ifndef NDEBUG
    xatomic_store_i32(&queue->id_produce_is_set, 0);
    xatomic_store_i32(&queue->id_consume_is_set, 0);
#endif
}

void xt_queue_term(xt_queue_t* queue)
{
    xt_signal_term(&queue->space_open);
    xt_signal_term(&queue->data_ready);
}

int xt_queue_produce(xt_queue_t* queue, void* value, int timeout_ms)
{
#ifndef NDEBUG
    if (xatomic_compare_exchange_i32(&queue->id_produce_is_set, 0, 1) == 0)
        queue->id_produce = xt_thread_current();
    XTHREAD_ASSERT(
        xt_thread_current() == queue->id_produce,
        "thread_queue_produce called from multiple threads");
#endif
    while (xatomic_load_i32(&queue->count) ==
           queue->size) // TODO: fix signal so that this can be an "if" instead of "while"
    {
        if (timeout_ms == 0)
            return 0;
        if (xt_signal_wait(
                &queue->space_open,
                timeout_ms == XTHREAD_QUEUE_WAIT_INFINITE ? XTHREAD_SIGNAL_WAIT_INFINITE : timeout_ms) == 0)
            return 0;
    }
    int tail                          = xatomic_fetch_add_i32(&queue->tail, 1);
    queue->values[tail % queue->size] = value;
    if (xatomic_fetch_add_i32(&queue->count, 1) == 0)
        xt_signal_raise(&queue->data_ready);
    return 1;
}

void* xt_queue_consume(xt_queue_t* queue, int timeout_ms)
{
#ifndef NDEBUG
    if (xatomic_compare_exchange_i32(&queue->id_consume_is_set, 0, 1) == 0)
        queue->id_consume = xt_thread_current();
    XTHREAD_ASSERT(
        xt_thread_current() == queue->id_consume,
        "thread_queue_consume called from multiple threads");
#endif
    while (xatomic_load_i32(&queue->count) ==
           0) // TODO: fix signal so that this can be an "if" instead of "while"
    {
        if (timeout_ms == 0)
            return NULL;
        if (xt_signal_wait(
                &queue->data_ready,
                timeout_ms == XTHREAD_QUEUE_WAIT_INFINITE ? XTHREAD_SIGNAL_WAIT_INFINITE : timeout_ms) == 0)
            return NULL;
    }
    int   head   = xatomic_fetch_add_i32(&queue->head, 1);
    void* retval = queue->values[head % queue->size];
    if (xatomic_fetch_sub_i32(&queue->count, 1) == queue->size)
        xt_signal_raise(&queue->space_open);
    return retval;
}

int xt_queue_count(xt_queue_t* queue) { return xatomic_load_i32(&queue->count); }

#endif /* XHL_THREAD_IMPL */
// clang-format on