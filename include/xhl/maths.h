#ifndef XHL_MATHS_H
#define XHL_MATHS_H
// Contains some functions from licensed libraries. See bottom of file
#include <stdint.h>

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

/* * * * * * * *
 * Scientific  *
 * * * * * * * */
// Rough benchmarks & margins of error were recorded for several of the following functions
// Find them here: https://github.com/Tremus/fastmaths

float xm_fastersin(float x);
float xm_fastersinfull(float x);

// This function accepts normalised halfpi values
float xm_fasttan(float x);

float xm_fastatan(float x);
float xm_fastatan2(float x, float y);

float xm_fasterlog(float a);
// ~12x faster than log10f. Lacks much precision
float xm_fasterlog10(float a);
// ~6.6x faster than log10f
float xm_fastlog10(float x);
// Inputs less than -87 will break
float xm_fastexp(float x);
float xm_fasterexp(float x);
// Only ~10% faster than exp2f. Barely less accurate
float xm_fastexp2(float p);

/* * * * *
 * Fancy *
 * * * * */

// Convert a midi value (0-127) to Hz. Assumes A440
float xm_midi_to_Hz(float midi);
// Accurate to ~0.0005 dB
float xm_fast_gain_to_dB(float gain);
// Denormalise to 20Hz-20kHz
// Perfect at low and mid ranges. 0.15Hz error margin close to 20kHz
float xm_fast_denomalise_Hz(float norm);
// More accurate around 0.5, less around 0 & 1
float xm_fast_normalise_Hz1(float Hz);
// More accurate around 0 & 1, less around 0.5
float xm_fast_normalise_Hz2(float Hz);

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

// Paul Minieros sin
float xm_fastersin(float x)
{
    static const float fouroverpi   = 1.2732395447351627f;
    static const float fouroverpisq = 0.40528473456935109f;
    static const float q            = 0.77633023248007499f;
    union
    {
        float    f;
        uint32_t i;
    } p = {0.22308510060189463f};

    union
    {
        float    f;
        uint32_t i;
    } vx          = {x};
    uint32_t sign = vx.i & 0x80000000;
    vx.i          &= 0x7FFFFFFF;

    float qpprox = fouroverpi * x - fouroverpisq * x * vx.f;

    p.i |= sign;

    return qpprox * (q + p.f * qpprox);
}
float xm_fastersinfull(float x)
{
    int   k    = (int)(x * 0.15915494309189534f);
    float half = (x < 0) ? -0.5f : 0.5f;
    return xm_fastersin((half + k) * TAUf - x);
}

// https://observablehq.com/@jrus/fasttan
float xm_fasttan(float x)
{
    // 3 add, 3 mult, 1 div
    float y = 1.0f - x * x;
    return x * (-0.0187108f * y + 0.31583526f + 1.27365776f / y);
}

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
    bool  swap       = fabs(x) < fabs(y);
    float atan_input = (swap ? x : y) / (swap ? y : x);
    if (fabs(x) == 0 && fabs(y) == 0)
        atan_input = 0;

    float res = xm_fastatan(atan_input);

    // If swapped, adjust atan output
    res = swap ? copysignf(HALF_PIf, atan_input) - res : res;
    // Adjust the result depending on the input quadrant
    if (x < 0.0f)
        res = copysignf(PIf, y) + res;

    return res;
}

// Blazing fast but lacks precision
// https://github.com/ekmett/approximate/blob/master/cbits/fast.c
/* 1065353216 - 486411 = 1064866805 */
float xm_fasterlog(float a)
{
    union xm_fi32 u = {a};
    return (u.i32 - 1064866805) * 8.262958405176314e-8f; /* 1 / 12102203.0; */
}

// fasterlog * log10e
float xm_fasterlog10(float a)
{
    union xm_fi32 u = {a};
    // return (u.x - 1064866805) * 8.262958405176314e-8f * log10e; /* 1 /
    return (u.i32 - 1064866805) * 3.5885572395641675e-8f; /* 1 / 12102203.0; */
}

// Adapted from Paul Mineiro log2
// https://github.com/romeric/fastapprox/blob/master/fastapprox/src/fastlog.h
float xm_fastlog10(float x)
{
    union xm_fi32 vx = {.f = x};
    union xm_fi32 mx = {.u32 = (vx.u32 & 0x007FFFFF) | 0x3f000000};
    float         y  = vx.u32;

    y *= 3.588557191657796e-8f;

    return y - 37.39560623879553f - 0.4509520553155725f * mx.f - 0.5195416459062518f / (0.3520887068f + mx.f);
}

// Paul Mineiro's exp2
// https://github.com/romeric/fastapprox/blob/master/fastapprox/src/fastexp.h
float xm_fastexp(float x)
{
    float         p      = 1.442695040f * x;
    float         offset = (p < 0) ? 1.0f : 0.0f;
    int           w      = (int)p;
    float         z      = p - w + offset;
    union xm_fi32 v      = {
             .u32 = (uint32_t)((1 << 23) * (p + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z))};

    return v.f;
}
float xm_fasterexp(float x)
{
    // p. mineiros faster exp
    // float p = x * 1.442695040f + 126.94269504f;
    // fi32  v = {.u32 = (uint32_t)(8388608 * p)};
    // return v.f;

    // ekmett_ub
    // https://github.com/ekmett/approximate/blob/master/cbits/fast.c
    union xm_fi32 u = {.i32 = (int32_t)(12102203.0f * x + 1065353217.0f)};
    return u.f;
}

// Paul Mineiro's exp2
// https://github.com/romeric/fastapprox/blob/master/fastapprox/src/fastexp.h
float xm_fastexp2(float p)
{
    float offset = (p < 0) ? 1.0f : 0.0f;
    // float clipp  = (p < -126) ? -126.0f : p;
    int           w = (int)p;
    float         z = p - w + offset;
    union xm_fi32 v = {
        .u32 = (uint32_t)((1 << 23) * (p + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z))};

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
    union xm_fi32 u = {.f = Hz * 0.05f};
    return (u.i32 - 1064866805) * 1.1920929114219645e-08f; // ankerl32
}

float xm_fast_normalise_Hz2(float Hz)
{
    union xm_fi32 u = {Hz * 0.05f};
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