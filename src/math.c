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

float tr_expf(float x) {
    // constants
    const float LN2 = 0.69314718056f;       // ln(2)
    const float INV_LN2 = 1.44269504089f;   // 1/ln(2)

    // reduce: x = k*ln2 + r,  with r in [-ln2/2, ln2/2]
    int k = (int)(x * INV_LN2);
    float r = x - k * LN2;

    // approximate e^r with a polynomial (order-3 minimax)
    // e^r ≈ 1 + r + r^2/2 + r^3/6
    float r2 = r * r;
    float r3 = r2 * r;
    float exp_r = 1.0f + r + 0.5f*r2 + 0.16666667f*r3;

    // reconstruct: exp(x) = 2^k * e^r
    int32_t bits = (k + 127) << 23;   // build 2^k in IEEE754
    float two_to_k = *(float*)&bits;

    return two_to_k * exp_r;
}

float tr_exp2f(float x)
{
    // Split x into integer and fractional parts
    int i = (int)x;
    float f = x - (float)i;

    // Polynomial approximation for 2^f, f in [0,1)
    // minimax cubic fit: 2^f ≈ 1 + f*(0.69314718 + f*(0.24022651 + f*0.05550411))
    float p = 1.0f + f * (0.69314718f + f * (0.24022651f + f * 0.05550411f));

    // Build 2^i via exponent bits
    int32_t e = (i + 127) << 23; // IEEE754 bias 127
    float two_to_i = *(float*)&e; // reinterpret as float

    return two_to_i * p;
}

float tr_logf(float x) {
    union { float f; uint32_t i; } v = { x };

    int exp = ((v.i >> 23) & 0xFF) - 127;   // unbiased exponent
    v.i = (v.i & 0x7FFFFF) | 0x3F800000;    // force mantissa into [1,2)

    float m = v.f;
    float y = m - 1.0f;

    // log(1+y) ≈ y - y^2/2 + y^3/3
    float y2 = y * y;
    float y3 = y2 * y;
    float log_m = y - 0.5f*y2 + (1.0f/3.0f)*y3;

    return (float)exp * 0.69314718056f + log_m;
}

// simple powf(x, y)
float tr_powf(float x, float y)
{
    return tr_expf(y * tr_logf(x));
}

float tr_roundf(float x)
{
    int i = (int)(x + (x >= 0.0f ? 0.5f : -0.5f));
    return (float)i;
}