#pragma once

#include "types.h"

#define TR_E        2.71828182845904523536   // e
#define TR_LOG2E    1.44269504088896340736   // log2(e)
#define TR_LOG10E   0.434294481903251827651  // log10(e)
#define TR_LN2      0.693147180559945309417  // ln(2)
#define TR_LN10     2.30258509299404568402   // ln(10)
#define TR_PI       3.14159265358979323846   // pi
#define TR_TWOPI    (2.0f * TR_PI)
#define TR_PI_2     1.57079632679489661923   // pi/2
#define TR_PI_4     0.785398163397448309616  // pi/4
#define TR_1_PI     0.318309886183790671538  // 1/pi
#define TR_2_PI     0.636619772367581343076  // 2/pi
#define TR_2_SQRTPI 1.12837916709551257390   // 2/sqrt(pi)
#define TR_SQRT2    1.41421356237309504880   // sqrt(2)
#define TR_SQRT1_2  0.707106781186547524401  // 1/sqrt(2)

static inline float tr_sqrtf(float x) { return __builtin_sqrtf(x); }
static inline float tr_fabsf(float x) { return __builtin_fabsf(x); }
static inline float tr_floorf(float x) { return __builtin_floorf(x); }

float tr_sinf(float x);
float tr_cosf(float x);
float tr_tanf(float x);
float tr_fmodf(float x, float y);
float tr_expf(float x);
float tr_exp2f(float x);
float tr_logf(float x);
float tr_powf(float x, float y);
float tr_roundf(float x);

static inline int signbit(float x)
{
    const uint32_t bits = *(uint32_t*)&x;
    return (bits >> 31) & 1;
}

static inline float fmaxf(float a, float b)
{
    return (a > b) ? a : b;
}

static inline float fminf(float a, float b)
{
    return (a < b) ? a : b;
}

static inline float float_lerp(float a, float b, float t)
{
    return a + t*(b - a);
}

static inline float float_remap(float value, float inputStart, float inputEnd, float outputStart, float outputEnd)
{
    return (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;
}

static inline float float_clamp(float value, float min, float max)
{
    float result = (value < min)? min : value;

    if (result > max) result = max;

    return result;
}

static inline float float2_distance(float2 v1, float2 v2)
{
    return tr_sqrtf((v1.x - v2.x)*(v1.x - v2.x) + (v1.y - v2.y)*(v1.y - v2.y));
}

static inline float2 float2_scale(float2 v, float s)
{
    return (float2){v.x * s, v.y * s};
}

static inline float2 float2_lerp(float2 v1, float2 v2, float amount)
{
    float2 result = { 0 };

    result.x = v1.x + amount*(v2.x - v1.x);
    result.y = v1.y + amount*(v2.y - v1.y);

    return result;
}

static inline float2 float2_normalize(float2 v)
{
    float2 result = { 0 };
    float length = tr_sqrtf((v.x*v.x) + (v.y*v.y));

    if (length > 0)
    {
        float ilength = 1.0f/length;
        result.x = v.x*ilength;
        result.y = v.y*ilength;
    }

    return result;
}