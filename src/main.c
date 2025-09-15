#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifndef __EMSCRIPTEN__
//#define TR_RECORDING_FEATURE
#endif

#include "timer.h"
#include "modules.h"
#include "modules.reflection.h"
#include "parser.h"
#include "platform.h"
#include "math.h"
#include "strbuf.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <float.h>
#include <string.h>

// debug stuff
#define TR_TRACE_MODULE_UPDATES 0
#define TR_TRACE_MODULE_GRAPH 0

#define TR_KNOB_RADIUS (18)
#define TR_KNOB_PADDING (2)
#define TR_KNOB_TIP_WIDTH (4.0f)
#define TR_KNOB_TIP_SIZE (0.5f) // multiplied by radius
#define TR_PLUG_RADIUS (12)
#define TR_PLUG_PADDING (4)
#define TR_PLUG_SNAP_RADIUS (TR_PLUG_RADIUS + 8)
#define TR_MODULE_PADDING (2)

#define COLOR_BACKGROUND COLOR_FROM_RGBA_HEX(0x3c5377ff)
#define COLOR_MODULE_BACKGROUND COLOR_FROM_RGBA_HEX(0xfff3c2ff)
#define COLOR_MODULE_TEXT COLOR_FROM_RGBA_HEX(0x3c5377ff)
#define COLOR_PLUG_HOLE COLOR_FROM_RGBA_HEX(0x303030ff)
#define COLOR_PLUG_BORDER COLOR_FROM_RGBA_HEX(0xa5a5a5ff)
#define COLOR_PLUG_HIGHLIGHT ((color_t){228, 168, 27, 255})
#define COLOR_PLUG_DISABLED ((color_t){200, 200, 200, 255})
#define COLOR_PLUG_OUTPUT COLOR_FROM_RGBA_HEX(0xddc55eff)
#define COLOR_KNOB COLOR_FROM_RGBA_HEX(0xddc55eff)
#define COLOR_KNOB_TIP COLOR_FROM_RGBA_HEX(0xffffffff)

#define COLOR_LED_ON ((color_t){0, 228, 48, 255})
#define COLOR_LED_OFF ((color_t){0, 0, 0, 255})

#define COLOR_WHITE ((color_t){255, 255, 255, 255})
#define COLOR_BLACK ((color_t){0, 0, 0, 255})

#define TR_CABLE_ALPHA 0.75f

#define TR_MODULE_DATA_POOL_SIZE (16 * 1024 * 1024)

typedef struct tr_module_pool
{
    uint8_t* data;
    size_t offset;
} tr_module_pool_t;

static void* tr_module_pool_alloc(tr_module_pool_t* pool, size_t size)
{
    void* data = pool->data + pool->offset;
    pool->offset += size;
    return data;
}

typedef struct tr_gui_module
{
    float x, y;
    enum tr_module_type type;
    void* data; // pointer to the real module data (tr_vco_t, tr_clock_t, etc...) based on type
} tr_gui_module_t;

#define TR_GUI_MODULE_COUNT 1024
#define TR_MAX_CABLES (4 * 1024)

typedef struct tr_output_plug
{
    float x, y;
    const tr_gui_module_t* module;
} tr_output_plug_t;

typedef struct tr_input_plug
{
    color_t color;
} tr_input_plug_t;

typedef struct rack
{
    tr_module_pool_t module_pool;
    tr_gui_module_t gui_modules[TR_GUI_MODULE_COUNT];
    size_t gui_module_count;
    uint32_t output_plugs_key[TR_MAX_CABLES];
    tr_output_plug_t output_plugs[TR_MAX_CABLES];
    uint32_t input_plugs_key[TR_MAX_CABLES];
    tr_input_plug_t input_plugs[TR_MAX_CABLES];
} rack_t;

static uint8_t g_module_pool_memory[64 * 1024 * 1024];
static uint8_t g_null_module[64 * 1024];

static uint8_t g_rb_memory[1024 * 1024];
static render_buffer_t g_rb = {g_rb_memory};

static inline uint32_t tr_ptr_hash32(const void *p)
{
    uint32_t x = (uint32_t)(uintptr_t)p;
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// -1 if not found
static int tr_hmget(const uint32_t* keys, const void* key)
{
    const uint32_t hash = tr_ptr_hash32(key);
    const size_t offset = hash % TR_MAX_CABLES;
    for (size_t i = 0; i < TR_MAX_CABLES; ++i)
    {
        const size_t j = (offset + i) % TR_MAX_CABLES;
        const uint32_t k = keys[j];
        if (k == hash)
        {
            return (int)j;
        }
        else if (k == 0u)
        {
            return -1;
        }
    }

    return -1;
}

static int tr_hmput(uint32_t* keys, const void* key)
{
    const uint32_t hash = tr_ptr_hash32(key);
    const size_t offset = hash % TR_MAX_CABLES;

    for (size_t i = 0; i < TR_MAX_CABLES; ++i)
    {
        const size_t j = (offset + i) % TR_MAX_CABLES;
        const uint32_t k = keys[j];
        if (k == 0u || k == hash)
        {
            keys[j] = hash;
            return (int)j;
        }
    }

    return -1; // we goofed
}

static void draw_rectangle_rounded(rectangle_t rec, float roundness, color_t color)
{
    cmd_draw_rectangle_rounded_t* cmd = rb_draw_rectangle_rounded(&g_rb);
    cmd->color = color;
    cmd->position = (float2){rec.x, rec.y};
    cmd->size = (float2){rec.width, rec.height};
    cmd->roundness = roundness;
}

void draw_text(font_t font, const char *text, float2 position, float fontSize, float spacing, color_t tint)
{
    (void)font;
    (void)spacing;
    rb_draw_text(&g_rb, text, position, fontSize, tint);
}

size_t tr_get_gui_module_index(const rack_t* rack, const tr_gui_module_t* module)
{
    return module - rack->gui_modules;
}

void* get_field_address(const tr_gui_module_t* module, size_t field_index)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    const size_t field_offset = module_info->fields[field_index].offset;
    return (uint8_t*)module->data + field_offset;
}

tr_gui_module_t* tr_rack_create_module(rack_t* rack, enum tr_module_type type)
{
    assert(rack->gui_module_count < TR_GUI_MODULE_COUNT);
    tr_gui_module_t* module = &rack->gui_modules[rack->gui_module_count];
    memset(module, 0, sizeof(tr_gui_module_t));
    module->type = type;

    //tr_module_pool_t* pool = &rack->module_pool[type];
    //assert(pool->elements != NULL);
    //const size_t module_data_index = pool->count++;
    //module->data = tr_module_pool_get(pool, module_data_index);

    const tr_module_info_t* module_info = &tr_module_infos[type];
    module->data = tr_module_pool_alloc(&rack->module_pool, module_info->struct_size);
    
    for (size_t i = 0; i < module_info->field_count; ++i)
    {
        const tr_module_field_info_t* field_info = &module_info->fields[i];
        switch (field_info->type)
        {
            case TR_MODULE_FIELD_INPUT_FLOAT:
                memcpy(get_field_address(module, i), &field_info->default_value, sizeof(field_info->default_value));
                break;
            default: break;
        }
    }

    ++rack->gui_module_count;
    return module;
}

void rack_init(rack_t* rack)
{
    memset(rack, 0, sizeof(rack_t));
    rack->module_pool.data = g_module_pool_memory;
}

typedef struct tr_cable_draw_command
{
    float2 from;
    float2 to;
    color_t color;
} tr_cable_draw_command_t;

typedef enum tr_input_type
{
    TR_INPUT_TYPE_KNOB,
    TR_INPUT_TYPE_INPUT_PLUG,
    TR_INPUT_TYPE_OUTPUT_PLUG,
    TR_INPUT_TYPE_COUNT,
} tr_input_type_t;

static const float g_input_snap_radius[TR_INPUT_TYPE_COUNT] = {
    TR_KNOB_RADIUS,
    TR_PLUG_SNAP_RADIUS,
    TR_PLUG_SNAP_RADIUS,
};

typedef struct tr_gui_input
{
    void* active_value;
    float active_int; // for int knob
    tr_gui_module_t* drag_module;
    float2 drag_offset;
    const float** drag_input;
    const float* drag_output;
    color_t drag_color;
    bool do_not_process_input;
    
    float closest_distance[TR_INPUT_TYPE_COUNT];
    const tr_gui_module_t* closest_module[TR_INPUT_TYPE_COUNT];
    size_t closest_field[TR_INPUT_TYPE_COUNT];
    tr_input_type_t closest_input;

    tr_cable_draw_command_t cable_draws[1024];
    size_t cable_draw_count;

    camera_t camera;
} tr_gui_input_t;

void tr_update_closest_input_state(tr_gui_input_t* state, const rack_t* rack, float2 mouse)
{
    for (int i = 0; i < TR_INPUT_TYPE_COUNT; ++i)
    {
        state->closest_distance[i] = FLT_MAX;
        state->closest_module[i] = NULL;
        state->closest_field[i] = SIZE_MAX;
    }
    
    for (size_t module_index = 0; module_index < rack->gui_module_count; ++module_index)
    {
        const tr_gui_module_t* module = &rack->gui_modules[module_index];
        const tr_module_info_t* module_info = &tr_module_infos[module->type];
        for (size_t i = 0; i < module_info->field_count; ++i)
        {
            const tr_module_field_info_t* field_info = &module_info->fields[i];

            tr_input_type_t input_type = TR_INPUT_TYPE_COUNT;
            switch (field_info->type)
            {
                case TR_MODULE_FIELD_INPUT_BUFFER:
                    input_type = TR_INPUT_TYPE_INPUT_PLUG;
                    break;
                case TR_MODULE_FIELD_BUFFER:
                    input_type = TR_INPUT_TYPE_OUTPUT_PLUG;
                    break;
                case TR_MODULE_FIELD_INPUT_FLOAT:
                case TR_MODULE_FIELD_INPUT_INT:
                    input_type = TR_INPUT_TYPE_KNOB;
                    break;
                default:
                    break;
            }

            if (input_type == TR_INPUT_TYPE_COUNT)
            {
                continue;
            }

            const float2 center = {(float)module->x + field_info->x, (float)module->y + field_info->y};
            const float distance = float2_distance(center, mouse) - g_input_snap_radius[input_type];
            
            if (distance < state->closest_distance[input_type])
            {
                state->closest_distance[input_type] = distance;
                state->closest_module[input_type] = module;
                state->closest_field[input_type] = i;
            }
        }
    }

    tr_input_type_t closest_input_type = TR_INPUT_TYPE_COUNT;
    float closest_input_distance = FLT_MAX;

    for (int i = 0; i < TR_INPUT_TYPE_COUNT; ++i)
    {
        if (state->closest_distance[i] < closest_input_distance)
        {
            closest_input_distance = state->closest_distance[i];
            closest_input_type = i;
        }
    }

    state->closest_input = closest_input_type;
}

static float2 tr_snap_to_input(tr_gui_input_t* state, tr_input_type_t input_type, float2 pos)
{
    const tr_gui_module_t* module = state->closest_module[input_type];
    if (module == NULL)
    {
        return pos;
    }

    const size_t field_index = state->closest_field[input_type];
    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    const tr_module_field_info_t* field_info = &module_info->fields[field_index];

    const float2 center = {(float)module->x + field_info->x, (float)module->y + field_info->y};
    //const bool in_range = float2_distance(pos, center) < 0.0f;
    const bool in_range = state->closest_distance[input_type] < 0.0f;
    return in_range ? center : pos;
}

static tr_gui_input_t g_input = {
    .camera.zoom = 1.0f,
};

typedef struct timer_buffer
{
    timer_t timer;
    size_t index;
    float samples[32];
} timer_buffer_t;

static float tb_avg(const timer_buffer_t* tb)
{
    float avg = 0.0f;
    for (size_t i = 0; i < tr_countof(tb->samples); ++i)
    {
        avg += tb->samples[i];
    }
    return avg * (1.0f / tr_countof(tb->samples));
}

static void tb_start(timer_buffer_t* tb)
{
    timer_start(&tb->timer);
}

static void tb_stop(timer_buffer_t* tb)
{
    tb->samples[tb->index] = (float)timer_reset(&tb->timer);
    tb->index = (tb->index + 1) % tr_countof(tb->samples);
}

typedef struct app
{
    rack_t rack;
#ifdef TR_RECORDING_FEATURE
    bool is_recording;
    int16_t* recording_samples;
    size_t recording_offset;
#endif

    float final_mix[TR_SAMPLE_COUNT];
    size_t final_mix_remaining;
    bool has_audio_callback_been_called_once;

    bool picker_mode;
    bool paused;
    bool single_step;
    float fadein;

    timer_buffer_t tb_resolve_module_graph;
    timer_buffer_t tb_produce_final_mix;
    timer_buffer_t tb_frame_update_draw;
    timer_buffer_t tb_draw_modules;
    timer_buffer_t tb_update_input;
} app_t;

static app_t g_app = {0};

static const color_t g_cable_colors[] = 
{
    { 255, 153, 148 },
    { 148, 148, 255 },
    { 148, 255, 188 },
    { 239, 145, 21 },
};

static color_t tr_random_cable_color()
{
    return g_cable_colors[rand() % tr_countof(g_cable_colors)];
}

void tr_serialize_input_float(tr_strbuf_t* sb, const char* name, float value)
{
    sb_append_cstring(sb, "input ");
    sb_append_cstring(sb, name);
    sb_append_cstring(sb, " ");
    sb_append_hex_float(sb, value);
    sb_append_cstring(sb, "\n");
}

void tr_serialize_input_int(tr_strbuf_t* sb, const char* name, int value)
{
    sb_append_cstring(sb, "input ");
    sb_append_cstring(sb, name);
    sb_append_cstring(sb, " ");
    sb_append_int(sb, value);
    sb_append_cstring(sb, "\n");
}

void tr_serialize_input_buffer(tr_strbuf_t* sb, const rack_t* rack, const char* name, const float** value)
{
    const float* buffer = *value;
    if (buffer == NULL)
    {
        return;
    }

    const int plug_idx = tr_hmget(rack->output_plugs_key, buffer);
    assert(plug_idx != -1);
    const tr_output_plug_t* plug = &rack->output_plugs[plug_idx];

    const size_t module_index = tr_get_gui_module_index(rack, plug->module);

    const tr_module_info_t* module_info = &tr_module_infos[plug->module->type];

    //const void* module_addr = tr_get_module_address(ctx->rack, plug->module->type, plug->module->index);
    const uintptr_t field_offset = (uintptr_t)buffer - (uintptr_t)plug->module->data;
    
    const tr_module_field_info_t* field_info = NULL;

    for (int i = 0; i < module_info->field_count; ++i)
    {
        if (field_offset == module_info->fields[i].offset)
        {
            field_info = &module_info->fields[i];
            break;
        }
    }
    assert(field_info != NULL);
    assert(field_info->type == TR_MODULE_FIELD_BUFFER);

    sb_append_cstring(sb, "input_buffer ");
    sb_append_cstring(sb, name);
    sb_append_cstring(sb, " > ");
    sb_append_cstring(sb, module_info->id);
    sb_append_cstring(sb, " ");
    sb_append_int(sb, (int)module_index);
    sb_append_cstring(sb, " ");
    sb_append_cstring(sb, field_info->name);
    sb_append_cstring(sb, "\n");
}

int tr_rack_serialize(tr_strbuf_t* sb, rack_t* rack)
{
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        const tr_gui_module_t* module = &rack->gui_modules[i];
        const char* name = tr_module_infos[module->type].id;
        
        sb_append_cstring(sb, "module ");
        sb_append_cstring(sb, name);
        sb_append_cstring(sb, " ");
        sb_append_int(sb, (int)i);
        sb_append_cstring(sb, " pos ");
        sb_append_int(sb, (int)module->x);
        sb_append_cstring(sb, " ");
        sb_append_int(sb, (int)module->y);
        sb_append_cstring(sb, "\n");
        
        const void* module_addr = module->data;
        
        const tr_module_info_t* module_info = &tr_module_infos[module->type];
        for (size_t field_index = 0; field_index < module_info->field_count; ++field_index)
        {
            const tr_module_field_info_t* field_info = &module_info->fields[field_index];
            switch (field_info->type)
            {
                case TR_MODULE_FIELD_INPUT_FLOAT:
                case TR_MODULE_FIELD_FLOAT:
                {
                    const float value = *(float*)((uint8_t*)module_addr + field_info->offset);
                    tr_serialize_input_float(sb, field_info->name, value);
                    break;
                }
                    
                case TR_MODULE_FIELD_INPUT_INT:
                case TR_MODULE_FIELD_INT:
                {
                    const int value = *(int*)((uint8_t*)module_addr + field_info->offset);
                    tr_serialize_input_int(sb, field_info->name, value);
                    break;
                }

                case TR_MODULE_FIELD_INPUT_BUFFER:
                {
                    const float** value = (const float**)((uint8_t*)module_addr + field_info->offset);
                    tr_serialize_input_buffer(sb, rack, field_info->name, value);
                    break;
                }

                case TR_MODULE_FIELD_BUFFER:
                    break;
            }
        }
    }

    sb_terminate(sb);
    return 0;
}

void tr_rack_deserialize(rack_t* rack, const char* input, size_t len)
{
    rack_init(rack);
    
    tr_parser_t parser;
    tr_parse_memory(&parser, input, len);
    
    for (size_t i = 0; i < parser.module_count; ++i)
    {
        const tr_parser_cmd_add_module_t* cmd = &parser.modules[i];

        //printf("ADD MODULE: %s %d %d\n", tr_module_infos[cmd->type].id, cmd->x, cmd->y);

        tr_gui_module_t* module = tr_rack_create_module(rack, cmd->type);
        module->x = cmd->x;
        module->y = cmd->y;

        const tr_module_info_t* module_info = &tr_module_infos[module->type];
        for (size_t field_index = 0; field_index < module_info->field_count; ++field_index)
        {
            if (module_info->fields[field_index].type == TR_MODULE_FIELD_BUFFER)
            {
                const tr_output_plug_t p = {cmd->x, cmd->y, module};
                float* field_addr = get_field_address(module, field_index);
                const int output_plug_idx = tr_hmput(rack->output_plugs_key, field_addr);
                rack->output_plugs[output_plug_idx] = p;
            }
        }
    }

    for (size_t i = 0; i < parser.value_count; ++i)
    {
        const tr_parser_cmd_set_value_t* cmd = &parser.values[i];

        const tr_gui_module_t* module = &rack->gui_modules[cmd->module_index];
        void* field_addr = (uint8_t*)module->data + cmd->field_offset;

        switch (cmd->type)
        {
            case TR_SET_VALUE:
                //printf("SET VALUE: %zu:%zu = %.*llx\n", cmd->module_index, cmd->field_offset, (int)cmd->value_size * 2, *(uint64_t*)cmd->value);
                memcpy(field_addr, &cmd->value, cmd->value_size);
                break;
            case TR_SET_VALUE_BUFFER:
                //printf("SET BUFFER: %zu:%zu = %zu:%zu\n", cmd->module_index, cmd->field_offset, cmd->target_module_index, cmd->target_field_offset);
                const tr_gui_module_t* target_module = &rack->gui_modules[cmd->target_module_index];
                void* target_field_addr = (uint8_t*)target_module->data + cmd->target_field_offset;
                //*(float**)field_addr = (float*)target_field_addr;
                memcpy(field_addr, &target_field_addr, sizeof(void*));

                const tr_input_plug_t plug = {tr_random_cable_color()};
                const int input_plug_idx = tr_hmput(rack->input_plugs_key, field_addr);
                rack->input_plugs[input_plug_idx] = plug;
                break;
        }
    }

#if 0
    while (1)
    {
        const tr_token_t tok = tr_next_token(&tokenizer);
        if (tok.type == TR_TOKEN_EOF)
        {
            break;
        }
        
        printf("(%s) %.*s\n", get_token_type_name(&tok), (int)tok.len, input + tok.pos);
    }
#endif


}

//static Font g_font;

void tr_gui_module_begin(tr_gui_module_t* module)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];

    const float2 mouse = get_screen_to_world(get_mouse_position(), g_input.camera);

    if (g_input.drag_module == module)
    {
        module->x = g_input.drag_offset.x + mouse.x;
        module->y = g_input.drag_offset.y + mouse.y;
    }

    rectangle_t bgrect = {(float)module->x, (float)module->y, (float)module_info->width, (float)module_info->height};
    draw_rectangle_rounded(
        bgrect, 
        0.2f,
        COLOR_MODULE_BACKGROUND);

    const char* name = tr_module_infos[module->type].id;
    const float fontsize = 22;

    const float2 textsize = measure_text(FONT_BERKELY_MONO, name, fontsize, 0);

    draw_text(
        FONT_BERKELY_MONO,
        tr_module_infos[module->type].id, 
        (float2){
            (float)(module->x + module_info->width / 2 - textsize.x / 2),
            (float)module->y + TR_MODULE_PADDING * 2},
        fontsize,
        0,
        COLOR_MODULE_TEXT);

    if (!g_input.do_not_process_input)
    {
        if (is_mouse_button_pressed(PL_MOUSE_BUTTON_LEFT))
        {
            if (mouse.x > module->x &&
                mouse.y > module->y &&
                mouse.x < module->x + module_info->width &&
                mouse.y < module->y + 26)
            {
                g_input.drag_module = module;
                g_input.drag_offset.x = module->x - mouse.x;
                g_input.drag_offset.y = module->y - mouse.y;
            }
        }
    }
}

void tr_gui_module_end(void)
{
}

void tr_gui_knob_base(float value, float min, float max, float x, float y, bool highlight)
{
    *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
        .position = {x, y},
        .radius = TR_KNOB_RADIUS,
        .color = highlight ? COLOR_PLUG_HIGHLIGHT : COLOR_KNOB,
    };
    
    const float t = (value - min) / fmaxf(max - min, 0.01f);
    const float t0 = (t - 0.5f) * 0.9f;
    const float angle = t0 * TR_TWOPI;

    *rb_draw_rectangle(&g_rb) = (cmd_draw_rectangle_t){
        .position = {x, y},
        .size = {TR_KNOB_TIP_WIDTH, TR_KNOB_RADIUS * TR_KNOB_TIP_SIZE},
        .origin = {TR_KNOB_TIP_WIDTH * 0.5f, TR_KNOB_RADIUS},
        .rotation = angle,
        .color = COLOR_KNOB_TIP,
    }; 
}

void tr_gui_knob_float(const tr_gui_module_t* module, size_t field_index)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    const tr_module_field_info_t* field_info = &module_info->fields[field_index];
    const float x = module->x + field_info->x;
    const float y = module->y + field_info->y;

    float* value = get_field_address(module, field_index);
    const float min = field_info->min;
    const float max = field_info->max;

    bool highlight = false;
    if (g_input.closest_module[g_input.closest_input] == module &&
        g_input.closest_field[g_input.closest_input] == field_index &&
        g_input.closest_distance[g_input.closest_input] < 0.0f)
    {
        highlight = true;
    }
    if (g_input.active_value == value)
    {
        highlight = true;
    }
    if (g_input.drag_input || g_input.drag_output)
    {
        highlight = false;
    }

    if (!g_input.do_not_process_input)
    {
        if (is_mouse_button_pressed(PL_MOUSE_BUTTON_LEFT))
        {
            const float2 center = {(float)x, (float)y};
            const float2 mouse = get_screen_to_world(get_mouse_position(), g_input.camera);
            if (float2_distance(mouse, center) < TR_KNOB_RADIUS)
            {
                g_input.active_value = value;
            }
        }
        
        if (g_input.active_value == value)
        {
            const float range = (max - min);
            *value -= get_mouse_delta().y * range * 0.01f;
            if (*value < min) *value = min;
            if (*value > max) *value = max;
        }
    }

    tr_gui_knob_base(*value, min, max, x, y, highlight);
}

void tr_gui_knob_int(const tr_gui_module_t* module, size_t field_index)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    const tr_module_field_info_t* field_info = &module_info->fields[field_index];
    const float x = module->x + field_info->x;
    const float y = module->y + field_info->y;
    int* value = get_field_address(module, field_index);
    
    bool highlight = false;
    if (g_input.closest_module[g_input.closest_input] == module &&
        g_input.closest_field[g_input.closest_input] == field_index &&
        g_input.closest_distance[g_input.closest_input] < 0.0f)
    {
        highlight = true;
    }
    if (g_input.active_value == value)
    {
        highlight = true;
    }
    if (g_input.drag_input || g_input.drag_output)
    {
        highlight = false;
    }

    if (g_input.do_not_process_input)
    {
        tr_gui_knob_base(0.0f, 0.0f, 1.0f, x, y, highlight);
    }
    else
    {
        const int min = field_info->min_int;
        const int max = field_info->max_int;
        
        if (is_mouse_button_pressed(PL_MOUSE_BUTTON_LEFT))
        {
            const float2 center = {(float)x, (float)y};
            const float2 mouse = get_screen_to_world(get_mouse_position(), g_input.camera);
            if (float2_distance(mouse, center) < TR_KNOB_RADIUS)
            {
                g_input.active_value = value;
                g_input.active_int = (float)*value;
            }
        }
        
        if (g_input.active_value == value)
        {
            g_input.active_int -= get_mouse_delta().y * (max - min) * 0.01f;

            *value = (int)roundf(g_input.active_int);
            if (*value < min) *value = min;
            if (*value > max) *value = max;
        }

        tr_gui_knob_base((float)*value, (float)min, (float)max, x, y, highlight);
    }
}

static void tr_gui_plug_input(rack_t* rack, const tr_gui_module_t* module, size_t field_index)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    const tr_module_field_info_t* field_info = &module_info->fields[field_index];
    const float x = module->x + field_info->x;
    const float y = module->y + field_info->y;
    const float2 center = {(float)x, (float)y};

    const float** value = get_field_address(module, field_index);

    //const bool dim = g_input.drag_input != NULL;

    bool highlight = false;
    if (g_input.closest_module[g_input.closest_input] == module &&
        g_input.closest_field[g_input.closest_input] == field_index &&
        g_input.closest_distance[g_input.closest_input] < 0.0f)
    {
        highlight = true;
    }
    if (g_input.active_value != NULL || g_input.drag_input != NULL)
    {
        highlight = false;
    }

    *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
        .position = {(float)x, (float)y},
        .radius = TR_PLUG_RADIUS,
        .color = highlight ? COLOR_PLUG_HIGHLIGHT : COLOR_PLUG_BORDER,
    };
    *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
        .position = {(float)x, (float)y},
        .radius = TR_PLUG_RADIUS - TR_PLUG_PADDING,
        .color = COLOR_PLUG_HOLE,
    };
    
    if (*value != NULL)
    {
        const int output_plug_idx = tr_hmget(rack->output_plugs_key, *value);
        const tr_output_plug_t* output_plug = &rack->output_plugs[output_plug_idx];
        const int input_plug_idx = tr_hmget(rack->input_plugs_key, value);
        const tr_input_plug_t* input_plug = &rack->input_plugs[input_plug_idx];

        g_input.cable_draws[g_input.cable_draw_count++] = (tr_cable_draw_command_t){
            .from = center,
            .to = {(float)output_plug->x, (float)output_plug->y},
            .color = input_plug->color,
        };
    }

    const float2 mouse = get_screen_to_world(get_mouse_position(), g_input.camera);

    if (g_input.drag_input == value)
    {
        g_input.cable_draws[g_input.cable_draw_count++] = (tr_cable_draw_command_t){
            .from = center,
            .to = tr_snap_to_input(&g_input, TR_INPUT_TYPE_OUTPUT_PLUG, mouse),
            .color = g_input.drag_color,
        };
    }
}

static void tr_gui_plug_output(rack_t* rack, const tr_gui_module_t* module, size_t field_index)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    const tr_module_field_info_t* field_info = &module_info->fields[field_index];
    const float x = module->x + field_info->x;
    const float y = module->y + field_info->y;
    const bool dim = g_input.drag_output != NULL;

    bool highlight = false;
    if (g_input.closest_module[g_input.closest_input] == module &&
        g_input.closest_field[g_input.closest_input] == field_index &&
        g_input.closest_distance[g_input.closest_input] < 0.0f)
    {
        highlight = true;
    }
    if (g_input.active_value != NULL)
    {
        highlight = false;
    }

    color_t bgcolor = COLOR_PLUG_OUTPUT;
    if (highlight) bgcolor = COLOR_PLUG_HIGHLIGHT;
    if (dim) bgcolor = COLOR_PLUG_DISABLED;
    
    const int rw = TR_PLUG_RADIUS + 4;
    rectangle_t bgrect = {(float)x - rw, (float)y - rw, 2.0f * rw, 2.0f * rw};
    draw_rectangle_rounded(bgrect, 0.25f, bgcolor);
    *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
        .position = {(float)x, (float)y},
        .radius = TR_PLUG_RADIUS,
        .color = COLOR_PLUG_BORDER,
    };
    *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
        .position = {(float)x, (float)y},
        .radius = TR_PLUG_RADIUS - TR_PLUG_PADDING,
        .color = COLOR_PLUG_HOLE,
    };

    const float* buffer = get_field_address(module, field_index);

    const float2 center = {(float)x, (float)y};
    const float2 mouse = get_screen_to_world(get_mouse_position(), g_input.camera);

    if (g_input.drag_output == buffer)
    {
        g_input.cable_draws[g_input.cable_draw_count++] = (tr_cable_draw_command_t){
            .from = center,
            .to = tr_snap_to_input(&g_input, TR_INPUT_TYPE_INPUT_PLUG, mouse),
            .color = g_input.drag_color,
        };
    }
    
    const tr_output_plug_t plug = {x, y, module};
    const int output_plug_idx = tr_hmput(rack->output_plugs_key, buffer);
    rack->output_plugs[output_plug_idx] = plug;
}

void tr_led(float x, float y, color_t color)
{
    *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
        .position = {x, y},
        .radius = 4.0f,
        .color = color,
    };
}

void tr_clock_decorate(tr_clock_t* clock, tr_gui_module_t* module)
{
    const float gate_x = module->x + tr_clock__fields[TR_CLOCK_out_gate].x;
    const float gate_y = module->y + tr_clock__fields[TR_CLOCK_out_gate].y;
    tr_led(gate_x, gate_y + TR_PLUG_RADIUS + 12, clock->phase < 0.5f ? COLOR_LED_ON : COLOR_LED_OFF);
}

void tr_clockdiv_decorate(tr_clockdiv_t* clockdiv, tr_gui_module_t* module)
{
    for (int i = 0; i < 8; ++i)
    {
        const float plug_x = module->x + tr_clockdiv__fields[TR_CLOCKDIV_out_0 + i].x;
        const float plug_y = module->y + tr_clockdiv__fields[TR_CLOCKDIV_out_0 + i].y;
        tr_led(plug_x, plug_y + TR_PLUG_RADIUS + 12, (clockdiv->state >> i) & 1 ? COLOR_LED_ON : COLOR_LED_OFF);
    }
}

void tr_seq8_decorate(tr_seq8_t* seq8, tr_gui_module_t* module)
{
    for (int i = 0; i < 8; ++i)
    {
        const tr_module_field_info_t* field = &tr_module_infos[module->type].fields[TR_SEQ8_in_cv_0 + i];
        tr_led(module->x + field->x, module->y + field->y + TR_KNOB_RADIUS + 8, seq8->step == i ? COLOR_LED_ON : COLOR_LED_OFF);
    }
}

void tr_quantizer_decorate(tr_quantizer_t* quantizer, tr_gui_module_t* module)
{
    const float x = module->x;
    const float y = module->y;
    const float kx = x + 24.0f;
    draw_text(FONT_BERKELY_MONO, g_tr_quantizer_mode_name[quantizer->in_mode], (float2){kx + TR_KNOB_RADIUS + 8, y + 40}, 20, 0, COLOR_MODULE_TEXT);
}

void tr_scope_decorate(tr_scope_t* scope, tr_gui_module_t* module)
{
    const float screen_x = module->x + 8;
    const float screen_y = module->y + 28;
    const float screen_w = 200 - 8*2;
    const float screen_h = 140;
    *rb_draw_rectangle(&g_rb) = (cmd_draw_rectangle_t){
        .position = {screen_x, screen_y},
        .size = {screen_w, screen_h},
        .color = {0, 0, 0, 255},
    }; 

    if (scope->in_0 != NULL)
    {
        float2* vertices = rb_draw_line_strip(&g_rb, TR_SAMPLE_COUNT, (color_t){255, 255, 0, 255});

        for (int i = 0; i < TR_SAMPLE_COUNT; ++i)
        {
            const float s0 = scope->in_0[i];
            const float x0 = float_remap(i + 0.0f, 0.0f, (float)TR_SAMPLE_COUNT, (float)screen_x, (float)screen_x + screen_w);
            const float y0 = float_remap(s0, -1.0f, 1.0f, (float)screen_y + screen_h, (float)screen_y);
            
            vertices[i] = (float2){x0, y0};
        }
    }
}

void tr_gui_module_draw(rack_t* rack, tr_gui_module_t* module)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    tr_gui_module_begin(module);

    for (size_t field_index = 0; field_index < module_info->field_count; ++field_index)
    {
        const tr_module_field_info_t* field_info = &module_info->fields[field_index];
        switch (field_info->type)
        {
            case TR_MODULE_FIELD_INPUT_FLOAT: tr_gui_knob_float(module, field_index); break;
            case TR_MODULE_FIELD_INPUT_INT: tr_gui_knob_int(module, field_index); break;
            case TR_MODULE_FIELD_INPUT_BUFFER: tr_gui_plug_input(rack, module, field_index); break;
            case TR_MODULE_FIELD_BUFFER: tr_gui_plug_output(rack, module, field_index); break;
            default: break;
        }
    }

    switch (module->type)
    {
        case TR_CLOCK: tr_clock_decorate(module->data, module); break;
        case TR_CLOCKDIV: tr_clockdiv_decorate(module->data, module); break;
        case TR_SEQ8: tr_seq8_decorate(module->data, module); break;
        case TR_QUANTIZER: tr_quantizer_decorate(module->data, module); break;
        case TR_SCOPE: tr_scope_decorate(module->data, module); break;
        default: break;
    }

    tr_gui_module_end();
}

void tr_update_modules(tr_gui_module_t** modules, size_t count)
{
#if TR_TRACE_MODULE_UPDATES
    printf("tr_update_modules:\n");
#endif

    for (size_t i = 0; i < count; ++i)
    {
        tr_gui_module_t* module = modules[i];

        switch (module->type)
        {
            case TR_VCO: tr_vco_update(module->data); break;
            case TR_CLOCK: tr_clock_update(module->data); break;
            case TR_CLOCKDIV: tr_clockdiv_update(module->data); break;
            case TR_SEQ8: tr_seq8_update(module->data); break;
            case TR_ADSR: tr_adsr_update(module->data); break;
            case TR_VCA: tr_vca_update(module->data); break;
            case TR_LP: tr_lp_update(module->data); break;
            case TR_MIXER: tr_mixer_update(module->data); break;
            case TR_NOISE: tr_noise_update(module->data); break;
            case TR_QUANTIZER: tr_quantizer_update(module->data); break;
            case TR_RANDOM: tr_random_update(module->data); break;
            default: break;
        }

#if TR_TRACE_MODULE_UPDATES
        printf("\t%s %zu\n", tr_module_infos[module->type].id, module->index);
#endif
    }
}

int tr_enumerate_inputs(const float* inputs[], const tr_gui_module_t* module)
{
    int count = 0;

    const tr_module_info_t* module_info = &tr_module_infos[module->type];

    for (size_t i = 0; i < module_info->field_count; ++i)
    {
        const tr_module_field_info_t* field = &module_info->fields[i];
        if (field->type == TR_MODULE_FIELD_INPUT_BUFFER)
        {
            inputs[count++] = *(float**)((uint8_t*)module->data + field->offset);
        }
    }

    return count;
}

typedef struct tr_module_sort_data
{
    uint32_t module_index;
    int distance;
} tr_module_sort_data_t;

static int tr_update_module_sort_function(const void* lhs, const void* rhs)
{
    return ((const tr_module_sort_data_t*)rhs)->distance - ((const tr_module_sort_data_t*)lhs)->distance;
}

size_t tr_collect_leaf_modules(rack_t* rack, const tr_gui_module_t* leaf_modules[])
{
    uint32_t output_mask[TR_GUI_MODULE_COUNT / 32];
    memset(output_mask, 0, sizeof(output_mask));
    
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        const float* inputs[64];
        const int input_count = tr_enumerate_inputs(inputs, &rack->gui_modules[i]);

        for (int input_index = 0; input_index < input_count; ++input_index)
        {
            if (inputs[input_index] == NULL)
            {
                continue;
            }

            const int output_plug_idx = tr_hmget(rack->output_plugs_key, inputs[input_index]);
            const tr_output_plug_t* output_plug = &rack->output_plugs[output_plug_idx];
            
            const size_t module_index = tr_get_gui_module_index(rack, output_plug->module);
            output_mask[module_index / 32] |= 1 << (module_index % 32);
        }
    }

    size_t count = 0;
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        if (output_mask[i / 32] & (1 << (i % 32)))
        {
            continue;
        }

        leaf_modules[count++] = &rack->gui_modules[i];
    }

    return count;
}

size_t tr_resolve_module_graph(tr_gui_module_t** update_modules, rack_t* rack)
{
    tb_start(&g_app.tb_resolve_module_graph);

    const tr_gui_module_t* leaf_modules[TR_GUI_MODULE_COUNT];
    const size_t leaf_count = tr_collect_leaf_modules(rack, leaf_modules);

    typedef struct stack_item
    {
        const tr_gui_module_t* module;
        const tr_gui_module_t* parent;
    } stack_item_t;
    
    stack_item_t stack[TR_GUI_MODULE_COUNT];
    int sp = 0;

    uint32_t distance[TR_GUI_MODULE_COUNT];
    memset(distance, 0, sizeof(distance));

    for (size_t leaf_index = 0; leaf_index < leaf_count; ++leaf_index)
    {
        const tr_gui_module_t* leaf_module = leaf_modules[leaf_index];
        assert(distance[tr_get_gui_module_index(rack, leaf_module)] == 0);

        assert(sp == 0);
        stack[sp++] = (stack_item_t){leaf_module};

        uint32_t visited_mask[TR_GUI_MODULE_COUNT / 32];
        memset(visited_mask, 0, sizeof(visited_mask));

#if TR_TRACE_MODULE_GRAPH
        printf("leaf %zu:\n", leaf_index);
#endif

        while (sp > 0)
        {
            const stack_item_t top = stack[--sp];
            const size_t module_index = tr_get_gui_module_index(rack, top.module);

            if (top.parent != NULL)
            {
#if TR_TRACE_MODULE_GRAPH
                printf("\t(%s %zu) <- (%s %zu)\n", 
                    tr_module_infos[top.module->type].id, top.module->index,
                    tr_module_infos[top.parent->type].id, top.parent->index);
#endif

                const size_t parent_module_index = tr_get_gui_module_index(rack, top.parent);
                assert(visited_mask[parent_module_index / 32] & (1 << (parent_module_index % 32)));
                const uint32_t next_distance = distance[parent_module_index] + 1;
                if (distance[module_index] < next_distance)
                {
#if TR_TRACE_MODULE_GRAPH
                    printf("\t\t[%u -> %u]\n", distance[module_index], next_distance);
#endif
                    distance[module_index] = next_distance;
                }
            }
            else
            {
#if TR_TRACE_MODULE_GRAPH
                printf("\t(%s %zu)\n", tr_module_infos[top.module->type].id, top.module->index);
#endif
            }

            if (visited_mask[module_index / 32] & (1 << (module_index % 32)))
            {
                continue;
            }

            visited_mask[module_index / 32] |= (1 << (module_index % 32));

            const float* inputs[64];
            const int input_count = tr_enumerate_inputs(inputs, top.module);

            for (int i = 0; i < input_count; ++i)
            {
                if (inputs[i] == NULL)
                {
                    continue;
                }

                const int output_plug_idx = tr_hmget(rack->output_plugs_key, inputs[i]);
                const tr_output_plug_t* output_plug = &rack->output_plugs[output_plug_idx];

                assert(sp < tr_countof(stack));
                stack[sp++] = (stack_item_t){output_plug->module, top.module};
            }
        }
    }

    tr_module_sort_data_t sort_data[TR_GUI_MODULE_COUNT];

    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        sort_data[i] = (tr_module_sort_data_t){(uint32_t)i, distance[i]};
    }

    qsort(sort_data, rack->gui_module_count, sizeof(tr_module_sort_data_t), tr_update_module_sort_function);

    size_t update_count = 0;
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        update_modules[update_count++] = &rack->gui_modules[sort_data[i].module_index];
    }

    tb_stop(&g_app.tb_resolve_module_graph);
    return update_count;
}

// static void float_to_int16_pcm(int16_t* samples, const float* buffer)
// {
//     assert(buffer != NULL);
//     for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
//     {
//         samples[i] = (int16_t)(float_clamp(buffer[i], -1.0f, 1.0f) * INT16_MAX);
//     }
// }

static const tr_speaker_t* tr_find_speaker(rack_t* rack)
{
    const tr_speaker_t* speaker = NULL;
    
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        const tr_gui_module_t* module = &rack->gui_modules[i];
        if (module->type != TR_SPEAKER)
        {
            continue;
        }
        
        const tr_speaker_t* s = (const tr_speaker_t*)module->data;
        if (s->in_audio == NULL)
        {
            continue;
        }

        speaker = s;
        break;
    }

    return speaker;
}

static void tr_produce_final_mix_internal(float* output, rack_t* rack)
{
    tr_gui_module_t* update_modules[TR_GUI_MODULE_COUNT];
    const size_t update_count = tr_resolve_module_graph(update_modules, rack);
    tr_update_modules(update_modules, update_count);

    const tr_speaker_t* speaker = tr_find_speaker(rack);
    if (speaker == NULL)
    {
        memset(output, 0, sizeof(float) * TR_SAMPLE_COUNT);
        return;
    }

    //float_to_int16_pcm(output, speaker->in_audio);
    memcpy(output, speaker->in_audio, sizeof(float) * TR_SAMPLE_COUNT);
}

void tr_produce_final_mix(float* output, rack_t* rack)
{
    tb_start(&g_app.tb_produce_final_mix);
    tr_produce_final_mix_internal(output, rack);
    tb_stop(&g_app.tb_produce_final_mix);
}

EMSCRIPTEN_KEEPALIVE
void tr_audio_callback(void* bufferData, size_t frames)
{
    float* samples = bufferData;
    size_t write_cursor = 0;

    //printf("frames: %d\n", frames);

    while (write_cursor < frames)
    {
        if (g_app.final_mix_remaining == 0)
        {
            g_app.final_mix_remaining = TR_SAMPLE_COUNT;
            tr_produce_final_mix(g_app.final_mix, &g_app.rack);
        }

        const size_t final_mix_cursor = TR_SAMPLE_COUNT - g_app.final_mix_remaining;
        const size_t frames_available = frames - write_cursor;

        size_t final_mix_available = g_app.final_mix_remaining;
        if (final_mix_available > frames_available)
        {
            final_mix_available = frames_available;
        }

        //printf("write_cursor: %zu, final_mix_cursor: %zu, final_mix_available: %zu\n", write_cursor, final_mix_cursor, final_mix_available);
        
        memcpy(samples + write_cursor, g_app.final_mix + final_mix_cursor, final_mix_available * sizeof(float));

        write_cursor += final_mix_available;
        g_app.final_mix_remaining -= final_mix_available;
    }

    g_app.has_audio_callback_been_called_once = true;
}

rectangle_t tr_compute_patch_bounds(rack_t* rack)
{
    if (rack->gui_module_count == 0)
    {
        return (rectangle_t){0.0f};
    }
    
    float min_x = FLT_MAX;
    float min_y = FLT_MAX;
    float max_x = -FLT_MAX;
    float max_y = -FLT_MAX;

    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        const tr_module_info_t* module_info = &tr_module_infos[rack->gui_modules[i].type];
        float x = (float)rack->gui_modules[i].x;
        float y = (float)rack->gui_modules[i].y;
        float w = (float)module_info->width;
        float h = (float)module_info->height;

        min_x = fminf(min_x, x);
        min_y = fminf(min_y, y);
        max_x = fmaxf(max_x, x + w);
        max_y = fmaxf(max_y, y + h);
    }

    return (rectangle_t){
        .x = min_x,
        .y = min_y,
        .width = max_x - min_x,
        .height = max_y - min_y,
    };
}

void tr_draw_cable(float2 a, float2 b, float slack, float thick, color_t color)
{
    float L = float2_distance(a, b);
    float sag = L * slack * 0.25f;
    
    float2 down = {0.0f, 1.0f};
    float2 sagVec = float2_scale(float2_normalize(down), sag);
    float2 mid = float2_lerp(a, b, 0.5f);
    float2 c = {
        mid.x + sagVec.x,
        mid.y + sagVec.y,
    };
    cmd_draw_spline_t* cmd = rb_draw_spline(&g_rb);
    cmd->p1 = a;
    cmd->c2 = c;
    cmd->p3 = b;
    cmd->thickness = thick;
    cmd->color = color;
}

static char g_serialization_buffer[1024 * 1024];

void tr_frame_update_draw(void)
{
    app_t* app = &g_app;
    rack_t* rack = &app->rack;

    tb_start(&app->tb_frame_update_draw);

#if 1
    g_input.camera.offset.x = get_screen_size().x * 0.5f;
    g_input.camera.offset.y = get_screen_size().y * 0.5f;
#else
    g_input.camera.offset = get_mouse_position();
#endif

    if (!app->picker_mode)
    {
        if (is_mouse_button_down(PL_MOUSE_BUTTON_RIGHT))
        {
            g_input.camera.target.x -= get_mouse_delta().x / g_input.camera.zoom;
            g_input.camera.target.y -= get_mouse_delta().y / g_input.camera.zoom;
        }
        
        g_input.camera.zoom += get_mouse_wheel_move().y * 0.2f;
        g_input.camera.zoom = float_clamp(g_input.camera.zoom, 0.2f, 4.0f);
    }

    if (is_key_down(PL_KEY_LEFT_CONTROL) && is_key_pressed(PL_KEY_S))
    {
        tr_strbuf_t sb = {g_serialization_buffer};
        tr_rack_serialize(&sb, rack);
        printf("%.*s", (int)sb.pos, sb.buf);
    }

    if (is_key_pressed(PL_KEY_TAB))
    {
        app->picker_mode = !app->picker_mode;
    }

    if (is_key_pressed(PL_KEY_SPACE))
    {
        app->paused = !app->paused;
    }

    if (app->paused && is_key_pressed(PL_KEY_S))
    {
        app->single_step = true;
    }

    tb_start(&app->tb_update_input);

    tr_update_closest_input_state(&g_input, rack, get_screen_to_world(get_mouse_position(), g_input.camera));

    if (is_mouse_button_pressed(PL_MOUSE_BUTTON_LEFT))
    {
        if (g_input.closest_distance[g_input.closest_input] < 0.0f)
        {
            const tr_gui_module_t* module = g_input.closest_module[g_input.closest_input];
            const size_t field = g_input.closest_field[g_input.closest_input];

            if (g_input.closest_input == TR_INPUT_TYPE_INPUT_PLUG)
            {
                const float** input = get_field_address(module, field);

                if (*input != NULL)
                {
                    const int input_plug_idx = tr_hmget(rack->input_plugs_key, input);
                    assert(input_plug_idx != -1);
                    const tr_input_plug_t* input_plug = &rack->input_plugs[input_plug_idx];

                    g_input.drag_output = *input;
                    g_input.drag_color = input_plug->color;
                    *input = NULL;
                }
                else
                {
                    g_input.drag_input = input;
                    g_input.drag_color = tr_random_cable_color();
                }
            }
            else if (g_input.closest_input == TR_INPUT_TYPE_OUTPUT_PLUG)
            {
                const float* output = get_field_address(module, field);

                g_input.drag_output = output;
                g_input.drag_color = tr_random_cable_color();
            }
        }
    }

    cursor_t cursor = PL_CURSOR_DEFAULT;

    if (g_input.closest_distance[g_input.closest_input] < 0.0f)
    {
        cursor = PL_CURSOR_GRAB;
    }

    if (g_input.drag_output != NULL ||
        g_input.drag_input != NULL ||
        g_input.active_value != NULL)
    {
        cursor = PL_CURSOR_GRABBING;
    }

    if (g_input.drag_module != NULL)
    {
        cursor = PL_CURSOR_MOVE;
    }

    if (g_input.drag_output != NULL &&
        g_input.closest_input == TR_INPUT_TYPE_OUTPUT_PLUG &&
        g_input.closest_distance[g_input.closest_input] < 0.0f)
    {
        cursor = PL_CURSOR_NO_DROP;
    }

    if (g_input.drag_input != NULL &&
        g_input.closest_input == TR_INPUT_TYPE_INPUT_PLUG &&
        g_input.closest_distance[g_input.closest_input] < 0.0f)
    {
        cursor = PL_CURSOR_NO_DROP;
    }

    platform_set_cursor(cursor);

    tb_stop(&app->tb_update_input);

    rb_begin(&g_rb, COLOR_BACKGROUND);

    *rb_camera_begin(&g_rb) = (cmd_camera_begin_t){g_input.camera};

#if 0
    {
        const float2 m = get_mouse_position();
        const float2 mw = get_screen_to_world(m, g_input.camera);

        // *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
        //     .position = {m.x, m.y},
        //     .radius = 8.0f,
        //     .color = (color_t){255, 0, 0, 255},
        // };
        *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
            .position = {mw.x, mw.y},
            .radius = 4.0f,
            .color = (color_t){0, 255, 0, 255},
        };

        //printf("%d %d\n", (int)m.x, (int)m.y);
    }
#endif

    g_input.cable_draw_count = 0;
    
    tb_start(&app->tb_draw_modules);

    g_input.do_not_process_input = app->picker_mode;
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        tr_gui_module_draw(rack, &rack->gui_modules[i]);
    }
    g_input.do_not_process_input = false;

    tb_stop(&app->tb_draw_modules);

    for (size_t i = 0; i < g_input.cable_draw_count; ++i)
    {
        const tr_cable_draw_command_t* draw = &g_input.cable_draws[i];
        tr_draw_cable(draw->from, draw->to, 1.0f, 6.0f, COLOR_ALPHA(draw->color, TR_CABLE_ALPHA));
        
        *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
            .position = draw->from,
            .radius = TR_PLUG_RADIUS - TR_PLUG_PADDING - 3.5f,
            .color = COLOR_ALPHA(draw->color, 1.0f),
        };
        *rb_draw_circle(&g_rb) = (cmd_draw_circle_t){
            .position = draw->to,
            .radius = TR_PLUG_RADIUS - TR_PLUG_PADDING - 3.5f,
            .color = COLOR_ALPHA(draw->color, 1.0f),
        };
    }

#if 0
    for (int i = 0; i < TR_INPUT_TYPE_COUNT; ++i)
    {
        const tr_gui_module_t* module = g_input.closest_module[i];
        if (module == NULL) continue;
        const size_t field_index = g_input.closest_field[i];
        const tr_module_info_t* module_info = &tr_module_infos[module->type];
        const tr_module_field_info_t* field_info = &module_info->fields[field_index];

        const float2 center = {(float)module->x + field_info->x, (float)module->y + field_info->y};
        const float2 mouse = get_screen_to_world(get_mouse_position(), g_input.camera);
        DrawLineV(center, mouse, MAGENTA);
    }
#endif

    rb_camera_end(&g_rb);

    if (is_mouse_button_released(PL_MOUSE_BUTTON_LEFT))
    {
        if (g_input.drag_input != NULL)
        {
            if (g_input.closest_distance[TR_INPUT_TYPE_OUTPUT_PLUG] < 0.0f)
            {
                const tr_gui_module_t* module = g_input.closest_module[TR_INPUT_TYPE_OUTPUT_PLUG];
                const size_t field_index = g_input.closest_field[TR_INPUT_TYPE_OUTPUT_PLUG];
                const float* output = get_field_address(module, field_index);

                const tr_input_plug_t plug = {g_input.drag_color};
                const int input_plug_idx = tr_hmput(rack->input_plugs_key, g_input.drag_input);
                rack->input_plugs[input_plug_idx] = plug;
                *g_input.drag_input = output;
            }
            else
            {
                *g_input.drag_input = NULL;
            }
        }

        if (g_input.drag_output != NULL)
        {
            if (g_input.closest_distance[TR_INPUT_TYPE_INPUT_PLUG] < 0.0f)
            {
                const tr_gui_module_t* module = g_input.closest_module[TR_INPUT_TYPE_INPUT_PLUG];
                const size_t field_index = g_input.closest_field[TR_INPUT_TYPE_INPUT_PLUG];
                const float** input = get_field_address(module, field_index);

                const tr_input_plug_t plug = {g_input.drag_color};
                const int input_plug_idx = tr_hmput(rack->input_plugs_key, input);
                rack->input_plugs[input_plug_idx] = plug;
                *input = g_input.drag_output;
            }
        }

        g_input.active_value = NULL;
        g_input.drag_module = NULL;
        g_input.drag_input = NULL;
        g_input.drag_output = NULL;
    }

    if (app->picker_mode)
    {
        *rb_draw_rectangle(&g_rb) = (cmd_draw_rectangle_t){
            .position = {0, 0},
            .size = get_screen_size(),
            .color = {0, 0, 0, 200},
        }; 

        g_input.do_not_process_input = true;

        int x = 10;
        int y = 10;

        for (size_t module_type = 0; module_type < TR_MODULE_COUNT; ++module_type)
        {
            const tr_module_info_t* module_info = &tr_module_infos[module_type];
            if (x + module_info->width >= get_screen_size().x)
            {
                x = 10;
                y += 180;
            }

            tr_gui_module_t m = {x, y, module_type, g_null_module};
            tr_gui_module_draw(rack, &m);

            if (is_mouse_button_pressed(PL_MOUSE_BUTTON_LEFT) && 
                g_input.drag_module == NULL)
            {
                const float2 mouse = get_mouse_position();
                if (mouse.x > x && 
                    mouse.y > y &&
                    mouse.x < x + module_info->width && 
                    mouse.y < y + module_info->height)
                {
                    tr_gui_module_t* module = tr_rack_create_module(rack, module_type);
                    module->x = (int)mouse.x;
                    module->y = (int)mouse.y;
                    g_input.drag_module = module;
                    g_input.drag_offset.x = -(mouse.x - x);
                    g_input.drag_offset.y = -(mouse.y - y);
                    app->picker_mode = false;
                }
            }

            x += module_info->width + 10;
        }

        g_input.do_not_process_input = false;
    }

#ifdef __EMSCRIPTEN__
    if (!g_app.has_audio_callback_been_called_once)
    {
        const char* message = "Click anywhere to enable audio playback.";
        const float2 message_size = measure_text(FONT_BERKELY_MONO, message, 22, 0);

        const float2 pos = {
            get_screen_size().x * 0.5f - message_size.x * 0.5f,
            50,
        };
        const rectangle_t bgrect = {
            pos.x - 12, pos.y - 12,
            message_size.x + 24, message_size.y + 24,
        };

        draw_rectangle_rounded(bgrect, 0.25f, COLOR_ALPHA(COLOR_BLACK, 0.5f));
        draw_text(FONT_BERKELY_MONO, message, pos, 22, 0, COLOR_WHITE);
    }
#endif

    {
        struct {const char* name; const timer_buffer_t* tb;} tb_draw_infos[] = {
            {"final_mix", &g_app.tb_produce_final_mix},
            {"module_graph", &g_app.tb_resolve_module_graph},
            {"update_input", &g_app.tb_update_input},
            {"draw_modules", &g_app.tb_draw_modules},
            {"frame", &g_app.tb_frame_update_draw},
        };

        const float font_size = 16.0f;
        float2 pos = {2.0f, get_screen_size().y - 4};
        
        for (size_t i = 0; i < tr_countof(tb_draw_infos); ++i)
        {
            char fmt[64];
            char message[64];

            const float avg = tb_avg(tb_draw_infos[i].tb);
            sprintf(fmt, "%s: %%1.3f ms", tb_draw_infos[i].name);
            sprintf(message, fmt, avg);
            const float2 message_size = measure_text(FONT_BERKELY_MONO, message, font_size, 0);
            pos.y -= message_size.y;
            draw_text(FONT_BERKELY_MONO, message, pos, font_size, 0, COLOR_WHITE);
        }
    }

    {
#if 0
        app->add_module_type = app->add_module_type % TR_MODULE_COUNT;

        const int menu_height = 42;

        DrawRectangle(0, 0, 10000, menu_height, COLOR_FROM_RGBA_HEX(0x303030ff));
        DrawText(tr_module_infos[app->add_module_type].id, 0, 0, 40, WHITE);

#ifdef TR_RECORDING_FEATURE
        const char* recording_text = app->is_recording ? "Press F5 to stop recording" : "Press F5 to start recording";
        DrawText(recording_text, 260, menu_height / 2 - 12, 24, WHITE);
        DrawCircle(240, menu_height / 2, 14, app->is_recording ? RED : GRAY);
#endif

        const float2 mouse = get_mouse_position();
        if (is_mouse_button_pressed(PL_MOUSE_BUTTON_LEFT) &&
            mouse.y < menu_height)
        {
            const float2 world_mouse = get_screen_to_world(mouse, g_input.camera);

            tr_gui_module_t* module = tr_rack_create_module(rack, app->add_module_type);
            module->x = (int)world_mouse.x;
            module->y = (int)world_mouse.y;
            g_input.drag_module = module;
            g_input.drag_offset.x = 0.0f;
            g_input.drag_offset.y = 0.0f;
        }
#endif
    }

    rb_end(&g_rb);
    platform_render(&g_rb);

#ifdef TR_RECORDING_FEATURE
    if (is_key_pressed(KEY_F5))
    {
        app->is_recording = !app->is_recording;
        
        if (app->is_recording)
        {
            app->recording_offset = 0;
        }
        else
        {
            const Wave wave = {
                .frameCount = (uint32_t)app->recording_offset,
                .sampleRate = TR_SAMPLE_RATE,
                .sampleSize = 16,
                .channels = 1,
                .data = app->recording_samples,
            };
            const bool export_result = ExportWave(wave, "recording.wav");
            if (!export_result)
            {
                fprintf(stderr, "Failed to export wav file.\n");
            }
        }
    }
#endif

    tb_stop(&g_app.tb_frame_update_draw);
}

static const char* TEST = "\
module speaker 0 pos 1357 -44 \
input_buffer in_audio > mixer 2 out_mix \
module vco 1 pos 661 1 \
input phase 0x40c1daf7 \
input in_v0 0x42ec0000 \
input_buffer in_voct > quantizer 8 out_cv \
module mixer 2 pos 1082 -49 \
input_buffer in_0 > lp 13 out_audio \
input_buffer in_1 > lp 11 out_audio \
input_buffer in_2 > lp 20 out_audio \
input in_vol0 0x3f3d70a4 \
input in_vol1 0x3f4f5c29 \
input in_vol2 0x3e0f5c29 \
input in_vol3 0x00000000 \
input in_vol_final 0x3f800000 \
module vco 3 pos 660 -173 \
input phase 0x3f212bd4 \
input in_v0 0x436b99a1 \
input_buffer in_voct > quantizer 7 out_cv \
module clock 4 pos -143 -20 \
input phase 0x3e4efbf4 \
input in_hz 0x40d04745 \
module seq8 5 pos 10 -115 \
input step 5 \
input trig 1 \
input_buffer in_step > clock 4 out_gate \
input in_cv_0 0x3e8f5c29 \
input in_cv_1 0x3e3851ec \
input in_cv_2 0x00000000 \
input in_cv_3 0x00000000 \
input in_cv_4 0x3eb851ec \
input in_cv_5 0x00000000 \
input in_cv_6 0x3f1c28f6 \
input in_cv_7 0x00000000 \
module seq8 6 pos 9 -8 \
input step 0 \
input trig 1 \
input_buffer in_step > clockdiv 17 out_1 \
input in_cv_0 0x3d8f5c29 \
input in_cv_1 0x3e8f5c29 \
input in_cv_2 0x3ef0a3d7 \
input in_cv_3 0x3f1eb852 \
input in_cv_4 0x00000000 \
input in_cv_5 0x3e8a3d71 \
input in_cv_6 0x3ef0a3d7 \
input in_cv_7 0x3f30a3d7 \
module quantizer 7 pos 437 -138 \
input in_mode 3 \
input_buffer in_cv > seq8 5 out_cv \
module quantizer 8 pos 439 -15 \
input in_mode 3 \
input_buffer in_cv > seq8 6 out_cv \
module adsr 9 pos 796 252 \
input value 0x3f7d01e2 \
input gate 1 \
input state 1 \
input in_attack 0x3e9f12c2 \
input in_decay 0x3e57d955 \
input in_sustain 0x3ed1eb85 \
input in_release 0x3f5ec0c6 \
input_buffer in_gate > clock 4 out_gate \
module adsr 10 pos 1658 337 \
input value 0x00000000 \
input gate 0 \
input state 0 \
input in_attack 0x00000000 \
input in_decay 0x00000000 \
input in_sustain 0x00000000 \
input in_release 0x00000000 \
module lp 11 pos 779 9 \
input value 0x00000000 \
input z 0x3f09f751 \
input_buffer in_audio > vco 1 out_saw \
input_buffer in_cut > adsr 9 out_env \
input in_cut0 0x00000000 \
input in_cut_mul 0x3eeb851f \
module vca 12 pos 1655 452 \
module lp 13 pos 779 -111 \
input value 0x00000000 \
input z 0x3dc1ef74 \
input_buffer in_audio > vco 3 out_saw \
input_buffer in_cut > adsr 9 out_env \
input in_cut0 0x00000000 \
input in_cut_mul 0x3ef5c28f \
module scope 14 pos 1098 87 \
input_buffer in_0 > lp 11 out_audio \
module scope 15 pos 1475 -97 \
input_buffer in_0 > mixer 2 out_mix \
module scope 16 pos 1099 -288 \
input_buffer in_0 > lp 13 out_audio \
module clockdiv 17 pos 10 -223 \
input gate 0x00000000 \
input state 70 \
input_buffer in_gate > clock 4 out_gate \
module adsr 18 pos 789 373 \
input value 0x3f113ec4 \
input gate 1 \
input state 0 \
input in_attack 0x3f452dcb \
input in_decay 0x3a83126f \
input in_sustain 0x3f800000 \
input in_release 0x3f053e2d \
input_buffer in_gate > clockdiv 17 out_2 \
module vco 19 pos 659 184 \
input phase 0x40acbe60 \
input in_v0 0x42ec0002 \
input_buffer in_voct > quantizer 22 out_cv \
module lp 20 pos 777 126 \
input value 0x00000000 \
input z 0x3f1a6f61 \
input_buffer in_audio > vco 19 out_saw \
input_buffer in_cut > adsr 18 out_env \
input in_cut0 0x3e0f5c29 \
input in_cut_mul 0x3f47ae14 \
module seq8 21 pos 9 104 \
input step 0 \
input trig 1 \
input_buffer in_step > clockdiv 17 out_2 \
input in_cv_0 0x00000000 \
input in_cv_1 0x3ef5c28f \
input in_cv_2 0x00000000 \
input in_cv_3 0x3f800000 \
input in_cv_4 0x00000000 \
input in_cv_5 0x3ef5c28f \
input in_cv_6 0x3f2e147b \
input in_cv_7 0x3f800000 \
module quantizer 22 pos 436 109 \
input in_mode 3 \
input_buffer in_cv > seq8 21 out_cv";

int main()
{
    app_t* app = &g_app;
    rack_t* rack = &app->rack;

    srand((unsigned int)time(NULL));

#if 0
    rack_init(rack);
    {
        tr_gui_module_t* speaker0 = tr_rack_create_module(rack, TR_SPEAKER);
        speaker0->x = 1280 / 2;
        speaker0->y = 720 / 2;
    }
#else
    tr_rack_deserialize(rack, TEST, strlen(TEST));

#if 1
    {
        tr_strbuf_t sb = {g_serialization_buffer};
        tr_rack_serialize(&sb, rack);
        printf("sb.pos: %zu\n", sb.pos);
        printf("%s", sb.buf);
    }
#endif

    const rectangle_t bounds = tr_compute_patch_bounds(rack);
    g_input.camera.target.x = bounds.x + bounds.width * 0.5f;
    g_input.camera.target.y = bounds.y + bounds.height * 0.5f;
#endif

    platform_init(TR_SAMPLE_RATE, TR_SAMPLE_COUNT, NULL);

#ifdef TR_RECORDING_FEATURE
    app->is_recording = false;
    app->recording_samples = malloc(86400ull * TR_SAMPLE_RATE * sizeof(int16_t));
    app->recording_offset = 0;
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(tr_frame_update_draw, 0, 1);
#else
    while (platform_running())
    {
        tr_frame_update_draw();
    }
#endif

    return 0;
}
