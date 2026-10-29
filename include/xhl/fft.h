/*
 * Released into the public domain by Tré Dudman - 2024
 * For licensing and more info see https://github.com/Tremus/xhl
 *
 * TLDR;
 * - FFT library
 * - tuned for audio workloads  where N = some power of 2 (32-8192)
 * - simd backends for AVX, AVX2 and NEON
 * - Single precision (float, f32) real transforms only.
 * - 30-50% faster SIMD than FFTW (accounting for FFTW built using SIMD)
 * - Scroll down to see a demo program
 *
 * DISCLAIMER:
 * The simd code is the result of LLMs running "agentic loops", constantly scoring themselves in terms of runtime speed,
 * logging the results, trying new simd approaches, etc in a loop. It's a brute force way of automating optimisation for
 * a well understood algorithm. It started from a basic naive implementation.
 *
 * Due to my personal programmer taste, I chose to sacrifice ~10ns in performance in some areas, to reduce overall code
 * size and drastically reduce runaway binary size (>50kb per backend) where possible. This makes performance numbers
 * look a little less busted on certain workloads. Some earlier iterations had ~78% speedup over FFTW for forward
 * transforms, while only a 30% speedup in their inverse. Together they kind of average out, with the current
 * implementation being only 3-5% slower than my best ever scores averaged out. The best scores in AVX2 typically needed
 * a lot of inlined, well pipelined code, but also created runaway binary sizes.
 *
 * USAGE:
 * #define XHL_FFT_IMPL within a .c file, then include this library afterwards
 * The gist of the API is this:
 *  - xfft_init_xxx(mem, n)                     Creates an FFT plan,
 *                                              Do this at startup
 *                                              Don't share this between threads
 *                                              Does not allocate, you must reserve the memory yourself
 *  - xfft_mem_size_xxx(n)                      Returns required allocation size for your plan.
 *                                              n must be a power of 2 eg. 32
 *  - xfft_forward_xxx(plan, in_buf, out_buf)   Processes an FFT using your plan. In place processing is supported
 *  - xfft_inverse_xxx(plan, in_buf, out_buf)   Processes an IFFT using your plan. In place processing is supported
 *
 * Duplicate APIs are provided for each backend eg.
 * xfft_forward_scalar()
 * xfft_forward_neon()
 * xfft_forward_avx()
 * xfft_forward_avx2()
 * xfft_forward_auto() <- use this API when you're getting started. It selects the best available API at runtime
 *
 * !!!IMPORTANT!!! - All of your memory used by this library are required to be aligned to XFFT_ALIGN (=64) bytes.
 *                   This is a requirement of AVX/AVX2
 *
 * USAGE ADVANCED:
 * Disable backends using  XFFT_DISABLE_SCALAR / _NEON / _AVX / _AVX2
 *
 * Use "#define XFFT_DEF static" before including this header to make everything static/extern/whatever floats your boat
 *
 * Backends that are not supported compile to stubs.
 *  eg 1. The NEON backend depends on (__ARM_NEON && __aarch64__). It will be a stub when you compile on x86 Windows
 *  eg 2. The AVX2 backend depends on (__AVX2__ && __FMA__). It will be a stub unless you pass flags like -mavx2 -mfma
 *        to your compiler
 *
 * This design is mostly done for quick first draft setup/integration convenience.
 * In the real world, you may have a program which claims to support old SSE4.2 era CPUs, but you want to take advantage
 * of newer AVX2 if available. This library is designed to help you do this.
 * Here's how:
 *  1. Define XFFT_NO_STUBS. Only enabled backends get defined (no stubs, no _auto functions), so the impl can be
 *     included in several .c files without duplicate symbols
 *  2. Build the AVX2 backend alone in its own .c file with -mavx2 -mfma, disabling the others. eg. XFFT_DISABLE_SCALAR
 *     Repeat this for your other desired backends.
 *  3. Write a wrapper that checks the CPU once at runtime (cpuid/xgetbv) and calls the matching xfft_xxx_avx2 / _avx /
 *     _neon / _scalar function. The implementation of the xfft "_auto" API may help you with inspiration for how to
 *     dispatch this yourself at runtime
 *
 * PERFORMANCE:
 * CPUs tested:
 * - Intel Ultra 9 185H
 * - iMac 2017 (Kaby Lake)
 * - M1 Macbook Air
 * - Wasm code running on the above CPUs
 *
 * AVX & AVX2 score 50% faster than FFTW on modern machines, and 30% faster on older ones
 *
 * NEON PFFFT is already faster than NEON FFTW, and this lib is about 35% faster than PFFFT.
 *
 * The scalar backend is tuned to run well as wasm on a variety of systems, not as native optimised code. It's about 20%
 * slower than FFTW, and 20% faster than PFFFT
 *
 * BENCHMARKING:
 * Sorry my benchmarking suite is private for now. Might include a reproducable testing suite here later if pushed.
 *
 * "trust my numbers bro"
 *
 * I tried being fair with FFTW benchmarking, comparing AVX2+FMA FFTW with xfft_avx2, NEON FFTW with xfft_neon etc.
 * On supporting devices, I also pinned all tests to a single performance core set to high priority, with thermal
 * throttling disabled, benchmarks were interleaved, and buffers were reused to keep cache hot.
 * Cycles were counted on Windows instead of time.
 *
 * On macOS it's tricky, but I hear the `kperf` framework has solutions for this, although they require `sudo`
 * permissions. I didn't go the extra mile here, too much friction. I used `mach_absolute_time()`, which costs ~40ns
 *
 * Wasm can't count cycles AFAIK, time is probably better here, and it's what I used in the scalar tests.
 *
 * One of my synthetic benchmarks tested how different iterations of these algorithms performed while thrashing the CPU
 * cache on other cores. I expected iterations "suffering" from binary bloat (eg. >100kb of instructions per transform,
 * far exceeding my L1 icache) to score lower when the CPU is under load, but they did not! This was an interesting
 * lesson for me. It means binary size and code length of this lib is purely cosmetic.
 *
 * TODO:
 * - Check GCC and MSVC compile usable code
 * - Move all my iteration logs here for research purposes, and further testing
 * - Move benchmarking suite here
 * - Make a nice performance benchmark chart to show off?
 * - AVX512 backend. I don't own any supporting CPUs to test this on. This feature is dependant on me owning one.
 * - SSE4.2 backend
 * - complex transforms
 * - double precision
 * - support non power of 2 sizes for n, eg. in the same way PFFFT supports any size that's a multiple of 32, we could
 *   do this for the scalar, NEON and SSE4.2 backends, and multiples of 64 for AVX/AVX2 backends NOTE: I have no need
 *   for doing any of the above, and I'm a busy guy with more important things to do
 *
 * Special thanks:
 * - PFFFT. It has the fastest NEON algo I tried, and it was an instrumental baseline in our benchmarking
 * - FFTW contributors. In every other backend, they were our baseline to beat.
 *   They still beat us in scalar wasm performance.
 * - PhastFT (https://github.com/smu160/PhastFT)
 *   Has impressive performance on higher workloads, particularly for complex transforms. We tried borrowing their ideas
 *   but they proved to be slower in our benchmarks for our workloads (smaller real transforms, n=256...8192)
 *   More testing is still needed for our NEON backend at higher workloads (>=8192), which is where ours starts to slow
 *   down. PhastFT may be superior here
 */

// Demo program / usage:
#if 0
// Build:
// clang -O2 -mavx2 -mfma demo.c
// clang-cl /O2 /arch:AVX2 demo.c
#define XHL_FFT_IMPL
#include <xhl/fft.h>

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define N                 1024

// Round a pointer up to the next XFFT_ALIGN boundary
#define XFFT_ALIGN_PTR(p) (void*)(((uintptr_t)(p) + XFFT_ALIGN - 1) & ~(uintptr_t)(XFFT_ALIGN - 1))

int main(void)
{
    // xfft never allocates. You own the plan memory and buffers, which must be XFFT_ALIGN byte aligned.
    // malloc doesn't guarantee that, so overallocate by XFFT_ALIGN and round the pointer up. Free the raw pointer.
    void*  plan_raw = malloc(xfft_mem_size_auto(N) + XFFT_ALIGN);
    void*  buf_raw  = malloc(N * sizeof(float) + XFFT_ALIGN);
    float* buf      = XFFT_ALIGN_PTR(buf_raw);

    // Plan once per size, outside your audio callback. Returns NULL if N isn't a supported power of 2
    void* plan = xfft_init_auto(XFFT_ALIGN_PTR(plan_raw), N);
    if (plan == NULL)
        return 1;

    // A unit sine on bin 8
    for (int i = 0; i < N; i++)
        buf[i] = sinf(2 * 3.14159265f * 8 * i / N);

    // Note that the in & out buffers are identical here.
    // We support in place processing, however partial overlapping buffers are UB
    // buf[0] = Re X[0], buf[1] = Re X[N/2], then Re X[k], Im X[k] interleaved for 1 <= k < N/2
    xfft_forward_auto(plan, buf, buf);
    printf("Bin 8 magnitude %.1f (expected %d)\n", hypotf(buf[16], buf[17]), N / 2);

    // Inverse, in place. Also unnormalised, so scale by 1/N to recover the input
    xfft_inverse_auto(plan, buf, buf);
    float expected_sample = sinf(2 * 3.14159265f * 8 * 3 / N);
    printf("Sample 3: %f (expected %f)\n", buf[3] / N, expected_sample);

    // Expected console output:
    // Bin 8 magnitude 512.0 (expected 512)
    // Sample 3: 0.146730 (expected 0.146730)

    free(plan_raw);
    free(buf_raw);
    return 0;
}
#endif

#ifndef XHL_FFT_H
#define XHL_FFT_H

#include <stddef.h>

#define XFFT_MIN_N 32
#define XFFT_MAX_N 2097152
#define XFFT_ALIGN 64

// Linkage of every public function. Define as static before including to keep the whole library private to one .c file
#ifndef XFFT_DEF
#define XFFT_DEF
#endif

#ifdef __cplusplus
extern "C" {
#endif

// auto - Use this API to get the best available API at runtime
// Returns the required size of "void* mem", to be used by xfft_init_xxx
// You must allocate this yourself and align to "XFFT_ALIGN" (64)
// n=32|64|128...8192
XFFT_DEF size_t      xfft_mem_size_auto(int n);
XFFT_DEF void*       xfft_init_auto(void* mem, int n); // Returns "void* plan"
XFFT_DEF void        xfft_forward_auto(void* plan, const float* in, float* out);
XFFT_DEF void        xfft_inverse_auto(void* plan, const float* in, float* out);
XFFT_DEF const char* xfft_tell_auto(void); // Returns "avx2"|"avx"|"neon"|"scalar"|"none"

// scalar
XFFT_DEF size_t xfft_mem_size_scalar(int n);
XFFT_DEF void*  xfft_init_scalar(void* mem, int n);
XFFT_DEF void   xfft_forward_scalar(void* plan, const float* in, float* out);
XFFT_DEF void   xfft_inverse_scalar(void* plan, const float* in, float* out);
XFFT_DEF const char*
xfft_tell_scalar(void); // Returns "scalar"|"scalar-stub". Other backends copy the pattern. eg. "avx-stub"

// neon
XFFT_DEF size_t      xfft_mem_size_neon(int n);
XFFT_DEF void*       xfft_init_neon(void* mem, int n);
XFFT_DEF void        xfft_forward_neon(void* plan, const float* in, float* out);
XFFT_DEF void        xfft_inverse_neon(void* plan, const float* in, float* out);
XFFT_DEF const char* xfft_tell_neon(void);

// avx
XFFT_DEF size_t      xfft_mem_size_avx(int n);
XFFT_DEF void*       xfft_init_avx(void* mem, int n);
XFFT_DEF void        xfft_forward_avx(void* plan, const float* in, float* out);
XFFT_DEF void        xfft_inverse_avx(void* plan, const float* in, float* out);
XFFT_DEF const char* xfft_tell_avx(void);

// avx2
XFFT_DEF size_t      xfft_mem_size_avx2(int n);
XFFT_DEF void*       xfft_init_avx2(void* mem, int n);
XFFT_DEF void        xfft_forward_avx2(void* plan, const float* in, float* out);
XFFT_DEF void        xfft_inverse_avx2(void* plan, const float* in, float* out);
XFFT_DEF const char* xfft_tell_avx2(void);

#ifdef __cplusplus
}
#endif
#endif // XHL_FFT_H

#ifdef XHL_FFT_IMPL
#undef XHL_FFT_IMPL

#include <math.h>
#include <stddef.h>
#include <stdint.h>

// Backends compiled in. The rest become stubs
#if !defined(XFFT_DISABLE_SCALAR)
#define XFFT_SCALAR_ENABLED
#endif
#if defined(__ARM_NEON) && defined(__aarch64__) && !defined(XFFT_DISABLE_NEON)
#define XFFT_NEON_ENABLED
#endif
#if defined(__AVX__) && !defined(XFFT_DISABLE_AVX)
#define XFFT_AVX_ENABLED
#endif
#if defined(__AVX2__) && defined(__FMA__) && !defined(XFFT_DISABLE_AVX2)
#define XFFT_AVX2_ENABLED
#endif
// AVX and AVX2 share their plan and some helpers (see "x86 shared")
#if defined(XFFT_AVX_ENABLED) || defined(XFFT_AVX2_ENABLED)
#define XFFT_X86_ENABLED
#endif

#if defined(XFFT_NEON_ENABLED)
#include <arm_neon.h>
#endif
#if defined(XFFT_X86_ENABLED)
#include <immintrin.h>
#endif

// Pauses the debugger in debug builds, compiled out in release. Define your own XFFT_ASSERT to override
#ifndef XFFT_ASSERT
#ifdef NDEBUG
#define XFFT_ASSERT(...)
#else
// Prevents off by one bugs in debuggers
static int _xfft_debugbreak_helper = 0;
#ifdef _WIN32
#define XFFT_ASSERT(cond) (((cond) ? (void)0 : __debugbreak()), _xfft_debugbreak_helper += 0)
#else
#define XFFT_ASSERT(cond) (((cond) ? (void)0 : __builtin_debugtrap()), _xfft_debugbreak_helper += 0)
#endif
#endif // NDEBUG
#endif // XFFT_ASSERT

#define XFFT_IS_ALIGNED(ptr) (((uintptr_t)(ptr) & (XFFT_ALIGN - 1)) == 0)

/*~*~*~*~*
 ~ Shared ~
 *~*~*~*~*/

#if defined(XFFT_SCALAR_ENABLED) || defined(XFFT_NEON_ENABLED) || defined(XFFT_X86_ENABLED)

// Inlining is what makes the radix and "twiddled or not" compile-time in each pass. The scalar backend is plain C99
// elsewhere.
#if defined(__GNUC__) || defined(__clang__)
#define FI static inline __attribute__((always_inline))
#define NI static __attribute__((noinline))
#else
#define FI static inline
#define NI static
#endif
#define HALF_PI_F  1.57079632679489661923f
#define SQRT1_2_F  0.70710678118654752440f
#define MAX_STAGES 8

#define ALIGN_UP(x) (((x) + XFFT_ALIGN - 1) & ~(size_t)(XFFT_ALIGN - 1))

// cos and sin of 2*pi*e/K, K a power of two. Folded into the first octant so sinf/cosf see |angle| <= pi/4.
static void unit_cs(int e, int K, float* c, float* s)
{
    e       &= K - 1;
    int   q  = (4 * e) / K;   // Quadrant
    int   r  = 4 * e - q * K; // Angle within the quadrant is (pi/2) * r/K
    float c0, s0;
    if (2 * r <= K)
    {
        float a = HALF_PI_F * ((float)r / (float)K);
        c0      = cosf(a);
        s0      = sinf(a);
    }
    else
    {
        float a = HALF_PI_F * ((float)(K - r) / (float)K);
        c0      = sinf(a);
        s0      = cosf(a);
    }
    switch (q)
    {
    case 0:
        *c = c0, *s = s0;
        break;
    case 1:
        *c = -s0, *s = c0;
        break;
    case 2:
        *c = -c0, *s = -s0;
        break;
    default:
        *c = s0, *s = -c0;
        break;
    }
}

#endif // Any backend

#if defined(XFFT_SCALAR_ENABLED) || defined(XFFT_X86_ENABLED)
// Floats of twiddles for a stage: W_(radix*span)^(m*j) for j < span, m = 1..radix-1, (re, im) each.
// Shared by scalar, AVX and AVX2. NEON stores each twiddle twice (stage_tw_neon)
static int stage_tw(int radix, int span) { return span > 1 ? 2 * (radix - 1) * span : 0; }
#endif

/*~*~*~*~*
 ~ Scalar ~
 *~*~*~*~*/

// xfft: scalar real FFT (plain C99 float, no SIMD). The fallback for CPUs and targets without a SIMD path (wasm).
//
// A real FFT of size N is a complex FFT of size M = N/2 on z[m] = x[2m] + i*x[2m+1], plus an O(N) pass to
// split the even/odd spectra apart (forward) or merge them (inverse). This is xfft_neon with a lane width of one
// complex value instead of four: the "lane FFT" is then the whole complex FFT, and the cross-lane pass, its
// twiddles and the fused last stage all disappear.
//
//   1. Complex FFT of M: radix-8 / radix-4 DIF stages, the last one plain. The first stage reads the input
//      as is (it's already z[m], interleaved), the middle ones run in place in zbuf, digit-reversed. The last one
//      (span 1) undoes the digit reversal as it stores: group g's output m is bin kb[g] + m*M/R, so it writes Z
//      to 'out' in natural order.
//   2. Real split in place in 'out', bins k and M-k per iteration, into the packed ordered layout.
//
// The inverse merges the spectrum into Z (natural order, in zbuf), then runs the same forward stages on it:
// IDFT(Z) = swap(DFT(swap(Z))), where swap exchanges re and im. The merge stores Z swapped and the last stage
// stores its output swapped, so both swaps are free and there are no inverse butterflies at all.

#if defined(XFFT_SCALAR_ENABLED)

typedef struct
{
    int      n;
    float*   stw;  // Stage twiddles. Per stage with span s > 1, per j < s: w^j, w^2j, w^3j..., each (re, im)
    float*   rtw;  // Real split twiddles: W_N^k for k = 1..M/2, interleaved (re, im), then the same times 0.5 (fwd)
    int32_t* kb;   // Per group g of the last stage (M/R of them): float offset of bin kb[g], its output 0
    float*   zbuf; // M complex, interleaved: the complex FFT's workspace (digit-reversed between stages)
} Plan_scalar;

static int log2i(int x)
{
    int l = 0;
    while (x > 1)
        x >>= 1, l++;
    return l;
}

// Stages in DIF order: radix 8s, then 4s, ending in a plain (span 1) radix 8 where there is one, e.g. 2048:
// 8,4,4,8. A radix-16 last stage spills on wasm with 16 registers: 8,8,16 was ~7% slower at 2048 under V8.
static int stages(int M, int* radix, int* span)
{
    // Runs on every call: shifts, not divisions by a variable radix (a serial chain of slow divides)
    int l = log2i(M), n4 = (3 - l % 3) % 3, n8 = (l - 2 * n4) / 3, S = 0;
    for (int t = 0; t < n8 + n4; t++)
    {
        int b      = n8 && (t < n8 - 1 || t == n8 + n4 - 1) ? 3 : 2;
        radix[S]   = 1 << b;
        l         -= b;
        span[S++]  = 1 << l;
    }
    return S;
}

// Carves the plan's arrays out of 'mem' (when non-null). Returns the bytes needed.
static size_t layout_scalar(int n, unsigned char* mem)
{
    int M = n / 2;
    int radix[MAX_STAGES], span[MAX_STAGES], S = stages(M, radix, span), nstw = 0;
    for (int t = 0; t < S; t++)
        nstw += stage_tw(radix[t], span[t]);

    size_t sizes[5] = {
        sizeof(Plan_scalar),
        nstw * sizeof(float),
        2 * M * sizeof(float),
        M / radix[S - 1] * sizeof(int32_t),
        2 * M * sizeof(float),
    };
    size_t offs[5], off = 0;
    for (int i = 0; i < 5; i++)
    {
        offs[i]  = off;
        off     += ALIGN_UP(sizes[i]);
    }
    if (mem)
    {
        Plan_scalar* plan = (Plan_scalar*)mem;
        plan->n           = n;
        plan->stw         = (float*)(mem + offs[1]);
        plan->rtw         = (float*)(mem + offs[2]);
        plan->kb          = (int32_t*)(mem + offs[3]);
        plan->zbuf        = (float*)(mem + offs[4]);
    }
    return off;
}

XFFT_DEF size_t xfft_mem_size_scalar(int n) { return layout_scalar(n, 0); }

XFFT_DEF void* xfft_init_scalar(void* mem, int n)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(mem));
    if (n < XFFT_MIN_N || n > XFFT_MAX_N || (n & (n - 1)))
        return 0;
    layout_scalar(n, mem);
    Plan_scalar* plan = mem;
    int          M    = n / 2;
    int          radix[MAX_STAGES], span[MAX_STAGES], S = stages(M, radix, span);
    float        c, s;

    {
        // Stage twiddles: W_Rs^(m*j)
        float* tw = plan->stw;
        for (int t = 0; t < S; t++)
            for (int j = 0; j < span[t] && span[t] > 1; j++)
                for (int m = 1; m < radix[t]; m++)
                {
                    unit_cs(m * j, radix[t] * span[t], &c, &s);
                    tw[0]  = c;
                    tw[1]  = -s;
                    tw    += 2;
                }
    }
    {
        // Real split twiddles: W_N^k for the merge, 0.5 W_N^k for the split (folds its 0.5 into the twiddle)
        for (int k = 1; k <= M / 2; k++)
        {
            unit_cs(k, n, &c, &s);
            plan->rtw[2 * k - 2]     = c;
            plan->rtw[2 * k - 1]     = -s;
            plan->rtw[M + 2 * k - 2] = 0.5f * c;
            plan->rtw[M + 2 * k - 1] = -0.5f * s;
        }
    }
    {
        // Digit reversal: bin k = m0 + r0*(m1 + r1*(m2 + ...)) ends up at position sum m_t * span_t. The last
        // digit is the top one, with span 1, so bins k < M/R are output 0 of last-stage group pos / R
        for (int k = 0; k < M / radix[S - 1]; k++)
        {
            int kk = k, pos = 0;
            for (int t = 0; t < S; t++)
            {
                pos += (kk % radix[t]) * span[t];
                kk  /= radix[t];
            }
            plan->kb[pos / radix[S - 1]] = 2 * k;
        }
    }
    return plan;
}

typedef struct
{
    float re, im;
} cpx;

FI cpx  c_ld(const float* p) { return (cpx){p[0], p[1]}; }
FI void c_st(float* p, cpx a)
{
    p[0] = a.re;
    p[1] = a.im;
}
FI cpx c_add(cpx a, cpx b) { return (cpx){a.re + b.re, a.im + b.im}; }
FI cpx c_sub(cpx a, cpx b) { return (cpx){a.re - b.re, a.im - b.im}; }
// a + i*b, a - i*b
FI cpx c_addi_scalar(cpx a, cpx b) { return (cpx){a.re - b.im, a.im + b.re}; }
FI cpx c_subi_scalar(cpx a, cpx b) { return (cpx){a.re + b.im, a.im - b.re}; }
// a * w, a * conj(w)
FI cpx c_mul_scalar(cpx a, cpx w) { return (cpx){a.re * w.re - a.im * w.im, a.re * w.im + a.im * w.re}; }
FI cpx c_mulc_scalar(cpx a, cpx w) { return (cpx){a.re * w.re + a.im * w.im, a.im * w.re - a.re * w.im}; }
// x + h*y and x - h*y for a real scalar h
FI cpx c_fma(cpx x, cpx y, float h) { return (cpx){x.re + h * y.re, x.im + h * y.im}; }
FI cpx c_fms(cpx x, cpx y, float h) { return (cpx){x.re - h * y.re, x.im - h * y.im}; }

// In-place 4-point DFT (W_4 = -i), natural order in and out
FI void bfly4_scalar(cpx* a, cpx* b, cpx* c, cpx* d)
{
    cpx t0 = c_add(*a, *c), t1 = c_sub(*a, *c);
    cpx t2 = c_add(*b, *d), t3 = c_sub(*b, *d);
    *a = c_add(t0, t2);
    *c = c_sub(t0, t2);
    *b = c_subi_scalar(t1, t3); // t1 - i*t3
    *d = c_addi_scalar(t1, t3); // t1 + i*t3
}

// Stores butterfly output m to d + m*ds: times twiddle t[m-1] unless in the last (untwiddled) stage, re and im
// swapped when sw
FI void put(float* d, ptrdiff_t ds, const float* t, int m, cpx y, int last, int sw)
{
    if (!last && m)
        y = c_mul_scalar(y, c_ld(t + 2 * (m - 1)));
    if (sw)
        y = (cpx){y.im, y.re};
    c_st(d + m * ds, y);
}

// One forward pass (DIF) of radix R and span s over M complex values: butterflies, then W_Rs^(m*j) from tw on
// output m. R, last and sw are compile-time constants at each call. Each output pair is twiddled and stored as soon
// as it's computed, which keeps fewer values live (wasm engines allocate registers in about the order the code is
// written). Twiddles are walked by index, not pointer, so the last pass never forms a pointer past the table. The
// last pass (span 1, untwiddled) stores group g's output m to bin kb[g] + m*M/R, natural order, with re and im
// swapped when sw (the inverse, see cfft).
FI void
pass_fwd(const float* src, float* dst, int M, int R, int s, const float* tw, const int32_t* kb, int last, int sw)
{
    ptrdiff_t ss = 2 * (ptrdiff_t)s, ks = 2 * (ptrdiff_t)(M / R);
    for (ptrdiff_t o0 = 0, g = 0; o0 < 2 * (ptrdiff_t)M; o0 += R * ss, g++)
    {
        ptrdiff_t w = 0;
        for (ptrdiff_t o = o0; o < o0 + ss; o += 2, w += 2 * (R - 1))
        {
            float*       d  = last ? dst + kb[g] : dst + o;
            ptrdiff_t    ds = last ? ks : ss;
            const float* t  = tw + w;
            cpx          x[8];
            for (int m = 0; m < R; m++)
                x[m] = c_ld(src + o + m * ss);
            if (R == 4)
            {
                cpx t0 = c_add(x[0], x[2]), t1 = c_sub(x[0], x[2]);
                cpx t2 = c_add(x[1], x[3]), t3 = c_sub(x[1], x[3]);
                put(d, ds, t, 0, c_add(t0, t2), last, sw);
                put(d, ds, t, 2, c_sub(t0, t2), last, sw);
                put(d, ds, t, 1, c_subi_scalar(t1, t3), last, sw); // t1 - i*t3
                put(d, ds, t, 3, c_addi_scalar(t1, t3), last, sw); // t1 + i*t3
            }
            else
            {
                // 4-point DFTs E (even inputs), O (odd), then X_q = E_q + W_8^q O_q, X_q+4 = E_q - W_8^q O_q.
                // W_8 = h(1 - i), W_8^3 = -h(1 + i)
                const float h = SQRT1_2_F;
                bfly4_scalar(&x[0], &x[2], &x[4], &x[6]);
                bfly4_scalar(&x[1], &x[3], &x[5], &x[7]);
                put(d, ds, t, 0, c_add(x[0], x[1]), last, sw);
                put(d, ds, t, 4, c_sub(x[0], x[1]), last, sw);
                cpx u1 = c_subi_scalar(x[3], x[3]); // W_8 O1 / h
                put(d, ds, t, 1, c_fma(x[2], u1, h), last, sw);
                put(d, ds, t, 5, c_fms(x[2], u1, h), last, sw);
                put(d, ds, t, 2, c_subi_scalar(x[4], x[5]), last, sw);
                put(d, ds, t, 6, c_addi_scalar(x[4], x[5]), last, sw);
                cpx u3 = c_addi_scalar(x[7], x[7]); // -W_8^3 O3 / h
                put(d, ds, t, 3, c_fms(x[6], u3, h), last, sw);
                put(d, ds, t, 7, c_fma(x[6], u3, h), last, sw);
            }
        }
    }
}

// Complex FFT of M from src (natural order) to out (natural order, re / im swapped when sw): the first stage reads
// src into zbuf, the middle ones run in place there (safe since each butterfly writes exactly the values it read),
// the last one stores to out. src may be zbuf itself. Radix, s == 1 and sw become compile-time at the pass calls:
// one copy of each pass, shared by every n, stage and direction.
static void cfft(const Plan_scalar* plan, const float* src, float* out, int sw)
{
    int          M = plan->n / 2, radix[MAX_STAGES], span[MAX_STAGES], S = stages(M, radix, span);
    float*       z  = plan->zbuf;
    const float* tw = plan->stw;
    for (int t = 0; t < S; t++)
    {
        const float* a = t ? z : src;
        float*       b = t < S - 1 ? z : out;
        int          s = span[t];
        if (radix[t] == 4)
            s > 1 ? pass_fwd(a, b, M, 4, s, tw, 0, 0, 0)
            : sw  ? pass_fwd(a, b, M, 4, 1, tw, plan->kb, 1, 1)
                  : pass_fwd(a, b, M, 4, 1, tw, plan->kb, 1, 0);
        else
            s > 1 ? pass_fwd(a, b, M, 8, s, tw, 0, 0, 0)
            : sw  ? pass_fwd(a, b, M, 8, 1, tw, plan->kb, 1, 1)
                  : pass_fwd(a, b, M, 8, 1, tw, plan->kb, 1, 0);
        tw += stage_tw(radix[t], s);
    }
}

XFFT_DEF void xfft_forward_scalar(void* p, const float* in, float* out)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(p));
    XFFT_ASSERT(XFFT_IS_ALIGNED(in));
    XFFT_ASSERT(XFFT_IS_ALIGNED(out));
    const Plan_scalar* plan = p;
    int                M    = plan->n / 2;

    // 1. Complex FFT: the input is already z[m], interleaved
    cfft(plan, in, out, 0);
    {
        // 2. Real split in place, bin k and its mirror M-k per iteration:
        // X[k] = E + W_N^k O, X[M-k] = conj(E - W_N^k O), E = (Z[k] + conj Z[M-k])/2, O = (Z[k] - conj Z[M-k])/2i.
        // With c = conj Z[M-k], e = Z[k] + c and t = 0.5 W_N^k (Z[k] - c): X[k] = e/2 - it, X[M-k] = conj(e/2 + it)
        // (fused multiply-adds where the target has them). At k = M/2 both stores write the same bin, same value
        const float* tw = plan->rtw + M;
        float        z0 = out[0], z1 = out[1];
        out[0] = z0 + z1;
        out[1] = z0 - z1;
        for (int k = 1; k <= M / 2; k++)
        {
            cpx a = c_ld(out + 2 * k), b = c_ld(out + 2 * (M - k));
            cpx e = {a.re + b.re, a.im - b.im};
            cpx t = c_mul_scalar((cpx){a.re - b.re, a.im + b.im}, c_ld(tw + 2 * k - 2));
            c_st(out + 2 * k, (cpx){0.5f * e.re + t.im, 0.5f * e.im - t.re});
            c_st(out + 2 * (M - k), (cpx){0.5f * e.re - t.im, -0.5f * e.im - t.re});
        }
    }
}

XFFT_DEF void xfft_inverse_scalar(void* p, const float* in, float* out)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(p));
    XFFT_ASSERT(XFFT_IS_ALIGNED(in));
    XFFT_ASSERT(XFFT_IS_ALIGNED(out));
    const Plan_scalar* plan = p;
    int                M    = plan->n / 2;
    float*             z    = plan->zbuf;

    {
        // 2. Merge into zbuf in natural order, bin k and its mirror per iteration, with c = conj X[M-k]:
        // A = X[k] + c, B = (X[k] - c) conj(W_N^k), Z[k] = A + iB, Z[M-k] = conj(A - iB).
        // Stored with re and im swapped, for step 1
        const float* tw = plan->rtw;
        z[0]            = in[0] - in[1];
        z[1]            = in[0] + in[1];
        for (int k = 1; k <= M / 2; k++)
        {
            cpx x = c_ld(in + 2 * k), y = c_ld(in + 2 * (M - k));
            cpx a = {x.re + y.re, x.im - y.im};
            cpx b = c_mulc_scalar((cpx){x.re - y.re, x.im + y.im}, c_ld(tw + 2 * k - 2));
            c_st(z + 2 * k, (cpx){a.im + b.re, a.re - b.im});
            c_st(z + 2 * (M - k), (cpx){b.re - a.im, a.re + b.im});
        }
    }
    // 1. Inverse complex FFT as the forward one: IDFT(Z) = swap(DFT(swap(Z))), swap exchanging re and im. Both swaps
    // are free (the merge's stores above, the last stage's). The output layout is z[m] interleaved
    cfft(plan, z, out, 1);
}

XFFT_DEF const char* xfft_tell_scalar(void) { return "scalar"; }

#elif !defined(XFFT_NO_STUBS) // Scalar disabled: planning fails

XFFT_DEF size_t      xfft_mem_size_scalar(int n) { return 64; }
XFFT_DEF void*       xfft_init_scalar(void* mem, int n) { return 0; }
XFFT_DEF void        xfft_forward_scalar(void* p, const float* in, float* out) {}
XFFT_DEF void        xfft_inverse_scalar(void* p, const float* in, float* out) {}
XFFT_DEF const char* xfft_tell_scalar(void) { return "scalar-stub"; }

#endif // XFFT_SCALAR_ENABLED

/*~*~*~*
 ~ NEON ~
 *~*~*~*/

// xfft: NEON real FFT (AArch64).
//
// A real FFT of size N is a complex FFT of size M = N/2 on z[m] = x[2m] + i*x[2m+1], plus an O(N) pass to
// split the even/odd spectra apart (forward) or merge them (inverse). The complex FFT is a four-step FFT
// shaped around 4 complex values per pair of vectors (the xfft_avx2 design, but with interleaved complex data):
//
//   1. Lane FFTs. The M points are 4 interleaved subsequences z[4j + r], so the 8 floats at position j (a 'cv':
//      two vectors, subsequences 0,1 then 2,3, each re,im interleaved) hold point j of every subsequence, and the
//      4 FFTs of length L = M/4 run in parallel with scalar twiddles. That is exactly the input's own layout, so
//      the first stage loads it as is. Radix-8 / radix-4 DIF stages, output in digit-reversed order. The last
//      stage (span 1) also multiplies by the cross-lane twiddles W_M^(r*k).
//   2. Cross-lane pass. For 2 bins k, k+1: gather Y_r through the digit-reversal table, radix 2 across
//      subsequences (r, r+2) in-vector, swap 64-bit halves between the two bins, and radix 2 again. Z[k + L*q]
//      comes out as 2 consecutive bins, interleaved, in natural order. No 4x4 transposes. When the last lane
//      stage is radix 4 (and not the only stage) it runs fused with this pass: the two butterflies holding bins
//      k and k+1 (+ j*L/4) go straight into the cross-lane radix 4, skipping a round trip through the workspace.
//   3. Real split, written straight into the packed ordered layout (already interleaved).
//
// Complex arithmetic on interleaved data uses FCMLA / FCADD (ARMv8.3 complex, full speed on Apple cores), with
// a plain NEON fallback. The inverse runs the adjoint of each step in reverse order (merge, inverse cross-lane,
// DIT lane stages). L >= 4 for every N >= 32, so no size needs a special path.

#if defined(XFFT_NEON_ENABLED)

typedef struct
{
    int    n;
    float* stw;    // Lane stage twiddles. Per stage with span s > 1, per j < s: w^j, w^2j, w^3j..., each as
                   // (re, im, re, im) so one vector load covers both complex lanes
    float* xtw;    // Cross-lane twiddles, applied in the last forward / first inverse lane stage. Per lane vector
                   // position p (bin k, rev[k] = 8p), subsequence r: W_M^(r * k) at floats 2r, 2r+1. Forward table
                   // (scaled by 0.5, which the split needs) then the inverse one (unscaled), 8L floats each:
                   // see xtw_inv
    float*   rtw;  // Real split twiddles: W_N^k for k = 1..M/2, interleaved (re, im)
    int32_t* rev;  // L entries: float offset into the lane workspace of lane FFT bin k in the DIF output
    float*   zbuf; // M complex, interleaved: the complex spectrum Z, natural order
} Plan_neon;

// Lane FFT stages in DIF order: radix 8 first, then radix 4 for what's left (one 4 when log2(L) % 3 == 2,
// two when it's 1). The last stage always has span 1, so it never needs stage twiddles.
static int lane_stages_neon(int L, int* radix, int* span)
{
    int S = 0, l = __builtin_ctz(L);
    for (int s = L; s > 1; S++)
    {
        radix[S]  = l == 2 || l == 4 ? 4 : 8;
        l        -= __builtin_ctz(radix[S]);
        s        /= radix[S];
        span[S]   = s;
    }
    return S;
}

// Floats of twiddles for a stage: W_(radix*span)^(m*j) for j < span, m = 1..radix-1, (re, im, re, im) each
static int stage_tw_neon(int radix, int span) { return span > 1 ? 4 * (radix - 1) * span : 0; }

// Carves the plan's arrays out of 'mem' (when non-null). Returns the bytes needed.
static size_t layout_neon(int n, unsigned char* mem)
{
    int M = n / 2, L = n / 8;
    int radix[MAX_STAGES], span[MAX_STAGES], S = lane_stages_neon(L, radix, span), nstw = 0;
    for (int t = 0; t < S; t++)
        nstw += stage_tw_neon(radix[t], span[t]);

    size_t sizes[6] = {
        sizeof(Plan_neon),
        nstw * sizeof(float),
        2 * L * 8 * sizeof(float),
        M * sizeof(float),
        L * sizeof(int32_t),
        2 * M * sizeof(float),
    };
    size_t offs[6], off = 0;
    for (int i = 0; i < 6; i++)
    {
        offs[i]  = off;
        off     += ALIGN_UP(sizes[i]);
    }
    if (mem)
    {
        Plan_neon* plan = (Plan_neon*)mem;
        plan->n         = n;
        plan->stw       = (float*)(mem + offs[1]);
        plan->xtw       = (float*)(mem + offs[2]);
        plan->rtw       = (float*)(mem + offs[3]);
        plan->rev       = (int32_t*)(mem + offs[4]);
        plan->zbuf      = (float*)(mem + offs[5]);
    }
    return off;
}

// The inverse cross-lane twiddles follow the forward ones
static float* xtw_inv(const Plan_neon* plan) { return plan->xtw + plan->n; }

XFFT_DEF size_t xfft_mem_size_neon(int n) { return layout_neon(n, 0); }

XFFT_DEF void* xfft_init_neon(void* mem, int n)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(mem));
    if (n < XFFT_MIN_N || n > XFFT_MAX_N || (n & (n - 1)))
        return 0;
    layout_neon(n, mem);
    Plan_neon* plan = mem;
    int        M = n / 2, L = n / 8;
    int        radix[MAX_STAGES], span[MAX_STAGES], S = lane_stages_neon(L, radix, span);
    float      c, s;

    {
        // Lane stage twiddles: W_Rs^(m*j)
        float* tw = plan->stw;
        for (int t = 0; t < S; t++)
            for (int j = 0; j < span[t] && span[t] > 1; j++)
                for (int m = 1; m < radix[t]; m++)
                {
                    unit_cs(m * j, radix[t] * span[t], &c, &s);
                    tw[0] = tw[2] = c;
                    tw[1] = tw[3]  = -s;
                    tw            += 4;
                }
    }
    {
        // Real split twiddles: W_N^k
        for (int k = 1; k <= M / 2; k++)
        {
            unit_cs(k, n, &c, &s);
            plan->rtw[2 * k - 2] = c;
            plan->rtw[2 * k - 1] = -s;
        }
    }
    {
        // Digit reversal: bin k = m0 + r0*(m1 + r1*(m2 + ...)) ends up at position sum m_t * span_t
        for (int k = 0; k < L; k++)
        {
            int kk = k, pos = 0;
            for (int t = 0; t < S; t++)
            {
                pos += (kk % radix[t]) * span[t];
                kk  /= radix[t];
            }
            plan->rev[k] = 8 * pos;
        }
    }
    {
        // Cross-lane twiddles W_M^(r*k), stored at bin k's lane vector position, subsequence r at floats 2r, 2r+1
        float* xi = xtw_inv(plan);
        for (int k = 0; k < L; k++)
            for (int r = 0; r < 4; r++)
            {
                unit_cs(r * k, M, &c, &s);
                xi[plan->rev[k] + 2 * r]            = c;
                xi[plan->rev[k] + 2 * r + 1]        = -s;
                plan->xtw[plan->rev[k] + 2 * r]     = 0.5f * c;
                plan->xtw[plan->rev[k] + 2 * r + 1] = -0.5f * s;
            }
    }
    return plan;
}

typedef float32x4_t v4;

// Two interleaved complex values per vector. a + i*b, a - i*b, a * w, a * conj(w), conj(a)
#if defined(__ARM_FEATURE_COMPLEX)
FI v4 c_addi_neon(v4 a, v4 b) { return vcaddq_rot90_f32(a, b); }
FI v4 c_subi_neon(v4 a, v4 b) { return vcaddq_rot270_f32(a, b); }
FI v4 c_mul_neon(v4 a, v4 w) { return vcmlaq_rot90_f32(vcmlaq_f32(vdupq_n_f32(0.0f), w, a), w, a); }
FI v4 c_mulc_neon(v4 a, v4 w) { return vcmlaq_rot270_f32(vcmlaq_f32(vdupq_n_f32(0.0f), w, a), w, a); }
#else
FI v4 c_sign_im(void) { return (v4){1.0f, -1.0f, 1.0f, -1.0f}; }
FI v4 c_addi_neon(v4 a, v4 b) { return vfmsq_f32(a, vrev64q_f32(b), c_sign_im()); }
FI v4 c_subi_neon(v4 a, v4 b) { return vfmaq_f32(a, vrev64q_f32(b), c_sign_im()); }
FI v4 c_mul_neon(v4 a, v4 w)
{
    return vfmsq_f32(vmulq_f32(a, vtrn1q_f32(w, w)), vrev64q_f32(a), vmulq_f32(vtrn2q_f32(w, w), c_sign_im()));
}
FI v4 c_mulc_neon(v4 a, v4 w)
{
    return vfmaq_f32(vmulq_f32(a, vtrn1q_f32(w, w)), vrev64q_f32(a), vmulq_f32(vtrn2q_f32(w, w), c_sign_im()));
}
#endif
FI v4 c_conj(v4 a)
{
    return vreinterpretq_f32_u64(veorq_u64(vreinterpretq_u64_f32(a), vdupq_n_u64(0x8000000000000000ull)));
}
// Swaps the two complex values of a vector
FI v4 c_swap(v4 a) { return vextq_f32(a, a, 2); }
// (lo of a, lo of b) and (hi of a, hi of b), in units of one complex value
FI v4 c_lo(v4 a, v4 b) { return vreinterpretq_f32_f64(vzip1q_f64(vreinterpretq_f64_f32(a), vreinterpretq_f64_f32(b))); }
FI v4 c_hi(v4 a, v4 b) { return vreinterpretq_f32_f64(vzip2q_f64(vreinterpretq_f64_f32(a), vreinterpretq_f64_f32(b))); }

// 4 complex values: subsequences 0,1 in a, 2,3 in b
typedef struct
{
    v4 a, b;
} cv_neon;

FI cv_neon cv_ld_neon(const float* p) { return (cv_neon){vld1q_f32(p), vld1q_f32(p + 4)}; }
FI void    cv_st_neon(float* p, cv_neon x)
{
    vst1q_f32(p, x.a);
    vst1q_f32(p + 4, x.b);
}
FI cv_neon cv_add_neon(cv_neon x, cv_neon y) { return (cv_neon){vaddq_f32(x.a, y.a), vaddq_f32(x.b, y.b)}; }
FI cv_neon cv_sub_neon(cv_neon x, cv_neon y) { return (cv_neon){vsubq_f32(x.a, y.a), vsubq_f32(x.b, y.b)}; }
FI cv_neon cv_addi(cv_neon x, cv_neon y) { return (cv_neon){c_addi_neon(x.a, y.a), c_addi_neon(x.b, y.b)}; }
FI cv_neon cv_subi(cv_neon x, cv_neon y) { return (cv_neon){c_subi_neon(x.a, y.a), c_subi_neon(x.b, y.b)}; }
// x * w and x * conj(w): w is one twiddle per complex lane (a then b at w, w + 4), or one scalar twiddle
// (re, im, re, im) for both when 'sc'
FI cv_neon cv_mul_neon(cv_neon x, const float* w, int sc)
{
    v4 w0 = vld1q_f32(w), w1 = sc ? w0 : vld1q_f32(w + 4);
    return (cv_neon){c_mul_neon(x.a, w0), c_mul_neon(x.b, w1)};
}
FI cv_neon cv_mulc_neon(cv_neon x, const float* w, int sc)
{
    v4 w0 = vld1q_f32(w), w1 = sc ? w0 : vld1q_f32(w + 4);
    return (cv_neon){c_mulc_neon(x.a, w0), c_mulc_neon(x.b, w1)};
}
// x + h*y and x - h*y for a real scalar h
FI cv_neon cv_fma(cv_neon x, cv_neon y, float h)
{
    return (cv_neon){vfmaq_n_f32(x.a, y.a, h), vfmaq_n_f32(x.b, y.b, h)};
}
FI cv_neon cv_fms(cv_neon x, cv_neon y, float h)
{
    return (cv_neon){vfmsq_n_f32(x.a, y.a, h), vfmsq_n_f32(x.b, y.b, h)};
}

// In-place 4-point DFT, natural order in and out. Forward uses W_4 = -i, inverse its conjugate.
FI void bfly4_neon(cv_neon* a, cv_neon* b, cv_neon* c, cv_neon* d, int inv)
{
    cv_neon t0 = cv_add_neon(*a, *c), t1 = cv_sub_neon(*a, *c);
    cv_neon t2 = cv_add_neon(*b, *d), t3 = cv_sub_neon(*b, *d);
    cv_neon mi = cv_subi(t1, t3); // t1 - i*t3
    cv_neon pi = cv_addi(t1, t3); // t1 + i*t3
    *a         = cv_add_neon(t0, t2);
    *c         = cv_sub_neon(t0, t2);
    *b         = inv ? pi : mi;
    *d         = inv ? mi : pi;
}

// In-place 8-point DFT over x[0..7], natural order in and out
FI void bfly8_neon(cv_neon* x, int inv)
{
    bfly4_neon(&x[0], &x[2], &x[4], &x[6], inv); // E0..E3
    bfly4_neon(&x[1], &x[3], &x[5], &x[7], inv); // O0..O3
    const float h  = SQRT1_2_F;
    cv_neon     e0 = x[0], e1 = x[2], e2 = x[4], e3 = x[6], o0 = x[1], o1 = x[3], o2 = x[5], o3 = x[7];
    // Z_q = E_q + W_8^q O_q, Z_q+4 = E_q - W_8^q O_q. W_8 = h(1 - i), W_8^3 = -h(1 + i), and the conjugates inverse
    cv_neon u1 = inv ? cv_addi(o1, o1) : cv_subi(o1, o1); // W_8^+-1 O1 / h
    cv_neon u3 = inv ? cv_subi(o3, o3) : cv_addi(o3, o3); // -W_8^+-3 O3 / h
    x[0]       = cv_add_neon(e0, o0);
    x[4]       = cv_sub_neon(e0, o0);
    x[1]       = cv_fma(e1, u1, h);
    x[5]       = cv_fms(e1, u1, h);
    x[2]       = inv ? cv_addi(e2, o2) : cv_subi(e2, o2);
    x[6]       = inv ? cv_subi(e2, o2) : cv_addi(e2, o2);
    x[3]       = cv_fms(e3, u3, h);
    x[7]       = cv_fma(e3, u3, h);
}

// In-place R-point DFT over x[0..R-1], natural order in and out
FI void bfly_neon(cv_neon* x, int R, int inv)
{
    if (R == 4)
        bfly4_neon(&x[0], &x[1], &x[2], &x[3], inv);
    else
        bfly8_neon(x, inv);
}

// One forward lane pass (DIF) of radix R and span s: butterflies, then W_Rs^(m*j) from tw on output m when s > 1,
// or the cross-lane twiddles xt when s == 1. R and s == 1 are compile-time constants at each call. Offsets are
// pointer-sized and the twiddles walk a pointer (see xfft_avx2 it14). tw is null when s == 1, so w only walks if s > 1
FI void lane_pass_fwd_neon(const float* src, float* dst, int L, int R, int s, const float* tw, const float* xt)
{
    ptrdiff_t ss = 8 * (ptrdiff_t)s;
    for (ptrdiff_t o0 = 0; o0 < 8 * (ptrdiff_t)L; o0 += R * ss)
    {
        const float* w = tw;
        for (ptrdiff_t o = o0; o < o0 + ss; o += 8, w += s > 1 ? 4 * (R - 1) : 0)
        {
            cv_neon x[8];
            for (int m = 0; m < R; m++)
                x[m] = cv_ld_neon(src + o + m * ss);
            bfly_neon(x, R, 0);
            _Pragma("clang loop unroll(full)") for (int m = 0; m < R; m++)
            {
                ptrdiff_t p = o + m * ss;
                if (s == 1)
                    x[m] = cv_mul_neon(x[m], xt + p, 0);
                else if (m)
                    x[m] = cv_mul_neon(x[m], w + 4 * (m - 1), 1);
                cv_st_neon(dst + p, x[m]);
            }
        }
    }
}

// Adjoint of lane_pass_fwd_neon: conj twiddles on the inputs, then inverse butterflies
FI void lane_pass_inv_neon(const float* src, float* dst, int L, int R, int s, const float* tw, const float* xt)
{
    ptrdiff_t ss = 8 * (ptrdiff_t)s;
    for (ptrdiff_t o0 = 0; o0 < 8 * (ptrdiff_t)L; o0 += R * ss)
    {
        const float* w = tw;
        for (ptrdiff_t o = o0; o < o0 + ss; o += 8, w += s > 1 ? 4 * (R - 1) : 0)
        {
            cv_neon x[8];
            _Pragma("clang loop unroll(full)") for (int m = 0; m < R; m++)
            {
                ptrdiff_t p = o + m * ss;
                x[m]        = cv_ld_neon(src + p);
                if (s == 1)
                    x[m] = cv_mulc_neon(x[m], xt + p, 0);
                else if (m)
                    x[m] = cv_mulc_neon(x[m], w + 4 * (m - 1), 1);
            }
            bfly_neon(x, R, 1);
            for (int m = 0; m < R; m++)
                cv_st_neon(dst + o + m * ss, x[m]);
        }
    }
}

// Only the last stage has span 1 (and only the cross-lane twiddles xt). radix and s == 1 become compile-time here.
// Not inlined: one copy of each pass, shared by every n and stage. A compile-time L and s were worth only 1-3%
// for ~10x the code (NOTES.md, "Code size and cold-call costs").
NI void lane_stage_fwd_neon(const float* src, float* dst, int L, int radix, int s, const float* tw, const float* xt)
{
    if (radix == 4)
        s > 1 ? lane_pass_fwd_neon(src, dst, L, 4, s, tw, 0) : lane_pass_fwd_neon(src, dst, L, 4, 1, 0, xt);
    else
        s > 1 ? lane_pass_fwd_neon(src, dst, L, 8, s, tw, 0) : lane_pass_fwd_neon(src, dst, L, 8, 1, 0, xt);
}

NI void lane_stage_inv_neon(const float* src, float* dst, int L, int radix, int s, const float* tw, const float* xt)
{
    if (radix == 4)
        s > 1 ? lane_pass_inv_neon(src, dst, L, 4, s, tw, 0) : lane_pass_inv_neon(src, dst, L, 4, 1, 0, xt);
    else
        s > 1 ? lane_pass_inv_neon(src, dst, L, 8, s, tw, 0) : lane_pass_inv_neon(src, dst, L, 8, 1, 0, xt);
}

// Cross-lane radix 4 for bins k (y0) and k+1 (y1): radix 2 over (r, r+2) inside the vectors, then over (r, r+1)
// between the vector halves. Z_q = sum_r Y_r (-i)^(rq) goes to Z[k + qL], Z[k+1 + qL] (one vector)
FI void cross_fwd(cv_neon y0, cv_neon y1, float* z, ptrdiff_t k, ptrdiff_t L)
{
    v4 p0 = vaddq_f32(y0.a, y0.b), q0 = vsubq_f32(y0.a, y0.b), p1 = vaddq_f32(y1.a, y1.b), q1 = vsubq_f32(y1.a, y1.b);
    v4 pl = c_lo(p0, p1), ph = c_hi(p0, p1), ql = c_lo(q0, q1), qh = c_hi(q0, q1);
    vst1q_f32(z + 2 * k, vaddq_f32(pl, ph));
    vst1q_f32(z + 2 * (k + L), c_subi_neon(ql, qh));
    vst1q_f32(z + 2 * (k + 2 * L), vsubq_f32(pl, ph));
    vst1q_f32(z + 2 * (k + 3 * L), c_addi_neon(ql, qh));
}

// Adjoint of cross_fwd: Z[k + qL], Z[k+1 + qL] -> bins k (*y0) and k+1 (*y1).
// pl = Z0 + Z2, ph = Z0 - Z2, ql = Z1 + Z3, qh = i(Z1 - Z3), then radix 2 over (r, r+2)
FI void cross_inv(const float* z, ptrdiff_t k, ptrdiff_t L, cv_neon* y0, cv_neon* y1)
{
    v4 z0 = vld1q_f32(z + 2 * k), z1 = vld1q_f32(z + 2 * (k + L));
    v4 z2 = vld1q_f32(z + 2 * (k + 2 * L)), z3 = vld1q_f32(z + 2 * (k + 3 * L));
    v4 pl = vaddq_f32(z0, z2), ph = vsubq_f32(z0, z2);
    v4 ql = vaddq_f32(z1, z3), qh = c_addi_neon(vdupq_n_f32(0.0f), vsubq_f32(z1, z3));
    v4 p0 = c_lo(pl, ph), p1 = c_hi(pl, ph), q0 = c_lo(ql, qh), q1 = c_hi(ql, qh);
    *y0 = (cv_neon){vaddq_f32(p0, q0), vsubq_f32(p0, q0)};
    *y1 = (cv_neon){vaddq_f32(p1, q1), vsubq_f32(p1, q1)};
}

XFFT_DEF void xfft_forward_neon(void* p, const float* in, float* out)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(p));
    XFFT_ASSERT(XFFT_IS_ALIGNED(in));
    XFFT_ASSERT(XFFT_IS_ALIGNED(out));
    const Plan_neon* plan = p;
    int              n = plan->n, M = n / 2, L = n / 8;
    // Lane FFT workspace: out (L positions of 8 floats = N floats), which the split overwrites last
    float* work = out;
    float* z    = plan->zbuf;

    {
        // 1. Lane FFTs: input (already in lane layout) -> work, digit-reversed. The last stage (span 1) applies the
        // cross-lane twiddles
        int radix[MAX_STAGES], span[MAX_STAGES], S = lane_stages_neon(L, radix, span);
        // With a radix-4 last stage (and more than one stage), it runs fused with the cross-lane pass below
        int          fused = S > 1 && radix[S - 1] == 4;
        const float* tw    = plan->stw;
        lane_stage_fwd_neon(in, work, L, radix[0], span[0], tw, plan->xtw);
        tw += stage_tw_neon(radix[0], span[0]);
        for (int t = 1; t < S - fused; t++)
        {
            lane_stage_fwd_neon(work, work, L, radix[t], span[t], tw, plan->xtw);
            tw += stage_tw_neon(radix[t], span[t]);
        }
        // rev in a local: the stores through float pointers would otherwise reload plan->rev every iteration.
        // rev[k + 1] = rev[k] + rev[1] for even k
        const int32_t* rev  = plan->rev;
        ptrdiff_t      off1 = rev[1];
        if (fused)
        {
            // 1+2. Last lane stage and cross-lane together, two butterflies per iteration: the one holding bins
            // kb + j*L/4 (j < 4, at rev[kb] + 8j) and the one holding kb+1 + j*L/4 (at rev[kb] + off1 + 8j)
            for (int kb = 0; kb < L / 4; kb += 2)
            {
                const float* b = work + rev[kb];
                const float* w = plan->xtw + rev[kb];
                cv_neon      x[4], y[4];
                for (int m = 0; m < 4; m++)
                {
                    x[m] = cv_ld_neon(b + 8 * m);
                    y[m] = cv_ld_neon(b + off1 + 8 * m);
                }
                bfly4_neon(&x[0], &x[1], &x[2], &x[3], 0);
                bfly4_neon(&y[0], &y[1], &y[2], &y[3], 0);
                for (int m = 0; m < 4; m++)
                    cross_fwd(
                        cv_mul_neon(x[m], w + 8 * m, 0),
                        cv_mul_neon(y[m], w + off1 + 8 * m, 0),
                        z,
                        kb + m * (L / 4),
                        L);
            }
        }
        else
        {
            // 2. Cross-lane: work (already twiddled) -> Z in natural order, bins k0, k0+1 per iteration
            for (int k0 = 0; k0 < L; k0 += 2)
                cross_fwd(cv_ld_neon(work + rev[k0]), cv_ld_neon(work + rev[k0] + off1), z, k0, L);
        }
    }
    {
        // 3. Real split, 2 bins k = k0+1, k0+2 and their mirrors M-k per iteration:
        // X[k] = E + W_N^k O, X[M-k] = conj(E - W_N^k O), E = (Z[k] + conj Z[M-k])/2, O = (Z[k] - conj Z[M-k])/2i.
        // The cross-lane twiddles already halved Z, so with c = conj Z[M-k] and T = W_N^k (Z[k] - c):
        // X[k] = (Z[k] + c) - iT, X[M-k] = conj((Z[k] + c) + iT)
        const float* tw = plan->rtw;
        out[0]          = 2.0f * (z[0] + z[1]);
        out[1]          = 2.0f * (z[0] - z[1]);
        for (int k0 = 0; k0 < M / 2; k0 += 2)
        {
            v4 a = vld1q_f32(z + 2 * k0 + 2);
            v4 c = c_conj(c_swap(vld1q_f32(z + 2 * (M - k0 - 2))));
            v4 e = vaddq_f32(a, c);
            v4 t = c_mul_neon(vsubq_f32(a, c), vld1q_f32(tw + 2 * k0));
            vst1q_f32(out + 2 * k0 + 2, c_subi_neon(e, t));
            vst1q_f32(out + 2 * (M - k0 - 2), c_swap(c_conj(c_addi_neon(e, t))));
        }
    }
}

XFFT_DEF void xfft_inverse_neon(void* p, const float* in, float* out)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(p));
    XFFT_ASSERT(XFFT_IS_ALIGNED(in));
    XFFT_ASSERT(XFFT_IS_ALIGNED(out));
    const Plan_neon* plan = p;
    int              n = plan->n, M = n / 2, L = n / 8;
    // Lane FFT workspace: out, as in xfft_forward_neon. The last lane stage then runs in place, which is safe since
    // each butterfly writes exactly the vectors it read. At 8192 the working set is larger than L1, so zbuf is mostly
    // evicted between calls and the merge's descending stores into it were slow (+40% for the merge): there Z goes
    // in out, which is still cached, and the lane FFTs run in zbuf. Smaller sizes are 2-3% faster without the swap.
    float* work = n >= 8192 ? plan->zbuf : out;
    float* z    = n >= 8192 ? out : plan->zbuf;

    {
        // 3. Merge, 2 bins k = k0+1, k0+2 and their mirrors per iteration, with c = conj X[M-k]:
        // A = X[k] + c, B = (X[k] - c) conj(W_N^k), Z[k] = A + iB, Z[M-k] = conj A + i conj B = conj(A - iB)
        // At 8192 z is out, so with in == out read in[0..1] before writing z[0]
        const float* tw = plan->rtw;
        float        x0 = in[0], x1 = in[1];
        z[0] = x0 + x1;
        z[1] = x0 - x1;
        for (int k0 = 0; k0 < M / 2; k0 += 2)
        {
            v4 x = vld1q_f32(in + 2 * k0 + 2);
            v4 c = c_conj(c_swap(vld1q_f32(in + 2 * (M - k0 - 2))));
            v4 a = vaddq_f32(x, c);
            v4 b = c_mulc_neon(vsubq_f32(x, c), vld1q_f32(tw + 2 * k0));
            vst1q_f32(z + 2 * k0 + 2, c_addi_neon(a, b));
            vst1q_f32(z + 2 * (M - k0 - 2), c_swap(c_conj(c_subi_neon(a, b))));
        }
    }
    {
        int radix[MAX_STAGES], span[MAX_STAGES], twoff[MAX_STAGES], S = lane_stages_neon(L, radix, span);
        int fused = S > 1 && radix[S - 1] == 4; // As in xfft_forward_neon
        for (int t = 0, off = 0; t < S; t++)
        {
            twoff[t]  = off;
            off      += stage_tw_neon(radix[t], span[t]);
        }
        const int32_t* rev  = plan->rev; // In a local, as in xfft_forward_neon
        ptrdiff_t      off1 = rev[1];
        if (fused)
        {
            // 2+1. Inverse cross-lane and the first inverse lane stage (span 1, conj cross-lane twiddles) together:
            // Z -> work, the adjoint of forward_n's fused loop
            for (int kb = 0; kb < L / 4; kb += 2)
            {
                float*       b = work + rev[kb];
                const float* w = xtw_inv(plan) + rev[kb];
                cv_neon      x[4], y[4];
                for (int m = 0; m < 4; m++)
                {
                    cross_inv(z, kb + m * (L / 4), L, &x[m], &y[m]);
                    x[m] = cv_mulc_neon(x[m], w + 8 * m, 0);
                    y[m] = cv_mulc_neon(y[m], w + off1 + 8 * m, 0);
                }
                bfly4_neon(&x[0], &x[1], &x[2], &x[3], 1);
                bfly4_neon(&y[0], &y[1], &y[2], &y[3], 1);
                for (int m = 0; m < 4; m++)
                {
                    cv_st_neon(b + 8 * m, x[m]);
                    cv_st_neon(b + off1 + 8 * m, y[m]);
                }
            }
        }
        else
        {
            // 2. Inverse cross-lane: Z -> work, digit-reversed (the conj twiddles come in the first inverse lane stage)
            for (int k0 = 0; k0 < L; k0 += 2)
            {
                cv_neon y0, y1;
                cross_inv(z, k0, L, &y0, &y1);
                cv_st_neon(work + rev[k0], y0);
                cv_st_neon(work + rev[k0] + off1, y1);
            }
        }
        // 1. Inverse lane FFTs (DIT): work -> output, natural order (the output layout is the lane layout). The first
        // of these (span 1) also applies the conj cross-lane twiddles, unless it ran fused above
        for (int t = S - 1 - fused; t > 0; t--)
            lane_stage_inv_neon(work, work, L, radix[t], span[t], plan->stw + twoff[t], xtw_inv(plan));
        lane_stage_inv_neon(work, out, L, radix[0], span[0], plan->stw, xtw_inv(plan));
    }
}

XFFT_DEF const char* xfft_tell_neon(void) { return "neon"; }

#elif !defined(XFFT_NO_STUBS) // No AArch64 NEON (or XFFT_DISABLE_NEON): planning fails

XFFT_DEF size_t      xfft_mem_size_neon(int n) { return 64; }
XFFT_DEF void*       xfft_init_neon(void* mem, int n) { return 0; }
XFFT_DEF void        xfft_forward_neon(void* p, const float* in, float* out) {}
XFFT_DEF void        xfft_inverse_neon(void* p, const float* in, float* out) {}
XFFT_DEF const char* xfft_tell_neon(void) { return "neon-stub"; }

#endif // XFFT_NEON_ENABLED

/*~*~*~*~*~*~*
 ~ x86 shared ~
 *~*~*~*~*~*~*/

// The plan, its planning code and the helpers that are identical in the AVX and AVX2 backends.
// Only AVX instructions here: it's compiled when either backend is.

#if defined(XFFT_X86_ENABLED)

typedef struct
{
    int    n;
    float* stw;    // Lane stage twiddles, scalars. Per stage with span s > 1, per j < s: w^j, w^2j, w^3j (re, im)
    float* xtw;    // Cross-lane twiddles, applied in the last forward / first inverse lane stage. Per lane vector
                   // position p (bin k, rev[k] = 16p), lane l: W_M^(LANE_SUB[l] * k), 8 re then 8 im
    float*   rtw;  // Real split twiddles: 0.5 * W_N^k for k = 1..M/2. M/2 re then M/2 im
    int32_t* rev;  // L entries: float offset into work of lane FFT bin k in the DIF output
    float*   work; // Lane workspace when L < 8 (otherwise 'out' is): L complex vectors, each 8 re then 8 im, then one
                   // junk vector for the unused cross-lane lanes
    float* zbuf;   // M re then M im: the complex spectrum Z, natural order. Then 8 zero floats for N = 64
                   // (xfft_forward_*)
    float* mat;    // N = 32 only: forward then inverse 32x32 matrices for dense32
} Plan_x86;

// Lane l holds subsequence LANE_SUB[l] (see cv_ld_il). It's its own inverse, so subsequence r is in lane LANE_SUB[r]
static const int LANE_SUB[8] = {0, 1, 4, 5, 2, 3, 6, 7};

// Lane FFT stages in DIF order: radix 4, with a final radix 8 when log2(L) is odd (radix 2 for L = 2).
// The last stage always has span 1, so it never needs twiddles.
static int lane_stages_x86(int L, int* radix, int* span)
{
    int S = 0;
    for (int s = L; s > 1; S++)
    {
        radix[S]  = s == 8 || s == 2 ? s : 4;
        s        /= radix[S];
        span[S]   = s;
    }
    return S;
}

// Carves the plan's arrays out of 'mem' (when non-null). Returns the bytes needed.
static size_t layout_x86(int n, unsigned char* mem)
{
    int M = n / 2, L = n / 16;
    int radix[MAX_STAGES], span[MAX_STAGES], S = lane_stages_x86(L, radix, span), nstw = 0;
    for (int t = 0; t < S; t++)
        nstw += stage_tw(radix[t], span[t]);

    size_t sizes[8] = {
        sizeof(Plan_x86),
        nstw * sizeof(float),
        L * 16 * sizeof(float),
        M * sizeof(float),
        L * sizeof(int32_t),
        (L + 1) * 16 * sizeof(float),
        (2 * M + 8) * sizeof(float),
        n == 32 ? 2 * 32 * 32 * sizeof(float) : 0,
    };
    size_t offs[8], off = 0;
    for (int i = 0; i < 8; i++)
    {
        offs[i]  = off;
        off     += ALIGN_UP(sizes[i]);
    }
    if (mem)
    {
        Plan_x86* plan = (Plan_x86*)mem;
        plan->n        = n;
        plan->stw      = (float*)(mem + offs[1]);
        plan->xtw      = (float*)(mem + offs[2]);
        plan->rtw      = (float*)(mem + offs[3]);
        plan->rev      = (int32_t*)(mem + offs[4]);
        plan->work     = (float*)(mem + offs[5]);
        plan->zbuf     = (float*)(mem + offs[6]);
        plan->mat      = (float*)(mem + offs[7]);
    }
    return off;
}

static void* init_x86(void* mem, int n)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(mem));
    if (n < XFFT_MIN_N || n > XFFT_MAX_N || (n & (n - 1)))
        return 0;
    layout_x86(n, mem);
    Plan_x86* plan = mem;
    int       M = n / 2, L = n / 16;
    int       radix[MAX_STAGES], span[MAX_STAGES], S = lane_stages_x86(L, radix, span);
    float     c, s;
    for (int i = 0; i < 8; i++)
        plan->zbuf[2 * M + i] = 0.0f; // Read by the N = 64 inverse cross-lane pass (see xfft_forward_*)

    {
        // Lane stage twiddles: W_Rs^(m*j)
        float* tw = plan->stw;
        for (int t = 0; t < S; t++)
            for (int j = 0; j < span[t] && span[t] > 1; j++)
                for (int m = 1; m < radix[t]; m++)
                {
                    unit_cs(m * j, radix[t] * span[t], &c, &s);
                    *tw++ = c;
                    *tw++ = -s;
                }
    }
    {
        // Real split twiddles: 0.5 * W_N^k
        for (int k = 1; k <= M / 2; k++)
        {
            unit_cs(k, n, &c, &s);
            plan->rtw[k - 1]         = 0.5f * c;
            plan->rtw[M / 2 + k - 1] = -0.5f * s;
        }
    }
    {
        // Digit reversal: bin k = m0 + r0*(m1 + r1*(m2 + ...)) ends up at position sum m_t * span_t
        for (int k = 0; k < L; k++)
        {
            int kk = k, pos = 0;
            for (int t = 0; t < S; t++)
            {
                pos += (kk % radix[t]) * span[t];
                kk  /= radix[t];
            }
            plan->rev[k] = 16 * pos;
        }
    }
    {
        // Cross-lane twiddles W_M^(r*k), stored at bin k's lane vector position, subsequence r in lane LANE_SUB[r]
        for (int k = 0; k < L; k++)
            for (int l = 0; l < 8; l++)
            {
                unit_cs(LANE_SUB[l] * k, M, &c, &s);
                plan->xtw[plan->rev[k] + l]     = c;
                plan->xtw[plan->rev[k] + 8 + l] = -s;
            }
    }
    if (n == 32)
    {
        // The matrices for dense32 (see there), forward then inverse. They only differ by the inverse's 2 on rows
        // 2p, 2p+1. Columns: 32 output floats [V0 V1 V2 V3], i = column % 16, column 16 = the "16" slot.
        for (int m = 0; m < 2; m++)
            for (int r = 0; r < 32; r++)
                for (int col = 0; col < 32; col++)
                {
                    int   i = col % 16, p = r / 2, a = col < 16; // a: an even-part column (V0, V1)
                    float v = 0.0f, sc = m ? 2.0f : 1.0f;
                    unit_cs(p * i, 32, &c, &s);
                    if (r == 0)
                        v = a || col == 16 ? 1.0f : 0.0f;
                    else if (r == 1)
                        v = a ? (i & 1 ? -1.0f : 1.0f) : col == 16 ? 1.0f : 0.0f;
                    else if (!(r & 1))
                        v = a ? sc * c : col == 16 ? sc * (p & 1 ? -1.0f : 1.0f) : 0.0f;
                    else
                        v = a || col == 16 ? 0.0f : -sc * s;
                    plan->mat[32 * 32 * m + 32 * r + col] = v;
                }
    }
    return plan;
}

typedef struct
{
    __m256 r, i;
} cv_x86;

FI cv_x86 cv_ld_x86(const float* p) { return (cv_x86){_mm256_load_ps(p), _mm256_load_ps(p + 8)}; }
FI void   cv_st_x86(float* p, cv_x86 a)
{
    _mm256_store_ps(p, a.r);
    _mm256_store_ps(p + 8, a.i);
}
// 8 interleaved complex values. Lane l holds point (0,1,4,5,2,3,6,7)[l]; cv_st_il undoes the same order.
FI cv_x86 cv_ld_il(const float* p)
{
    __m256 a = _mm256_load_ps(p), b = _mm256_load_ps(p + 8);
    return (cv_x86){_mm256_shuffle_ps(a, b, 0x88), _mm256_shuffle_ps(a, b, 0xDD)};
}
FI void cv_st_il(float* p, cv_x86 a)
{
    _mm256_store_ps(p, _mm256_unpacklo_ps(a.r, a.i));
    _mm256_store_ps(p + 8, _mm256_unpackhi_ps(a.r, a.i));
}
FI cv_x86 cv_add_x86(cv_x86 a, cv_x86 b) { return (cv_x86){_mm256_add_ps(a.r, b.r), _mm256_add_ps(a.i, b.i)}; }
FI cv_x86 cv_sub_x86(cv_x86 a, cv_x86 b) { return (cv_x86){_mm256_sub_ps(a.r, b.r), _mm256_sub_ps(a.i, b.i)}; }

// Shuffles compete with adds for p1/p5, and most 256-bit shuffles (unpck, vpermilps, vperm2f128, vpermps) are p5
// only. So 128-bit halves are moved with loads (vinsertf128 from memory) and stores (vextractf128 to memory).

// 4x4 transposes within each 128-bit half of a[0..3]. Only even/odd vshufps (0x88/0xDD), which can issue on
// p1 or p5; unpck and the 0x44/0xEE shuffles (that clang turns into unpcklpd) are p5 only.
FI void transpose4x2(__m256* a)
{
    __m256 t0 = _mm256_shuffle_ps(a[0], a[1], 0x88), t1 = _mm256_shuffle_ps(a[0], a[1], 0xDD);
    __m256 t2 = _mm256_shuffle_ps(a[2], a[3], 0x88), t3 = _mm256_shuffle_ps(a[2], a[3], 0xDD);
    a[0] = _mm256_shuffle_ps(t0, t2, 0x88);
    a[1] = _mm256_shuffle_ps(t1, t3, 0x88);
    a[2] = _mm256_shuffle_ps(t0, t2, 0xDD);
    a[3] = _mm256_shuffle_ps(t1, t3, 0xDD);
}

// v[l] lane i = b[off[i] + l], for l, i < 8
FI void gather_transpose8(const float* b, const int32_t* off, __m256* v)
{
    for (int i = 0; i < 4; i++)
    {
        const float *p = b + off[i], *q = b + off[i + 4];
        v[i]     = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(p)), _mm_load_ps(q), 1);
        v[i + 4] = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(p + 4)), _mm_load_ps(q + 4), 1);
    }
    transpose4x2(v);
    transpose4x2(v + 4);
}

// The integer forms of the in-lane reverse (vpshufd) and unpack (vpunpck*dq) can issue on p1 or p5; the float forms
// (vpermilps, vunpck*ps) are p5 only.
// 8 floats from p, reversed
FI __m256 load_rev(const float* p)
{
    __m256 a = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(p + 4)), _mm_load_ps(p), 1);
    return _mm256_permute_ps(a, 0x1B);
}
// p[0..7] = v reversed. Left for clang to merge (vperm2f128 + one 256-bit store): two 128-bit stores in the merge pass
// were 2% faster at 2048 inv but 7% slower at 4096 inv (xfft_avx it2)
FI void store_rev(float* p, __m256 v)
{
    v = _mm256_permute_ps(v, 0x1B);
    _mm_store_ps(p, _mm256_extractf128_ps(v, 1));
    _mm_store_ps(p + 4, _mm256_castps256_ps128(v));
}
// (re, im) of 8 interleaved complex values at p, in natural order or reversed
FI void load_il8(const float* p, __m256* re, __m256* im)
{
    __m256 a = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_loadu_ps(p)), _mm_loadu_ps(p + 8), 1);      // 0 1 | 4 5
    __m256 b = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_loadu_ps(p + 4)), _mm_loadu_ps(p + 12), 1); // 2 3 | 6 7
    *re      = _mm256_shuffle_ps(a, b, 0x88);
    *im      = _mm256_shuffle_ps(a, b, 0xDD);
}
FI void load_il8_rev(const float* p, __m256* re, __m256* im)
{
    __m256 a = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(p + 12)), _mm_load_ps(p + 4), 1); // 6 7 | 2 3
    __m256 b = _mm256_insertf128_ps(_mm256_castps128_ps256(_mm_load_ps(p + 8)), _mm_load_ps(p), 1);      // 4 5 | 0 1
    *re      = _mm256_shuffle_ps(a, b, 0x22);
    *im      = _mm256_shuffle_ps(a, b, 0x77);
}

// Offsets from work + rev[k0] of the 8 bins k0 + i in a cross-lane group. Digit reversal is a bit permutation, so
// rev[k0 + i] = rev[k0] + rev[i] when k0 is a multiple of 8. Lanes i >= L (N = 64 only, so k0 = 0) get the junk vector.
FI void cross_offsets(const Plan_x86* plan, int32_t* off)
{
    int L = plan->n / 16;
    for (int i = 0; i < 8; i++)
        off[i] = i < L ? plan->rev[i] : 16 * L;
}

// Z rows j = 0..15 of a cross-lane group (zre rows q = j, zim rows q = j - 8, since zim = zre + 8L) are at b0 + j*L
// floats. With a runtime L, clang keeps 16 row pointers or offsets and reloads them from the stack each group (~16 of
// ~278 uops in a rename-bound loop). Spelled out as base + index*scale from 3 bases (b0, b2 = b0 + 2L, b7 = b0 + 7L)
// and 2 indexes (l1 = L, l3 = 3L, in bytes), every row is one addressing mode and only 5 registers stay live:
//   b0: 0, 1 (l1), 2 (l1*2), 3 (l3), 4 (l1*4), 6 (l3*2), 8 (l1*8), 12 (l3*4)
//   b2: 5 (l3), 10 (l1*8), 14 (l3*4)          b7: 7, 9 (l1*2), 11 (l1*4), 13 (l3*2), 15 (l1*8)
// Inline asm because clang re-derives the offsets from any C spelling. {att|intel}: clang-cl defaults to Intel syntax.
// volatile + "memory" also keeps the stores in row order, which N = 64 relies on (see xfft_forward_*). vmovups: N = 64
// rows are only 16-byte aligned (no cost when aligned, as for N >= 128).
#define ZROW_ST(v, base, idx, sc)                                                                                      \
    __asm__ volatile("{vmovups %0, (%1,%2," #sc ")|vmovups [%1+%2*" #sc "], %0}" ::"x"(v), "r"(base), "r"(idx)         \
                     : "memory")
#define ZROW_ST0(v, base) __asm__ volatile("{vmovups %0, (%1)|vmovups [%1], %0}" ::"x"(v), "r"(base) : "memory")
#define ZROW_LD(v, base, idx, sc)                                                                                      \
    __asm__ volatile("{vmovups (%1,%2," #sc "), %0|vmovups %0, [%1+%2*" #sc "]}"                                       \
                     : "=x"(v)                                                                                         \
                     : "r"(base), "r"(idx)                                                                             \
                     : "memory")
#define ZROW_LD0(v, base) __asm__ volatile("{vmovups (%1), %0|vmovups %0, [%1]}" : "=x"(v) : "r"(base) : "memory")

#endif // XFFT_X86_ENABLED

/*~*~*~*
 ~ AVX2 ~
 *~*~*~*/

// xfft: AVX2 real FFT.
//
// A real FFT of size N is a complex FFT of size M = N/2 on z[m] = x[2m] + i*x[2m+1], plus an O(N) pass to
// split the even/odd spectra apart (forward) or merge them (inverse). The complex FFT is a four-step FFT
// shaped around 8-wide vectors:
//
//   1. Lane FFTs. The M points are 8 interleaved subsequences z[8j + r], one per vector lane, so vector j
//      holds point j of every subsequence and the 8 FFTs of length L = M/8 run lane-parallel with scalar
//      (broadcast) twiddles. Radix-4 DIF stages on split re/im vectors; output is in digit-reversed order.
//   2. Cross-lane pass. For 8 bins k: gather Y_r[k] through the digit-reversal table, transpose 8x8 so each
//      vector holds one subsequence, twiddle by W_M^(r*k) and do a radix-8 DFT across subsequences.
//      Z[k + L*q] comes out as 8 consecutive bins, in natural order.
//   3. Real split, written straight into the packed ordered layout.
//
// The inverse runs the adjoint of each step in reverse order (merge, inverse cross-lane, DIT lane stages).
// N = 32 skips all of this: it's a matrix product in registers (dense32).

#if defined(XFFT_AVX2_ENABLED)

XFFT_DEF size_t xfft_mem_size_avx2(int n) { return layout_x86(n, 0); }
XFFT_DEF void*  xfft_init_avx2(void* mem, int n) { return init_x86(mem, n); }

// a * w
FI cv_x86 cv_mul_avx2(cv_x86 a, __m256 wr, __m256 wi)
{
    return (cv_x86){_mm256_fmsub_ps(a.r, wr, _mm256_mul_ps(a.i, wi)), _mm256_fmadd_ps(a.r, wi, _mm256_mul_ps(a.i, wr))};
}
// a * conj(w)
FI cv_x86 cv_mulc_avx2(cv_x86 a, __m256 wr, __m256 wi)
{
    return (cv_x86){_mm256_fmadd_ps(a.r, wr, _mm256_mul_ps(a.i, wi)), _mm256_fmsub_ps(a.i, wr, _mm256_mul_ps(a.r, wi))};
}
FI cv_x86 cv_mul_bc_avx2(cv_x86 a, const float* w)
{
    return cv_mul_avx2(a, _mm256_broadcast_ss(w), _mm256_broadcast_ss(w + 1));
}
FI cv_x86 cv_mulc_bc_avx2(cv_x86 a, const float* w)
{
    return cv_mulc_avx2(a, _mm256_broadcast_ss(w), _mm256_broadcast_ss(w + 1));
}

// x + y and x - y as FMAs with 'one' = 1.0 (hidden from clang behind an empty asm, or it folds them back into adds).
// Adds only issue on p1/p5, which shuffle-heavy loops (the cross-lane pass) saturate, while p0 does FMAs only.
FI cv_x86 cv_add1(cv_x86 a, cv_x86 b, __m256 one)
{
    return (cv_x86){_mm256_fmadd_ps(b.r, one, a.r), _mm256_fmadd_ps(b.i, one, a.i)};
}
FI cv_x86 cv_sub1(cv_x86 a, cv_x86 b, __m256 one)
{
    return (cv_x86){_mm256_fnmadd_ps(b.r, one, a.r), _mm256_fnmadd_ps(b.i, one, a.i)};
}

// In-place 4-point DFT, natural order in and out. Forward uses W_4 = -i, inverse its conjugate. The first layer
// goes through FMAs (see cv_add1) when 'one' is non-null.
FI void bfly4_avx2(cv_x86* a, cv_x86* b, cv_x86* c, cv_x86* d, int inv, const __m256* one)
{
    cv_x86 t0 = one ? cv_add1(*a, *c, *one) : cv_add_x86(*a, *c), t1 = one ? cv_sub1(*a, *c, *one) : cv_sub_x86(*a, *c);
    cv_x86 t2 = one ? cv_add1(*b, *d, *one) : cv_add_x86(*b, *d), t3 = one ? cv_sub1(*b, *d, *one) : cv_sub_x86(*b, *d);
    cv_x86 mi = {_mm256_add_ps(t1.r, t3.i), _mm256_sub_ps(t1.i, t3.r)}; // t1 - i*t3
    cv_x86 pi = {_mm256_sub_ps(t1.r, t3.i), _mm256_add_ps(t1.i, t3.r)}; // t1 + i*t3
    *a        = cv_add_x86(t0, t2);
    *c        = cv_sub_x86(t0, t2);
    *b        = inv ? pi : mi;
    *d        = inv ? mi : pi;
}

// In-place 8-point DFT over x[0..7], natural order in and out. 'one': see bfly4_avx2
FI void bfly8_avx2(cv_x86* x, int inv, const __m256* one)
{
    bfly4_avx2(&x[0], &x[2], &x[4], &x[6], inv, one); // E0..E3
    bfly4_avx2(&x[1], &x[3], &x[5], &x[7], inv, one); // O0..O3
    __m256 h  = _mm256_set1_ps(SQRT1_2_F);
    cv_x86 e0 = x[0], e1 = x[2], e2 = x[4], e3 = x[6], o0 = x[1], o1 = x[3], o2 = x[5], o3 = x[7];
    __m256 s1 = _mm256_add_ps(o1.r, o1.i), d1 = _mm256_sub_ps(o1.i, o1.r);
    __m256 s3 = _mm256_add_ps(o3.r, o3.i), d3 = _mm256_sub_ps(o3.i, o3.r);
    // Z_q = E_q + W_8^q O_q, Z_q+4 = E_q - W_8^q O_q, with W_8^q folded into the adds
    x[0] = cv_add_x86(e0, o0);
    x[4] = cv_sub_x86(e0, o0);
    if (!inv)
    {
        // W O1 = (s1, d1)h, W^2 O2 = (o2.i, -o2.r), W^3 O3 = (d3, -s3)h
        x[1] = (cv_x86){_mm256_fmadd_ps(s1, h, e1.r), _mm256_fmadd_ps(d1, h, e1.i)};
        x[5] = (cv_x86){_mm256_fnmadd_ps(s1, h, e1.r), _mm256_fnmadd_ps(d1, h, e1.i)};
        x[2] = (cv_x86){_mm256_add_ps(e2.r, o2.i), _mm256_sub_ps(e2.i, o2.r)};
        x[6] = (cv_x86){_mm256_sub_ps(e2.r, o2.i), _mm256_add_ps(e2.i, o2.r)};
        x[3] = (cv_x86){_mm256_fmadd_ps(d3, h, e3.r), _mm256_fnmadd_ps(s3, h, e3.i)};
        x[7] = (cv_x86){_mm256_fnmadd_ps(d3, h, e3.r), _mm256_fmadd_ps(s3, h, e3.i)};
    }
    else
    {
        // W^-1 O1 = (-d1, s1)h, W^-2 O2 = (-o2.i, o2.r), W^-3 O3 = (-s3, -d3)h
        x[1] = (cv_x86){_mm256_fnmadd_ps(d1, h, e1.r), _mm256_fmadd_ps(s1, h, e1.i)};
        x[5] = (cv_x86){_mm256_fmadd_ps(d1, h, e1.r), _mm256_fnmadd_ps(s1, h, e1.i)};
        x[2] = (cv_x86){_mm256_sub_ps(e2.r, o2.i), _mm256_add_ps(e2.i, o2.r)};
        x[6] = (cv_x86){_mm256_add_ps(e2.r, o2.i), _mm256_sub_ps(e2.i, o2.r)};
        x[3] = (cv_x86){_mm256_fnmadd_ps(s3, h, e3.r), _mm256_fnmadd_ps(d3, h, e3.i)};
        x[7] = (cv_x86){_mm256_fmadd_ps(s3, h, e3.r), _mm256_fmadd_ps(d3, h, e3.i)};
    }
}

// b[off[i] + l] = v[l] lane i, the inverse of gather_transpose8. Clobbers v.
FI void transpose_scatter8_avx2(__m256* v, float* b, const int32_t* off)
{
    transpose4x2(v);
    transpose4x2(v + 4);
    for (int i = 0; i < 4; i++)
    {
        float *p = b + off[i], *q = b + off[i + 4];
        _mm_store_ps(p, _mm256_castps256_ps128(v[i]));
        _mm_store_ps(q, _mm256_extractf128_ps(v[i], 1));
        _mm_store_ps(p + 4, _mm256_castps256_ps128(v[i + 4]));
        _mm_store_ps(q + 4, _mm256_extractf128_ps(v[i + 4], 1));
    }
}

// 8 interleaved complex values at p from (re, im), in natural order or reversed
FI void store_il8_avx2(float* p, __m256 re, __m256 im)
{
    __m256 lo = _mm256_unpacklo_ps(re, im), hi = _mm256_unpackhi_ps(re, im); // [0 1 | 4 5], [2 3 | 6 7]
    _mm_storeu_ps(p, _mm256_castps256_ps128(lo));
    _mm_storeu_ps(p + 4, _mm256_castps256_ps128(hi));
    _mm_storeu_ps(p + 8, _mm256_extractf128_ps(lo, 1));
    _mm_storeu_ps(p + 12, _mm256_extractf128_ps(hi, 1));
}
FI void store_il8_rev_avx2(float* p, __m256 re, __m256 im)
{
    re        = _mm256_permute_ps(re, 0x1B);
    im        = _mm256_permute_ps(im, 0x1B);
    __m256 lo = _mm256_unpacklo_ps(re, im), hi = _mm256_unpackhi_ps(re, im); // [3 2 | 7 6], [1 0 | 5 4]
    _mm_store_ps(p, _mm256_extractf128_ps(lo, 1));
    _mm_store_ps(p + 4, _mm256_extractf128_ps(hi, 1));
    _mm_store_ps(p + 8, _mm256_castps256_ps128(lo));
    _mm_store_ps(p + 12, _mm256_castps256_ps128(hi));
}

// In-place R-point DFT over x[0..R-1], natural order in and out
FI void bfly_avx2(cv_x86* x, int R, int inv)
{
    if (R == 2)
    {
        cv_x86 t = x[0];
        x[0]     = cv_add_x86(t, x[1]);
        x[1]     = cv_sub_x86(t, x[1]);
    }
    else if (R == 4)
        bfly4_avx2(&x[0], &x[1], &x[2], &x[3], inv, 0);
    else
        bfly8_avx2(x, inv, 0);
}

// One forward lane pass (DIF) of radix R and span s: butterflies, then W_Rs^(m*j) from tw on output m when s > 1,
// or the cross-lane twiddle vectors xt (s == 1, when non-null). Reads interleaved input when 'il'. R, s == 1 and
// xt == null are compile-time constants at each call. Offsets are pointer-sized and the twiddles walk a pointer:
// int index math here cost ~20 integer ops per butterfly, on the same ports as the FP work.
FI void lane_pass_fwd_avx2(const float* src, int il, float* dst, int L, int R, int s, const float* tw, const float* xt)
{
    ptrdiff_t ss = 16 * (ptrdiff_t)s;
    for (ptrdiff_t o0 = 0; o0 < 16 * (ptrdiff_t)L; o0 += R * ss)
    {
        const float* w = tw;
        for (ptrdiff_t o = o0; o < o0 + ss; o += 16, w += 2 * (R - 1))
        {
            cv_x86 x[8];
            for (int m = 0; m < R; m++)
                x[m] = il ? cv_ld_il(src + o + m * ss) : cv_ld_x86(src + o + m * ss);
            bfly_avx2(x, R, 0);
            _Pragma("clang loop unroll(full)") for (int m = 0; m < R; m++)
            {
                ptrdiff_t p = o + m * ss;
                if (xt)
                    x[m] = cv_mul_avx2(x[m], _mm256_load_ps(xt + p), _mm256_load_ps(xt + p + 8));
                else if (m && s > 1)
                    x[m] = cv_mul_bc_avx2(x[m], w + 2 * (m - 1));
                cv_st_x86(dst + p, x[m]);
            }
        }
    }
}

// Adjoint of lane_pass_fwd_avx2: conj twiddles on the inputs, then inverse butterflies. Writes interleaved when 'il'.
FI void lane_pass_inv_avx2(const float* src, float* dst, int il, int L, int R, int s, const float* tw, const float* xt)
{
    ptrdiff_t ss = 16 * (ptrdiff_t)s;
    for (ptrdiff_t o0 = 0; o0 < 16 * (ptrdiff_t)L; o0 += R * ss)
    {
        const float* w = tw;
        for (ptrdiff_t o = o0; o < o0 + ss; o += 16, w += 2 * (R - 1))
        {
            cv_x86 x[8];
            _Pragma("clang loop unroll(full)") for (int m = 0; m < R; m++)
            {
                ptrdiff_t p = o + m * ss;
                x[m]        = cv_ld_x86(src + p);
                if (xt)
                    x[m] = cv_mulc_avx2(x[m], _mm256_load_ps(xt + p), _mm256_load_ps(xt + p + 8));
                else if (m && s > 1)
                    x[m] = cv_mulc_bc_avx2(x[m], w + 2 * (m - 1));
            }
            bfly_avx2(x, R, 1);
            for (int m = 0; m < R; m++)
                il ? cv_st_il(dst + o + m * ss, x[m]) : cv_st_x86(dst + o + m * ss, x[m]);
        }
    }
}

// Only the last stage has span 1 (and only the cross-lane twiddles xt), and only it can be radix 2 or 8.
// Not inlined: one copy of each pass, shared by every n and stage. Halved the code for ~1-3% at 2048.
NI void
lane_stage_fwd_avx2(const float* src, int il, float* dst, int L, int radix, int s, const float* tw, const float* xt)
{
    if (s > 1)
        lane_pass_fwd_avx2(src, il, dst, L, 4, s, tw, 0);
    else if (radix == 2)
        lane_pass_fwd_avx2(src, il, dst, L, 2, 1, 0, xt);
    else if (radix == 4)
        lane_pass_fwd_avx2(src, il, dst, L, 4, 1, 0, xt);
    else
        lane_pass_fwd_avx2(src, il, dst, L, 8, 1, 0, xt);
}

NI void
lane_stage_inv_avx2(const float* src, float* dst, int il, int L, int radix, int s, const float* tw, const float* xt)
{
    if (s > 1)
        lane_pass_inv_avx2(src, dst, il, L, 4, s, tw, 0);
    else if (radix == 2)
        lane_pass_inv_avx2(src, dst, il, L, 2, 1, 0, xt);
    else if (radix == 4)
        lane_pass_inv_avx2(src, dst, il, L, 4, 1, 0, xt);
    else
        lane_pass_inv_avx2(src, dst, il, L, 8, 1, 0, xt);
}

// Adds row 'r' of m times s[r] to a[c0..c1-1]
FI void dense_row_avx2(const float* m, const float* s, int r, int c0, int c1, __m256* a)
{
    __m256 x = _mm256_broadcast_ss(s + r);
    for (int c = c0; c < c1; c++)
        a[c] = _mm256_fmadd_ps(x, _mm256_load_ps(m + 32 * r + 8 * c), a[c]);
}

// N = 32 as a matrix product, in registers. v = sum_r s[r] * m[32r .. 32r+31], where the forward and inverse are
// split into even and odd parts so each input feeds only 2 or 3 of the 4 output vectors (81 FMAs instead of 128):
//   forward: s = x0, x16, then u_t = x_t + x_(32-t), v_t = x_t - x_(32-t) for t = 1..15.
//            v = Re X[0..15], then Re X[16] (in the free Im X[0] slot), Im X[1..15]: packed order once interleaved.
//   inverse: s = the packed input (X0, X16, Re X[k], Im X[k]).
//            v = A[0..15], then A[16], B[1..15]: x_t = A_t + B_t, x_(32-t) = A_t - B_t (even/odd parts).
// Rows 0, 1 and 2p only feed v[0..2] (lane 0 of v[2]), rows 2p+1 only v[2..3]. Two accumulator sets for overlap.
FI void dense32_avx2(const float* m, const float* s, __m256* v)
{
    __m256 a[2][4];
    for (int c = 0; c < 4; c++)
        a[0][c] = a[1][c] = _mm256_setzero_ps();
    for (int p = 1; p < 15; p += 2)
        for (int g = 0; g < 2; g++)
        {
            dense_row_avx2(m, s, 2 * (p + g), 0, 3, a[g]);
            dense_row_avx2(m, s, 2 * (p + g) + 1, 2, 4, a[g]);
        }
    dense_row_avx2(m, s, 30, 0, 3, a[0]);
    dense_row_avx2(m, s, 31, 2, 4, a[0]);
    dense_row_avx2(m, s, 0, 0, 3, a[1]);
    dense_row_avx2(m, s, 1, 0, 3, a[1]);
    for (int c = 0; c < 4; c++)
        v[c] = _mm256_add_ps(a[0][c], a[1][c]);
}

XFFT_DEF void xfft_forward_avx2(void* p, const float* in, float* out)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(p));
    XFFT_ASSERT(XFFT_IS_ALIGNED(in));
    XFFT_ASSERT(XFFT_IS_ALIGNED(out));
    const Plan_x86* plan = p;
    if (plan->n == 32)
    {
        // s = x0, x16, u1, v1, ..., u15, v15 (and u16, v16, unused): store_il8_avx2 does the u/v interleave
        _Alignas(32) float s[34];
        __m256             lo = _mm256_loadu_ps(in + 1), hi = _mm256_loadu_ps(in + 9);
        __m256             rlo = load_rev(in + 24), rhi = load_rev(in + 16), v[4];
        store_il8_avx2(s + 2, _mm256_add_ps(lo, rlo), _mm256_sub_ps(lo, rlo));
        store_il8_avx2(s + 18, _mm256_add_ps(hi, rhi), _mm256_sub_ps(hi, rhi));
        s[0] = in[0];
        s[1] = in[16];
        dense32_avx2(plan->mat, s, v);
        store_il8_avx2(out, v[0], v[2]);
        store_il8_avx2(out + 16, v[1], v[3]);
        return;
    }

    // N >= 64, runtime n. Cross-lane groups are always 8 bins wide. At N = 64 (L = 4) only lanes 0..3 hold bins:
    // lanes 4..7 gather the junk vector after plan->work, and since Z rows are only 4 floats apart, each row store also
    // writes 4 junk floats over the start of the next row, which that row's store (always later, ZROW_ST) overwrites.
    // The last one lands in the 8 floats after zbuf. The junk vector is zeroed first, so those are always 0: the N = 64
    // inverse reads them into its junk lanes, and nonzero junk could otherwise grow from call to call into inf / NaN.
    // A masked path for N = 64 instead costs 2-5% at 512-1024 in a shared copy, or a second copy (NOTES.md it20).
    int n = plan->n, M = n / 2, L = n / 16;
    // Lane FFT workspace: out, which the split overwrites last, to keep the cache footprint down. L < 8 needs the
    // junk vector after plan->work.
    float* work = L < 8 ? plan->work : out;
    float* zre  = plan->zbuf;
    float* zim  = plan->zbuf + M;
    _mm256_store_ps(plan->work + 16 * L, _mm256_setzero_ps());
    _mm256_store_ps(plan->work + 16 * L + 8, _mm256_setzero_ps());

    {
        // 1. Lane FFTs: interleaved input -> work, digit-reversed
        int          radix[MAX_STAGES], span[MAX_STAGES], S = lane_stages_x86(L, radix, span);
        const float* tw = plan->stw;
        // The last stage (span 1) also applies the cross-lane twiddles
        lane_stage_fwd_avx2(in, 1, work, L, radix[0], span[0], tw, plan->xtw);
        tw += stage_tw(radix[0], span[0]);
        for (int t = 1; t < S; t++)
        {
            lane_stage_fwd_avx2(work, 0, work, L, radix[t], span[t], tw, plan->xtw);
            tw += stage_tw(radix[t], span[t]);
        }
    }
    {
        // 2. Cross-lane: work (already twiddled) -> Z in natural order
        __m256    one = _mm256_set1_ps(1.0f);
        ptrdiff_t l1 = 4 * (ptrdiff_t)L, l3 = 3 * l1; // Z row stride in bytes, and 3x (ZROW_ST)
        int32_t   off[8];
        __asm__("" : "+x"(one));
        cross_offsets(plan, off);
        for (int k0 = 0; k0 < L; k0 += 8)
        {
            const float* b = work + plan->rev[k0];
            __m256       vr[8], vi[8];
            gather_transpose8(b, off, vr);
            gather_transpose8(b + 8, off, vi);
            // bfly8_avx2's first layer as FMAs: this loop is short of p1/p5 (adds + vshufps), and with plain adds clang
            // also fuses the transpose's last vshufps pair into vhaddps/vhsubps (3 uops each, 2 on p5)
            cv_x86 u[8];
            for (int r = 0; r < 8; r++)
                u[r] = (cv_x86){vr[LANE_SUB[r]], vi[LANE_SUB[r]]};
            bfly8_avx2(u, 0, &one);
            // Rows in ascending order (see above)
            char *b0 = (char*)(zre + k0), *b2 = b0 + 2 * l1, *b7 = b0 + 7 * l1;
            ZROW_ST0(u[0].r, b0);
            ZROW_ST(u[1].r, b0, l1, 1);
            ZROW_ST(u[2].r, b0, l1, 2);
            ZROW_ST(u[3].r, b0, l3, 1);
            ZROW_ST(u[4].r, b0, l1, 4);
            ZROW_ST(u[5].r, b2, l3, 1);
            ZROW_ST(u[6].r, b0, l3, 2);
            ZROW_ST0(u[7].r, b7);
            ZROW_ST(u[0].i, b0, l1, 8);
            ZROW_ST(u[1].i, b7, l1, 2);
            ZROW_ST(u[2].i, b2, l1, 8);
            ZROW_ST(u[3].i, b7, l1, 4);
            ZROW_ST(u[4].i, b0, l3, 4);
            ZROW_ST(u[5].i, b7, l3, 2);
            ZROW_ST(u[6].i, b2, l3, 4);
            ZROW_ST(u[7].i, b7, l1, 8);
        }
    }
    {
        // 3. Real split, 8 bins k = k0+1..k0+8 and their mirrors M-k per iteration:
        // X[k] = E + W_N^k O, X[M-k] = conj(E - W_N^k O), E = (Z[k] + conj Z[M-k])/2, O = (Z[k] - conj Z[M-k])/2i
        const float* twr  = plan->rtw;
        const float* twi  = plan->rtw + M / 2;
        const __m256 half = _mm256_set1_ps(0.5f);
        out[0]            = zre[0] + zim[0];
        out[1]            = zre[0] - zim[0];
        for (int k0 = 0; k0 < M / 2; k0 += 8)
        {
            __m256 ar = _mm256_loadu_ps(zre + k0 + 1), ai = _mm256_loadu_ps(zim + k0 + 1);
            __m256 br = load_rev(zre + M - k0 - 8), bi = load_rev(zim + M - k0 - 8);
            __m256 sr = _mm256_add_ps(ar, br), dr = _mm256_sub_ps(ar, br);
            __m256 si = _mm256_add_ps(ai, bi), di = _mm256_sub_ps(ai, bi);
            __m256 hr = _mm256_load_ps(twr + k0), hi = _mm256_load_ps(twi + k0);
            __m256 tr = _mm256_fmadd_ps(hr, si, _mm256_mul_ps(hi, dr));
            __m256 ti = _mm256_fmsub_ps(hi, si, _mm256_mul_ps(hr, dr));
            __m256 xr = _mm256_fmadd_ps(half, sr, tr), xi = _mm256_fmadd_ps(half, di, ti);
            __m256 yr = _mm256_fmsub_ps(half, sr, tr), yi = _mm256_fnmadd_ps(half, di, ti);
            store_il8_avx2(out + 2 * k0 + 2, xr, xi);
            store_il8_rev_avx2(out + 2 * (M - k0 - 8), yr, yi);
        }
    }
}

XFFT_DEF void xfft_inverse_avx2(void* p, const float* in, float* out)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(p));
    XFFT_ASSERT(XFFT_IS_ALIGNED(in));
    XFFT_ASSERT(XFFT_IS_ALIGNED(out));
    const Plan_x86* plan = p;
    if (plan->n == 32)
    {
        __m256 v[4], rev = _mm256_setr_epi32(7, 6, 5, 4, 3, 2, 1, 0);
        dense32_avx2(plan->mat + 32 * 32, in, v);
        __m256 b0 = _mm256_blend_ps(v[2], _mm256_setzero_ps(), 1); // B[0..7], B[0] = 0 (lane 0 holds A[16])
        _mm256_store_ps(out, _mm256_add_ps(v[0], b0));
        _mm256_store_ps(out + 8, _mm256_add_ps(v[1], v[3]));
        out[16] = _mm256_cvtss_f32(v[2]);
        // x[32-t] = A_t - B_t: t = 15..8 at out[17..24], t = 7..1 at out[25..31] (t = 0 would land past the end)
        _mm256_storeu_ps(out + 17, _mm256_permutevar8x32_ps(_mm256_sub_ps(v[1], v[3]), rev));
        _mm256_maskstore_ps(
            out + 25,
            _mm256_setr_epi32(-1, -1, -1, -1, -1, -1, -1, 0),
            _mm256_permutevar8x32_ps(_mm256_sub_ps(v[0], b0), rev));
        return;
    }

    // N >= 64, runtime n. At N = 64, the junk lanes 4..7 of each Z row load read the next row (the last one the zero
    // floats after zbuf), and the scatter sends them to the junk vector (see xfft_forward_avx2).
    int n = plan->n, M = n / 2, L = n / 16;
    // Lane FFT workspace: out, as in xfft_forward_avx2. The last lane stage then runs in place, which is safe since
    // each butterfly writes (interleaved) exactly the vectors it read.
    float* work = L < 8 ? plan->work : out;
    float* zre  = plan->zbuf;
    float* zim  = plan->zbuf + M;

    {
        // 3. Merge, 8 bins k = k0+1..k0+8 and their mirrors per iteration:
        // A = X[k] + conj X[M-k], B = (X[k] - conj X[M-k]) conj(W_N^k), Z[k] = A + iB, Z[M-k] = conj A + i conj B
        const float* twr = plan->rtw;
        const float* twi = plan->rtw + M / 2;
        zre[0]           = in[0] + in[1];
        zim[0]           = in[0] - in[1];
        for (int k0 = 0; k0 < M / 2; k0 += 8)
        {
            __m256 xr, xi, mr, mi;
            load_il8(in + 2 * k0 + 2, &xr, &xi);
            load_il8_rev(in + 2 * (M - k0 - 8), &mr, &mi);
            __m256 ar = _mm256_add_ps(xr, mr), ai = _mm256_sub_ps(xi, mi);
            __m256 dr = _mm256_sub_ps(xr, mr), di = _mm256_add_ps(xi, mi);
            // conj(W_N^k) = 2 * (hr, -hi). b = B/2, and the 2 goes into the FMAs
            __m256 hr = _mm256_load_ps(twr + k0), hi = _mm256_load_ps(twi + k0), two = _mm256_set1_ps(2.0f);
            __m256 br = _mm256_fmadd_ps(dr, hr, _mm256_mul_ps(di, hi));
            __m256 bi = _mm256_fmsub_ps(di, hr, _mm256_mul_ps(dr, hi));
            _mm256_storeu_ps(zre + k0 + 1, _mm256_fnmadd_ps(bi, two, ar));
            _mm256_storeu_ps(zim + k0 + 1, _mm256_fmadd_ps(br, two, ai));
            store_rev(zre + M - k0 - 8, _mm256_fmadd_ps(bi, two, ar));
            store_rev(zim + M - k0 - 8, _mm256_fmsub_ps(br, two, ai));
        }
    }
    {
        // 2. Inverse cross-lane: Z -> work, digit-reversed (the conj twiddles come in the first inverse lane stage)
        ptrdiff_t l1 = 4 * (ptrdiff_t)L, l3 = 3 * l1; // As in xfft_forward_avx2
        int32_t   off[8];
        cross_offsets(plan, off);
        for (int k0 = 0; k0 < L; k0 += 8)
        {
            cv_x86      u[8];
            const char *b0 = (const char*)(zre + k0), *b2 = b0 + 2 * l1, *b7 = b0 + 7 * l1;
            ZROW_LD0(u[0].r, b0);
            ZROW_LD(u[1].r, b0, l1, 1);
            ZROW_LD(u[2].r, b0, l1, 2);
            ZROW_LD(u[3].r, b0, l3, 1);
            ZROW_LD(u[4].r, b0, l1, 4);
            ZROW_LD(u[5].r, b2, l3, 1);
            ZROW_LD(u[6].r, b0, l3, 2);
            ZROW_LD0(u[7].r, b7);
            ZROW_LD(u[0].i, b0, l1, 8);
            ZROW_LD(u[1].i, b7, l1, 2);
            ZROW_LD(u[2].i, b2, l1, 8);
            ZROW_LD(u[3].i, b7, l1, 4);
            ZROW_LD(u[4].i, b0, l3, 4);
            ZROW_LD(u[5].i, b7, l3, 2);
            ZROW_LD(u[6].i, b2, l3, 4);
            ZROW_LD(u[7].i, b7, l1, 8);
            bfly8_avx2(u, 1, 0);
            __m256 vr[8], vi[8];
            for (int r = 0; r < 8; r++)
            {
                vr[LANE_SUB[r]] = u[r].r;
                vi[LANE_SUB[r]] = u[r].i;
            }
            float* b = work + plan->rev[k0];
            transpose_scatter8_avx2(vr, b, off);
            transpose_scatter8_avx2(vi, b + 8, off);
        }
    }
    {
        // 1. Inverse lane FFTs (DIT): work -> interleaved output, natural order
        int radix[MAX_STAGES], span[MAX_STAGES], twoff[MAX_STAGES], S = lane_stages_x86(L, radix, span);
        for (int t = 0, off = 0; t < S; t++)
        {
            twoff[t]  = off;
            off      += stage_tw(radix[t], span[t]);
        }
        // The first of these (span 1) also applies the conj cross-lane twiddles
        for (int t = S - 1; t > 0; t--)
            lane_stage_inv_avx2(work, work, 0, L, radix[t], span[t], plan->stw + twoff[t], plan->xtw);
        lane_stage_inv_avx2(work, out, 1, L, radix[0], span[0], plan->stw, plan->xtw);
    }
}

XFFT_DEF const char* xfft_tell_avx2(void) { return "avx2"; }

#elif !defined(XFFT_NO_STUBS) // No AVX2 + FMA (or XFFT_DISABLE_AVX2): planning fails

XFFT_DEF size_t      xfft_mem_size_avx2(int n) { return 64; }
XFFT_DEF void*       xfft_init_avx2(void* mem, int n) { return 0; }
XFFT_DEF void        xfft_forward_avx2(void* p, const float* in, float* out) {}
XFFT_DEF void        xfft_inverse_avx2(void* p, const float* in, float* out) {}
XFFT_DEF const char* xfft_tell_avx2(void) { return "avx2-stub"; }

#endif // XFFT_AVX2_ENABLED

/*~*~*~*
 ~ AVX  ~
 *~*~*~*/

// xfft: AVX real FFT (no AVX2, no FMA). Ported from xfft_avx2.
//
// A real FFT of size N is a complex FFT of size M = N/2 on z[m] = x[2m] + i*x[2m+1], plus an O(N) pass to
// split the even/odd spectra apart (forward) or merge them (inverse). The complex FFT is a four-step FFT
// shaped around 8-wide vectors:
//
//   1. Lane FFTs. The M points are 8 interleaved subsequences z[8j + r], one per vector lane, so vector j
//      holds point j of every subsequence and the 8 FFTs of length L = M/8 run lane-parallel with scalar
//      (broadcast) twiddles. Radix-4 DIF stages on split re/im vectors; output is in digit-reversed order.
//   2. Cross-lane pass. For 8 bins k: gather Y_r[k] through the digit-reversal table, transpose 8x8 so each
//      vector holds one subsequence, twiddle by W_M^(r*k) and do a radix-8 DFT across subsequences.
//      Z[k + L*q] comes out as 8 consecutive bins, in natural order.
//   3. Real split, written straight into the packed ordered layout.
//
// The inverse runs the adjoint of each step in reverse order (merge, inverse cross-lane, DIT lane stages).
// N = 32 skips all of this: it's a matrix product in registers (dense32).

#if defined(XFFT_AVX_ENABLED)

XFFT_DEF size_t xfft_mem_size_avx(int n) { return layout_x86(n, 0); }
XFFT_DEF void*  xfft_init_avx(void* mem, int n) { return init_x86(mem, n); }

// a * w
FI cv_x86 cv_mul_avx(cv_x86 a, __m256 wr, __m256 wi)
{
    return (cv_x86){
        _mm256_sub_ps(_mm256_mul_ps(a.r, wr), _mm256_mul_ps(a.i, wi)),
        _mm256_add_ps(_mm256_mul_ps(a.r, wi), _mm256_mul_ps(a.i, wr))};
}
// a * conj(w)
FI cv_x86 cv_mulc_avx(cv_x86 a, __m256 wr, __m256 wi)
{
    return (cv_x86){
        _mm256_add_ps(_mm256_mul_ps(a.r, wr), _mm256_mul_ps(a.i, wi)),
        _mm256_sub_ps(_mm256_mul_ps(a.i, wr), _mm256_mul_ps(a.r, wi))};
}
FI cv_x86 cv_mul_bc_avx(cv_x86 a, const float* w)
{
    return cv_mul_avx(a, _mm256_broadcast_ss(w), _mm256_broadcast_ss(w + 1));
}
FI cv_x86 cv_mulc_bc_avx(cv_x86 a, const float* w)
{
    return cv_mulc_avx(a, _mm256_broadcast_ss(w), _mm256_broadcast_ss(w + 1));
}

// In-place 4-point DFT, natural order in and out. Forward uses W_4 = -i, inverse its conjugate.
FI void bfly4_avx(cv_x86* a, cv_x86* b, cv_x86* c, cv_x86* d, int inv)
{
    cv_x86 t0 = cv_add_x86(*a, *c), t1 = cv_sub_x86(*a, *c);
    cv_x86 t2 = cv_add_x86(*b, *d), t3 = cv_sub_x86(*b, *d);
    cv_x86 mi = {_mm256_add_ps(t1.r, t3.i), _mm256_sub_ps(t1.i, t3.r)}; // t1 - i*t3
    cv_x86 pi = {_mm256_sub_ps(t1.r, t3.i), _mm256_add_ps(t1.i, t3.r)}; // t1 + i*t3
    *a        = cv_add_x86(t0, t2);
    *c        = cv_sub_x86(t0, t2);
    *b        = inv ? pi : mi;
    *d        = inv ? mi : pi;
}

// In-place 8-point DFT over x[0..7], natural order in and out
FI void bfly8_avx(cv_x86* x, int inv)
{
    bfly4_avx(&x[0], &x[2], &x[4], &x[6], inv); // E0..E3
    bfly4_avx(&x[1], &x[3], &x[5], &x[7], inv); // O0..O3
    __m256 h  = _mm256_set1_ps(SQRT1_2_F);
    cv_x86 e0 = x[0], e1 = x[2], e2 = x[4], e3 = x[6], o0 = x[1], o1 = x[3], o2 = x[5], o3 = x[7];
    // s, d already scaled by h = sqrt(1/2)
    __m256 s1 = _mm256_mul_ps(_mm256_add_ps(o1.r, o1.i), h), d1 = _mm256_mul_ps(_mm256_sub_ps(o1.i, o1.r), h);
    __m256 s3 = _mm256_mul_ps(_mm256_add_ps(o3.r, o3.i), h), d3 = _mm256_mul_ps(_mm256_sub_ps(o3.i, o3.r), h);
    // Z_q = E_q + W_8^q O_q, Z_q+4 = E_q - W_8^q O_q, with W_8^q folded into the adds
    x[0] = cv_add_x86(e0, o0);
    x[4] = cv_sub_x86(e0, o0);
    if (!inv)
    {
        // W O1 = (s1, d1)h, W^2 O2 = (o2.i, -o2.r), W^3 O3 = (d3, -s3)h
        x[1] = (cv_x86){_mm256_add_ps(e1.r, s1), _mm256_add_ps(e1.i, d1)};
        x[5] = (cv_x86){_mm256_sub_ps(e1.r, s1), _mm256_sub_ps(e1.i, d1)};
        x[2] = (cv_x86){_mm256_add_ps(e2.r, o2.i), _mm256_sub_ps(e2.i, o2.r)};
        x[6] = (cv_x86){_mm256_sub_ps(e2.r, o2.i), _mm256_add_ps(e2.i, o2.r)};
        x[3] = (cv_x86){_mm256_add_ps(e3.r, d3), _mm256_sub_ps(e3.i, s3)};
        x[7] = (cv_x86){_mm256_sub_ps(e3.r, d3), _mm256_add_ps(e3.i, s3)};
    }
    else
    {
        // W^-1 O1 = (-d1, s1)h, W^-2 O2 = (-o2.i, o2.r), W^-3 O3 = (-s3, -d3)h
        x[1] = (cv_x86){_mm256_sub_ps(e1.r, d1), _mm256_add_ps(e1.i, s1)};
        x[5] = (cv_x86){_mm256_add_ps(e1.r, d1), _mm256_sub_ps(e1.i, s1)};
        x[2] = (cv_x86){_mm256_sub_ps(e2.r, o2.i), _mm256_add_ps(e2.i, o2.r)};
        x[6] = (cv_x86){_mm256_add_ps(e2.r, o2.i), _mm256_sub_ps(e2.i, o2.r)};
        x[3] = (cv_x86){_mm256_sub_ps(e3.r, s3), _mm256_sub_ps(e3.i, d3)};
        x[7] = (cv_x86){_mm256_add_ps(e3.r, s3), _mm256_add_ps(e3.i, d3)};
    }
}

// b[off[i] + l] = v[l] lane i for i < 8, the inverse of gather_transpose8. Clobbers v.
// 'whole': 256-bit stores (2 more shuffles each), for when the next pass reads them back right away (L <= 16,
// 1-2 groups): a 256-bit load from two 128-bit stores can't be store-forwarded and waits for them to reach L1.
// From L = 32 the shuffles cost more than that saves (it4).
FI void transpose_scatter8_avx(__m256* v, float* b, const int32_t* off, int whole)
{
    transpose4x2(v);
    transpose4x2(v + 4);
    for (int i = 0; i < 4; i++)
    {
        if (whole)
        {
            _mm256_store_ps(b + off[i], _mm256_insertf128_ps(v[i], _mm256_castps256_ps128(v[i + 4]), 1));
            _mm256_store_ps(b + off[i + 4], _mm256_permute2f128_ps(v[i], v[i + 4], 0x31));
            continue;
        }
        float *p = b + off[i], *q = b + off[i + 4];
        _mm_store_ps(p, _mm256_castps256_ps128(v[i]));
        _mm_store_ps(q, _mm256_extractf128_ps(v[i], 1));
        _mm_store_ps(p + 4, _mm256_castps256_ps128(v[i + 4]));
        _mm_store_ps(q + 4, _mm256_extractf128_ps(v[i + 4], 1));
    }
}

// p, hidden from clang. Stores through it aren't merged with stores through p: clang would merge two 128-bit stores
// into one 256-bit store with a vinsertf128 / vperm2f128 (p5) in front, and 128-bit stores cost the same anyway
// (Sandy Bridge splits 256-bit stores in two).
FI float* opaque(float* p)
{
    __asm__("" : "+r"(p));
    return p;
}

// 8 interleaved complex values at p from (re, im), in natural order or reversed
FI void store_il8_avx(float* p, __m256 re, __m256 im)
{
    __m256 lo = _mm256_unpacklo_ps(re, im), hi = _mm256_unpackhi_ps(re, im); // [0 1 | 4 5], [2 3 | 6 7]
    float* q = opaque(p);
    _mm_storeu_ps(p, _mm256_castps256_ps128(lo));
    _mm_storeu_ps(q + 4, _mm256_castps256_ps128(hi));
    _mm_storeu_ps(p + 8, _mm256_extractf128_ps(lo, 1));
    _mm_storeu_ps(q + 12, _mm256_extractf128_ps(hi, 1));
}
FI void store_il8_rev_avx(float* p, __m256 re, __m256 im)
{
    re        = _mm256_permute_ps(re, 0x1B);
    im        = _mm256_permute_ps(im, 0x1B);
    __m256 lo = _mm256_unpacklo_ps(re, im), hi = _mm256_unpackhi_ps(re, im); // [3 2 | 7 6], [1 0 | 5 4]
    float* q = opaque(p);
    _mm_store_ps(p, _mm256_extractf128_ps(lo, 1));
    _mm_store_ps(q + 4, _mm256_extractf128_ps(hi, 1));
    _mm_store_ps(p + 8, _mm256_castps256_ps128(lo));
    _mm_store_ps(q + 12, _mm256_castps256_ps128(hi));
}

// In-place R-point DFT over x[0..R-1], natural order in and out
FI void bfly_avx(cv_x86* x, int R, int inv)
{
    if (R == 2)
    {
        cv_x86 t = x[0];
        x[0]     = cv_add_x86(t, x[1]);
        x[1]     = cv_sub_x86(t, x[1]);
    }
    else if (R == 4)
        bfly4_avx(&x[0], &x[1], &x[2], &x[3], inv);
    else
        bfly8_avx(x, inv);
}

// One forward lane pass (DIF) of radix R and span s: butterflies, then W_Rs^(m*j) from tw on output m when s > 1,
// or the cross-lane twiddle vectors xt when s == 1 (the last stage). Reads interleaved input when 'il'. R and s == 1
// are compile-time constants at each call (a runtime 'xt != null' test made clang emit both bodies in the radix-8
// loop and branch per butterfly). Offsets are pointer-sized and the twiddles walk a pointer:
// int index math here cost ~20 integer ops per butterfly, on the same ports as the FP work.
FI void lane_pass_fwd_avx(const float* src, int il, float* dst, int L, int R, int s, const float* tw, const float* xt)
{
    ptrdiff_t ss = 16 * (ptrdiff_t)s;
    for (ptrdiff_t o0 = 0; o0 < 16 * (ptrdiff_t)L; o0 += R * ss)
    {
        const float* w = tw;
        for (ptrdiff_t o = o0; o < o0 + ss; o += 16, w += 2 * (R - 1))
        {
            cv_x86 x[8];
            for (int m = 0; m < R; m++)
                x[m] = il ? cv_ld_il(src + o + m * ss) : cv_ld_x86(src + o + m * ss);
            bfly_avx(x, R, 0);
            _Pragma("clang loop unroll(full)") for (int m = 0; m < R; m++)
            {
                ptrdiff_t p = o + m * ss;
                if (s == 1)
                    x[m] = cv_mul_avx(x[m], _mm256_load_ps(xt + p), _mm256_load_ps(xt + p + 8));
                else if (m && s > 1 && o != o0) // j = 0: w = 1
                    x[m] = cv_mul_bc_avx(x[m], w + 2 * (m - 1));
                cv_st_x86(dst + p, x[m]);
            }
        }
    }
}

// Adjoint of lane_pass_fwd_avx: conj twiddles on the inputs, then inverse butterflies. Writes interleaved when 'il'.
FI void lane_pass_inv_avx(const float* src, float* dst, int il, int L, int R, int s, const float* tw, const float* xt)
{
    ptrdiff_t ss = 16 * (ptrdiff_t)s;
    for (ptrdiff_t o0 = 0; o0 < 16 * (ptrdiff_t)L; o0 += R * ss)
    {
        const float* w = tw;
        for (ptrdiff_t o = o0; o < o0 + ss; o += 16, w += 2 * (R - 1))
        {
            cv_x86 x[8];
            _Pragma("clang loop unroll(full)") for (int m = 0; m < R; m++)
            {
                ptrdiff_t p = o + m * ss;
                x[m]        = cv_ld_x86(src + p);
                if (s == 1)
                    x[m] = cv_mulc_avx(x[m], _mm256_load_ps(xt + p), _mm256_load_ps(xt + p + 8));
                else if (m && s > 1 && o != o0) // j = 0: w = 1
                    x[m] = cv_mulc_bc_avx(x[m], w + 2 * (m - 1));
            }
            bfly_avx(x, R, 1);
            for (int m = 0; m < R; m++)
                il ? cv_st_il(dst + o + m * ss, x[m]) : cv_st_x86(dst + o + m * ss, x[m]);
        }
    }
}

// Only the last stage has span 1 (and only the cross-lane twiddles xt), and only it can be radix 2 or 8.
// Not inlined: one copy of each pass, shared by every n and stage. Halved the code for ~1-3% at 2048.
NI void
lane_stage_fwd_avx(const float* src, int il, float* dst, int L, int radix, int s, const float* tw, const float* xt)
{
    if (s > 1)
        lane_pass_fwd_avx(src, il, dst, L, 4, s, tw, 0);
    else if (radix == 2)
        lane_pass_fwd_avx(src, il, dst, L, 2, 1, 0, xt);
    else if (radix == 4)
        lane_pass_fwd_avx(src, il, dst, L, 4, 1, 0, xt);
    else
        lane_pass_fwd_avx(src, il, dst, L, 8, 1, 0, xt);
}

NI void
lane_stage_inv_avx(const float* src, float* dst, int il, int L, int radix, int s, const float* tw, const float* xt)
{
    if (s > 1)
        lane_pass_inv_avx(src, dst, il, L, 4, s, tw, 0);
    else if (radix == 2)
        lane_pass_inv_avx(src, dst, il, L, 2, 1, 0, xt);
    else if (radix == 4)
        lane_pass_inv_avx(src, dst, il, L, 4, 1, 0, xt);
    else
        lane_pass_inv_avx(src, dst, il, L, 8, 1, 0, xt);
}

// Adds row 'r' of m times s[r] to a[c0..c1-1]
FI void dense_row_avx(const float* m, const float* s, int r, int c0, int c1, __m256* a)
{
    __m256 x = _mm256_broadcast_ss(s + r);
    for (int c = c0; c < c1; c++)
        a[c] = _mm256_add_ps(a[c], _mm256_mul_ps(x, _mm256_load_ps(m + 32 * r + 8 * c)));
}

// N = 32 as a matrix product, in registers. v = sum_r s[r] * m[32r .. 32r+31], where the forward and inverse are
// split into even and odd parts so each input feeds only 2 or 3 of the 4 output vectors (81 FMAs instead of 128):
//   forward: s = x0, x16, then u_t = x_t + x_(32-t), v_t = x_t - x_(32-t) for t = 1..15.
//            v = Re X[0..15], then Re X[16] (in the free Im X[0] slot), Im X[1..15]: packed order once interleaved.
//   inverse: s = the packed input (X0, X16, Re X[k], Im X[k]).
//            v = A[0..15], then A[16], B[1..15]: x_t = A_t + B_t, x_(32-t) = A_t - B_t (even/odd parts).
// Rows 0, 1 and 2p only feed v[0..2] (lane 0 of v[2]), rows 2p+1 only v[2..3]. Two accumulator sets for overlap.
FI void dense32_avx(const float* m, const float* s, __m256* v)
{
    __m256 a[2][4];
    for (int c = 0; c < 4; c++)
        a[0][c] = a[1][c] = _mm256_setzero_ps();
    for (int p = 1; p < 15; p += 2)
        for (int g = 0; g < 2; g++)
        {
            dense_row_avx(m, s, 2 * (p + g), 0, 3, a[g]);
            dense_row_avx(m, s, 2 * (p + g) + 1, 2, 4, a[g]);
        }
    dense_row_avx(m, s, 30, 0, 3, a[0]);
    dense_row_avx(m, s, 31, 2, 4, a[0]);
    dense_row_avx(m, s, 0, 0, 3, a[1]);
    dense_row_avx(m, s, 1, 0, 3, a[1]);
    for (int c = 0; c < 4; c++)
        v[c] = _mm256_add_ps(a[0][c], a[1][c]);
}

XFFT_DEF void xfft_forward_avx(void* p, const float* in, float* out)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(p));
    XFFT_ASSERT(XFFT_IS_ALIGNED(in));
    XFFT_ASSERT(XFFT_IS_ALIGNED(out));
    const Plan_x86* plan = p;
    if (plan->n == 32)
    {
        // s = x0, x16, u1, v1, ..., u15, v15 (and u16, v16, unused): store_il8_avx does the u/v interleave
        _Alignas(32) float s[34];
        __m256             lo = _mm256_loadu_ps(in + 1), hi = _mm256_loadu_ps(in + 9);
        __m256             rlo = load_rev(in + 24), rhi = load_rev(in + 16), v[4];
        store_il8_avx(s + 2, _mm256_add_ps(lo, rlo), _mm256_sub_ps(lo, rlo));
        store_il8_avx(s + 18, _mm256_add_ps(hi, rhi), _mm256_sub_ps(hi, rhi));
        s[0] = in[0];
        s[1] = in[16];
        dense32_avx(plan->mat, s, v);
        store_il8_avx(out, v[0], v[2]);
        store_il8_avx(out + 16, v[1], v[3]);
        return;
    }

    // N >= 64, runtime n. Cross-lane groups are always 8 bins wide. At N = 64 (L = 4) only lanes 0..3 hold bins:
    // lanes 4..7 gather the junk vector after plan->work, and since Z rows are only 4 floats apart, each row store also
    // writes 4 junk floats over the start of the next row, which that row's store (always later, ZROW_ST) overwrites.
    // The last one lands in the 8 floats after zbuf. The junk vector is zeroed first, so those are always 0: the N = 64
    // inverse reads them into its junk lanes, and nonzero junk could otherwise grow from call to call into inf / NaN.
    int n = plan->n, M = n / 2, L = n / 16;
    // Lane FFT workspace: out, which the split overwrites last, to keep the cache footprint down. L < 8 needs the
    // junk vector after plan->work.
    float* work = L < 8 ? plan->work : out;
    float* zre  = plan->zbuf;
    float* zim  = plan->zbuf + M;
    _mm256_store_ps(plan->work + 16 * L, _mm256_setzero_ps());
    _mm256_store_ps(plan->work + 16 * L + 8, _mm256_setzero_ps());

    {
        // 1. Lane FFTs: interleaved input -> work, digit-reversed
        int          radix[MAX_STAGES], span[MAX_STAGES], S = lane_stages_x86(L, radix, span);
        const float* tw = plan->stw;
        // The last stage (span 1) also applies the cross-lane twiddles
        lane_stage_fwd_avx(in, 1, work, L, radix[0], span[0], tw, plan->xtw);
        tw += stage_tw(radix[0], span[0]);
        for (int t = 1; t < S; t++)
        {
            lane_stage_fwd_avx(work, 0, work, L, radix[t], span[t], tw, plan->xtw);
            tw += stage_tw(radix[t], span[t]);
        }
    }
    {
        // 2. Cross-lane: work (already twiddled) -> Z in natural order
        ptrdiff_t l1 = 4 * (ptrdiff_t)L, l3 = 3 * l1; // Z row stride in bytes, and 3x (ZROW_ST)
        int32_t   off[8];
        cross_offsets(plan, off);
        for (int k0 = 0; k0 < L; k0 += 8)
        {
            const float* b = work + plan->rev[k0];
            __m256       vr[8], vi[8];
            gather_transpose8(b, off, vr);
            gather_transpose8(b + 8, off, vi);
            // The empty asm stops clang fusing the transpose's last vshufps pair with bfly8_avx's first adds into
            // vhaddps/vhsubps (3 uops each, 2 of them shuffles)
            cv_x86 u[8];
            for (int r = 0; r < 8; r++)
            {
                u[r] = (cv_x86){vr[LANE_SUB[r]], vi[LANE_SUB[r]]};
                __asm__("" : "+x"(u[r].r), "+x"(u[r].i));
            }
            bfly8_avx(u, 0);
            // Rows in ascending order (see above)
            char *b0 = (char*)(zre + k0), *b2 = b0 + 2 * l1, *b7 = b0 + 7 * l1;
            ZROW_ST0(u[0].r, b0);
            ZROW_ST(u[1].r, b0, l1, 1);
            ZROW_ST(u[2].r, b0, l1, 2);
            ZROW_ST(u[3].r, b0, l3, 1);
            ZROW_ST(u[4].r, b0, l1, 4);
            ZROW_ST(u[5].r, b2, l3, 1);
            ZROW_ST(u[6].r, b0, l3, 2);
            ZROW_ST0(u[7].r, b7);
            ZROW_ST(u[0].i, b0, l1, 8);
            ZROW_ST(u[1].i, b7, l1, 2);
            ZROW_ST(u[2].i, b2, l1, 8);
            ZROW_ST(u[3].i, b7, l1, 4);
            ZROW_ST(u[4].i, b0, l3, 4);
            ZROW_ST(u[5].i, b7, l3, 2);
            ZROW_ST(u[6].i, b2, l3, 4);
            ZROW_ST(u[7].i, b7, l1, 8);
        }
    }
    {
        // 3. Real split, 8 bins k = k0+1..k0+8 and their mirrors M-k per iteration:
        // X[k] = E + W_N^k O, X[M-k] = conj(E - W_N^k O), E = (Z[k] + conj Z[M-k])/2, O = (Z[k] - conj Z[M-k])/2i
        const float* twr  = plan->rtw;
        const float* twi  = plan->rtw + M / 2;
        const __m256 half = _mm256_set1_ps(0.5f);
        out[0]            = zre[0] + zim[0];
        out[1]            = zre[0] - zim[0];
        for (ptrdiff_t k0 = 0; k0 < M / 2; k0 += 8)
        {
            __m256 ar = _mm256_loadu_ps(zre + k0 + 1), ai = _mm256_loadu_ps(zim + k0 + 1);
            __m256 br = load_rev(zre + M - k0 - 8), bi = load_rev(zim + M - k0 - 8);
            __m256 sr = _mm256_add_ps(ar, br), dr = _mm256_sub_ps(ar, br);
            __m256 si = _mm256_add_ps(ai, bi), di = _mm256_sub_ps(ai, bi);
            __m256 hr = _mm256_load_ps(twr + k0), hi = _mm256_load_ps(twi + k0);
            __m256 tr = _mm256_add_ps(_mm256_mul_ps(hr, si), _mm256_mul_ps(hi, dr));
            __m256 ti = _mm256_sub_ps(_mm256_mul_ps(hi, si), _mm256_mul_ps(hr, dr));
            sr        = _mm256_mul_ps(half, sr);
            di        = _mm256_mul_ps(half, di);
            __m256 xr = _mm256_add_ps(sr, tr), xi = _mm256_add_ps(di, ti);
            __m256 yr = _mm256_sub_ps(sr, tr), yi = _mm256_sub_ps(ti, di);
            store_il8_avx(out + 2 * k0 + 2, xr, xi);
            store_il8_rev_avx(out + 2 * (M - k0 - 8), yr, yi);
        }
    }
}

XFFT_DEF void xfft_inverse_avx(void* p, const float* in, float* out)
{
    XFFT_ASSERT(XFFT_IS_ALIGNED(p));
    XFFT_ASSERT(XFFT_IS_ALIGNED(in));
    XFFT_ASSERT(XFFT_IS_ALIGNED(out));
    const Plan_x86* plan = p;
    if (plan->n == 32)
    {
        __m256 v[4];
        dense32_avx(plan->mat + 32 * 32, in, v);
        __m256 b0 = _mm256_blend_ps(v[2], _mm256_setzero_ps(), 1); // B[0..7], B[0] = 0 (lane 0 holds A[16])
        _mm256_store_ps(out, _mm256_add_ps(v[0], b0));
        _mm256_store_ps(out + 8, _mm256_add_ps(v[1], v[3]));
        out[16] = _mm256_cvtss_f32(v[2]);
        // x[32-t] = A_t - B_t: t = 15..8 at out[17..24], t = 7..1 at out[25..31] (t = 0 would land past the end)
        __m256 d1 = _mm256_sub_ps(v[1], v[3]), d0 = _mm256_sub_ps(v[0], b0);
        _mm256_storeu_ps(out + 17, _mm256_permute_ps(_mm256_permute2f128_ps(d1, d1, 1), 0x1B));
        _mm256_maskstore_ps(
            out + 25,
            _mm256_setr_epi32(-1, -1, -1, -1, -1, -1, -1, 0),
            _mm256_permute_ps(_mm256_permute2f128_ps(d0, d0, 1), 0x1B));
        return;
    }

    // N >= 64, runtime n. At N = 64, the junk lanes 4..7 of each Z row load read the next row (the last one the zero
    // floats after zbuf), and the scatter sends them to the junk vector (see xfft_forward_avx).
    int n = plan->n, M = n / 2, L = n / 16;
    // Lane FFT workspace: out, as in xfft_forward_avx. The last lane stage then runs in place, which is safe since
    // each butterfly writes (interleaved) exactly the vectors it read.
    float* work = L < 8 ? plan->work : out;
    float* zre  = plan->zbuf;
    float* zim  = plan->zbuf + M;

    {
        // 3. Merge, 8 bins k = k0+1..k0+8 and their mirrors per iteration:
        // A = X[k] + conj X[M-k], B = (X[k] - conj X[M-k]) conj(W_N^k), Z[k] = A + iB, Z[M-k] = conj A + i conj B
        const float* twr = plan->rtw;
        const float* twi = plan->rtw + M / 2;
        zre[0]           = in[0] + in[1];
        zim[0]           = in[0] - in[1];
        __m256 two       = _mm256_set1_ps(2.0f);
        __asm__("" : "+x"(two)); // Or clang turns h * 2 into h + h, on the add ports
        for (ptrdiff_t k0 = 0; k0 < M / 2; k0 += 8)
        {
            __m256 xr, xi, mr, mi;
            load_il8(in + 2 * k0 + 2, &xr, &xi);
            load_il8_rev(in + 2 * (M - k0 - 8), &mr, &mi);
            __m256 ar = _mm256_add_ps(xr, mr), ai = _mm256_sub_ps(xi, mi);
            __m256 dr = _mm256_sub_ps(xr, mr), di = _mm256_add_ps(xi, mi);
            // conj(W_N^k) = 2 * (hr, -hi)
            __m256 hr = _mm256_mul_ps(_mm256_load_ps(twr + k0), two), hi = _mm256_mul_ps(_mm256_load_ps(twi + k0), two);
            __m256 br = _mm256_add_ps(_mm256_mul_ps(dr, hr), _mm256_mul_ps(di, hi));
            __m256 bi = _mm256_sub_ps(_mm256_mul_ps(di, hr), _mm256_mul_ps(dr, hi));
            _mm256_storeu_ps(zre + k0 + 1, _mm256_sub_ps(ar, bi));
            _mm256_storeu_ps(zim + k0 + 1, _mm256_add_ps(ai, br));
            store_rev(zre + M - k0 - 8, _mm256_add_ps(ar, bi));
            store_rev(zim + M - k0 - 8, _mm256_sub_ps(br, ai));
        }
    }
    {
        // 2. Inverse cross-lane: Z -> work, digit-reversed (the conj twiddles come in the first inverse lane stage)
        ptrdiff_t l1 = 4 * (ptrdiff_t)L, l3 = 3 * l1; // As in xfft_forward_avx
        int32_t   off[8];
        cross_offsets(plan, off);
        for (int k0 = 0; k0 < L; k0 += 8)
        {
            cv_x86      u[8];
            const char *b0 = (const char*)(zre + k0), *b2 = b0 + 2 * l1, *b7 = b0 + 7 * l1;
            ZROW_LD0(u[0].r, b0);
            ZROW_LD(u[1].r, b0, l1, 1);
            ZROW_LD(u[2].r, b0, l1, 2);
            ZROW_LD(u[3].r, b0, l3, 1);
            ZROW_LD(u[4].r, b0, l1, 4);
            ZROW_LD(u[5].r, b2, l3, 1);
            ZROW_LD(u[6].r, b0, l3, 2);
            ZROW_LD0(u[7].r, b7);
            ZROW_LD(u[0].i, b0, l1, 8);
            ZROW_LD(u[1].i, b7, l1, 2);
            ZROW_LD(u[2].i, b2, l1, 8);
            ZROW_LD(u[3].i, b7, l1, 4);
            ZROW_LD(u[4].i, b0, l3, 4);
            ZROW_LD(u[5].i, b7, l3, 2);
            ZROW_LD(u[6].i, b2, l3, 4);
            ZROW_LD(u[7].i, b7, l1, 8);
            bfly8_avx(u, 1);
            __m256 vr[8], vi[8];
            for (int r = 0; r < 8; r++)
            {
                vr[LANE_SUB[r]] = u[r].r;
                vi[LANE_SUB[r]] = u[r].i;
            }
            float* b = work + plan->rev[k0];
            transpose_scatter8_avx(vr, b, off, L <= 16);
            transpose_scatter8_avx(vi, b + 8, off, L <= 16);
        }
    }
    {
        // 1. Inverse lane FFTs (DIT): work -> interleaved output, natural order
        int radix[MAX_STAGES], span[MAX_STAGES], twoff[MAX_STAGES], S = lane_stages_x86(L, radix, span);
        for (int t = 0, off = 0; t < S; t++)
        {
            twoff[t]  = off;
            off      += stage_tw(radix[t], span[t]);
        }
        // The first of these (span 1) also applies the conj cross-lane twiddles
        for (int t = S - 1; t > 0; t--)
            lane_stage_inv_avx(work, work, 0, L, radix[t], span[t], plan->stw + twoff[t], plan->xtw);
        lane_stage_inv_avx(work, out, 1, L, radix[0], span[0], plan->stw, plan->xtw);
    }
}

XFFT_DEF const char* xfft_tell_avx(void) { return "avx"; }

#elif !defined(XFFT_NO_STUBS) // No AVX (or XFFT_DISABLE_AVX): planning fails

XFFT_DEF size_t      xfft_mem_size_avx(int n) { return 64; }
XFFT_DEF void*       xfft_init_avx(void* mem, int n) { return 0; }
XFFT_DEF void        xfft_forward_avx(void* p, const float* in, float* out) {}
XFFT_DEF void        xfft_inverse_avx(void* p, const float* in, float* out) {}
XFFT_DEF const char* xfft_tell_avx(void) { return "avx-stub"; }

#endif // XFFT_AVX_ENABLED

/*~*~*~*~*
 ~  Auto  ~
 *~*~*~*~*/

#if !defined(XFFT_NO_STUBS)

// The auto functions use the first backend that's both compiled in and supported by this CPU: AVX2, AVX, NEON, then
// scalar. Every backend has a function for each API (real or stub), so dispatch needs no #ifs. The choice only depends
// on the CPU, so it's made once and cached: every auto plan comes from the same backend, and the plan doesn't need to
// record which one.

enum
{
    XFFT_AUTO_UNKNOWN, // Not chosen yet
    XFFT_AUTO_NONE,    // Nothing compiled in: the scalar stub
    XFFT_AUTO_SCALAR,
    XFFT_AUTO_NEON,
    XFFT_AUTO_AVX,
    XFFT_AUTO_AVX2,
};

#if defined(XFFT_X86_ENABLED)
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

// CPUID leaf, subleaf 0: r = eax, ebx, ecx, edx
static void xfft_cpuid(unsigned leaf, unsigned* r)
{
#if defined(_MSC_VER) && !defined(__clang__)
    __cpuidex((int*)r, (int)leaf, 0);
#else
    __cpuid_count(leaf, 0, r[0], r[1], r[2], r[3]);
#endif
}

// XCR0: the register state the OS saves on a context switch. Only valid when CPUID reports OSXSAVE.
// Inline asm because clang's _xgetbv needs the xsave target feature.
static unsigned long long xfft_xgetbv0(void)
{
#if defined(_MSC_VER) && !defined(__clang__)
    return _xgetbv(0);
#else
    unsigned lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((unsigned long long)hi << 32) | lo;
#endif
}
#endif // XFFT_X86_ENABLED

// Written once on the first call. Every thread computes the same value, so a racing first call only repeats the work.
static int xfft_auto_cached = XFFT_AUTO_UNKNOWN;

static int xfft_auto_backend(void)
{
    int b = xfft_auto_cached;
    if (b != XFFT_AUTO_UNKNOWN)
        return b;

    b = XFFT_AUTO_NONE;
#if defined(XFFT_SCALAR_ENABLED)
    b = XFFT_AUTO_SCALAR;
#endif
#if defined(XFFT_NEON_ENABLED)
    b = XFFT_AUTO_NEON; // Every AArch64 CPU has NEON
#endif
#if defined(XFFT_X86_ENABLED)
    {
        unsigned r0[4], r1[4], r7[4] = {0};
        xfft_cpuid(0, r0); // eax: highest leaf
        xfft_cpuid(1, r1);
        if (r0[0] >= 7)
            xfft_cpuid(7, r7);
        // ecx bit 27 OSXSAVE, bit 28 AVX, and the OS saves the xmm + ymm registers (XCR0 bits 1, 2)
        int avx = (r1[2] >> 27 & 1) && (r1[2] >> 28 & 1) && (xfft_xgetbv0() & 6) == 6;
        // Leaf 7 ebx bit 5 AVX2, leaf 1 ecx bit 12 FMA
        int avx2 = avx && (r7[1] >> 5 & 1) && (r1[2] >> 12 & 1);
#if defined(XFFT_AVX_ENABLED)
        if (avx)
            b = XFFT_AUTO_AVX;
#endif
#if defined(XFFT_AVX2_ENABLED)
        if (avx2)
            b = XFFT_AUTO_AVX2;
#endif
        (void)avx2;
    }
#endif
    xfft_auto_cached = b;
    return b;
}

XFFT_DEF size_t xfft_mem_size_auto(int n)
{
    switch (xfft_auto_backend())
    {
    case XFFT_AUTO_AVX2:
        return xfft_mem_size_avx2(n);
    case XFFT_AUTO_AVX:
        return xfft_mem_size_avx(n);
    case XFFT_AUTO_NEON:
        return xfft_mem_size_neon(n);
    default:
        return xfft_mem_size_scalar(n);
    }
}

XFFT_DEF void* xfft_init_auto(void* mem, int n)
{
    switch (xfft_auto_backend())
    {
    case XFFT_AUTO_AVX2:
        return xfft_init_avx2(mem, n);
    case XFFT_AUTO_AVX:
        return xfft_init_avx(mem, n);
    case XFFT_AUTO_NEON:
        return xfft_init_neon(mem, n);
    default:
        return xfft_init_scalar(mem, n);
    }
}

XFFT_DEF void xfft_forward_auto(void* plan, const float* in, float* out)
{
    switch (xfft_auto_backend())
    {
    case XFFT_AUTO_AVX2:
        xfft_forward_avx2(plan, in, out);
        break;
    case XFFT_AUTO_AVX:
        xfft_forward_avx(plan, in, out);
        break;
    case XFFT_AUTO_NEON:
        xfft_forward_neon(plan, in, out);
        break;
    default:
        xfft_forward_scalar(plan, in, out);
        break;
    }
}

XFFT_DEF void xfft_inverse_auto(void* plan, const float* in, float* out)
{
    switch (xfft_auto_backend())
    {
    case XFFT_AUTO_AVX2:
        xfft_inverse_avx2(plan, in, out);
        break;
    case XFFT_AUTO_AVX:
        xfft_inverse_avx(plan, in, out);
        break;
    case XFFT_AUTO_NEON:
        xfft_inverse_neon(plan, in, out);
        break;
    default:
        xfft_inverse_scalar(plan, in, out);
        break;
    }
}

XFFT_DEF const char* xfft_tell_auto(void)
{
    switch (xfft_auto_backend())
    {
    case XFFT_AUTO_AVX2:
        return "avx2";
    case XFFT_AUTO_AVX:
        return "avx";
    case XFFT_AUTO_NEON:
        return "neon";
    case XFFT_AUTO_SCALAR:
        return "scalar";
    default:
        return "none";
    }
}

#endif // !XFFT_NO_STUBS

#undef FI
#undef NI
#undef HALF_PI_F
#undef SQRT1_2_F
#undef MAX_STAGES
#undef ALIGN_UP
#undef XFFT_IS_ALIGNED
#undef ZROW_ST
#undef ZROW_ST0
#undef ZROW_LD
#undef ZROW_LD0
#undef XFFT_SCALAR_ENABLED
#undef XFFT_NEON_ENABLED
#undef XFFT_AVX_ENABLED
#undef XFFT_AVX2_ENABLED
#undef XFFT_X86_ENABLED

#endif // XHL_FFT_IMPL
