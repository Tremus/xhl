#ifndef XHL_MATHS_H
#define XHL_MATHS_H
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

uint32_t xm_xorshift32(uint32_t x);
uint64_t xm_xorshift64(uint32_t x);

#ifdef __cplusplus
}
#endif

#endif // XHL_MATHS_H

#ifdef XHL_MATHS_IMPL
#undef XHL_MATHS_IMPL

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

// https://en.wikipedia.org/wiki/Xorshift
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

#endif // XHL_MATHS_IMPL