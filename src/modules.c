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
// tr_seq8_t
//

void tr_seq8_update(tr_seq8_t* seq)
{
    //assert(seq->in_step != NULL);

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        const float gate = seq->in_step != NULL ? seq->in_step[i] : 0.0f;
        const uint8_t trig = gate > 0.0f;
        
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

void tr_adsr_update(tr_adsr_t* adsr)
{
    if (adsr->in_gate == NULL)
    {
        memset(adsr->out_env, 0, sizeof(adsr->out_env));
        return;
    }

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        if (adsr->in_gate[i] > 0.0f)
        {
            if (adsr->decay)
            {
                adsr->value -= (1.0f / TR_SAMPLE_RATE) / adsr->in_decay;
                adsr->value = fmaxf(adsr->value, adsr->in_sustain);
            }
            else
            {
                adsr->value += (1.0f / TR_SAMPLE_RATE) / adsr->in_attack;
                
                if (adsr->value >= 1.0f)
                {
                    adsr->decay = 1;
                }
            }
        }
        else
        {
            adsr->value -= (1.0f / TR_SAMPLE_RATE) / adsr->in_release;
            adsr->decay = 0;
        }
        
        adsr->value = fminf(adsr->value, 1.0f);
        adsr->value = fmaxf(adsr->value, 0.0f);

        adsr->out_env[i] = adsr->value;
    }
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
        if (lp->in_cut != NULL)
        {
            cut += lp->in_cut[i] * lp->in_cut_mul;
        }

        const float c = cut;
        const float w = 1.0f - (c*c);

        lp->value = (lp->value * w) + ((1.0f - w) * lp->in_audio[i]);
        lp->value = Clamp(lp->value, -10.0f, 10.0f);
        
        lp->out_audio[i] = lp->value;
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