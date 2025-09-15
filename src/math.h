#pragma once

#include "types.h"

#define sqrtf(x)  __builtin_sqrtf(x)

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
    return sqrtf((v1.x - v2.x)*(v1.x - v2.x) + (v1.y - v2.y)*(v1.y - v2.y));
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
    float length = sqrtf((v.x*v.x) + (v.y*v.y));

    if (length > 0)
    {
        float ilength = 1.0f/length;
        result.x = v.x*ilength;
        result.y = v.y*ilength;
    }

    return result;
}