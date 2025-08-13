
#ifdef __EMSCRIPTEN__
#define TR_AUDIO_CALLBACK
#endif

#ifndef __EMSCRIPTEN__
#define TR_TIMER_MODULE
#define TR_RECORDING_FEATURE
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifdef TR_TIMER_MODULE
#include "timer.h"
#endif

#include "modules.h"
#include "modules.reflection.h"

#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include <raylib.h>
#include <raymath.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

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
RACK_MODULE_POOL(tr_scope);

typedef struct tr_gui_module
{
    int x, y;
    enum tr_module_type type;
    size_t index;
} tr_gui_module_t;

#define TR_GUI_MODULE_COUNT 1024

typedef struct tr_output_plug
{
    int x, y;
    const tr_gui_module_t* module;
} tr_output_plug_t;

typedef struct tr_output_plug_kv
{
    const float* key;
    tr_output_plug_t value;
} tr_output_plug_kv_t;

typedef struct tr_input_plug
{
    Color color;
} tr_input_plug_t;

typedef struct tr_input_plug_kv
{
    const float** key;
    tr_input_plug_t value;
} tr_input_plug_kv_t;

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
    tr_scope_pool_t scope;
    
    tr_gui_module_t gui_modules[TR_GUI_MODULE_COUNT];
    size_t gui_module_count;
} rack_t;

size_t tr_get_gui_module_index(const rack_t* rack, const tr_gui_module_t* module)
{
    return module - rack->gui_modules;
}

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
        case TR_SCOPE: return rack->scope.count++;
        case TR_MODULE_COUNT: break;
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
    memset(rack, 0, sizeof(rack_t));
}

static void final_mix(int16_t* samples, const float* buffer)
{
    assert(buffer != NULL);
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        samples[i] = (int16_t)(buffer[i] * INT16_MAX);
    }
}

typedef struct tr_cable_draw_command
{
    Vector2 from;
    Vector2 to;
    Color color;
} tr_cable_draw_command_t;

typedef struct tr_gui_input
{
    void* active_value;
    float active_int; // for int knob
    tr_gui_module_t* drag_module;
    const float** drag_input;
    const float* drag_output;
    Color drag_color;
    tr_output_plug_kv_t* output_plugs;
    tr_input_plug_kv_t* input_plugs;
    tr_gui_module_t* draw_module;

    tr_cable_draw_command_t cable_draws[1024];
    size_t cable_draw_count;

    Camera2D camera;
} tr_gui_input_t;

static tr_gui_input_t g_input = {
    .camera.zoom = 1.0f,
};

#define TR_KNOB_RADIUS (18)
#define TR_KNOB_PADDING (2)
#define TR_PLUG_RADIUS (14)
#define TR_PLUG_PADDING (4)
#define TR_MODULE_PADDING (2)

#define TR_PLUG_HIGHLIGHT_COLOR (GetColor(0xe0780fff))

static const Color g_cable_colors[] = 
{
    { 253, 249, 0, 255 },     // Yellow
    { 255, 203, 0, 255 },     // Gold
    { 255, 161, 0, 255 },     // Orange
    { 255, 109, 194, 255 },   // Pink
    { 230, 41, 55, 255 },     // Red
    { 190, 33, 55, 255 },     // Maroon
    { 0, 228, 48, 255 },      // Green
    { 0, 158, 47, 255 },      // Lime
    { 102, 191, 255, 255 },   // Sky Blue
    { 200, 122, 255, 255 },   // Purple
    { 135, 60, 190, 255 },    // Violet
    { 211, 176, 131, 255 },   // Beige
};

static Color tr_random_cable_color()
{
    return g_cable_colors[rand() % tr_countof(g_cable_colors)];
}

typedef struct tr_serialize_context
{
    rack_t* rack;
    char* out;
    size_t pos;
} tr_serialize_context_t;

void tr_serialize_input_float(tr_serialize_context_t* ctx, const char* name, float value)
{
    ctx->pos += sprintf(ctx->out + ctx->pos, "input %s %f\n", name, value);
}

void tr_serialize_input_int(tr_serialize_context_t* ctx, const char* name, int value)
{
    ctx->pos += sprintf(ctx->out + ctx->pos, "input %s %d\n", name, value);
}

void* tr_get_module_address(rack_t* rack, enum tr_module_type type, size_t index)
{
    void* pool_base_addr = NULL;

    const tr_module_info_t* module_info = &tr_module_infos[type];

    switch (type)
    {
        case TR_VCO: pool_base_addr = rack->vco.elements; break;
        case TR_CLOCK: pool_base_addr = rack->clock.elements; break;
        case TR_SEQ8: pool_base_addr = rack->seq8.elements; break;
        case TR_ADSR: pool_base_addr = rack->adsr.elements; break;
        case TR_VCA: pool_base_addr = rack->vca.elements; break;
        case TR_LP: pool_base_addr = rack->lp.elements; break;
        case TR_MIXER: pool_base_addr = rack->mixer.elements; break;
        case TR_NOISE: pool_base_addr = rack->noise.elements; break;
        case TR_QUANTIZER: pool_base_addr = rack->quantizer.elements; break;
        case TR_RANDOM: pool_base_addr = rack->random.elements; break;
        case TR_SPEAKER: pool_base_addr = rack->speaker.elements; break;
        case TR_SCOPE: pool_base_addr = rack->scope.elements; break;
    }
    
    void* module_addr = (uint8_t*)pool_base_addr + (index * module_info->struct_size);
    return module_addr;
}

void tr_serialize_input_buffer(tr_serialize_context_t* ctx, const char* name, const float** value)
{
    const float* buffer = *value;
    if (buffer == NULL)
    {
        return;
    }

    const tr_output_plug_kv_t* plug_kv = hmgetp_null(g_input.output_plugs, buffer);
    assert(plug_kv != NULL);
    const tr_output_plug_t* plug = &plug_kv->value;

    const size_t module_index = tr_get_gui_module_index(ctx->rack, plug->module);

    const tr_module_info_t* module_info = &tr_module_infos[plug->module->type];

    const void* module_addr = tr_get_module_address(ctx->rack, plug->module->type, plug->module->index);
    const uintptr_t field_offset = (uintptr_t)buffer - (uintptr_t)module_addr;
    
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

    const Color color = hmget(g_input.input_plugs, value).color;

    ctx->pos += sprintf(ctx->out + ctx->pos, "input_buffer %s > %s %zu %s color %hhu %hhu %hhu\n", name, module_info->id, module_index, field_info->name, color.r, color.g, color.b);
}

size_t tr_rack_serialize(char* out, rack_t* rack)
{
    tr_serialize_context_t ctx = {rack, out};
    
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        const tr_gui_module_t* module = &rack->gui_modules[i];
        const char* name = tr_module_infos[module->type].id;
        
        ctx.pos += sprintf(out + ctx.pos, "module %s %zu pos %d %d\n", name, i, module->x, module->y);
        
        const void* module_addr = tr_get_module_address(rack, module->type, module->index);
        
        const tr_module_info_t* module_info = &tr_module_infos[module->type];
        for (size_t field_index = 0; field_index < module_info->field_count; ++field_index)
        {
            const tr_module_field_info_t* field_info = &module_info->fields[field_index];
            if (field_info->type == TR_MODULE_FIELD_INPUT_FLOAT)
            {
                const float value = *(float*)((uint8_t*)module_addr + field_info->offset);
                tr_serialize_input_float(&ctx, field_info->name, value);
            }
            else if (field_info->type == TR_MODULE_FIELD_INPUT_INT)
            {
                const int value = *(int*)((uint8_t*)module_addr + field_info->offset);
                tr_serialize_input_int(&ctx, field_info->name, value);
            }
            else if (field_info->type == TR_MODULE_FIELD_INPUT_BUFFER)
            {
                const float** value = (float**)((uint8_t*)module_addr + field_info->offset);
                tr_serialize_input_buffer(&ctx, field_info->name, value);
            }
        }
    }

    return ctx.pos;
}

typedef struct tr_tokenizer
{
    const char* buf;
    size_t len;
    size_t pos;
} tr_tokenizer_t;

typedef struct tr_token
{
    enum
    {
        TR_TOKEN_ID,
        TR_TOKEN_NUMBER,
        TR_TOKEN_ARROW,
        TR_TOKEN_EOF,
    } type;
    size_t pos;
    size_t len;
} tr_token_t;

const char* get_token_type_name(const tr_token_t* t)
{
    switch (t->type)
    {
        case TR_TOKEN_ID: return "ID";
        case TR_TOKEN_NUMBER: return "NUMBER";
        case TR_TOKEN_ARROW: return "ARROW";
        case TR_TOKEN_EOF: return "EOF";
        default: return NULL;
    }
}

static const tr_token_t tr_eof_token = {TR_TOKEN_EOF};

bool tr_advance(tr_tokenizer_t* t)
{
    if (t->pos >= t->len)
    {
        return false;
    }

    ++t->pos;
    return true;
}

tr_token_t tr_next_token_internal(tr_tokenizer_t* t)
{
    while (isspace(t->buf[t->pos]) || t->buf[t->pos] == '\n')
    {
        if (!tr_advance(t)) return tr_eof_token;
    }
    
    if (t->pos >= t->len)
    {
        return tr_eof_token;
    }

    const char c = t->buf[t->pos];
    if (c == '>')
    {
        const tr_token_t tok = {TR_TOKEN_ARROW, t->pos, 1};
        if (!tr_advance(t)) return tr_eof_token;
        return tok;
    }
    else if (isdigit(c))
    {
        const size_t start = t->pos;
        while (isdigit(t->buf[t->pos]) || t->buf[t->pos] == '.')
        {
            if (!tr_advance(t)) return tr_eof_token;
        }
        const tr_token_t tok = {TR_TOKEN_NUMBER, start, t->pos - start};
        return tok;
    }
    else if (isalpha(c))
    {
        const size_t start = t->pos;
        while (isalnum(t->buf[t->pos]) || t->buf[t->pos] == '_')
        {
            if (!tr_advance(t)) return tr_eof_token;
        }
        const tr_token_t tok = {TR_TOKEN_ID, start, t->pos - start};
        return tok;
    }
    else
    {
        printf("unexpected character: %c\n", t->buf[t->pos]);
        assert(0);
        return tr_eof_token;
    }
}

tr_token_t tr_next_token(tr_tokenizer_t* t)
{
    tr_token_t tok = tr_next_token_internal(t);
    printf("(%s) %.*s\n", get_token_type_name(&tok), (int)tok.len, t->buf + tok.pos);
    return tok;
}

int tr_token_strcmp(const char* buf, const tr_token_t* tok, const char* str)
{
    const size_t len = strlen(str);
    if (tok->len != len)
    {
        return -1;
    }

    return strncmp(buf + tok->pos, str, len);
}

int tr_token_to_int(const char* buf, const tr_token_t* tok)
{
    assert(tok->type == TR_TOKEN_NUMBER);
    char temp[64];
    memcpy(temp, buf + tok->pos, tok->len);
    temp[tok->len] = '\0';
    return atoi(temp);
}

typedef struct tr_parser_cmd_add_module
{
    enum tr_module_type type;
    int x, y;
} tr_parser_cmd_add_module_t;

typedef struct tr_parser_cmd_set_value
{
    enum 
    {
        TR_SET_VALUE_INT,
        TR_SET_VALUE_FLOAT,
        TR_SET_VALUE_BUFFER,
    } type;

    size_t module_index;
    size_t field_offset;

    // TR_SET_VALUE_INT
    int int_value;

    // TR_SET_VALUE_FLOAT
    float float_value;

    // TR_SET_VALUE_BUFFER
    size_t target_module_index;
    size_t target_field_offset;
    Color color;
} tr_parser_cmd_set_value_t;

typedef struct tr_parser_cmd_connect
{
    size_t source_module_index;
    size_t input_offset;
    size_t target_module_index;
    size_t buffer_offset;
} tr_parser_cmd_connect_t;

typedef struct tr_parser
{
    size_t module_count;
    size_t value_count;
    tr_parser_cmd_add_module_t modules[1024];
    tr_parser_cmd_set_value_t values[1024];
} tr_parser_t;

int tr_parse_line(tr_parser_t* p, tr_tokenizer_t* t)
{
    tr_token_t tok = tr_next_token(t);
    if (tok.type == TR_TOKEN_EOF)
    {
        return 1;
    }

    assert(tok.type == TR_TOKEN_ID);
    
    if (tr_token_strcmp(t->buf, &tok, "module") == 0)
    {
        tr_token_t module_id_tok = tr_next_token(t);
        assert(module_id_tok.type == TR_TOKEN_ID);

        tr_token_t module_index_tok = tr_next_token(t);
        assert(module_index_tok.type == TR_TOKEN_NUMBER);

        tr_token_t pos_tok = tr_next_token(t);
        assert(pos_tok.type == TR_TOKEN_ID);
        assert(tr_token_strcmp(t->buf, &pos_tok, "pos") == 0);

        tr_token_t x_tok = tr_next_token(t);
        assert(x_tok.type == TR_TOKEN_NUMBER);

        tr_token_t y_tok = tr_next_token(t);
        assert(y_tok.type == TR_TOKEN_NUMBER);
        
        enum tr_module_type module_type = TR_MODULE_COUNT;
        for (int i = 0; i < TR_MODULE_COUNT; ++i)
        {
            if (tr_token_strcmp(t->buf, &module_id_tok, tr_module_infos[i].id) == 0)
            {
                module_type = i;
                break;
            }
        }
        assert(module_type != TR_MODULE_COUNT);
        
        p->modules[p->module_count++] = (tr_parser_cmd_add_module_t){
            .type = module_type,
            .x = tr_token_to_int(t->buf, &x_tok),
            .y = tr_token_to_int(t->buf, &y_tok),
        };
    }
    else if (tr_token_strcmp(t->buf, &tok, "input") == 0)
    {
        assert(p->module_count > 0);
        const size_t module_index = p->module_count - 1;
        const tr_module_info_t* module_info = &tr_module_infos[p->modules[module_index].type];

        tr_token_t field_id_tok = tr_next_token(t);
        assert(field_id_tok.type == TR_TOKEN_ID);
        
        const tr_module_field_info_t* field_info = NULL;

        for (int i = 0; i < module_info->field_count; ++i)
        {
            if (tr_token_strcmp(t->buf, &field_id_tok, module_info->fields[i].name) == 0)
            {
                field_info = &module_info->fields[i];
                break;
            }
        }
        assert(field_info != NULL);
        assert(field_info->type == TR_MODULE_FIELD_INPUT_FLOAT || field_info->type == TR_MODULE_FIELD_INPUT_INT);

        tr_token_t field_value_tok = tr_next_token(t);
        assert(field_value_tok.type == TR_TOKEN_NUMBER);
        
        tr_parser_cmd_set_value_t* value = &p->values[p->value_count++];
        value->module_index = module_index;
        value->field_offset = field_info->offset;
        if (field_info->type == TR_MODULE_FIELD_INPUT_FLOAT)
        {
            value->type = TR_SET_VALUE_FLOAT;
            value->float_value = (float)atof(t->buf + field_value_tok.pos);
        }
        else
        {
            value->type = TR_SET_VALUE_INT;
            value->int_value = atoi(t->buf + field_value_tok.pos);
        }
    }
    else if (tr_token_strcmp(t->buf, &tok, "input_buffer") == 0)
    {
        assert(p->module_count > 0);
        const size_t module_index = p->module_count - 1;
        const tr_module_info_t* module_info = &tr_module_infos[p->modules[module_index].type];

        tr_token_t field_id_tok = tr_next_token(t);
        assert(field_id_tok.type == TR_TOKEN_ID);
        
        const tr_module_field_info_t* field_info = NULL;
        for (int i = 0; i < module_info->field_count; ++i)
        {
            if (tr_token_strcmp(t->buf, &field_id_tok, module_info->fields[i].name) == 0)
            {
                field_info = &module_info->fields[i];
                break;
            }
        }
        assert(field_info != NULL);
        assert(field_info->type == TR_MODULE_FIELD_INPUT_BUFFER);

        tr_token_t arrow_tok = tr_next_token(t);
        assert(arrow_tok.type == TR_TOKEN_ARROW);

        tr_token_t target_module_tok = tr_next_token(t);
        assert(target_module_tok.type == TR_TOKEN_ID);

        enum tr_module_type target_module_type = TR_MODULE_COUNT;
        for (int i = 0; i < TR_MODULE_COUNT; ++i)
        {
            if (tr_token_strcmp(t->buf, &target_module_tok, tr_module_infos[i].id) == 0)
            {
                target_module_type = i;
                break;
            }
        }
        assert(target_module_type != TR_MODULE_COUNT);
        const tr_module_info_t* target_module_info = &tr_module_infos[target_module_type];
        
        tr_token_t module_index_tok = tr_next_token(t);
        assert(module_index_tok.type == TR_TOKEN_NUMBER);

        tr_token_t target_field_tok = tr_next_token(t);
        assert(target_field_tok.type == TR_TOKEN_ID);

        const tr_module_field_info_t* target_field_info = NULL;
        for (int i = 0; i < target_module_info->field_count; ++i)
        {
            if (tr_token_strcmp(t->buf, &target_field_tok, target_module_info->fields[i].name) == 0)
            {
                target_field_info = &target_module_info->fields[i];
                break;
            }
        }
        assert(target_field_info != NULL);
        assert(target_field_info->type == TR_MODULE_FIELD_BUFFER);

        tr_token_t color_id_tok = tr_next_token(t);
        assert(color_id_tok.type == TR_TOKEN_ID);
        assert(tr_token_strcmp(t->buf, &color_id_tok, "color") == 0);

        const tr_token_t color_r_tok = tr_next_token(t);
        const tr_token_t color_g_tok = tr_next_token(t);
        const tr_token_t color_b_tok = tr_next_token(t);
        const uint8_t r = (uint8_t)tr_token_to_int(t->buf, &color_r_tok);
        const uint8_t g = (uint8_t)tr_token_to_int(t->buf, &color_g_tok);
        const uint8_t b = (uint8_t)tr_token_to_int(t->buf, &color_b_tok);

        p->values[p->value_count++] = (tr_parser_cmd_set_value_t){
            .type = TR_SET_VALUE_BUFFER,
            .module_index = module_index,
            .field_offset = field_info->offset,
            .target_module_index = tr_token_to_int(t->buf, &module_index_tok),
            .target_field_offset = target_field_info->offset,
            .color = {r, g, b, 0xff},
        };
    }

    return 0;
}

void tr_rack_deserialize(rack_t* rack, const char* input, size_t len)
{
    memset(rack, 0, sizeof(rack_t));
    
    tr_tokenizer_t tokenizer = {input, len};
    tr_parser_t parser = {0};

    while (1)
    {
        if (tr_parse_line(&parser, &tokenizer))
        {
            break;
        }
    }
    
    for (size_t i = 0; i < parser.module_count; ++i)
    {
        const tr_parser_cmd_add_module_t* cmd = &parser.modules[i];

        printf("ADD MODULE: %s %d %d\n", tr_module_infos[cmd->type].id, cmd->x, cmd->y);

        tr_gui_module_t* module = tr_rack_create_module(rack, cmd->type);
        module->x = cmd->x;
        module->y = cmd->y;
    }

    for (size_t i = 0; i < parser.value_count; ++i)
    {
        const tr_parser_cmd_set_value_t* cmd = &parser.values[i];

        const tr_gui_module_t* module = &rack->gui_modules[cmd->module_index];
        void* module_addr = tr_get_module_address(rack, module->type, module->index);
        void* field_addr = (uint8_t*)module_addr + cmd->field_offset;

        switch (cmd->type)
        {
            case TR_SET_VALUE_FLOAT:
                printf("SET FLOAT: %zu:%zu = %f\n", cmd->module_index, cmd->field_offset, cmd->float_value);
                memcpy(field_addr, &cmd->float_value, sizeof(float));
                break;
            case TR_SET_VALUE_INT:
                printf("SET INT: %zu:%zu = %d\n", cmd->module_index, cmd->field_offset, cmd->int_value);
                memcpy(field_addr, &cmd->int_value, sizeof(int));
                break;
            case TR_SET_VALUE_BUFFER:
                printf("SET BUFFER: %zu:%zu = %zu:%zu\n", cmd->module_index, cmd->field_offset, cmd->target_module_index, cmd->target_field_offset);
                const tr_gui_module_t* target_module = &rack->gui_modules[cmd->target_module_index];
                void* target_module_addr = tr_get_module_address(rack, target_module->type, target_module->index);
                void* target_field_addr = (uint8_t*)target_module_addr + cmd->target_field_offset;
                //*(float**)field_addr = (float*)target_field_addr;
                memcpy(field_addr, &target_field_addr, sizeof(void*));

                const tr_input_plug_t plug = {cmd->color};
                hmput(g_input.input_plugs, field_addr, plug);
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

void tr_gui_module_begin(tr_gui_module_t* module, int width, int height)
{
    g_input.draw_module = module;

    if (g_input.drag_module == module)
    {
        module->x += (int)GetMouseDelta().x;
        module->y += (int)GetMouseDelta().y;
    }

    DrawRectangle(module->x, module->y, width, height, GRAY);
    DrawRectangle(
        module->x + TR_MODULE_PADDING, 
        module->y + TR_MODULE_PADDING, 
        width - TR_MODULE_PADDING * 2,
        height - TR_MODULE_PADDING * 2,
        DARKGRAY);

    DrawText(
        tr_module_infos[module->type].id, 
        module->x + TR_MODULE_PADDING * 2, 
        module->y + TR_MODULE_PADDING * 2,
        20,
        WHITE);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), g_input.camera);
        if (mouse.x > module->x &&
            mouse.y > module->y &&
            mouse.x < module->x + width && 
            mouse.y < module->y + 20)
        {
            g_input.drag_module = module;
        }
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
        const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), g_input.camera);
        if (Vector2Distance(mouse, center) < TR_KNOB_RADIUS)
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
        const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), g_input.camera);
        if (Vector2Distance(mouse, center) < TR_KNOB_RADIUS)
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
    const bool highlight = g_input.drag_output != NULL;

    DrawCircle(x, y, TR_PLUG_RADIUS, LIGHTGRAY);

    Color inner_color = DARKGRAY;
    if (highlight)
    {
        inner_color = TR_PLUG_HIGHLIGHT_COLOR;
    }
    if (*value != NULL)
    {
        inner_color = hmget(g_input.input_plugs, value).color;
    }
    DrawCircle(x, y, TR_PLUG_RADIUS - TR_PLUG_PADDING, inner_color);
    
    const Vector2 center = {(float)x, (float)y};
    const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), g_input.camera);

    if (g_input.drag_output != NULL && 
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        Vector2Distance(mouse, center) < TR_PLUG_RADIUS)
    {
        *value = g_input.drag_output;
        g_input.drag_output = NULL;

        const tr_input_plug_t plug = {g_input.drag_color};
        hmput(g_input.input_plugs, value, plug);
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (Vector2Distance(mouse, center) < TR_PLUG_RADIUS)
        {
            g_input.drag_input = value;
            g_input.drag_color = tr_random_cable_color();
        }
    }

    if (g_input.drag_input == value)
    {
        g_input.cable_draws[g_input.cable_draw_count++] = (tr_cable_draw_command_t){
            .from = center,
            .to = mouse,
            .color = g_input.drag_color,
        };
    }

    if (*value != NULL)
    {
        const tr_output_plug_t plug = hmget(g_input.output_plugs, *value);
        const tr_input_plug_t input_plug = hmget(g_input.input_plugs, value);
        g_input.cable_draws[g_input.cable_draw_count++] = (tr_cable_draw_command_t){
            .from = center,
            .to = {(float)plug.x, (float)plug.y},
            .color = ColorAlpha(input_plug.color, 0.65f),
        };
    }
}

void tr_gui_plug_output(const float* buffer, int x, int y)
{
    const bool highlight = g_input.drag_input != NULL;
    
    const int rw = TR_PLUG_RADIUS + 2;
    DrawRectangle(x - rw, y - rw, 2 * rw, 2 * rw, WHITE);
    DrawCircle(x, y, TR_PLUG_RADIUS, LIGHTGRAY);
    DrawCircle(x, y, TR_PLUG_RADIUS - TR_PLUG_PADDING, highlight ? TR_PLUG_HIGHLIGHT_COLOR : DARKGRAY);

    const Vector2 center = {(float)x, (float)y};
    const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), g_input.camera);

    if (g_input.drag_input != NULL && 
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
        Vector2Distance(mouse, center) < TR_PLUG_RADIUS)
    {
        const tr_input_plug_t plug = {g_input.drag_color};
        hmput(g_input.input_plugs, g_input.drag_input, plug);

        *g_input.drag_input = buffer;
        g_input.drag_input = NULL;
    }
    
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (Vector2Distance(mouse, center) < TR_PLUG_RADIUS)
        {
            g_input.drag_output = buffer;
            g_input.drag_color = tr_random_cable_color();
        }
    }

    if (g_input.drag_output == buffer)
    {
        g_input.cable_draws[g_input.cable_draw_count++] = (tr_cable_draw_command_t){
            .from = center,
            .to = mouse,
            .color = g_input.drag_color,
        };
    }

    const tr_output_plug_t plug = {x, y, g_input.draw_module};
    hmput(g_input.output_plugs, buffer, plug);
}

void tr_vco_draw(tr_vco_t* vco, tr_gui_module_t* module)
{
    assert(module->type == TR_VCO);
    tr_gui_module_begin(module, 100, 200);
    tr_gui_knob(&vco->in_v0, 20.0f, 1000.0f, module->x + 24, module->y + 50);
    tr_gui_plug_input(&vco->in_voct, module->x + 20, module->y + 80);
    tr_gui_plug_output(vco->out_sin, module->x + 70, module->y + 50);
    tr_gui_plug_output(vco->out_saw, module->x + 70, module->y + 85);
    tr_gui_plug_output(vco->out_sqr, module->x + 70, module->y + 120);
    tr_gui_module_end();
}

void tr_clock_draw(tr_clock_t* clock, tr_gui_module_t* module)
{
    tr_gui_module_begin(module, 100, 100);
    tr_gui_knob(&clock->in_hz, 0.01f, 50.0f, module->x + 24, module->y + 50);
    tr_gui_plug_output(clock->out_gate, module->x + 70, module->y + 50);
    tr_gui_module_end();
}

void tr_seq8_draw(tr_seq8_t* seq8, tr_gui_module_t* module)
{
    tr_gui_module_begin(module, 400, 100);
    for (int i = 0; i < 8; ++i)
    {
        const int kx = module->x + 50 + (i * (TR_KNOB_RADIUS*2 + 4));
        const int ky = module->y + 50;
        tr_gui_knob(&seq8->in_cv_0 + i, 0.001f, 1.0f, kx, ky);
        DrawCircle(kx, ky + TR_KNOB_RADIUS + 8, 4.0f, seq8->step == i ? GREEN : BLACK);
    }
    tr_gui_plug_input(&seq8->in_step, module->x + 20, module->y + 50);
    tr_gui_plug_output(seq8->out_cv, module->x + 380, module->y + 50);
    tr_gui_module_end();
}

void tr_adsr_draw(tr_adsr_t* adsr, tr_gui_module_t* module)
{
    tr_gui_module_begin(module, 200, 100);
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
    tr_gui_module_begin(module, 200, 100);
    const int x = module->x;
    const int y = module->y;
    tr_gui_plug_input(&vca->in_audio, x + 24 + (0 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_plug_input(&vca->in_cv, x + 24 + (1 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_plug_output(vca->out_mix, x + 24 + (2 * (TR_KNOB_RADIUS*2+4)), y + 50);
    tr_gui_module_end();
}

void tr_lp_draw(tr_lp_t* lp, tr_gui_module_t* module)
{
    tr_gui_module_begin(module, 250, 100);
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
    tr_gui_module_begin(module, 250, 100);
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
    tr_gui_module_begin(module, 100, 100);
    tr_gui_plug_output(noise->out_white, module->x + 24, module->y + 50);
    tr_gui_module_end();
}

void tr_random_draw(tr_random_t* random, tr_gui_module_t* module)
{
    tr_gui_module_begin(module, 100, 100);
    tr_gui_knob(&random->in_speed, 0.0f, 10.0f, module->x + 24, module->y + 50);
    tr_gui_plug_output(random->out_cv, module->x + 80, module->y + 50);
    tr_gui_module_end();
}

void tr_quantizer_draw(tr_quantizer_t* quantizer, tr_gui_module_t* module)
{
    tr_gui_module_begin(module, 250, 100);
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
    tr_gui_module_begin(module, 100, 100);
    tr_gui_plug_input(&speaker->in_audio, module->x + 40, module->y + 50);
    tr_gui_module_end();
}

void tr_scope_draw(tr_scope_t* scope, tr_gui_module_t* module)
{
    tr_gui_module_begin(module, 200, 220);
    tr_gui_plug_input(&scope->in_0, module->x + TR_PLUG_RADIUS + 8, module->y + 220 - TR_PLUG_RADIUS - 8);
    
    const int screen_x = module->x + 8;
    const int screen_y = module->y + 28;
    const int screen_w = 200 - 8*2;
    const int screen_h = 140;
    DrawRectangle(screen_x, screen_y, screen_w, screen_h, BLACK);

    if (scope->in_0 != NULL)
    {
        for (int i = 0; i < TR_SAMPLE_COUNT - 1; ++i)
        {
            const float s0 = scope->in_0[i];
            const float s1 = scope->in_0[i + 1];

            const float r0 = Clamp((s0 + 1.0f) * 0.5f, 0.0f, 1.0f);
            const float r1 = Clamp((s1 + 1.0f) * 0.5f, 0.0f, 1.0f);
            
            const float x0 = ((i + 0) / (float)TR_SAMPLE_COUNT) * screen_w + screen_x;
            const float x1 = ((i + 1) / (float)TR_SAMPLE_COUNT) * screen_w + screen_x;
            
            const float y0 = r0 * screen_h + screen_y;
            const float y1 = r1 * screen_h + screen_y;

            DrawLineEx((Vector2){x0, y0}, (Vector2){x1, y1}, 1.0f, YELLOW);
        }
    }

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
        case TR_SCOPE: tr_scope_draw(&rack->scope.elements[module->index], module); break;
        case TR_MODULE_COUNT: assert(0);
    }
}

void tr_update_modules(rack_t* rack, tr_gui_module_t** modules, size_t count)
{
#ifndef __EMSCRIPTEN__
    //printf("tr_update_modules:\n");
#endif

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
            case TR_SPEAKER: break;
            case TR_SCOPE: break;
            case TR_MODULE_COUNT: assert(0);
        }

#ifndef __EMSCRIPTEN__
        //printf("%s %zu\n", g_tr_gui_modinfo[module->type].name, module->index);
#endif
    }
}

void tr_enumerate_inputs(const float** inputs, int* count, rack_t* rack, const tr_gui_module_t* module)
{
    *count = 0;

    switch (module->type)
    {
        case TR_VCO: {
            const tr_vco_t* vco = &rack->vco.elements[module->index];
            inputs[(*count)++] = vco->in_voct;
            break;
        }
        case TR_SEQ8: {
            const tr_seq8_t* seq8 = &rack->seq8.elements[module->index];
            inputs[(*count)++] = seq8->in_step;
            break;
        }
        case TR_SPEAKER: {
            const tr_speaker_t* speaker = &rack->speaker.elements[module->index];
            inputs[(*count)++] = speaker->in_audio;
            break;
        }
        case TR_QUANTIZER: {
            const tr_quantizer_t* quantizer = &rack->quantizer.elements[module->index];
            inputs[(*count)++] = quantizer->in_cv;
            break;
        }
        case TR_LP: {
            const tr_lp_t* lp = &rack->lp.elements[module->index];
            inputs[(*count)++] = lp->in_audio;
            inputs[(*count)++] = lp->in_cut;
            break;
        }
        case TR_ADSR: {
            const tr_adsr_t* adsr = &rack->adsr.elements[module->index];
            inputs[(*count)++] = adsr->in_gate;
            break;
        }
        case TR_MIXER: {
            const tr_mixer_t* mixer = &rack->mixer.elements[module->index];
            inputs[(*count)++] = mixer->in_0;
            inputs[(*count)++] = mixer->in_1;
            inputs[(*count)++] = mixer->in_2;
            inputs[(*count)++] = mixer->in_3;
            break;
        }
        case TR_VCA: {
            const tr_vca_t* vca = &rack->vca.elements[module->index];
            inputs[(*count)++] = vca->in_audio;
            inputs[(*count)++] = vca->in_cv;
            break;
        }
        // these have no inputs
        case TR_CLOCK:
        case TR_NOISE:
        case TR_SCOPE:
        case TR_RANDOM:
            break;
        case TR_MODULE_COUNT: assert(0);
    }
}

typedef struct tr_module_sort_data
{
    uint32_t module_index;
    int distance;
} tr_module_sort_data_t;

int tr_update_module_sort_function(const void* lhs, const void* rhs)
{
    return ((const tr_module_sort_data_t*)rhs)->distance - ((const tr_module_sort_data_t*)lhs)->distance;
}

size_t tr_resolve_module_graph(tr_gui_module_t** update_modules, rack_t* rack, const tr_gui_module_t* speaker)
{
    typedef struct stack_item
    {
        const tr_gui_module_t* module;
        const tr_gui_module_t* parent;
    } stack_item_t;
    
    stack_item_t stack[TR_GUI_MODULE_COUNT];
    int sp = 0;

    stack[sp++] = (stack_item_t){speaker};

    uint32_t visited_mask[TR_GUI_MODULE_COUNT / 32];
    memset(visited_mask, 0, sizeof(visited_mask));

    uint32_t distance[TR_GUI_MODULE_COUNT];
    memset(distance, 0, sizeof(distance));

    while (sp > 0)
    {
        const stack_item_t top = stack[--sp];
        const size_t module_index = tr_get_gui_module_index(rack, top.module);
        
        if (visited_mask[module_index / 32] & (1 << (module_index % 32)))
        {
            continue;
        }

        visited_mask[module_index / 32] |= (1 << (module_index % 32));

        if (top.parent != NULL)
        {
            const size_t parent_module_index = tr_get_gui_module_index(rack, top.parent);
            assert(visited_mask[parent_module_index / 32] & (1 << (parent_module_index % 32)));
            const uint32_t next_distance = distance[parent_module_index] + 1;
            if (distance[module_index] < next_distance)
            {
                distance[module_index] = next_distance;
            }
        }

        const float* inputs[64];
        int input_count;
        tr_enumerate_inputs(inputs, &input_count, rack, top.module);

        for (int i = 0; i < input_count; ++i)
        {
            if (inputs[i] == NULL)
            {
                continue;
            }

            const tr_output_plug_t plug = hmget(g_input.output_plugs, inputs[i]);

            assert(sp < tr_countof(stack));
            stack[sp++] = (stack_item_t){plug.module, top.module};
        }
    }

    tr_module_sort_data_t sort_data[TR_GUI_MODULE_COUNT];
    size_t update_count = 0;

    for (uint32_t mask_i = 0; mask_i < (TR_GUI_MODULE_COUNT / 32); ++mask_i)
    {
        if (visited_mask[mask_i] == 0)
        {
            continue;
        }

        const uint32_t mask = visited_mask[mask_i];
        for (uint32_t bit = 0; bit < 32; ++bit)
        {
            if (mask & (1 << bit))
            {
                const uint32_t module_index = mask_i * 32 + bit;
                sort_data[update_count++] = (tr_module_sort_data_t){module_index, distance[module_index]};
            }
        }
    }

    qsort(sort_data, update_count, sizeof(tr_gui_module_t*), tr_update_module_sort_function);

    for (size_t i = 0; i < update_count; ++i)
    {
        update_modules[i] = &rack->gui_modules[sort_data[i].module_index];
    }

    return update_count;
}

void tr_produce_final_mix(int16_t* output, rack_t* rack)
{
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
        memset(output, 0, sizeof(int16_t) * TR_SAMPLE_COUNT);
        return;
    }

    tr_gui_module_t* update_modules[TR_GUI_MODULE_COUNT];
    const size_t update_count = tr_resolve_module_graph(update_modules, rack, speaker);

    tr_update_modules(rack, update_modules, update_count);

    assert(speaker->type == TR_SPEAKER);
    final_mix(output, rack->speaker.elements[speaker->index].in_audio);
}

typedef struct app
{
    rack_t* rack;
    AudioStream stream;
    enum tr_module_type add_module_type;
#ifdef TR_RECORDING_FEATURE
    bool is_recording;
    int16_t* recording_samples;
    size_t recording_offset;
#endif
#ifdef TR_AUDIO_CALLBACK
    int16_t final_mix[TR_SAMPLE_COUNT];
    size_t final_mix_remaining;
#endif
} app_t;

static app_t g_app = {0};

#ifdef TR_AUDIO_CALLBACK
void tr_audio_callback(void *bufferData, unsigned int frames)
{
    int16_t* samples = bufferData;
    size_t write_cursor = 0;

    //printf("frames: %d\n", frames);

    while (write_cursor < frames)
    {
        if (g_app.final_mix_remaining == 0)
        {
            g_app.final_mix_remaining = TR_SAMPLE_COUNT;
            tr_produce_final_mix(g_app.final_mix, g_app.rack);
        }

        const size_t final_mix_cursor = TR_SAMPLE_COUNT - g_app.final_mix_remaining;
        const size_t frames_available = frames - write_cursor;

        size_t final_mix_available = g_app.final_mix_remaining;
        if (final_mix_available > frames_available)
        {
            final_mix_available = frames_available;
        }

        //printf("write_cursor: %zu, final_mix_cursor: %zu, final_mix_available: %zu\n", write_cursor, final_mix_cursor, final_mix_available);
        
        memcpy(samples + write_cursor, g_app.final_mix + final_mix_cursor, final_mix_available * sizeof(int16_t));

        write_cursor += final_mix_available;
        g_app.final_mix_remaining -= final_mix_available;
    }
}
#endif

void tr_frame_update_draw(void)
{
    app_t* app = &g_app;
    rack_t* rack = app->rack;

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        g_input.camera.offset = Vector2Add(g_input.camera.offset, GetMouseDelta());
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
    {
        char* buffer = malloc(1024 * 1024);
        size_t len = tr_rack_serialize(buffer, rack);
        printf("%.*s", (int)len, buffer);
        free(buffer);
    }

    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode2D(g_input.camera);

    g_input.cable_draw_count = 0;
    
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        tr_gui_module_draw(rack, &rack->gui_modules[i]);
    }

    for (size_t i = 0; i < g_input.cable_draw_count; ++i)
    {
        const tr_cable_draw_command_t* draw = &g_input.cable_draws[i];
        DrawLineEx(draw->from, draw->to, 6.0f, draw->color);
        //DrawLineBezier(draw->from, draw->to, 6.0f, draw->color);
    }

    EndMode2D();

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
    {
        if (g_input.drag_input != NULL)
        {
            *g_input.drag_input = NULL;
        }
        g_input.active_value = NULL;
        g_input.drag_module = NULL;
        g_input.drag_input = NULL;
        g_input.drag_output = NULL;
    }
    
    {
        if (GetMouseWheelMoveV().y < 0.0f)
        {
            app->add_module_type += 1;
        }
        else if (GetMouseWheelMoveV().y > 0.0f)
        {
            app->add_module_type += TR_MODULE_COUNT - 1;
        }
        app->add_module_type = app->add_module_type % TR_MODULE_COUNT;

        const int menu_height = 42;

        DrawRectangle(0, 0, 10000, menu_height, GetColor(0x303030ff));
        DrawText(tr_module_infos[app->add_module_type].id, 0, 0, 40, WHITE);

#ifdef TR_RECORDING_FEATURE
        const char* recording_text = app->is_recording ? "Press F5 to stop recording" : "Press F5 to start recording";
        DrawText(recording_text, 260, menu_height / 2 - 12, 24, WHITE);
        DrawCircle(240, menu_height / 2, 14, app->is_recording ? RED : GRAY);
#endif

        const Vector2 mouse = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
            mouse.y < menu_height)
        {
            const Vector2 world_mouse = GetScreenToWorld2D(mouse, g_input.camera);

            tr_gui_module_t* module = tr_rack_create_module(rack, app->add_module_type);
            module->x = (int)world_mouse.x;
            module->y = (int)world_mouse.y;
            g_input.drag_module = module;
        }
    }

#ifdef TR_RECORDING_FEATURE
    if (IsKeyPressed(KEY_F5))
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

    EndDrawing();

#ifndef TR_AUDIO_CALLBACK
    if (IsAudioStreamProcessed(app->stream))
    {
#ifdef TR_TIMER_MODULE
        timer_t timer;
        timer_start(&timer);
#endif

        int16_t final_mix[TR_SAMPLE_COUNT];
        tr_produce_final_mix(final_mix, rack);
        
        UpdateAudioStream(app->stream, final_mix, TR_SAMPLE_COUNT);

#ifdef TR_RECORDING_FEATURE
        if (app->is_recording)
        {
            memcpy(app->recording_samples + app->recording_offset, final_mix, sizeof(final_mix));
            app->recording_offset += TR_SAMPLE_COUNT;
        }
#endif

#ifdef TR_TIMER_MODULE
        //const double ms = timer_reset(&timer);
        //printf("final_mix: %.3f us\n", ms * 1000.0);
#endif
    }
#endif
}

static const char* TEST = "\
module speaker 0 pos 1014 368 \
input_buffer in_audio > mixer 6 out_mix color 190 33 55 \
module vco 1 pos 156 259 \
input in_v0 0.000000 \
module clock 2 pos 147 116 \
input in_hz 6.008801 \
module seq8 3 pos 313 107 \
input_buffer in_step > clock 2 out_gate color 211 176 131 \
input in_cv_0 0.000000 \
input in_cv_1 0.480520 \
input in_cv_2 0.640360 \
input in_cv_3 0.000000 \
input in_cv_4 0.000000 \
input in_cv_5 0.580420 \
input in_cv_6 0.000000 \
input in_cv_7 0.680320 \
module seq8 4 pos 318 270 \
input_buffer in_step > clock 2 out_gate color 135 60 190 \
input in_cv_0 0.000000 \
input in_cv_1 0.330670 \
input in_cv_2 0.000000 \
input in_cv_3 0.619380 \
input in_cv_4 0.000000 \
input in_cv_5 0.000000 \
input in_cv_6 0.470530 \
input in_cv_7 0.820179 \
module vco 5 pos 459 417 \
input in_v0 314.000000 \
input_buffer in_voct > quantizer 9 out_cv color 230 41 55 \
module mixer 6 pos 835 177 \
input_buffer in_0 > lp 7 out_audio color 190 33 55 \
input_buffer in_1 > lp 14 out_audio color 0 228 48 \
input_buffer in_2 > lp 13 out_audio color 135 60 190 \
input in_vol0 0.270000 \
input in_vol1 0.290000 \
input in_vol2 0.410000 \
input in_vol3 0.000000 \
module lp 7 pos 704 417 \
input_buffer in_audio > vco 5 out_saw color 0 228 48 \
input_buffer in_cut > adsr 8 out_env color 0 228 48 \
input in_cut0 0.100000 \
input in_cut_mul 0.340000 \
module adsr 8 pos 787 309 \
input in_attack 0.001000 \
input in_decay 0.070930 \
input in_sustain 0.000000 \
input in_release 0.240760 \
input_buffer in_gate > clock 2 out_gate color 135 60 190 \
module quantizer 9 pos 179 490 \
input in_mode 1 \
input_buffer in_cv > seq8 3 out_cv color 102 191 255 \
module quantizer 10 pos 182 610 \
input in_mode 1 \
input_buffer in_cv > seq8 4 out_cv color 190 33 55 \
module vco 11 pos 536 549 \
input in_v0 314.000000 \
input_buffer in_voct > quantizer 10 out_cv color 190 33 55 \
module noise 12 pos 662 642 \
module lp 13 pos 780 633 \
input_buffer in_audio > noise 12 out_white color 190 33 55 \
input_buffer in_cut > adsr 8 out_env color 211 176 131 \
input in_cut0 0.010000 \
input in_cut_mul 0.240000 \
module lp 14 pos 731 519 \
input_buffer in_audio > vco 11 out_saw color 230 41 55 \
input_buffer in_cut > adsr 8 out_env color 0 228 48 \
input in_cut0 0.000000 \
input in_cut_mul 0.480000";

int main()
{
    app_t* app = &g_app;

    app->rack = calloc(1, sizeof(rack_t));
    //rack_init(app->rack);

#if 0
    {
        tr_gui_module_t* speaker0 = tr_rack_create_module(rack, TR_SPEAKER);
        speaker0->x = 1280 / 2;
        speaker0->y = 720 / 2;
    }
#else
    tr_rack_deserialize(app->rack, TEST, strlen(TEST));
#endif

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "tinyrack");

    InitAudioDevice();
    assert(IsAudioDeviceReady());
    SetAudioStreamBufferSizeDefault(TR_SAMPLE_COUNT);

    app->stream = LoadAudioStream(TR_SAMPLE_RATE, 16, 1);
#ifdef TR_AUDIO_CALLBACK
    SetAudioStreamCallback(app->stream, tr_audio_callback);
#endif
    PlayAudioStream(app->stream);

    app->add_module_type = TR_VCO;

#ifdef TR_RECORDING_FEATURE
    app->is_recording = false;
    app->recording_samples = malloc(86400ull * TR_SAMPLE_RATE * sizeof(int16_t));
    app->recording_offset = 0;
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(tr_frame_update_draw, 0, 1);
#else
    while (!WindowShouldClose())
    {
        tr_frame_update_draw();
    }
#endif

    return 0;
}
