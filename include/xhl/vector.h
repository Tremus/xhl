#ifndef XHL_VECTOR_H
#define XHL_VECTOR_H
// clang-format off

typedef union
{
    struct { unsigned char tweak, patch, minor, major; };
    struct { unsigned char r, g, b, a; };
    struct { unsigned char red, green, blue, alpha; };
    unsigned int u32;
    int   i32;
    float f32;
} xvecu;

typedef union
{
    struct { int left, right; };
    struct { int l, r; };
    struct { int top, bottom; };
    struct { int t, b; };
    struct { int start, end; };
    struct { int x, y; };
    struct { int height, width; };
    struct { int h, w; };
    int data[2];
} xvec2i;

typedef union
{
    struct { float left, right; };
    struct { float l, r; };
    struct { float top, bottom; };
    struct { float t, b; };
    struct { float start, end; };
    struct { float x, y; };
    struct { float height, width; };
    struct { float h, w; };
    struct { float real, imag; };
    struct { float re, im; };
    struct { float angle, magnitude; };
    struct { float phase, mag; };
    float data[2];
    unsigned long long u64;
} xvec2f;

typedef union xvec3i
{
    struct { int x, y, z; };
    int data[3];
} xvec3i;

typedef union
{
    struct { float x, y, z; };
    struct { float r, g, b; };
    struct { xvec2f position; float skew; };
    struct { float start, end, distance; };
    float data[3];
} xvec3f;

typedef union
{
    struct { int x, y, width, height; };
    struct { xvec2i position, size; };
    int data[4];
} xvec4i;

typedef union
{
    struct { float x, y, width, height; };
    struct { float r, g, b, a; };
    struct { xvec2f position, size; };
    float data[4];
} xvec4f;

// clang-format on
#endif // XHL_VECTOR_H