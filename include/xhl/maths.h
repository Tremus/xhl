#ifndef XHL_MATHS_H
#define XHL_MATHS_H
// Contains some functions from licensed libraries. See bottom of file
#include <stdint.h>

#define XM_Ef       2.718281828459045f
#define XM_TAUf     6.283185307179586f
#define XM_1_TAUf   0.15915494309189535f
#define XM_PIf      3.141592653589793f
#define XM_1_PIf    0.3183098861837907f
#define XM_HALF_PIf 1.5707963267948966f
#define XM_LN2f     0.6931471805599453f
#define XM_1_LN2f   1.4426950408889634f
#define XM_LN10f    2.302585092994046f
#define XM_1_LN10f  0.43429448190325176f
#define XM_SQRT2f   1.41421356237309504880f
#define XM_SQRT1_2f 0.707106781186547524401f

#ifdef __cplusplus
extern "C" {
#endif

int      xm_mini(int a, int b);
unsigned xm_minu(unsigned a, unsigned b);
int64_t  xm_minll(int64_t a, int64_t b);
uint64_t xm_minull(uint64_t a, uint64_t b);
float    xm_minf(float a, float b);
double   xm_mind(double a, double b);

int      xm_maxi(int a, int b);
unsigned xm_maxu(unsigned a, unsigned b);
int64_t  xm_maxll(int64_t a, int64_t b);
uint64_t xm_maxull(uint64_t a, uint64_t b);
float    xm_maxf(float a, float b);
double   xm_maxd(double a, double b);

int      xm_clampi(int v, int lo, int hi);
unsigned xm_clampu(unsigned v, unsigned lo, unsigned hi);
int64_t  xm_clampll(int64_t v, int64_t lo, int64_t hi);
uint64_t xm_clampull(uint64_t v, uint64_t lo, uint64_t hi);
float    xm_clampf(float v, float lo, float hi);
double   xm_clampd(double v, double lo, double hi);

// Integer linear interpolation requires numerator over denominator
int      xm_lerpi(int num, int denom, int start, int end);
unsigned xm_lerpu(unsigned nu, unsigned de, unsigned s, unsigned e);
int64_t  xm_lerpll(int64_t num, int64_t den, int64_t s, int64_t e);
uint64_t xm_lerpull(uint64_t n, uint64_t d, uint64_t s, uint64_t e);
float    xm_lerpf(float frac, float start, float end);
double   xm_lerpd(double frac, double start, double end);

float  xm_mapf(float v, float start1, float end1, float start2, float end2);
double xm_mapd(double v, double start1, double end1, double start2, double end2);

float  xm_normf(float v, float start, float end);
double xm_normd(double v, double start, double end);

int xm_droundi(double v);
int xm_dfloori(double v);

/*i*i*i*i*i*
 i Complex i
 *i*i*i*i*i*/
// clang-format off
union xm_complexf
{
    struct { float real, imag; };
    struct { float re, im; };
};
typedef union xm_complexf xm_complexf;
// clang-format on
xm_complexf xm_caddf(float a_re, float a_im, float b_re, float b_im);
xm_complexf xm_csubf(float a_re, float a_im, float b_re, float b_im);
xm_complexf xm_cmulf(float a_re, float a_im, float b_re, float b_im);
xm_complexf xm_cdivf(float a_re, float a_im, float b_re, float b_im);

xm_complexf xm_csqrtf(float re, float im);
xm_complexf xm_cexpf(float re, float im);
xm_complexf xm_clogf(float re, float im);
xm_complexf xm_clog2f(float re, float im);
xm_complexf xm_clog10f(float re, float im);
xm_complexf xm_cpowf(float a_re, float a_im, float b_re, float b_im);

xm_complexf xm_csinf(float re, float im);
xm_complexf xm_ccosf(float re, float im);
xm_complexf xm_ctanf(float re, float im);
xm_complexf xm_csinhf(float re, float im);
xm_complexf xm_ccoshf(float re, float im);
xm_complexf xm_ctanhf(float re, float im);

/*%*%*%*%*%*%*%*
 % Scientific  %
 *%*%*%*%*%*%*%*/

// Rough benchmarks & margins of error were recorded for several of the following functions
// Find them here: https://github.com/Tremus/fastmaths

float xm_fastsin(float x);
float xm_fastcos(float x);
float xm_fasttan(float x);

float xm_fastersin(float x);
float xm_fastersinfull(float x);
float xm_fastercos(float x);
float xm_fastercosfull(float x);
float xm_fastertan(float x);

// Accepts input of πx/2
float xm_fasttan_normalised(float x);

float xm_fastsinh(float x);
float xm_fastcosh(float x);
float xm_fasttanh(float x);
float xm_fastcoth(float x);
float xm_fastsech(float x);
float xm_fastcsch(float x);

float xm_fastatan(float x);
float xm_fastatan2(float x, float y);

float xm_fastlog(float a);
// ~6x faster than log2f
float xm_fastlog2(float a);
// ~6.6x faster than log10f
float xm_fastlog10(float x);

float xm_fasterlog(float a);
// ~12x faster than log10f. Lacks much precision
float xm_fasterlog10(float a);
// Inputs less than -87 will break
float xm_fastexp(float x);
float xm_fasterexp(float x);
// Only ~10% faster than exp2f. Barely less accurate
float xm_fastexp2(float p);

/*@*@*@*@*
 @ Fancy @
 *@*@*@*@*/

// Convert a midi value (0-127) to Hz. Assumes A440
float xm_midi_to_Hz(float midi);
// Accurate to ~0.0005 dB
float xm_fast_gain_to_dB(float gain);
float xm_fast_dB_to_gain(float dB);
// Denormalise to 20Hz-20kHz
// Perfect at low and mid ranges. 0.15Hz error margin close to 20kHz
float xm_fast_denomalise_Hz(float norm);
// More accurate around 0.5, less around 0 & 1
float xm_fast_normalise_Hz1(float Hz);
// More accurate around 0 & 1, less around 0.5
float xm_fast_normalise_Hz2(float Hz);

/*0*1*0*1*
 1 Bits  0
 *0*1*0*1*/

uint64_t xm_align_up(uint64_t value, uint64_t alignment);

uint32_t xm_next_po2u(uint32_t x);
uint64_t xm_next_po2ull(uint64_t x);

// 'Count leading zero'. Count num zeros starting from the most significant bit. 0 == 32, 1 == 31, 2 == 30, 4 == 29 etc.
uint32_t xm_clzu(uint32_t x);
uint64_t xm_clzull(uint64_t x);
// 'Count trailing zeros'. Count num zeros starting from the least significant bit. 0 == 32, 1 == 0, 2 == 1, 4 == 2 etc.
uint32_t xm_ctzu(uint32_t x);
uint64_t xm_ctzull(uint64_t x);
// Count 1 bits
uint32_t xm_popcountu(uint32_t x);
uint32_t xm_popcountull(uint64_t x);

// https://en.wikipedia.org/wiki/Xorshift
uint32_t xm_xorshift32(uint32_t x);
uint64_t xm_xorshift64(uint32_t x);

#ifdef __cplusplus
}
#endif

#endif // XHL_MATHS_H

#ifdef XHL_MATHS_IMPL
#undef XHL_MATHS_IMPL
#include <math.h>

union xm_fi32
{
    float    f;
    int32_t  i32;
    uint32_t u32;
};
typedef union xm_fi32 xm_fi32;

int      xm_mini(int a, int b) { return a > b ? b : a; }
unsigned xm_minu(unsigned a, unsigned b) { return a > b ? b : a; }
int64_t  xm_minll(int64_t a, int64_t b) { return a > b ? b : a; }
uint64_t xm_minull(uint64_t a, uint64_t b) { return a > b ? b : a; }
float    xm_minf(float a, float b) { return a > b ? b : a; }
double   xm_mind(double a, double b) { return a > b ? b : a; }

int      xm_maxi(int a, int b) { return a < b ? b : a; }
unsigned xm_maxu(unsigned a, unsigned b) { return a < b ? b : a; }
int64_t  xm_maxll(int64_t a, int64_t b) { return a < b ? b : a; }
uint64_t xm_maxull(uint64_t a, uint64_t b) { return a < b ? b : a; }
float    xm_maxf(float a, float b) { return a < b ? b : a; }
double   xm_maxd(double a, double b) { return a < b ? b : a; }

int      xm_clampi(int v, int lo, int hi) { return v < lo ? lo : (hi < v ? hi : v); }
unsigned xm_clampu(unsigned v, unsigned lo, unsigned hi) { return v < lo ? lo : (hi < v ? hi : v); }
int64_t  xm_clampll(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : (hi < v ? hi : v); }
uint64_t xm_clampull(uint64_t v, uint64_t lo, uint64_t hi) { return v < lo ? lo : (hi < v ? hi : v); }
float    xm_clampf(float v, float lo, float hi) { return v < lo ? lo : (hi < v ? hi : v); }
double   xm_clampd(double v, double lo, double hi) { return v < lo ? lo : (hi < v ? hi : v); }

// Integer linear interpolation requires numerator over denominator
int      xm_lerpi(int num, int denom, int start, int end) { return start + (num * (end - start)) / denom; }
unsigned xm_lerpu(unsigned nu, unsigned de, unsigned s, unsigned e) { return s + (nu * (e - s)) / de; }
int64_t  xm_lerpll(int64_t num, int64_t den, int64_t s, int64_t e) { return s + (num * (e - s)) / den; }
uint64_t xm_lerpull(uint64_t n, uint64_t d, uint64_t s, uint64_t e) { return s + (n * (e - s)) / d; }
float    xm_lerpf(float frac, float start, float end) { return start + frac * (end - start); }
double   xm_lerpd(double frac, double start, double end) { return start + frac * (end - start); }

float  xm_mapf(float v, float s1, float e1, float s2, float e2) { return s2 + ((e2 - s2) * (v - s1)) / (e1 - s1); }
double xm_mapd(double v, double s1, double e1, double s2, double e2) { return s2 + ((e2 - s2) * (v - s1)) / (e1 - s1); }

float  xm_normf(float v, float start, float end) { return (v - start) / (end - start); }
double xm_normd(double v, double start, double end) { return (v - start) / (end - start); }

// https://stackoverflow.com/a/429812
int xm_droundi(double d)
{
    union Cast
    {
        double d;
        int    i[2];
    };
    union Cast c;
    c.d = d + 6755399441055744.0;
    return c.i[0];
}
int xm_dfloori(double d)
{
    union Cast
    {
        double d;
        int    i[2];
    };
    union Cast c;
    c.d = d + 6755399441055743.5;
    return c.i[0];
}

xm_complexf xm_caddf(float a_re, float a_im, float b_re, float b_im)
{
    xm_complexf c;
    c.re = a_re + b_re;
    c.im = a_im + b_im;
    return c;
}

xm_complexf xm_csubf(float a_re, float a_im, float b_re, float b_im)
{
    xm_complexf c;
    c.re = a_re - b_re;
    c.im = a_im - b_im;
    return c;
}

xm_complexf xm_cmulf(float a_re, float a_im, float b_re, float b_im)
{
    xm_complexf c;
    c.real = a_re * b_re - a_im * b_im;
    c.imag = a_re * b_im + a_im * b_re;
    return c;
}

xm_complexf xm_cdivf(float a_re, float a_im, float b_re, float b_im)
{
    xm_complexf c;
    c.real = (a_re * b_re + a_im * b_im) / (b_re * b_re + b_im * b_im);
    c.imag = (a_im * b_re - a_re * b_im) / (b_re * b_re + b_im * b_im);
    return c;
}

xm_complexf xm_csqrtf(float re, float im)
{
    xm_complexf c;

    float mag = hypotf(re, im);
    c.re      = sqrtf((mag + re) * 0.5f);
    c.im      = sqrtf((mag - re) * 0.5f);
    c.im      = copysignf(c.im, im);
    return c;
}
xm_complexf xm_cexpf(float re, float im)
{
    xm_complexf c;

    float re_exp = xm_fastexp(re);
    // These sin & cos parts must be VERY accurate.
    // Fast maths versions won't work well here
    c.re = re_exp * cosf(im);
    c.im = re_exp * sinf(im);
    return c;
}
xm_complexf xm_clogf(float re, float im)
{
    xm_complexf c;

    float mag = hypotf(re, im);
    c.re      = xm_fastlog(mag);
    c.im      = atan2f(im, re); // our own xm_fastatan2 isn't accurate enough
    return c;
}
xm_complexf xm_clog2f(float re, float im)
{
    xm_complexf c = xm_clogf(re, im);
    return xm_cmulf(c.re, c.im, XM_1_LN2f, 0);
}
xm_complexf xm_clog10f(float re, float im)
{
    xm_complexf c = xm_clogf(re, im);
    return xm_cmulf(c.re, c.im, XM_1_LN10f, 0);
}
xm_complexf xm_cpowf(float a_re, float a_im, float b_re, float b_im)
{
    xm_complexf c = xm_clogf(a_re, a_im);
    c             = xm_cmulf(c.re, c.im, b_re, b_im);
    return xm_cexpf(c.re, c.im);
}

// https://en.wikipedia.org/wiki/Trigonometric_functions#In_the_complex_plane
xm_complexf xm_csinf(float re, float im)
{
    xm_complexf c;
    c.re = xm_fastersinfull(re) * xm_fastcosh(im);
    c.im = xm_fastercosfull(re) * xm_fastsinh(im);
    return c;
}

xm_complexf xm_ccosf(float re, float im)
{
    xm_complexf c;
    c.re = xm_fastercosfull(re) * xm_fastcosh(im);
    c.im = -xm_fastersinfull(re) * xm_fastsinh(im);
    return c;
}

xm_complexf xm_ctanf(float re, float im)
{
    xm_complexf n, d;
    n = xm_csinf(re, im);
    d = xm_ccosf(re, im);
    return xm_cdivf(n.re, n.im, d.re, d.im);
}

xm_complexf xm_csinhf(float re, float im)
{
    xm_complexf c;
    c.re = xm_fastercosfull(im) * xm_fastsinh(re);
    c.im = xm_fastersinfull(im) * xm_fastcosh(re);
    return c;
}
xm_complexf xm_ccoshf(float re, float im)
{
    xm_complexf c;
    c.re = xm_fastercosfull(im) * xm_fastcosh(re);
    c.im = xm_fastersinfull(im) * xm_fastsinh(re);
    return c;
}

xm_complexf xm_ctanhf(float re, float im)
{
    xm_complexf n, d;
    n = xm_csinhf(re, im);
    d = xm_ccoshf(re, im);
    return xm_cdivf(n.re, n.im, d.re, d.im);
}

float xm_fastsin(float x)
{
    static const float fouroverpi   = 1.2732395447351627f;
    static const float fouroverpisq = 0.40528473456935109f;
    static const float q            = 0.78444488374548933f;

    xm_fi32  p    = {.f = 0.20363937680730309f};
    xm_fi32  r    = {.f = 0.015124940802184233f};
    xm_fi32  s    = {.f = -0.0032225901625579573f};
    xm_fi32  vx   = {.f = x};
    uint32_t sign = vx.u32 & 0x80000000;
    vx.u32        = vx.u32 & 0x7FFFFFFF;

    float qpprox   = fouroverpi * x - fouroverpisq * x * vx.f;
    float qpproxsq = qpprox * qpprox;

    p.u32 |= sign;
    r.u32 |= sign;
    s.u32 ^= sign;

    return q * qpprox + qpproxsq * (p.f + qpproxsq * (r.f + qpproxsq * s.f));
}

float xm_fastcos(float x) { return xm_fastsin(x + ((x > XM_HALF_PIf) ? (XM_HALF_PIf - XM_TAUf) : XM_HALF_PIf)); }
float xm_fasttan(float x) { return xm_fastsin(x) / xm_fastsin(x + XM_HALF_PIf); }

// Paul Minieros fastersin
float xm_fastersin(float x)
{
    static const float fouroverpi   = 1.2732395447351627f;
    static const float fouroverpisq = 0.40528473456935109f;
    static const float q            = 0.77633023248007499f;

    xm_fi32  p     = {.f = 0.22308510060189463f};
    xm_fi32  vx    = {.f = x};
    uint32_t sign  = vx.u32 & 0x80000000;
    vx.u32        &= 0x7FFFFFFF;

    float qpprox = fouroverpi * x - fouroverpisq * x * vx.f;

    p.u32 |= sign;

    return qpprox * (q + p.f * qpprox);
}

float xm_fastersinfull(float x)
{
    int   k    = (int)(x * 0.15915494309189534f);
    float half = (x < 0) ? -0.5f : 0.5f;
    return xm_fastersin((half + k) * XM_TAUf - x);
}

// Paul Minieros cos
float xm_fastercos(float x) { return xm_fastersin(x + ((x > XM_HALF_PIf) ? (XM_HALF_PIf - XM_TAUf) : XM_HALF_PIf)); }
float xm_fastercosfull(float x) { return xm_fastersinfull(x + XM_HALF_PIf); }
float xm_fastertan(float x) { return xm_fastersin(x) / xm_fastercos(x); }

// https://observablehq.com/@jrus/fasttan
float xm_fasttan_normalised(float x)
{
    // 3 add, 3 mult, 1 div
    float y = 1.0f - x * x;
    return x * (-0.0187108f * y + 0.31583526f + 1.27365776f / y);
}

// https://en.wikipedia.org/wiki/Hyperbolic_functions#Definitions
float xm_fastsinh(float x) { return (xm_fastexp(x) - xm_fastexp(-x)) * 0.5f; }
float xm_fastcosh(float x) { return (xm_fastexp(x) + xm_fastexp(-x)) * 0.5f; }
float xm_fasttanh(float x) { return (xm_fastexp(x) - xm_fastexp(-x)) / (xm_fastexp(x) + xm_fastexp(-x)); }
float xm_fastcoth(float x)
{
    float a = xm_fastexp(x);
    float b = xm_fastexp(-x);
    return (a + b) / (a - b);
}
float xm_fastsech(float x) { return 2.0f / (xm_fastexp(x) + xm_fastexp(-x)); }
float xm_fastcsch(float x) { return 2.0f / (xm_fastexp(x) - xm_fastexp(-x)); }

// Code: https://gist.github.com/bitonic/d0f5a0a44e37d4f0be03d34d47acb6cf
// Algorithm: sheet 11 of  "Approximations for digital computers", C. Hastings, 1955
float xm_fastatan(float x)
{
    float x_sq = x * x;
    return x * (0.99997726f +
                x_sq * (-0.33262347f +
                        x_sq * (0.19354346f + x_sq * (-0.11643287f + x_sq * (0.05265332f + x_sq * -0.01172120f)))));
}

// Code: https://gist.github.com/bitonic/d0f5a0a44e37d4f0be03d34d47acb6cf
float xm_fastatan2(float x, float y)
{
    bool  swap       = fabsf(x) < fabsf(y);
    float atan_input = (swap ? x : y) / (swap ? y : x);
    if (fabsf(x) == 0 && fabsf(y) == 0)
        atan_input = 0;

    float res = xm_fastatan(atan_input);

    // If swapped, adjust atan output
    res = swap ? copysignf(XM_HALF_PIf, atan_input) - res : res;
    // Adjust the result depending on the input quadrant
    if (x < 0.0f)
        res = copysignf(XM_PIf, y) + res;

    return res;
}

float xm_fastlog(float x) { return xm_fastlog2(x) * XM_LN2f; }

float xm_fastlog2(float x)
{
    xm_fi32 vx = {.f = x};
    xm_fi32 mx = {.u32 = (vx.u32 & 0x007FFFFF) | 0x3f000000};
    float   y  = vx.u32;

    y *= 1.1920928955078125e-7f;

    return y - 124.22551499f - 1.498030302f * mx.f - 1.72587999f / (0.3520887068f + mx.f);
}

// Adapted from Paul Mineiro log2
// https://github.com/romeric/fastapprox/blob/master/fastapprox/src/fastlog.h
float xm_fastlog10(float x)
{
    xm_fi32 vx = {.f = x};
    xm_fi32 mx = {.u32 = (vx.u32 & 0x007FFFFF) | 0x3f000000};
    float   y  = vx.u32;

    y *= 3.588557191657796e-8f;

    return y - 37.39560623879553f - 0.4509520553155725f * mx.f - 0.5195416459062518f / (0.3520887068f + mx.f);
}

// Blazing fast but lacks precision
// https://github.com/ekmett/approximate/blob/master/cbits/fast.c
/* 1065353216 - 486411 = 1064866805 */
float xm_fasterlog(float a)
{
    xm_fi32 u = {a};
    return (u.i32 - 1064866805) * 8.262958405176314e-8f; /* 1 / 12102203.0; */
}

// fasterlog * log10e
float xm_fasterlog10(float a)
{
    xm_fi32 u = {a};
    // return (u.x - 1064866805) * 8.262958405176314e-8f * log10e; /* 1 /
    return (u.i32 - 1064866805) * 3.5885572395641675e-8f; /* 1 / 12102203.0; */
}

// Paul Mineiro's exp2(log2(e))
// https://github.com/romeric/fastapprox/blob/master/fastapprox/src/fastexp.h
float xm_fastexp(float x) { return xm_fastexp2(x * 1.442695040f); }
float xm_fasterexp(float x)
{
    // p. mineiros faster exp
    // float p = x * 1.442695040f + 126.94269504f;
    // fi32  v = {.u32 = (uint32_t)(8388608 * p)};
    // return v.f;

    // ekmett_ub
    // https://github.com/ekmett/approximate/blob/master/cbits/fast.c
    xm_fi32 u = {.i32 = (int32_t)(12102203.0f * x + 1065353217.0f)};
    return u.f;
}

// Paul Mineiro's exp2
// https://github.com/romeric/fastapprox/blob/master/fastapprox/src/fastexp.h
float xm_fastexp2(float p)
{
    float offset = (p < 0) ? 1.0f : 0.0f;
    // float clipp  = (p < -126) ? -126.0f : p;
    int     w = (int)p;
    float   z = p - w + offset;
    xm_fi32 v = {.u32 = (uint32_t)((1 << 23) * (p + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z))};

    return v.f;
}

float xm_fast_gain_to_dB(float gain) { return 20.0f * xm_fastlog10(gain); }

float xm_fast_denomalise_Hz(float norm) { return 20 * xm_fastexp2(norm * 10); }

// Adapted from the ankerl & ekmett fast log algorithms
// log(Hz / 20) / log(2) / 10
// log(Hz * 0.05) / (LN2 * 0.1)
// log(Hz * 0.05) * 0.14426950408889633
// https://github.com/ekmett/approximate/blob/master/cbits/fast.c
float xm_fast_normalise_Hz1(float Hz)
{
    xm_fi32 u = {.f = Hz * 0.05f};
    return (u.i32 - 1064866805) * 1.1920929114219645e-08f; // ankerl32
}

float xm_fast_normalise_Hz2(float Hz)
{
    xm_fi32 u = {Hz * 0.05f};
    return (u.i32 - 1065353217) * 1.1920929114219645e-08f; // ekmett_lb
}

// pow(2, (midi - 69) / 12) * 440
float xm_midi_to_Hz(float midi)
{
    float Hz = xm_fastexp2((midi - 69.0f) * 0.0833333f) * 440.0f;
    // Ceiling of 20kHz. Removing the following line can break filters and cause infinity errors
    return Hz > 20000.0f ? 20000.0f : Hz;
}

float xm_fast_dB_to_gain(float dB) { return xm_fastexp2(dB * 0.166666667); }

uint64_t xm_align_up(uint64_t value, uint64_t alignment)
{
    uint64_t inc = (alignment - (value % alignment)) % alignment;
    return value + inc;
}

uint32_t xm_next_po2u(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

uint64_t xm_next_po2ull(uint64_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

#if defined(__GNUC__) || defined(__clang__)

uint32_t xm_clzu(uint32_t x) { return __builtin_clz(x); }
uint64_t xm_clzull(uint64_t x) { return __builtin_clzll(x); }
uint32_t xm_ctzu(uint32_t x) { return __builtin_ctz(x); }
uint64_t xm_ctzull(uint64_t x) { return __builtin_ctzll(x); }

uint32_t xm_popcountu(uint32_t x) { return __builtin_popcount(x); }
uint32_t xm_popcountull(uint64_t x) { return __builtin_popcountll(x); }

#elif defined(_MSC_VER)

#include <intrin.h>

uint32_t xm_clzu(uint32_t Mask)
{
    unsigned long Index;
    return _BitScanReverse(&Index, Mask) ? 31 - Index : 32;
}

uint64_t xm_clzull(uint64_t Mask)
{
    unsigned long Index;
    return _BitScanReverse64(&Index, Mask) ? 63 - Index : 64;
}

uint32_t xm_ctzu(uint32_t Mask)
{
    unsigned long Index;
    return _BitScanForward(&Index, Mask) ? Index : 32;
}

uint64_t xm_ctzull(uint64_t Mask)
{
    unsigned long Index;
    return _BitScanForward64(&Index, Mask) ? Index : 64;
}

uint32_t xm_popcountu(uint32_t x) { return __popcnt(x); }
uint32_t xm_popcountull(uint64_t x) { return __popcnt64(x); }

#else
#error Unknown compiler!
#endif

uint32_t xm_xorshift32(uint32_t x)
{
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}
uint64_t xm_xorshift64(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
}

/* https://github.com/romeric/fastapprox/blob/master/fastapprox/COPYING

Copyright (c) 2011, Paul Mineiro
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
  * Neither the name of Paul Mineiro nor the names of its contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*/
/* https://github.com/ekmett/approximate/blob/master/LICENSE

Copyright 2011 Edward Kmett

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

3. Neither the name of the author nor the names of his contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#endif // XHL_MATHS_IMPL