#include "math.h"

float tr_sinf(float x)
{
    // wrap to [-π, π]
    if (x < -TR_PI) x += TR_TWOPI;
    if (x >  TR_PI) x -= TR_TWOPI;

    // parabolic approximation
    float y = 1.27323954f * x - 0.405284735f * x * (x < 0 ? -x : x);
    //y = 0.225f * (y * (y < 0 ? -y : y) - y) + y;
    return y;
}

float tr_cosf(float x)
{
    return tr_sinf(x + 1.57079632f); // π/2
}

float tr_tanf(float x)
{
    return tr_sinf(x) / tr_cosf(x);
}

float tr_fmodf(float x, float y)
{
    int q = (int)(x / y);
    float r = x - (float)q * y;
    if (r < 0) r += y; // force positive result
    return r;
}

float tr_expf(float x)
{
    // range reduction
    int i = (int)(x * 1.4426950409f); // 1/ln(2)
    float f = x - i * 0.69314718056f; // ln(2)

    // 2^f ~ 1 + f + f^2/2
    float r = 1.0f + f + 0.5f * f * f;

    // scale back by 2^i
    return r * (1 << i);
}

// fast logf approximation (valid for x > 0, not too far from 1)
static inline float fast_logf(float x) {
    int e = 0;
    // extract exponent by normalizing
    while (x > 2.0f) { x *= 0.5f; e++; }
    while (x < 1.0f) { x *= 2.0f; e--; }

    float y = x - 1.0f;
    float y2 = y * y;
    // log(1+y) ~ y - y^2/2
    return (y - 0.5f * y2) + e * 0.69314718056f; // ln(2)
}

// simple powf(x, y)
float tr_powf(float x, float y)
{
    return tr_expf(y * fast_logf(x));
}

float tr_roundf(float x)
{
    int i = (int)(x + (x >= 0.0f ? 0.5f : -0.5f));
    return (float)i;
}