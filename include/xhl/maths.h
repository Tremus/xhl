#ifndef XHL_MATHS_H
#define XHL_MATHS_H
#include <stdint.h>

static inline int      xm_mini(int a, int b) { return a > b ? b : a; }
static inline unsigned xm_minu(unsigned a, unsigned b) { return a > b ? b : a; }
static inline int64_t  xm_minll(int64_t a, int64_t b) { return a > b ? b : a; }
static inline uint64_t xm_minull(uint64_t a, uint64_t b) { return a > b ? b : a; }
static inline float    xm_minf(float a, float b) { return a > b ? b : a; }
static inline double   xm_mind(double a, double b) { return a > b ? b : a; }

static inline int      xm_maxi(int a, int b) { return a < b ? b : a; }
static inline unsigned xm_maxu(unsigned a, unsigned b) { return a < b ? b : a; }
static inline int64_t  xm_maxll(int64_t a, int64_t b) { return a < b ? b : a; }
static inline uint64_t xm_maxull(uint64_t a, uint64_t b) { return a < b ? b : a; }
static inline float    xm_maxf(float a, float b) { return a < b ? b : a; }
static inline double   xm_maxd(double a, double b) { return a < b ? b : a; }

static inline int      xm_clampi(int v, int lo, int hi) { return v < lo ? lo : (hi < v ? hi : v); }
static inline unsigned xm_clampu(unsigned v, unsigned lo, unsigned hi) { return v < lo ? lo : (hi < v ? hi : v); }
static inline int64_t  xm_clampll(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : (hi < v ? hi : v); }
static inline uint64_t xm_clampull(uint64_t v, uint64_t lo, uint64_t hi) { return v < lo ? lo : (hi < v ? hi : v); }
static inline float    xm_clampf(float v, float lo, float hi) { return v < lo ? lo : (hi < v ? hi : v); }
static inline double   xm_clampd(double v, double lo, double hi) { return v < lo ? lo : (hi < v ? hi : v); }

// Integer linear interpolation requires numerator over denominator
static inline int xm_lerpi(int num, int denom, int start, int end) { return start + (num * (end - start)) / denom; }
static inline unsigned xm_lerpu(unsigned nu, unsigned de, unsigned s, unsigned e) { return s + (nu * (e - s)) / de; }
static inline int64_t  xm_lerpll(int64_t num, int64_t den, int64_t s, int64_t e) { return s + (num * (e - s)) / den; }
static inline uint64_t xm_lerpull(uint64_t n, uint64_t d, uint64_t s, uint64_t e) { return s + (n * (e - s)) / d; }
static inline float    xm_lerpf(float frac, float start, float end) { return start + frac * (end - start); }
static inline double   xm_lerpd(double frac, double start, double end) { return start + frac * (end - start); }

// https://en.wikipedia.org/wiki/Xorshift
static inline uint32_t xm_xorshift32(uint32_t x)
{
    /* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}
static inline uint64_t xm_xorshift64(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
}

#endif // XHL_MATHS_H
