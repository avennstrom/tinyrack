
#include "timer.h"
#include "audio.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <raylib.h>
#include <raymath.h>

#define TR_SAMPLE_RATE  44100
#define TR_SAMPLE_COUNT (2048) // 2048/44100 ~= 46ms

#define TR_PI 3.141592f
#define TR_TWOPI (2.0f * TR_PI)

typedef float tr_buf[TR_SAMPLE_COUNT];

float tr_hz(float voct)
{
    return voct * 100.0f;
}

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

    TR_MODULE_COUNT,
};

typedef struct tr_vco
{
    float phase;

    float in_v0;
    const float* in_voct;

    tr_buf out_sin;
    tr_buf out_sqr;
    tr_buf out_saw;
} tr_vco_t;

static void tr_vco_update(tr_vco_t* vco)
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

typedef struct tr_clock
{
    float phase;

    float in_hz;
    tr_buf out_gate;
} tr_clock_t;

static void tr_clock_update(tr_clock_t* clock)
{
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        clock->phase += (clock->in_hz / TR_SAMPLE_RATE) * TR_TWOPI;
        clock->phase = fmodf(clock->phase, TR_TWOPI);
        const float s = sinf(clock->phase);
        clock->out_gate[i] = signbit(s) ? 0.0f : 5.0f;
    }
}

typedef struct tr_seq8
{
    uint8_t step;
    uint8_t trig;

    const float* in_step;
    float in_cv[8];

    tr_buf out_cv;
} tr_seq8_t;

static void tr_seq8_update(tr_seq8_t* seq)
{
    assert(seq->in_step != NULL);

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        const float gate = seq->in_step[i];
        const uint8_t trig = gate > 0.0f;
        
        if (trig != seq->trig)
        {
            if (trig)
            {
                seq->step = (seq->step + 1) % 8;
            }
            seq->trig = trig;
        }
        
        seq->out_cv[i] = seq->in_cv[seq->step];
    }
}

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

static void tr_adsr_update(tr_adsr_t* adsr)
{
    assert(adsr->in_gate != NULL);

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

typedef struct tr_vca
{
    const float* in_audio;
    const float* in_cv;

    tr_buf out_mix;
} tr_vca_t;

static void tr_vca_update(tr_vca_t* vca)
{
    assert(vca->in_audio != NULL);
    assert(vca->in_cv != NULL);

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        vca->out_mix[i] = vca->in_audio[i] * vca->in_cv[i];
    }
}

typedef struct tr_lp
{
    float value;

    const float* in_audio;
    const float* in_cut;
    float in_cut0;
    float in_cut_mul;

    tr_buf out_audio;
} tr_lp_t;

static void tr_lp_update(tr_lp_t* lp)
{
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

void tr_mixer_accum(float* mix, const float* in, float mul)
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
        mixer->out_mix[i] = tanhf(mixer->out_mix[i]);
    }
}

typedef struct tr_noise
{
    uint32_t rng;

    tr_buf out_white;
} tr_noise_t;

void tr_noise_update(tr_noise_t* noise)
{
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        noise->rng = noise->rng * 1664525u + 1013904223u;
        noise->out_white[i] = (noise->rng / (float)UINT32_MAX) * 2.0f - 1.0f;
    }
}

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

void tr_quantizer_apply_lut(tr_quantizer_t* quantizer, const uint8_t* lut)
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

typedef struct tr_random
{
    uint64_t sample;

    float in_speed;
    tr_buf out_cv;
} tr_random_t;

void tr_random_update(tr_random_t* random)
{
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        ++random->sample;

        const float t = (random->sample / (float)TR_SAMPLE_RATE) * random->in_speed;

        random->out_cv[i] = sinf(t) * sinf(t * 3.0f) * sinf(t * 5.0f);
    }
}

typedef struct rack
{
    tr_vco_t vco0;
    tr_clock_t clock0;
    tr_seq8_t seq0;
    tr_adsr_t adsr0;
    tr_lp_t lp0;
    tr_quantizer_t quantizer0;
    tr_mixer_t mixer0;
    const float* master;
} rack_t;

void rack_init(rack_t* rack)
{
    rack->vco0 = (tr_vco_t){
        .in_v0 = 440.0f / 2.0f,
        .in_voct = rack->quantizer0.out_cv,
    };
    rack->clock0 = (tr_clock_t){
        .in_hz = 5.0f,
    };
    rack->seq0 = (tr_seq8_t){
        .in_step = rack->clock0.out_gate,
        .in_cv = {
            0.0f / 12.0f,
            3.0f / 12.0f,
            7.0f / 12.0f,
            0.0f / 12.0f,
            3.0f / 12.0f,
            7.0f / 12.0f,
            0.0f / 12.0f,
            2.0f / 12.0f,
        },
    };
    rack->adsr0 = (tr_adsr_t){
        .in_attack = 0.01f,
        .in_decay = 0.2f,
        .in_sustain = 0.0f,
        .in_release = 0.15f,
        .in_gate = rack->clock0.out_gate,
    };
    rack->lp0 = (tr_lp_t){
        .in_audio = rack->vco0.out_sqr,
        .in_cut = rack->adsr0.out_env,
        .in_cut_mul = 0.12f,
    };
    rack->quantizer0 = (tr_quantizer_t){
        .in_mode = TR_QUANTIZER_MINOR,
        .in_cv = rack->seq0.out_cv,
    };
    rack->mixer0 = (tr_mixer_t){
        .in_0 = rack->lp0.out_audio,
        .in_vol0 = 0.6f,
    };
    rack->master = rack->mixer0.out_mix;
}

static void final_mix(int16_t* samples, rack_t* rack)
{
    tr_clock_update(&rack->clock0);
    tr_adsr_update(&rack->adsr0);
    tr_seq8_update(&rack->seq0);
    tr_quantizer_update(&rack->quantizer0);
    tr_vco_update(&rack->vco0);
    tr_lp_update(&rack->lp0);
    tr_mixer_update(&rack->mixer0);

    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        samples[i] = (int16_t)(rack->master[i] * INT16_MAX);
    }
}

typedef struct tr_gui_modinfo
{
    const char* name;
    int width;
    int height;
} tr_gui_modinfo_t;

tr_gui_modinfo_t g_tr_gui_modinfo[TR_MODULE_COUNT] = {
    [TR_VCO] = {"vco", 100, 100},
    [TR_CLOCK] = {"clock", 100, 100},
    [TR_SEQ8] = {"seq8", 400, 100},
    [TR_ADSR] = {"adsr", 200, 100},
    [TR_LP] = {"lp", 250, 100},
    [TR_QUANTIZER] = {"quantizer", 250, 100},
};

typedef struct tr_gui_input
{
    void* active_value;
    float active_int; // for int knob
} tr_gui_input_t;

static tr_gui_input_t g_input;

#define TR_KNOB_RADIUS (18)
#define TR_KNOB_PADDING (2)
#define TR_MODULE_PADDING (2)

void tr_gui_module_begin(int x, int y, enum tr_module_type type)
{
    const tr_gui_modinfo_t* modinfo = &g_tr_gui_modinfo[type];
    DrawRectangle(x, y, modinfo->width, modinfo->height, GRAY);
    DrawRectangle(
        x + TR_MODULE_PADDING, 
        y + TR_MODULE_PADDING, 
        modinfo->width - TR_MODULE_PADDING * 2,
        modinfo->height - TR_MODULE_PADDING * 2,
        DARKGRAY);

    DrawText(
        modinfo->name, 
        x + TR_MODULE_PADDING * 2, 
        y + TR_MODULE_PADDING * 2,
        20,
        WHITE);
}

void tr_gui_module_end(void)
{
}

void tr_gui_knob_base(float value, float min, float max, int x, int y)
{
    DrawCircle(x, y, TR_KNOB_RADIUS, LIGHTGRAY);
    DrawCircle(x, y, TR_KNOB_RADIUS - TR_KNOB_PADDING, GRAY);
    
    const float t = (value - min) / (max - min);
    const float t0 = (t - 0.5f) * 0.9f;
    const float dot_x = sinf(t0 * TR_TWOPI);
    const float dot_y = cosf(t0 * TR_TWOPI);
    
    DrawCircle(
        (int)(x + dot_x * TR_KNOB_RADIUS*0.9f),
        (int)(y - dot_y * TR_KNOB_RADIUS*0.9f),
        4.0f,
        ORANGE);
}

void tr_gui_knob(float* value, float min, float max, int x, int y)
{
    tr_gui_knob_base(*value, min, max, x, y);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        const Vector2 center = {(float)x, (float)y};
        if (Vector2Distance(GetMousePosition(), center) < TR_KNOB_RADIUS)
        {
            g_input.active_value = value;
        }
    }
    
    if (g_input.active_value == value)
    {
        const float range = (max - min);
        *value -= GetMouseDelta().y * range * 0.01f;
        if (*value < min) *value = min;
        if (*value > max) *value = max;
    }
}

void tr_gui_knob_int(int* value, int min, int max, int x, int y)
{
    tr_gui_knob_base((float)*value, (float)min, (float)max, x, y);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        const Vector2 center = {(float)x, (float)y};
        if (Vector2Distance(GetMousePosition(), center) < TR_KNOB_RADIUS)
        {
            g_input.active_value = value;
            g_input.active_int = (float)*value;
        }
    }
    
    if (g_input.active_value == value)
    {
        g_input.active_int -= GetMouseDelta().y * (max - min) * 0.01f;

        *value = (int)roundf(g_input.active_int);
        if (*value < min) *value = min;
        if (*value > max) *value = max;
    }
}

void tr_vco_draw(tr_vco_t* vco, int x, int y)
{
    tr_gui_module_begin(x, y, TR_VCO);
    tr_gui_knob(&vco->in_v0, 20.0f, 1000.0f, x + 24, y + 50);
    tr_gui_module_end();
}

void tr_clock_draw(tr_clock_t* clock, int x, int y)
{
    tr_gui_module_begin(x, y, TR_CLOCK);
    tr_gui_knob(&clock->in_hz, 0.01f, 50.0f, x + 24, y + 50);
    tr_gui_module_end();
}

void tr_seq8_draw(tr_seq8_t* seq8, int x, int y)
{
    tr_gui_module_begin(x, y, TR_SEQ8);
    for (int i = 0; i < 8; ++i)
    {
        const int kx = x + 24 + (i * (TR_KNOB_RADIUS*2 + 4));
        const int ky = y + 50;
        tr_gui_knob(&seq8->in_cv[i], 0.001f, 1.0f, kx, ky);
        DrawCircle(kx, ky + TR_KNOB_RADIUS + 8, 4.0f, seq8->step == i ? GREEN : BLACK);
    }
    tr_gui_module_end();
}

void tr_adsr_draw(tr_adsr_t* adsr, int x, int y)
{
    tr_gui_module_begin(x, y, TR_ADSR);
    tr_gui_knob(&adsr->in_attack, 0.001f, 1.0f, x + 24 + (0 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&adsr->in_decay, 0.001f, 1.0f, x + 24 + (1 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&adsr->in_sustain, 0.0f, 1.0f, x + 24 + (2 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&adsr->in_release, 0.001f, 1.0f, x + 24 + (3 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_module_end();
}

void tr_lp_draw(tr_lp_t* lp, int x, int y)
{
    tr_gui_module_begin(x, y, TR_LP);
    tr_gui_knob(&lp->in_cut0, 0.0f, 1.0f, x + 24 + (0 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&lp->in_cut_mul, 0.0f, 2.0f, x + 24 + (1 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_module_end();
}

void tr_quantizer_draw(tr_quantizer_t* quantizer, int x, int y)
{
    tr_gui_module_begin(x, y, TR_QUANTIZER);
    const int kx = x + 24;
    tr_gui_knob_int(&quantizer->in_mode, 0, TR_QUANTIZER_MODE_COUNT - 1, kx, y + 50);
    DrawText(g_tr_quantizer_mode_name[quantizer->in_mode], kx + TR_KNOB_RADIUS + 8, y + 40, 20, WHITE);
    tr_gui_module_end();
}

typedef struct tr_gui_module
{
    int x, y;
    enum tr_module_type type;
    void* module;
} tr_gui_module_t;

int main()
{
    rack_t rack = {0};
    rack_init(&rack);

    timer_t timer;
    timer_start(&timer);
    
    InitWindow(640, 480, "tinyrack");

    audio_t* audio = audio_create(TR_SAMPLE_RATE, TR_SAMPLE_COUNT);

    while (1)
    {
        if (WindowShouldClose())
        {
            break;
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            g_input.active_value = NULL;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        
        tr_vco_draw(&rack.vco0, 20, 20);
        tr_clock_draw(&rack.clock0, 140, 20);
        tr_adsr_draw(&rack.adsr0, 260, 20);
        tr_seq8_draw(&rack.seq0, 20, 140);
        tr_lp_draw(&rack.lp0, 290, 260);
        tr_quantizer_draw(&rack.quantizer0, 20, 260);

        EndDrawing();

        int16_t* samples = audio_fill(audio);
        if (samples != NULL)
        {
            timer_reset(&timer);
            final_mix(samples, &rack);
            const double ms = timer_reset(&timer);

            printf("final_mix: %f us\n", ms * 1000.0);
        }
    }

    return 0;
}
