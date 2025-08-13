#pragma once

#include "config.h"

#include <stdint.h>

enum tr_module_type
{
    TR_VCO,
    TR_CLOCK,
    TR_SEQ8,
    TR_ADSR,
    TR_VCA,
    TR_LP,
    TR_MIXER,
    TR_NOISE,
    TR_QUANTIZER,
    TR_RANDOM,
    TR_SPEAKER,
    TR_SCOPE,

    TR_MODULE_COUNT,
};

typedef float tr_buf[TR_SAMPLE_COUNT];

typedef struct tr_vco
{
    float phase;

    float in_v0;
    const float* in_voct;

    tr_buf out_sin;
    tr_buf out_sqr;
    tr_buf out_saw;
} tr_vco_t;
void tr_vco_update(tr_vco_t* vco);

typedef struct tr_clock
{
    float phase;

    float in_hz;
    tr_buf out_gate;
} tr_clock_t;
void tr_clock_update(tr_clock_t* clock);

typedef struct tr_seq8
{
    uint8_t step;
    uint8_t trig;

    const float* in_step;
    float in_cv_0;
    float in_cv_1;
    float in_cv_2;
    float in_cv_3;
    float in_cv_4;
    float in_cv_5;
    float in_cv_6;
    float in_cv_7;

    tr_buf out_cv;
} tr_seq8_t;
void tr_seq8_update(tr_seq8_t* seq);

typedef struct tr_adsr
{
    float value;
    int decay;

    float in_attack;
    float in_decay;
    float in_sustain;
    float in_release;
    
    const float* in_gate;

    tr_buf out_env;
} tr_adsr_t;
void tr_adsr_update(tr_adsr_t* adsr);

typedef struct tr_vca
{
    const float* in_audio;
    const float* in_cv;

    tr_buf out_mix;
} tr_vca_t;
void tr_vca_update(tr_vca_t* vca);

typedef struct tr_lp
{
    float value;

    const float* in_audio;
    const float* in_cut;
    float in_cut0;
    float in_cut_mul;

    tr_buf out_audio;
} tr_lp_t;
void tr_lp_update(tr_lp_t* lp);

typedef struct tr_mixer
{
    const float* in_0;
    const float* in_1;
    const float* in_2;
    const float* in_3;

    float in_vol0;
    float in_vol1;
    float in_vol2;
    float in_vol3;

    tr_buf out_mix;
} tr_mixer_t;
void tr_mixer_update(tr_mixer_t* mixer);

typedef struct tr_noise
{
    uint32_t rng;

    tr_buf out_white;
} tr_noise_t;
void tr_noise_update(tr_noise_t* noise);

enum tr_quantizer_mode
{
    TR_QUANTIZER_CHROMATIC,
    TR_QUANTIZER_MINOR,
    TR_QUANTIZER_MINOR_TRIAD,
    TR_QUANTIZER_MAJOR,
    TR_QUANTIZER_MAJOR_TRIAD,
    TR_QUANTIZER_PENTATONIC,
    TR_QUANTIZER_BLUES,

    TR_QUANTIZER_MODE_COUNT,
};

static const char* g_tr_quantizer_mode_name[TR_QUANTIZER_MODE_COUNT] = {
    "chromatic",
    "minor",
    "minor triad",
    "major",
    "major triad",
    "pentatonic",
    "blues",
};

typedef struct tr_quantizer
{
    int in_mode; // enum tr_quantizer_mode
    const float* in_cv;
    tr_buf out_cv;
} tr_quantizer_t;
void tr_quantizer_update(tr_quantizer_t* quantizer);

typedef struct tr_random
{
    uint64_t sample;

    float in_speed;
    tr_buf out_cv;
} tr_random_t;
void tr_random_update(tr_random_t* random);

typedef struct tr_speaker
{
    const float* in_audio;
} tr_speaker_t;

typedef struct tr_scope
{
    const float* in_0;
} tr_scope_t;