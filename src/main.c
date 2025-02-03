#include "timer.h"
#include "audio.h"

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <raylib.h>
#include <raymath.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#define TR_SAMPLE_RATE  44100
#define TR_SAMPLE_COUNT (2048) // 2048/44100 ~= 46ms

#define TR_PI 3.141592f
#define TR_TWOPI (2.0f * TR_PI)

#define WIDTH 1280
#define HEIGHT 720

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
    TR_SPEAKER,

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

typedef struct tr_vca
{
    const float* in_audio;
    const float* in_cv;

    tr_buf out_mix;
} tr_vca_t;

static void tr_vca_update(tr_vca_t* vca)
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

typedef struct tr_speaker
{
    const float* in_audio;
} tr_speaker_t;

//
// GUI
//

#define RACK_MODULE_POOL(Type) \
    typedef struct Type##_pool \
    { \
        size_t count; \
        Type##_t elements[1024]; \
    } Type##_pool_t;

RACK_MODULE_POOL(tr_vco);
RACK_MODULE_POOL(tr_clock);
RACK_MODULE_POOL(tr_seq8);
RACK_MODULE_POOL(tr_adsr);
RACK_MODULE_POOL(tr_vca);
RACK_MODULE_POOL(tr_lp);
RACK_MODULE_POOL(tr_mixer);
RACK_MODULE_POOL(tr_noise);
RACK_MODULE_POOL(tr_quantizer);
RACK_MODULE_POOL(tr_random);
RACK_MODULE_POOL(tr_speaker);

typedef struct tr_gui_module
{
    int x, y;
    enum tr_module_type type;
    size_t index;
} tr_gui_module_t;

#define TR_GUI_MODULE_COUNT 1024

typedef struct tr_plug
{
    int x, y;
    const tr_gui_module_t* module;
} tr_plug_t;

typedef struct tr_plug_kv
{
    const float* key;
    tr_plug_t value;
} tr_plug_kv_t;

typedef struct rack
{
    tr_vco_pool_t vco;
    tr_clock_pool_t clock;
    tr_seq8_pool_t seq8;
    tr_adsr_pool_t adsr;
    tr_vca_pool_t vca;
    tr_lp_pool_t lp;
    tr_mixer_pool_t mixer;
    tr_noise_pool_t noise;
    tr_quantizer_pool_t quantizer;
    tr_random_pool_t random;
    tr_speaker_pool_t speaker;
    
    tr_gui_module_t gui_modules[TR_GUI_MODULE_COUNT];
    size_t gui_module_count;
} rack_t;

size_t tr_rack_allocate_module(rack_t* rack, enum tr_module_type type)
{
    switch (type)
    {
        case TR_VCO: return rack->vco.count++;
        case TR_CLOCK: return rack->clock.count++;
        case TR_SEQ8: return rack->seq8.count++;
        case TR_ADSR: return rack->adsr.count++;
        case TR_VCA: return rack->vca.count++;
        case TR_LP: return rack->lp.count++;
        case TR_MIXER: return rack->mixer.count++;
        case TR_NOISE: return rack->noise.count++;
        case TR_QUANTIZER: return rack->quantizer.count++;
        case TR_RANDOM: return rack->random.count++;
        case TR_SPEAKER: return rack->speaker.count++;
    }
    assert(0);
    return (size_t)-1;
}

tr_gui_module_t* tr_rack_create_module(rack_t* rack, enum tr_module_type type)
{
    assert(rack->gui_module_count < TR_GUI_MODULE_COUNT);
    tr_gui_module_t* module = &rack->gui_modules[rack->gui_module_count++];
    memset(module, 0, sizeof(tr_gui_module_t));

    module->type = type;
    module->index = tr_rack_allocate_module(rack, type);

    return module;
}

void rack_init(rack_t* rack)
{
    tr_gui_module_t* speaker0 = tr_rack_create_module(rack, TR_SPEAKER);
    speaker0->x = WIDTH / 2;
    speaker0->y = HEIGHT / 2;
}

static void final_mix(int16_t* samples, const float* buffer)
{
    assert(buffer != NULL);
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        samples[i] = (int16_t)(buffer[i] * INT16_MAX);
    }
}

typedef struct tr_gui_modinfo
{
    const char* name;
    int width;
    int height;
} tr_gui_modinfo_t;

tr_gui_modinfo_t g_tr_gui_modinfo[TR_MODULE_COUNT] = {
    [TR_VCO] = {"vco", 100, 200},
    [TR_VCA] = {"vca", 200, 100},
    [TR_CLOCK] = {"clock", 100, 100},
    [TR_SEQ8] = {"seq8", 400, 100},
    [TR_ADSR] = {"adsr", 200, 100},
    [TR_LP] = {"lp", 250, 100},
    [TR_QUANTIZER] = {"quantizer", 250, 100},
    [TR_MIXER] = {"mixer", 250, 100},
    [TR_SPEAKER] = {"speaker", 100, 100},
    [TR_NOISE] = {"noise", 100, 100},
    [TR_RANDOM] = {"random", 100, 100},
};

typedef struct tr_gui_input
{
    void* active_value;
    float active_int; // for int knob
    tr_gui_module_t* drag_module;
    const float** drag_plug;
    tr_plug_kv_t* plugs;
    tr_gui_module_t* draw_module;
} tr_gui_input_t;

static tr_gui_input_t g_input;

#define TR_KNOB_RADIUS (18)
#define TR_KNOB_PADDING (2)
#define TR_PLUG_RADIUS (14)
#define TR_PLUG_PADDING (4)
#define TR_MODULE_PADDING (2)

void tr_gui_module_begin(tr_gui_module_t* module)
{
    g_input.draw_module = module;

    const tr_gui_modinfo_t* modinfo = &g_tr_gui_modinfo[module->type];
    DrawRectangle(module->x, module->y, modinfo->width, modinfo->height, GRAY);
    DrawRectangle(
        module->x + TR_MODULE_PADDING, 
        module->y + TR_MODULE_PADDING, 
        modinfo->width - TR_MODULE_PADDING * 2,
        modinfo->height - TR_MODULE_PADDING * 2,
        DARKGRAY);

    DrawText(
        modinfo->name, 
        module->x + TR_MODULE_PADDING * 2, 
        module->y + TR_MODULE_PADDING * 2,
        20,
        WHITE);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        const Vector2 mouse = GetMousePosition();
        if (mouse.x > module->x &&
            mouse.y > module->y &&
            mouse.x < module->x + modinfo->width && 
            mouse.y < module->y + 20)
        {
            g_input.drag_module = module;
        }
    }

    if (g_input.drag_module == module)
    {
        module->x += (int)GetMouseDelta().x;
        module->y += (int)GetMouseDelta().y;
    }
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

void tr_gui_plug_input(const float** value, int x, int y)
{
    (void)value;
    
    const bool dim = g_input.drag_plug != NULL && g_input.drag_plug != value;

    DrawCircle(x, y, TR_PLUG_RADIUS, LIGHTGRAY);
    DrawCircle(x, y, TR_PLUG_RADIUS - TR_PLUG_PADDING, dim ? LIGHTGRAY : DARKGRAY);
    
    const Vector2 center = {(float)x, (float)y};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (Vector2Distance(GetMousePosition(), center) < TR_PLUG_RADIUS)
        {
            g_input.drag_plug = value;
        }
    }

    if (g_input.drag_plug == value)
    {
        DrawLineEx(center, GetMousePosition(), 8.0f, ORANGE);
    }

    if (*value != NULL)
    {
        const tr_plug_t plug = hmget(g_input.plugs, *value);
        DrawLineEx(center, (Vector2){(float)plug.x, (float)plug.y}, 8.0f, RED);
    }
}

void tr_gui_plug_output(const float* buffer, int x, int y)
{
    DrawCircle(x, y, TR_PLUG_RADIUS, LIGHTGRAY);
    DrawCircle(x, y, TR_PLUG_RADIUS - TR_PLUG_PADDING, DARKGRAY);

    const Vector2 center = {(float)x, (float)y};
    if (g_input.drag_plug != NULL && 
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        Vector2Distance(GetMousePosition(), center) < TR_PLUG_RADIUS)
    {
        *g_input.drag_plug = buffer;
        g_input.drag_plug = NULL;
    }

    const tr_plug_t plug = {x, y, g_input.draw_module};
    hmput(g_input.plugs, buffer, plug);
}

void tr_vco_draw(tr_vco_t* vco, tr_gui_module_t* module)
{
    assert(module->type == TR_VCO);
    tr_gui_module_begin(module);
    tr_gui_knob(&vco->in_v0, 20.0f, 1000.0f, module->x + 24, module->y + 50);
    tr_gui_plug_input(&vco->in_voct, module->x + 20, module->y + 80);
    tr_gui_plug_output(vco->out_sin, module->x + 70, module->y + 50);
    tr_gui_plug_output(vco->out_saw, module->x + 70, module->y + 85);
    tr_gui_plug_output(vco->out_sqr, module->x + 70, module->y + 120);
    tr_gui_module_end();
}

void tr_clock_draw(tr_clock_t* clock, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    tr_gui_knob(&clock->in_hz, 0.01f, 50.0f, module->x + 24, module->y + 50);
    tr_gui_plug_output(clock->out_gate, module->x + 70, module->y + 50);
    tr_gui_module_end();
}

void tr_seq8_draw(tr_seq8_t* seq8, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    for (int i = 0; i < 8; ++i)
    {
        const int kx = module->x + 50 + (i * (TR_KNOB_RADIUS*2 + 4));
        const int ky = module->y + 50;
        tr_gui_knob(&seq8->in_cv[i], 0.001f, 1.0f, kx, ky);
        DrawCircle(kx, ky + TR_KNOB_RADIUS + 8, 4.0f, seq8->step == i ? GREEN : BLACK);
    }
    tr_gui_plug_input(&seq8->in_step, module->x + 20, module->y + 50);
    tr_gui_plug_output(seq8->out_cv, module->x + 380, module->y + 50);
    tr_gui_module_end();
}

void tr_adsr_draw(tr_adsr_t* adsr, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    const int x = module->x;
    const int y = module->y;
    tr_gui_knob(&adsr->in_attack, 0.001f, 1.0f, x + 24 + (0 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&adsr->in_decay, 0.001f, 1.0f, x + 24 + (1 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&adsr->in_sustain, 0.0f, 1.0f, x + 24 + (2 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&adsr->in_release, 0.001f, 1.0f, x + 24 + (3 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_plug_input(&adsr->in_gate, x + 24, y + 80);
    tr_gui_plug_output(adsr->out_env, x + 60, y + 80);
    tr_gui_module_end();
}

void tr_vca_draw(tr_vca_t* vca, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    const int x = module->x;
    const int y = module->y;
    tr_gui_plug_input(&vca->in_audio, x + 24 + (0 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_plug_input(&vca->in_cv, x + 24 + (1 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_plug_output(vca->out_mix, x + 24 + (2 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_module_end();
}

void tr_lp_draw(tr_lp_t* lp, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    const int x = module->x;
    const int y = module->y;
    tr_gui_knob(&lp->in_cut0, 0.0f, 1.0f, x + 24 + (0 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&lp->in_cut_mul, 0.0f, 2.0f, x + 24 + (1 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_plug_input(&lp->in_audio, x + 110, y + 50);
    tr_gui_plug_input(&lp->in_cut, x + 150, y + 50);
    tr_gui_plug_output(lp->out_audio, x + 190, y + 50);
    tr_gui_module_end();
}

void tr_mixer_draw(tr_mixer_t* mixer, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    const int x = module->x;
    const int y = module->y;
    tr_gui_knob(&mixer->in_vol0, 0.0f, 1.0f, x + 24 + (0 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&mixer->in_vol1, 0.0f, 1.0f, x + 24 + (1 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&mixer->in_vol2, 0.0f, 1.0f, x + 24 + (2 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_knob(&mixer->in_vol3, 0.0f, 1.0f, x + 24 + (3 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_plug_input(&mixer->in_0, x + 24 + (0 * (TR_KNOB_RADIUS*2+4)), y + 80);
    tr_gui_plug_input(&mixer->in_1, x + 24 + (1 * (TR_KNOB_RADIUS*2+4)), y + 80);
    tr_gui_plug_input(&mixer->in_2, x + 24 + (2 * (TR_KNOB_RADIUS*2+4)), y + 80);
    tr_gui_plug_input(&mixer->in_3, x + 24 + (3 * (TR_KNOB_RADIUS*2+4)), y + 80);
    tr_gui_plug_output(mixer->out_mix, x + 24 + (4 * (TR_KNOB_RADIUS*2+4)), y + 80);
    tr_gui_module_end();
}

void tr_noise_draw(tr_noise_t* noise, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    tr_gui_plug_output(noise->out_white, module->x + 24, module->y + 50);
    tr_gui_module_end();
}

void tr_random_draw(tr_random_t* random, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    tr_gui_knob(&random->in_speed, 0.0f, 10.0f, module->x + 24, module->y + 50);
    tr_gui_plug_output(random->out_cv, module->x + 80, module->y + 50);
    tr_gui_module_end();
}

void tr_quantizer_draw(tr_quantizer_t* quantizer, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    const int x = module->x;
    const int y = module->y;
    const int kx = x + 24;
    tr_gui_knob_int(&quantizer->in_mode, 0, TR_QUANTIZER_MODE_COUNT - 1, kx, y + 50);
    DrawText(g_tr_quantizer_mode_name[quantizer->in_mode], kx + TR_KNOB_RADIUS + 8, y + 40, 20, WHITE);
    tr_gui_plug_input(&quantizer->in_cv, x + 20, y + 80);
    tr_gui_plug_output(quantizer->out_cv, x + 80, y + 80);
    tr_gui_module_end();
}

void tr_speaker_draw(tr_speaker_t* speaker, tr_gui_module_t* module)
{
    tr_gui_module_begin(module);
    tr_gui_plug_input(&speaker->in_audio, module->x + 40, module->y + 50);
    tr_gui_module_end();
}

void tr_gui_module_draw(rack_t* rack, tr_gui_module_t* module)
{
    switch (module->type)
    {
        case TR_VCO: tr_vco_draw(&rack->vco.elements[module->index], module); break;
        case TR_CLOCK: tr_clock_draw(&rack->clock.elements[module->index], module); break;
        case TR_SEQ8: tr_seq8_draw(&rack->seq8.elements[module->index], module); break;
        case TR_ADSR: tr_adsr_draw(&rack->adsr.elements[module->index], module); break;
        case TR_VCA: tr_vca_draw(&rack->vca.elements[module->index], module); break;
        case TR_LP: tr_lp_draw(&rack->lp.elements[module->index], module); break;
        case TR_MIXER: tr_mixer_draw(&rack->mixer.elements[module->index], module); break;
        case TR_NOISE: tr_noise_draw(&rack->noise.elements[module->index], module); break;
        case TR_QUANTIZER: tr_quantizer_draw(&rack->quantizer.elements[module->index], module); break;
        case TR_RANDOM: tr_random_draw(&rack->random.elements[module->index], module); break;
        case TR_SPEAKER: tr_speaker_draw(&rack->speaker.elements[module->index], module); break;
    }
}

void tr_update_modules(rack_t* rack, tr_gui_module_t** modules, size_t count)
{
    printf("tr_update_modules:\n");
    for (size_t i = 0; i < count; ++i)
    {
        tr_gui_module_t* module = modules[i];
        switch (module->type)
        {
            case TR_VCO: tr_vco_update(&rack->vco.elements[module->index]); break;
            case TR_CLOCK: tr_clock_update(&rack->clock.elements[module->index]); break;
            case TR_SEQ8: tr_seq8_update(&rack->seq8.elements[module->index]); break;
            case TR_ADSR: tr_adsr_update(&rack->adsr.elements[module->index]); break;
            case TR_VCA: tr_vca_update(&rack->vca.elements[module->index]); break;
            case TR_LP: tr_lp_update(&rack->lp.elements[module->index]); break;
            case TR_MIXER: tr_mixer_update(&rack->mixer.elements[module->index]); break;
            case TR_NOISE: tr_noise_update(&rack->noise.elements[module->index]); break;
            case TR_QUANTIZER: tr_quantizer_update(&rack->quantizer.elements[module->index]); break;
            case TR_RANDOM: tr_random_update(&rack->random.elements[module->index]); break;
        }

        printf("%s %zu\n", g_tr_gui_modinfo[module->type].name, module->index);
    }
}

bool is_input_updated(rack_t* rack, const float* buffer, const uint32_t* flood_mask)
{
    if (buffer == NULL)
    {
        return true;
    }

    const tr_plug_t plug = hmget(g_input.plugs, buffer);
    const ptrdiff_t parent = plug.module - rack->gui_modules;
    return (flood_mask[parent / 32] & (1 << (parent % 32))) != 0;
}

int main()
{
    rack_t* rack = calloc(1, sizeof(rack_t));
    rack_init(rack);

    timer_t timer;
    timer_start(&timer);
    
    InitWindow(WIDTH, HEIGHT, "tinyrack");

    audio_t* audio = audio_create(TR_SAMPLE_RATE, TR_SAMPLE_COUNT);

    enum tr_module_type add_module_type = TR_VCO;

    while (1)
    {
        if (WindowShouldClose())
        {
            break;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        
        for (size_t i = 0; i < rack->gui_module_count; ++i)
        {
            tr_gui_module_draw(rack, &rack->gui_modules[i]);
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
        {
            g_input.active_value = NULL;
            g_input.drag_module = NULL;
            g_input.drag_plug = NULL;
        }
        
        {
            if (GetMouseWheelMoveV().y < 0.0f)
            {
                add_module_type += 1;
            }
            else if (GetMouseWheelMoveV().y > 0.0f)
            {
                add_module_type += TR_MODULE_COUNT - 1;
            }
            add_module_type = add_module_type % TR_MODULE_COUNT;

            const int menu_height = 42;

            DrawRectangle(0, 0, WIDTH, menu_height, GetColor(0x303030ff));
            DrawText(g_tr_gui_modinfo[add_module_type].name, 0, 0, 40, WHITE);

            const Vector2 mouse = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
                mouse.y < menu_height)
            {
                tr_gui_module_t* module = tr_rack_create_module(rack, add_module_type);
                module->x = (int)mouse.x;
                module->y = (int)mouse.y;
                g_input.drag_module = module;
            }
        }

        EndDrawing();

        int16_t* samples = audio_fill(audio);
        if (samples != NULL)
        {
            timer_reset(&timer);
            
            uint32_t flood_mask_0[TR_GUI_MODULE_COUNT / 32];
            uint32_t flood_mask_1[TR_GUI_MODULE_COUNT / 32];
            tr_gui_module_t* update_modules[TR_GUI_MODULE_COUNT];
            size_t update_count = 0;

            memset(flood_mask_0, 0, sizeof(flood_mask_0));
            memset(flood_mask_1, 0, sizeof(flood_mask_1));

            // Find root modules (modules that have no connected inputs i.e. no dependencies)
            for (size_t i = 0; i < rack->gui_module_count; ++i)
            {
                tr_gui_module_t* module = &rack->gui_modules[i];
                
                size_t connected_input_count = 0;
                
                switch (module->type)
                {
                    case TR_VCO:
                        const tr_vco_t* vco = &rack->vco.elements[module->index];
                        if (vco->in_voct != NULL) ++connected_input_count;
                        break;
                    case TR_SEQ8:
                        const tr_seq8_t* seq8 = &rack->seq8.elements[module->index];
                        if (seq8->in_step != NULL) ++connected_input_count;
                        break;
                    case TR_SPEAKER:
                        const tr_speaker_t* speaker = &rack->speaker.elements[module->index];
                        if (speaker->in_audio != NULL) ++connected_input_count;
                        break;
                    case TR_QUANTIZER:
                        const tr_quantizer_t* quantizer = &rack->quantizer.elements[module->index];
                        if (quantizer->in_cv != NULL) ++connected_input_count;
                        break;
                    case TR_LP:
                        const tr_lp_t* lp = &rack->lp.elements[module->index];
                        if (lp->in_audio != NULL) ++connected_input_count;
                        if (lp->in_cut != NULL) ++connected_input_count;
                        break;
                    case TR_ADSR:
                        const tr_adsr_t* adsr = &rack->adsr.elements[module->index];
                        if (adsr->in_gate != NULL) ++connected_input_count;
                        break;
                    case TR_MIXER:
                        const tr_mixer_t* mixer = &rack->mixer.elements[module->index];
                        if (mixer->in_0 != NULL) ++connected_input_count;
                        if (mixer->in_1 != NULL) ++connected_input_count;
                        if (mixer->in_2 != NULL) ++connected_input_count;
                        if (mixer->in_3 != NULL) ++connected_input_count;
                        break;
                }

                if (connected_input_count == 0)
                {
                    flood_mask_1[i / 32] |= (1 << (i % 32));
                    update_modules[update_count++] = module;
                }
            }

            while (update_count != rack->gui_module_count)
            {
                memcpy(flood_mask_0, flood_mask_1, sizeof(flood_mask_0));

                // Find modules whose dependencies are all updated
                for (size_t i = 0; i < rack->gui_module_count; ++i)
                {
                    tr_gui_module_t* module = &rack->gui_modules[i];

                    if (flood_mask_0[i / 32] & (1 << (i % 32)))
                    {
                        // already updated
                        continue;
                    }
                    
                    bool all_inputs_valid = true;
                    
                    switch (module->type)
                    {
                        case TR_SPEAKER:
                            const tr_speaker_t* speaker = &rack->speaker.elements[module->index];
                            all_inputs_valid &= is_input_updated(rack, speaker->in_audio, flood_mask_0);
                            break;
                        case TR_SEQ8:
                            const tr_seq8_t* seq8 = &rack->seq8.elements[module->index];
                            all_inputs_valid &= is_input_updated(rack, seq8->in_step, flood_mask_0);
                            break;
                        case TR_VCO:
                            const tr_vco_t* vco = &rack->vco.elements[module->index];
                            all_inputs_valid &= is_input_updated(rack, vco->in_voct, flood_mask_0);
                            break;
                        case TR_QUANTIZER:
                            const tr_quantizer_t* quantizer = &rack->quantizer.elements[module->index];
                            all_inputs_valid &= is_input_updated(rack, quantizer->in_cv, flood_mask_0);
                            break;
                        case TR_LP:
                            const tr_lp_t* lp = &rack->lp.elements[module->index];
                            all_inputs_valid &= is_input_updated(rack, lp->in_audio, flood_mask_0);
                            all_inputs_valid &= is_input_updated(rack, lp->in_cut, flood_mask_0);
                            break;
                        case TR_ADSR:
                            const tr_adsr_t* adsr = &rack->adsr.elements[module->index];
                            all_inputs_valid &= is_input_updated(rack, adsr->in_gate, flood_mask_0);
                            break;
                        case TR_MIXER:
                            const tr_mixer_t* mixer = &rack->mixer.elements[module->index];
                            all_inputs_valid &= is_input_updated(rack, mixer->in_0, flood_mask_0);
                            all_inputs_valid &= is_input_updated(rack, mixer->in_1, flood_mask_0);
                            all_inputs_valid &= is_input_updated(rack, mixer->in_2, flood_mask_0);
                            all_inputs_valid &= is_input_updated(rack, mixer->in_3, flood_mask_0);
                            break;
                    }

                    if (all_inputs_valid)
                    {
                        flood_mask_1[i / 32] |= (1 << (i % 32));
                        update_modules[update_count++] = module;
                    }
                }
            }

            tr_update_modules(rack, update_modules, update_count);

            tr_gui_module_t* speaker = NULL;
            for (size_t i = 0; i < rack->gui_module_count; ++i)
            {
                tr_gui_module_t* module = &rack->gui_modules[i];
                if (module->type == TR_SPEAKER &&
                    rack->speaker.elements[module->index].in_audio != NULL)
                {
                    speaker = module;
                    break;
                }
            }

            if (speaker == NULL)
            {
                memset(samples, 0, sizeof(int16_t) * TR_SAMPLE_COUNT);
            }
            else
            {
                assert(speaker->type == TR_SPEAKER);
                final_mix(samples, rack->speaker.elements[speaker->index].in_audio);
            }

            const double ms = timer_reset(&timer);
            printf("final_mix: %f us\n", ms * 1000.0);
        }
    }

    return 0;
}
