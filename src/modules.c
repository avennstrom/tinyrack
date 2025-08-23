#include "modules.h"

#include <math.h>
#include <string.h>
#include <raymath.h>

float tr_hz(float voct)
{
    return voct * 100.0f;
}

//
// tr_vco_t
//

void tr_vco_update(tr_vco_t* vco)
{
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        float f = vco->in_v0;
        if (vco->in_voct != NULL)
        {
            f = vco->in_v0 * powf(2.0f, vco->in_voct[i]);
        }

        vco->phase += (f / TR_SAMPLE_RATE) * TR_TWOPI;
        vco->phase = fmodf(vco->phase, TR_TWOPI);

        const float s = sinf(vco->phase);
        
        vco->out_sin[i] = s;
        vco->out_sqr[i] = signbit(s) ? 1.0f : -1.0f;
        vco->out_saw[i] = vco->phase / TR_PI - 1.0f;
    }
}

//
// tr_clock_t
//

void tr_clock_update(tr_clock_t* clock)
{
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        clock->phase += (clock->in_hz / TR_SAMPLE_RATE) * 1.0f;
        clock->phase = fmodf(clock->phase, 1.0f);
        clock->out_gate[i] = clock->phase < 0.5f ? 5.0f : 0.0f;
    }
}

//
// tr_clockdiv_t
//

void tr_clockdiv_update(tr_clockdiv_t* clockdiv)
{
    int gate = clockdiv->gate;
    int state = clockdiv->state;

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        const int g = clockdiv->in_gate != NULL ? clockdiv->in_gate[i] > 0.0f : 0;
        const int edge = g && g != gate;
        gate = g;
        
        if (edge)
        {
            state = (state + 1) & 0xff;
        }
        
        clockdiv->out_0[i] = (state & 0b00000001) ? 1.0f : -1.0f;
        clockdiv->out_1[i] = (state & 0b00000010) ? 1.0f : -1.0f;
        clockdiv->out_2[i] = (state & 0b00000100) ? 1.0f : -1.0f;
        clockdiv->out_3[i] = (state & 0b00001000) ? 1.0f : -1.0f;
        clockdiv->out_4[i] = (state & 0b00010000) ? 1.0f : -1.0f;
        clockdiv->out_5[i] = (state & 0b00100000) ? 1.0f : -1.0f;
        clockdiv->out_6[i] = (state & 0b01000000) ? 1.0f : -1.0f;
        clockdiv->out_7[i] = (state & 0b10000000) ? 1.0f : -1.0f;
    }

    clockdiv->gate = gate;
    clockdiv->state = state;
}

//
// tr_seq8_t
//

void tr_seq8_update(tr_seq8_t* seq)
{
    //assert(seq->in_step != NULL);

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        const float gate = seq->in_step != NULL ? seq->in_step[i] : 0.0f;
        const int trig = gate > 0.0f;
        
        if (trig != seq->trig)
        {
            if (trig)
            {
                seq->step = (seq->step + 1) % 8;
            }
            seq->trig = trig;
        }
        
        seq->out_cv[i] = (&seq->in_cv_0)[seq->step];
    }
}

//
// tr_adsr_t
//

enum
{
    TR_LUT_ADSR_DECAY,
};



void tr_adsr_lut_init(void)
{

}

enum
{
    TR_ADSR_STATE_ATTACK,
    TR_ADSR_STATE_DECAY,
    TR_ADSR_STATE_SUSTAIN,
    TR_ADSR_STATE_RELEASE,
};

static inline float tr__time_to_coeff(float t_sec)
{
    // t_sec == 0 → coeff 0 for instant jump
    //if (t_sec <= 0.0f) return 0.0f;
    float c = expf(-1.0f / (t_sec * (float)TR_SAMPLE_RATE));
    // numerical guard
    return Clamp(c, 0.0f, 1.0f);
}

static inline float map_knob_exp(float x, float t_min, float t_max, float skew)
{
    // raise the knob to 'skew' to compress the low end
    float xs = powf(x, skew);
    // exponential mapping across decades
    float ratio = t_max / fmaxf(t_min, 1e-9f);
    return t_min * powf(ratio, xs);
}

#define TR_ADSR_EPSILON (1e-5f)

void tr_adsr_update(tr_adsr_t* adsr)
{
    if (adsr->in_gate == NULL)
    {
        memset(adsr->out_env, 0, sizeof(adsr->out_env));
        return;
    }

    const float a_coeff = tr__time_to_coeff(map_knob_exp(adsr->in_attack, 0.0005f, 10.0f, 1.5f));
    const float d_coeff = tr__time_to_coeff(adsr->in_decay);
    const float r_coeff = tr__time_to_coeff(map_knob_exp(adsr->in_release, 0.0005f, 10.0f, 1.2f));

    float value = adsr->value;
    int state = adsr->state;
    int gate = adsr->gate;

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        const int g = adsr->in_gate[i] > 0.0f;
        const int edge = g != gate;
        const int note_on = edge && g;
        const int note_off = edge && !g;
        
        gate = g;

        if (note_on)    state = TR_ADSR_STATE_ATTACK;
        if (note_off)   state = TR_ADSR_STATE_RELEASE;
        
        switch (state)
        {
            case TR_ADSR_STATE_ATTACK:
                value += (1.0f - value) * (1.0f - a_coeff);
                if (value >= 1.0f - TR_ADSR_EPSILON) {
                    value = 1.0f;
                    state = TR_ADSR_STATE_DECAY;
                }
                break;
            case TR_ADSR_STATE_DECAY:
                value += (adsr->in_sustain - value) * (1.0f - d_coeff);
                if (fabsf(value - adsr->in_sustain) <= TR_ADSR_EPSILON) {
                    value = adsr->in_sustain;
                    adsr->state = TR_ADSR_STATE_SUSTAIN;
                }
                break;
            case TR_ADSR_STATE_SUSTAIN:
                value += (adsr->in_sustain - value) * 0.001f;
                break;
            case TR_ADSR_STATE_RELEASE:
                value += (0.0f - value) * (1.0f - r_coeff);
                break;
        }
        
        value = fminf(value, 1.0f);
        value = fmaxf(value, 0.0f);

        adsr->out_env[i] = value;
    }

    adsr->value = value;
    adsr->state = state;
    adsr->gate = gate;
}

//
// tr_vca_t
//

void tr_vca_update(tr_vca_t* vca)
{
    if (vca->in_audio == NULL || 
        vca->in_cv == NULL)
    {
        memset(vca->out_mix, 0, sizeof(vca->out_mix));
        return;
    }

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        vca->out_mix[i] = vca->in_audio[i] * vca->in_cv[i];
    }
}

//
// tr_lp_t
//

static inline float tpt_lp1_process(float* z, float x, float cutoffHz)
{
    // Safety: clamp cutoff (avoid Nyquist and negatives)
    //const float nyq = 0.5f * TR_SAMPLE_RATE;
    if (cutoffHz < 0.0f) cutoffHz = 0.0f;
    if (cutoffHz > 0.495f * TR_SAMPLE_RATE) cutoffHz = 0.495f * TR_SAMPLE_RATE;

    // TPT coefficient: g = tan(pi * fc / fs)
    const float g = tanf(TR_PI * cutoffHz / TR_SAMPLE_RATE);
    const float a = g / (1.0f + g);      // bilinear transform form

    // Zero-denormal trick: add tiny dc to break subnormals
    const float x_nd = x + 1e-24f;

    // One-pole TPT
    float v = (x_nd - *z) * a;         // input to integrator
    float y = v + *z;                  // output (lowpass)
    *z = y + v;                        // update state (trapezoidal integration)

    return y;
}

static inline float control_to_hz(float u, float minHz, float maxHz, float fs)
{
    // constrain and keep inside (0, Nyquist)
    if (u < 0.0f) u = 0.0f; if (u > 1.0f) u = 1.0f;
    if (maxHz > 0.495f * fs) maxHz = 0.495f * fs;
    if (minHz < 0.0f) minHz = 0.0f;
    if (minHz > maxHz) minHz = maxHz;

    const float logMin = logf(fmaxf(minHz, 1e-6f));
    const float logMax = logf(fmaxf(maxHz, 1e-6f));
    const float fc = expf(logMin + (logMax - logMin) * u);
    return fc;
}

void tr_lp_update(tr_lp_t* lp)
{
    if (lp->in_audio == NULL)
    {
        memset(lp->out_audio, 0, sizeof(float) * TR_SAMPLE_COUNT);
        return;
    }

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        float cut = lp->in_cut0;
        if (lp->in_cut != NULL) cut += lp->in_cut[i] * lp->in_cut_mul;

        const float cutoff = control_to_hz(cut, 1.0f, 20000.0f, TR_SAMPLE_RATE);

        const float y = tpt_lp1_process(&lp->z, lp->in_audio[i], cutoff);
        
        lp->out_audio[i] = y;
    }
}

//
// tr_mixer_t
//

static void tr_mixer_accum(float* mix, const float* in, float mul)
{
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        mix[i] += in[i] * mul;
    }
}

void tr_mixer_update(tr_mixer_t* mixer)
{
    memset(mixer->out_mix, 0, sizeof(mixer->out_mix));

    if (mixer->in_0 != NULL) tr_mixer_accum(mixer->out_mix, mixer->in_0, mixer->in_vol0);
    if (mixer->in_1 != NULL) tr_mixer_accum(mixer->out_mix, mixer->in_1, mixer->in_vol1);
    if (mixer->in_2 != NULL) tr_mixer_accum(mixer->out_mix, mixer->in_2, mixer->in_vol2);
    if (mixer->in_3 != NULL) tr_mixer_accum(mixer->out_mix, mixer->in_3, mixer->in_vol3);

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        mixer->out_mix[i] *= mixer->in_vol_final;
    }

#if 0
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        mixer->out_mix[i] = tanhf(mixer->out_mix[i]);
    }
#endif
}

//
// tr_noise_t
//
#define TR_NOISE_RED_FC_HZ       5.0f
#define TR_NOISE_BLUE_HP_FC_HZ   5.0f
#define TR_NOISE_BLUE_TAME_FC_HZ 8000.0f

static inline float tr_onepole_a_from_fc(float fc)
{
    return expf(-2.0f * (float)TR_PI * fc / (float)TR_SAMPLE_RATE);
}

void tr_noise_update(tr_noise_t* noise)
{
    const float a_red  = tr_onepole_a_from_fc(TR_NOISE_RED_FC_HZ);
    const float b_red  = sqrtf(fmaxf(0.f, 1.f - a_red*a_red));
    //const float a_hp   = tr_onepole_a_from_fc(TR_NOISE_BLUE_HP_FC_HZ);
    //const float a_tame = tr_onepole_a_from_fc(TR_NOISE_BLUE_TAME_FC_HZ);

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        noise->rng = noise->rng * 1664525u + 1013904223u;
        const float w = (noise->rng / (float)UINT32_MAX) * 2.0f - 1.0f;

        noise->red_state = a_red * noise->red_state + b_red * w;

        noise->out_white[i] = w;
        noise->out_red[i] = noise->red_state;
    }
}

//
// tr_quantizer_t
//
const uint8_t tr_quantizer_lut_minor[12] = {
    0, 0, 2, 3, 3, 5, 5, 7, 7, 9, 10, 10
};
const uint8_t tr_quantizer_lut_minor_triad[12] = {
    0, 0, 0, 3, 3, 3, 3, 7, 7, 7, 7, 7
};
const uint8_t tr_quantizer_lut_major[12] = {
    0, 0, 2, 4, 4, 5, 5, 7, 7, 9, 11, 11
};
const uint8_t tr_quantizer_lut_major_triad[12] = {
    0, 0, 0, 4, 4, 4, 4, 7, 7, 7, 7, 7
};
const uint8_t tr_quantizer_lut_pentatonic[12] = {
    0, 0, 0, 3, 3, 5, 5, 7, 7, 7, 10, 10
};
const uint8_t tr_quantizer_lut_blues[12] = {
    0, 0, 0, 3, 3, 5, 6, 7, 7, 7, 10, 10
};

static void tr_quantizer_apply_lut(tr_quantizer_t* quantizer, const uint8_t* lut)
{
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        const int offset = 12 * 12;
        const float cv = quantizer->in_cv[i];
        const int note = (int)floorf(cv * 12.0f) + offset;
        const int newnote = (note / 12) * 12 + lut[note % 12] - offset;
        quantizer->out_cv[i] = newnote / 12.0f;
    }
}

void tr_quantizer_update(tr_quantizer_t* quantizer)
{
    if (quantizer->in_cv == NULL)
    {
        for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
        {
            quantizer->out_cv[i] = 0.0f;
        }
        return;
    }

    switch (quantizer->in_mode)
    {
        case TR_QUANTIZER_CHROMATIC:
            for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
            {
                float cv = quantizer->in_cv[i];
                cv = floorf(cv * 12.0f) / 12.0f;
                quantizer->out_cv[i] = cv;
            }
            break;
        case TR_QUANTIZER_MINOR:
            tr_quantizer_apply_lut(quantizer, tr_quantizer_lut_minor);
            break;
        case TR_QUANTIZER_MINOR_TRIAD:
            tr_quantizer_apply_lut(quantizer, tr_quantizer_lut_minor_triad);
            break;
        case TR_QUANTIZER_MAJOR:
            tr_quantizer_apply_lut(quantizer, tr_quantizer_lut_major);
            break;
        case TR_QUANTIZER_MAJOR_TRIAD:
            tr_quantizer_apply_lut(quantizer, tr_quantizer_lut_major_triad);
            break;
        case TR_QUANTIZER_PENTATONIC:
            tr_quantizer_apply_lut(quantizer, tr_quantizer_lut_pentatonic);
            break;
        case TR_QUANTIZER_BLUES:
            tr_quantizer_apply_lut(quantizer, tr_quantizer_lut_blues);
            break;
    }
}

//
// tr_random_t
//

void tr_random_update(tr_random_t* random)
{
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        ++random->sample;

        const float t = (random->sample / (float)TR_SAMPLE_RATE) * random->in_speed;

        random->out_cv[i] = sinf(t) * sinf(t * 3.0f) * sinf(t * 5.0f);
    }
}