#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define TR_AUDIO_CALLBACK
#endif

#ifndef __EMSCRIPTEN__
#define TR_RECORDING_FEATURE
#endif

#include "timer.h"
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
#include <time.h>
#include <float.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "font.h"

// config
#define TR_TRACE_MODULE_UPDATES 0
#define TR_TRACE_MODULE_GRAPH 0


//
// GUI
//

#define TR_MODULE_POOL_SIZE 1024

typedef struct tr_module_pool
{
    size_t count;
    size_t element_size;
    void* elements;
} tr_module_pool_t;

static void* tr_module_pool_get(const tr_module_pool_t* pool, size_t index)
{
    return (uint8_t*)pool->elements + (pool->element_size * index);
}

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
    tr_module_pool_t module_pool[TR_MODULE_COUNT];
    tr_gui_module_t gui_modules[TR_GUI_MODULE_COUNT];
    size_t gui_module_count;
} rack_t;

size_t tr_get_gui_module_index(const rack_t* rack, const tr_gui_module_t* module)
{
    return module - rack->gui_modules;
}

void* tr_get_module_address(rack_t* rack, enum tr_module_type type, size_t index)
{
    const tr_module_pool_t* pool = &rack->module_pool[type];
    assert(pool->elements != NULL);
    return tr_module_pool_get(pool, index);
}

void* get_field_address(rack_t* rack, enum tr_module_type type, size_t module_index, size_t field_index)
{
    const tr_module_info_t* module_info = &tr_module_infos[type];
    void* module_addr = tr_get_module_address(rack, type, module_index);
    const size_t field_offset = module_info->fields[field_index].offset;
    return (uint8_t*)module_addr + field_offset;
}

size_t tr_rack_allocate_module(rack_t* rack, enum tr_module_type type)
{
    tr_module_pool_t* pool = &rack->module_pool[type];
    assert(pool->elements != NULL);
    return pool->count++;
}

tr_gui_module_t* tr_rack_create_module(rack_t* rack, enum tr_module_type type)
{
    assert(rack->gui_module_count < TR_GUI_MODULE_COUNT);
    tr_gui_module_t* module = &rack->gui_modules[rack->gui_module_count];
    memset(module, 0, sizeof(tr_gui_module_t));
    module->type = type;
    module->index = tr_rack_allocate_module(rack, type);
    
    const tr_module_info_t* module_info = &tr_module_infos[type];
    for (size_t i = 0; i < module_info->field_count; ++i)
    {
        const tr_module_field_info_t* field_info = &module_info->fields[i];
        switch (field_info->type)
        {
            case TR_MODULE_FIELD_INPUT_FLOAT:
                memcpy(get_field_address(rack, type, module->index, i), &field_info->default_value, sizeof(field_info->default_value));
                break;
            default: break;
        }
    }

    ++rack->gui_module_count;
    return module;
}

void rack_init(rack_t* rack)
{
    for (int i = 0; i < TR_MODULE_COUNT; ++i)
    {
        free(rack->module_pool[i].elements);
    }

    memset(rack, 0, sizeof(rack_t));

    for (int i = 0; i < TR_MODULE_COUNT; ++i)
    {
        rack->module_pool[i].element_size = tr_module_infos[i].struct_size;
        rack->module_pool[i].elements = calloc(TR_MODULE_POOL_SIZE, tr_module_infos[i].struct_size);
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
    Vector2 drag_offset;
    const float** drag_input;
    const float* drag_output;
    Color drag_color;
    tr_output_plug_kv_t* output_plugs;
    tr_input_plug_kv_t* input_plugs;
    bool do_not_process_input;

    tr_cable_draw_command_t cable_draws[1024];
    size_t cable_draw_count;

    Camera2D camera;
} tr_gui_input_t;

static tr_gui_input_t g_input = {
    .camera.zoom = 1.0f,
};

typedef struct app
{
    rack_t rack;
    AudioStream stream;
#ifdef TR_RECORDING_FEATURE
    bool is_recording;
    int16_t* recording_samples;
    size_t recording_offset;
#endif
#ifdef TR_AUDIO_CALLBACK
    int16_t final_mix[TR_SAMPLE_COUNT];
    size_t final_mix_remaining;
    bool has_audio_callback_been_called_once;
#endif
    bool picker_mode;
    bool paused;
    bool single_step;
    float fadein;

    size_t timer_index;
    double timer_produce_final_mix[32];
} app_t;

static app_t g_app = {0};

static const Color g_cable_colors[] = 
{
    { 255, 153, 148 },
    { 148, 148, 255 },
    { 148, 255, 188 },
    { 170, 166, 151 },
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

    //const Color color = hmget(g_input.input_plugs, value).color;

    ctx->pos += sprintf(ctx->out + ctx->pos, "input_buffer %s > %s %zu %s\n", name, module_info->id, module_index, field_info->name);
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
            switch (field_info->type)
            {
                case TR_MODULE_FIELD_INPUT_FLOAT:
                case TR_MODULE_FIELD_FLOAT:
                {
                    const float value = *(float*)((uint8_t*)module_addr + field_info->offset);
                    tr_serialize_input_float(&ctx, field_info->name, value);
                    break;
                }
                    
                case TR_MODULE_FIELD_INPUT_INT:
                case TR_MODULE_FIELD_INT:
                {
                    const int value = *(int*)((uint8_t*)module_addr + field_info->offset);
                    tr_serialize_input_int(&ctx, field_info->name, value);
                    break;
                }

                case TR_MODULE_FIELD_INPUT_BUFFER:
                {
                    const float** value = (const float**)((uint8_t*)module_addr + field_info->offset);
                    tr_serialize_input_buffer(&ctx, field_info->name, value);
                    break;
                }

                case TR_MODULE_FIELD_BUFFER:
                    break;
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
    else if (isdigit(c) || c == '-')
    {
        const size_t start = t->pos;
        while (isdigit(t->buf[t->pos]) || t->buf[t->pos] == '.' || t->buf[t->pos] == '-')
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
        TR_SET_VALUE,
        TR_SET_VALUE_BUFFER,
    } type;

    size_t module_index;
    size_t field_offset;

    // TR_SET_VALUE
    uint8_t value[8];
    size_t value_size;

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
        assert(
            field_info->type == TR_MODULE_FIELD_FLOAT || 
            field_info->type == TR_MODULE_FIELD_INT ||
            field_info->type == TR_MODULE_FIELD_INPUT_FLOAT || 
            field_info->type == TR_MODULE_FIELD_INPUT_INT);

        tr_token_t field_value_tok = tr_next_token(t);
        assert(field_value_tok.type == TR_TOKEN_NUMBER);
        
        tr_parser_cmd_set_value_t* value = &p->values[p->value_count++];
        value->type = TR_SET_VALUE;
        value->module_index = module_index;
        value->field_offset = field_info->offset;
        if (field_info->type == TR_MODULE_FIELD_FLOAT ||
            field_info->type == TR_MODULE_FIELD_INPUT_FLOAT)
        {
            const float v = (float)atof(t->buf + field_value_tok.pos);
            memcpy(value->value, &v, sizeof(v));
            value->value_size = sizeof(v);
        }
        else
        {
            const int v = atoi(t->buf + field_value_tok.pos);
            memcpy(value->value, &v, sizeof(v));
            value->value_size = sizeof(v);
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

#if 0
        tr_token_t color_id_tok = tr_next_token(t);
        assert(color_id_tok.type == TR_TOKEN_ID);
        assert(tr_token_strcmp(t->buf, &color_id_tok, "color") == 0);

        const tr_token_t color_r_tok = tr_next_token(t);
        const tr_token_t color_g_tok = tr_next_token(t);
        const tr_token_t color_b_tok = tr_next_token(t);
        const uint8_t r = (uint8_t)tr_token_to_int(t->buf, &color_r_tok);
        const uint8_t g = (uint8_t)tr_token_to_int(t->buf, &color_g_tok);
        const uint8_t b = (uint8_t)tr_token_to_int(t->buf, &color_b_tok);
        (void)r;
        (void)g;
        (void)b;
#endif

        p->values[p->value_count++] = (tr_parser_cmd_set_value_t){
            .type = TR_SET_VALUE_BUFFER,
            .module_index = module_index,
            .field_offset = field_info->offset,
            .target_module_index = tr_token_to_int(t->buf, &module_index_tok),
            .target_field_offset = target_field_info->offset,
            //.color = {r, g, b, 0xff},
            .color = tr_random_cable_color(),
        };
    }

    return 0;
}

void tr_parse(tr_parser_t* p, tr_tokenizer_t* t)
{
    for (;;)
    {
        if (tr_parse_line(p, t))
        {
            break;
        }
    }
}

void tr_rack_deserialize(rack_t* rack, const char* input, size_t len)
{
    rack_init(rack);
    hmfree(g_input.input_plugs);
    hmfree(g_input.output_plugs);
    
    tr_tokenizer_t tokenizer = {input, len};
    tr_parser_t parser = {0};
    tr_parse(&parser, &tokenizer);
    
    for (size_t i = 0; i < parser.module_count; ++i)
    {
        const tr_parser_cmd_add_module_t* cmd = &parser.modules[i];

        printf("ADD MODULE: %s %d %d\n", tr_module_infos[cmd->type].id, cmd->x, cmd->y);

        tr_gui_module_t* module = tr_rack_create_module(rack, cmd->type);
        module->x = cmd->x;
        module->y = cmd->y;

        const tr_module_info_t* module_info = &tr_module_infos[module->type];
        for (size_t field_index = 0; field_index < module_info->field_count; ++field_index)
        {
            if (module_info->fields[field_index].type == TR_MODULE_FIELD_BUFFER)
            {
                const tr_output_plug_t p = {cmd->x, cmd->y, module};
                float* field_addr = get_field_address(rack, module->type, module->index, field_index);
                hmput(g_input.output_plugs, field_addr, p);
            }
        }
    }

    for (size_t i = 0; i < parser.value_count; ++i)
    {
        const tr_parser_cmd_set_value_t* cmd = &parser.values[i];

        const tr_gui_module_t* module = &rack->gui_modules[cmd->module_index];
        void* module_addr = tr_get_module_address(rack, module->type, module->index);
        void* field_addr = (uint8_t*)module_addr + cmd->field_offset;

        switch (cmd->type)
        {
            case TR_SET_VALUE:
                printf("SET VALUE: %zu:%zu = %.*llx\n", cmd->module_index, cmd->field_offset, (int)cmd->value_size * 2, *(uint64_t*)cmd->value);
                memcpy(field_addr, &cmd->value, cmd->value_size);
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

#define TR_KNOB_RADIUS (18)
#define TR_KNOB_PADDING (2)
#define TR_PLUG_RADIUS (12)
#define TR_PLUG_PADDING (4)
#define TR_MODULE_PADDING (2)

#define COLOR_BACKGROUND GetColor(0x3c5377ff)
#define COLOR_MODULE_BACKGROUND GetColor(0xfff3c2ff)
#define COLOR_MODULE_TEXT GetColor(0x3c5377ff)
#define COLOR_PLUG_HOLE GetColor(0x303030ff)
#define COLOR_PLUG_BORDER GetColor(0xa5a5a5ff)
#define COLOR_PLUG_HIGHLIGHT GetColor(0xe0780fff)
#define COLOR_PLUG_OUTPUT GetColor(0xddc55eff)
#define COLOR_KNOB GetColor(0xddc55eff)
#define COLOR_KNOB_TIP GetColor(0xffffffff)

#define TR_CABLE_ALPHA 0.75f

static Font g_font;

void tr_gui_module_begin(tr_gui_module_t* module)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];

    const Vector2 mouse = GetScreenToWorld2D(GetMousePosition(), g_input.camera);

    if (g_input.drag_module == module)
    {
        module->x = (int)(g_input.drag_offset.x + mouse.x);
        module->y = (int)(g_input.drag_offset.y + mouse.y);
    }

    Rectangle bgrect = {(float)module->x, (float)module->y, (float)module_info->width, (float)module_info->height};
    DrawRectangleRounded(
        bgrect, 
        0.2f,
        16,
        COLOR_MODULE_BACKGROUND);

    const char* name = tr_module_infos[module->type].id;
    const float fontsize = 22;

    const Vector2 textsize = MeasureTextEx(g_font, name, fontsize, 0);

    DrawTextEx(
        g_font,
        tr_module_infos[module->type].id, 
        (Vector2){
            (float)(module->x + module_info->width / 2 - textsize.x / 2),
            (float)module->y + TR_MODULE_PADDING * 2},
        fontsize,
        0,
        COLOR_MODULE_TEXT);

    if (!g_input.do_not_process_input)
    {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
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

void tr_gui_knob_base(float value, float min, float max, int x, int y)
{
    DrawCircle(x, y, TR_KNOB_RADIUS, COLOR_KNOB);
    
    const float t = (value - min) / fmaxf(max - min, 0.01f);
    const float t0 = (t - 0.5f) * 0.9f;
    const float dot_x = sinf(t0 * TR_TWOPI);
    const float dot_y = cosf(t0 * TR_TWOPI);
    
    DrawCircleV(
        (Vector2){
            (x + dot_x * TR_KNOB_RADIUS*0.9f),
            (y - dot_y * TR_KNOB_RADIUS*0.9f),
        },
        4.0f,
        COLOR_KNOB_TIP);
}

void tr_gui_knob_float(rack_t* rack, const tr_gui_module_t* module, const tr_module_field_info_t* field)
{
    const int x = module->x + field->x;
    const int y = module->y + field->y;

    void* module_addr = tr_get_module_address(rack, module->type, module->index);
    float* value = (float*)((uint8_t*)module_addr + field->offset);
    const float min = field->min;
    const float max = field->max;

    if (!g_input.do_not_process_input)
    {
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

    tr_gui_knob_base(*value, min, max, x, y);
}

void tr_gui_knob_int(rack_t* rack, const tr_gui_module_t* module, const tr_module_field_info_t* field)
{
    const int x = module->x + field->x;
    const int y = module->y + field->y;

    if (g_input.do_not_process_input)
    {
        tr_gui_knob_base(0.0f, 0.0f, 1.0f, x, y);
    }
    else
    {
        void* module_addr = tr_get_module_address(rack, module->type, module->index);
        int* value = (int*)((uint8_t*)module_addr + field->offset);
        const int min = field->min_int;
        const int max = field->max_int;
        
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

        tr_gui_knob_base((float)*value, (float)min, (float)max, x, y);
    }
}

void tr_gui_plug_input(rack_t* rack, const tr_gui_module_t* module, const tr_module_field_info_t* field)
{
    const int x = module->x + field->x;
    const int y = module->y + field->y;

    const bool highlight = g_input.drag_output != NULL;

    DrawCircle(x, y, TR_PLUG_RADIUS, COLOR_PLUG_BORDER);

    Color inner_color = COLOR_PLUG_HOLE;
    if (highlight)
    {
        inner_color = COLOR_PLUG_HIGHLIGHT;
    }
    // if (*value != NULL)
    // {
    //     inner_color = hmget(g_input.input_plugs, value).color;
    // }
    DrawCircle(x, y, TR_PLUG_RADIUS - TR_PLUG_PADDING, inner_color);

    void* module_addr = tr_get_module_address(rack, module->type, module->index);
    const float** value = (const float**)((uint8_t*)module_addr + field->offset);
    const Vector2 center = {(float)x, (float)y};
    
    if (!g_input.do_not_process_input)
    {
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
                .color = ColorAlpha(g_input.drag_color, TR_CABLE_ALPHA),
            };
        }
    }

    if (*value != NULL)
    {
        const tr_output_plug_t plug = hmget(g_input.output_plugs, *value);
        const tr_input_plug_t input_plug = hmget(g_input.input_plugs, value);
        g_input.cable_draws[g_input.cable_draw_count++] = (tr_cable_draw_command_t){
            .from = center,
            .to = {(float)plug.x, (float)plug.y},
            .color = ColorAlpha(input_plug.color, TR_CABLE_ALPHA),
        };
    }
}

void tr_gui_plug_output(rack_t* rack, const tr_gui_module_t* module, const tr_module_field_info_t* field)
{
    const int x = module->x + field->x;
    const int y = module->y + field->y;

    const bool highlight = g_input.drag_input != NULL;
    
    const int rw = TR_PLUG_RADIUS + 4;
    Rectangle bgrect = {(float)x - rw, (float)y - rw, 2.0f * rw, 2.0f * rw};
    DrawRectangleRounded(bgrect, 0.25f, 8, COLOR_PLUG_OUTPUT);
    DrawCircle(x, y, TR_PLUG_RADIUS, COLOR_PLUG_BORDER);
    DrawCircle(x, y, TR_PLUG_RADIUS - TR_PLUG_PADDING, highlight ? COLOR_PLUG_HIGHLIGHT : COLOR_PLUG_HOLE);

    if (!g_input.do_not_process_input)
    {
        void* module_addr = tr_get_module_address(rack, module->type, module->index);
        const float* buffer = (float*)((uint8_t*)module_addr + field->offset);

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
                .color = ColorAlpha(g_input.drag_color, TR_CABLE_ALPHA),
            };
        }

        const tr_output_plug_t plug = {x, y, module};
        hmput(g_input.output_plugs, buffer, plug);
    }
}

void tr_led(int x, int y, Color color)
{
    DrawCircle(x, y, 4.0f, color);
}

void tr_clock_decorate(tr_clock_t* clock, tr_gui_module_t* module)
{
    const int gate_x = module->x + tr_clock__fields[TR_CLOCK_out_gate].x;
    const int gate_y = module->y + tr_clock__fields[TR_CLOCK_out_gate].y;
    tr_led(gate_x, gate_y + TR_PLUG_RADIUS + 12, clock->phase < 0.5f ? GREEN : BLACK);
}

void tr_clockdiv_decorate(tr_clockdiv_t* clockdiv, tr_gui_module_t* module)
{
    for (int i = 0; i < 8; ++i)
    {
        const int plug_x = module->x + tr_clockdiv__fields[TR_CLOCKDIV_out_0 + i].x;
        const int plug_y = module->y + tr_clockdiv__fields[TR_CLOCKDIV_out_0 + i].y;
        tr_led(plug_x, plug_y + TR_PLUG_RADIUS + 12, (clockdiv->state >> i) & 1 ? GREEN : BLACK);
    }
}

void tr_seq8_decorate(tr_seq8_t* seq8, tr_gui_module_t* module)
{
    for (int i = 0; i < 8; ++i)
    {
        const tr_module_field_info_t* field = &tr_module_infos[module->type].fields[TR_SEQ8_in_cv_0 + i];
        tr_led(module->x + field->x, module->y + field->y + TR_KNOB_RADIUS + 8, seq8->step == i ? GREEN : BLACK);
    }
}

void tr_quantizer_decorate(tr_quantizer_t* quantizer, tr_gui_module_t* module)
{
    const int x = module->x;
    const int y = module->y;
    const int kx = x + 24;
    DrawTextEx(g_font, g_tr_quantizer_mode_name[quantizer->in_mode], (Vector2){(float)kx + TR_KNOB_RADIUS + 8, (float)y + 40}, 20, 0, COLOR_MODULE_TEXT);
}

void tr_scope_decorate(tr_scope_t* scope, tr_gui_module_t* module)
{
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
            
            const float x0 = Remap(i + 0.0f, 0.0f, (float)TR_SAMPLE_COUNT, (float)screen_x, (float)screen_x + screen_w);
            const float x1 = Remap(i + 1.0f, 0.0f, (float)TR_SAMPLE_COUNT, (float)screen_x, (float)screen_x + screen_w);
            
            const float y0 = Remap(s0, -1.0f, 1.0f, (float)screen_y + screen_h, (float)screen_y);
            const float y1 = Remap(s1, -1.0f, 1.0f, (float)screen_y + screen_h, (float)screen_y);

            DrawLineEx((Vector2){x0, y0}, (Vector2){x1, y1}, 1.0f, YELLOW);
        }
    }
}

void tr_gui_module_draw(rack_t* rack, tr_gui_module_t* module)
{
    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    tr_gui_module_begin(module);

    for (int i = 0; i < module_info->field_count; ++i)
    {
        const tr_module_field_info_t* field_info = &module_info->fields[i];
        switch (field_info->type)
        {
            case TR_MODULE_FIELD_FLOAT: break;
            case TR_MODULE_FIELD_INT: break;
            case TR_MODULE_FIELD_INPUT_FLOAT: tr_gui_knob_float(rack, module, field_info); break;
            case TR_MODULE_FIELD_INPUT_INT: tr_gui_knob_int(rack, module, field_info); break;
            case TR_MODULE_FIELD_INPUT_BUFFER: tr_gui_plug_input(rack, module, field_info); break;
            case TR_MODULE_FIELD_BUFFER: tr_gui_plug_output(rack, module, field_info); break;
        }
    }

    void* module_addr = tr_get_module_address(rack, module->type, module->index);
    switch (module->type)
    {
        case TR_CLOCK: tr_clock_decorate(module_addr, module); break;
        case TR_CLOCKDIV: tr_clockdiv_decorate(module_addr, module); break;
        case TR_SEQ8: tr_seq8_decorate(module_addr, module); break;
        case TR_QUANTIZER: tr_quantizer_decorate(module_addr, module); break;
        case TR_SCOPE: tr_scope_decorate(module_addr, module); break;
        default: break;
    }

    tr_gui_module_end();
}

void tr_update_modules(rack_t* rack, tr_gui_module_t** modules, size_t count)
{
#if TR_TRACE_MODULE_UPDATES
    printf("tr_update_modules:\n");
#endif

    for (size_t i = 0; i < count; ++i)
    {
        tr_gui_module_t* module = modules[i];
        void* module_addr = tr_get_module_address(rack, module->type, module->index);

        switch (module->type)
        {
            case TR_VCO: tr_vco_update(module_addr); break;
            case TR_CLOCK: tr_clock_update(module_addr); break;
            case TR_CLOCKDIV: tr_clockdiv_update(module_addr); break;
            case TR_SEQ8: tr_seq8_update(module_addr); break;
            case TR_ADSR: tr_adsr_update(module_addr); break;
            case TR_VCA: tr_vca_update(module_addr); break;
            case TR_LP: tr_lp_update(module_addr); break;
            case TR_MIXER: tr_mixer_update(module_addr); break;
            case TR_NOISE: tr_noise_update(module_addr); break;
            case TR_QUANTIZER: tr_quantizer_update(module_addr); break;
            case TR_RANDOM: tr_random_update(module_addr); break;
            default: break;
        }

#if TR_TRACE_MODULE_UPDATES
        printf("\t%s %zu\n", tr_module_infos[module->type].id, module->index);
#endif
    }
}

void tr_enumerate_inputs(const float* inputs[], int* count, rack_t* rack, const tr_gui_module_t* module)
{
    *count = 0;

    const tr_module_info_t* module_info = &tr_module_infos[module->type];
    const void* module_addr = tr_get_module_address(rack, module->type, module->index);

    for (size_t i = 0; i < module_info->field_count; ++i)
    {
        const tr_module_field_info_t* field = &module_info->fields[i];
        if (field->type == TR_MODULE_FIELD_INPUT_BUFFER)
        {
            inputs[(*count)++] = *(float**)((uint8_t*)module_addr + field->offset);
        }
    }
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
        int input_count;
        tr_enumerate_inputs(inputs, &input_count, rack, &rack->gui_modules[i]);

        for (int input_index = 0; input_index < input_count; ++input_index)
        {
            if (inputs[input_index] == NULL)
            {
                continue;
            }

            const tr_output_plug_t plug = hmget(g_input.output_plugs, inputs[input_index]);
            const size_t module_index = tr_get_gui_module_index(rack, plug.module);
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

    return update_count;
}

static void float_to_int16_pcm(int16_t* samples, const float* buffer)
{
    assert(buffer != NULL);
    for (size_t i = 0; i < TR_SAMPLE_COUNT; ++i)
    {
        samples[i] = (int16_t)(Clamp(buffer[i], -1.0f, 1.0f) * INT16_MAX);
    }
}

static void tr_produce_final_mix_internal(int16_t* output, rack_t* rack)
{
    tr_gui_module_t* update_modules[TR_GUI_MODULE_COUNT];
    const size_t update_count = tr_resolve_module_graph(update_modules, rack);
    tr_update_modules(rack, update_modules, update_count);

    const tr_gui_module_t* speaker = NULL;
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        const tr_gui_module_t* module = &rack->gui_modules[i];
        if (module->type != TR_SPEAKER)
        {
            continue;
        }
        
        const tr_speaker_t* m = tr_module_pool_get(&rack->module_pool[TR_SPEAKER], module->index);
        if (m->in_audio == NULL)
        {
            continue;
        }

        speaker = module;
        break;
    }

    if (speaker == NULL)
    {
        memset(output, 0, sizeof(int16_t) * TR_SAMPLE_COUNT);
        return;
    }

    assert(speaker->type == TR_SPEAKER);
    const tr_speaker_t* m = tr_module_pool_get(&rack->module_pool[TR_SPEAKER], speaker->index);
    float_to_int16_pcm(output, m->in_audio);
}

void tr_produce_final_mix(int16_t* output, rack_t* rack)
{
    timer_t timer;
    timer_start(&timer);

    tr_produce_final_mix_internal(output, rack);
    
    const double ms = timer_reset(&timer);
    //printf("final_mix: %.3f ms\n", ms);

    g_app.timer_produce_final_mix[g_app.timer_index % tr_countof(g_app.timer_produce_final_mix)] = ms;
    ++g_app.timer_index;
}

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
        
        memcpy(samples + write_cursor, g_app.final_mix + final_mix_cursor, final_mix_available * sizeof(int16_t));

        write_cursor += final_mix_available;
        g_app.final_mix_remaining -= final_mix_available;
    }

    g_app.has_audio_callback_been_called_once = true;
}
#endif

Rectangle ComputePatchBounds(rack_t* rack)
{
    if (rack->gui_module_count == 0)
    {
        return (Rectangle){0.0f};
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

    return (Rectangle){
        .x = min_x,
        .y = min_y,
        .width = max_x - min_x,
        .height = max_y - min_y,
    };
}

void DrawHangingCableQuadratic(Vector2 a, Vector2 b, float slack, float thick, Color color)
{
    float L = Vector2Distance(a, b);
    float sag = L * slack * 0.25f;
    
    Vector2 down = {0.0f, 1.0f};
    Vector2 sagVec = Vector2Scale(Vector2Normalize(down), sag);
    Vector2 mid = Vector2Lerp(a, b, 0.5f);
    Vector2 c = Vector2Add(mid, sagVec);
    DrawSplineSegmentBezierQuadratic(a, c, b, thick, color);
}

void tr_frame_update_draw(void)
{
    app_t* app = &g_app;
    rack_t* rack = &app->rack;

#if 1
    g_input.camera.offset.x = GetScreenWidth() * 0.5f;
    g_input.camera.offset.y = GetScreenHeight() * 0.5f;
#else
    g_input.camera.offset = GetMousePosition();
#endif

    if (!app->picker_mode)
    {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            g_input.camera.target.x -= GetMouseDelta().x / g_input.camera.zoom;
            g_input.camera.target.y -= GetMouseDelta().y / g_input.camera.zoom;
        }
        
        g_input.camera.zoom += GetMouseWheelMoveV().y * 0.2f;
        g_input.camera.zoom = Clamp(g_input.camera.zoom, 0.2f, 4.0f);
    }

    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyPressed(KEY_S))
    {
        char* buffer = malloc(1024 * 1024);
        size_t len = tr_rack_serialize(buffer, rack);
        printf("%.*s", (int)len, buffer);
        free(buffer);
    }

    if (IsKeyPressed(KEY_TAB))
    {
        app->picker_mode = !app->picker_mode;
    }

    if (IsKeyPressed(KEY_SPACE))
    {
        app->paused = !app->paused;
    }

    if (app->paused && IsKeyPressed(KEY_S))
    {
        app->single_step = true;
    }

    BeginDrawing();
    ClearBackground(COLOR_BACKGROUND);

    BeginMode2D(g_input.camera);

#if 0
    {
        const Vector2 m = GetMousePosition();
        const Vector2 mw = GetScreenToWorld2D(m, g_input.camera);

        DrawCircle((int)m.x, (int)m.y, 8.0f, RED);
        DrawCircle((int)mw.x, (int)mw.y, 4.0f, GREEN);

        printf("%d %d\n", (int)m.x, (int)m.y);
    }
#endif

    g_input.cable_draw_count = 0;
    
    g_input.do_not_process_input = app->picker_mode;
    for (size_t i = 0; i < rack->gui_module_count; ++i)
    {
        tr_gui_module_draw(rack, &rack->gui_modules[i]);
    }
    g_input.do_not_process_input = false;

    for (size_t i = 0; i < g_input.cable_draw_count; ++i)
    {
        const tr_cable_draw_command_t* draw = &g_input.cable_draws[i];
        //DrawLineEx(draw->from, draw->to, 6.0f, draw->color);
        //DrawLineBezier(draw->from, draw->to, 6.0f, draw->color);
        DrawHangingCableQuadratic(draw->from, draw->to, 1.0f, 6.0f, draw->color);
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

    if (app->picker_mode)
    {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.7f));

        g_input.do_not_process_input = true;

        int x = 10;
        int y = 10;

        for (size_t i = 0; i < TR_MODULE_COUNT; ++i)
        {
            const tr_module_info_t* module_info = &tr_module_infos[i];
            if (x + module_info->width >= GetScreenWidth())
            {
                x = 10;
                y += 180;
            }

            tr_gui_module_t m = {x, y, i, .index = TR_MODULE_POOL_SIZE - 1};
            tr_gui_module_draw(rack, &m);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && 
                g_input.drag_module == NULL)
            {
                const Vector2 mouse = GetMousePosition();
                if (mouse.x > x && 
                    mouse.y > y &&
                    mouse.x < x + module_info->width && 
                    mouse.y < y + module_info->height)
                {
                    tr_gui_module_t* module = tr_rack_create_module(rack, i);
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

#ifdef TR_AUDIO_CALLBACK
    if (!g_app.has_audio_callback_been_called_once)
    {
        const char* message = "Click anywhere to enable audio playback.";
        const Vector2 message_size = MeasureTextEx(g_font, message, 22, 0);

        const Vector2 pos = {
            GetScreenWidth() * 0.5f - message_size.x * 0.5f,
            50,
        };
        const Rectangle bgrect = {
            pos.x - 12, pos.y - 12,
            message_size.x + 24, message_size.y + 24,
        };

        DrawRectangleRounded(bgrect, 0.25f, 8, ColorAlpha(BLACK, 0.5f));
        DrawTextEx(g_font, message, pos, 22, 0, WHITE);
    }
#endif

    {
        double avg = 0.0;
        for (size_t i = 0; i < tr_countof(g_app.timer_produce_final_mix); ++i)
        {
            avg += g_app.timer_produce_final_mix[i];
        }
        avg *= (1.0 / tr_countof(g_app.timer_produce_final_mix));

        char message[64];
        sprintf(message, "final mix: %1.3f ms", avg);
        const Vector2 message_size = MeasureTextEx(g_font, message, 22, 0);
        
        const Vector2 pos = {0.0f, GetScreenHeight() - message_size.y};
        DrawTextEx(g_font, message, pos, 22, 0, WHITE);
    }

    {
#if 0
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
            g_input.drag_offset.x = 0.0f;
            g_input.drag_offset.y = 0.0f;
        }
#endif
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
    if (IsAudioStreamProcessed(app->stream) && (!app->paused || app->single_step))
    {
        app->single_step = false;

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
    }
#endif
}

static const char* TEST = "\
module speaker 0 pos 1357 -44 \
input_buffer in_audio > mixer 2 out_mix \
module vco 1 pos 661 1 \
input phase 6.057979 \
input in_v0 118.000000 \
input_buffer in_voct > quantizer 8 out_cv \
module mixer 2 pos 1082 -49 \
input_buffer in_0 > lp 13 out_audio \
input_buffer in_1 > lp 11 out_audio \
input_buffer in_2 > lp 20 out_audio \
input in_vol0 0.740000 \
input in_vol1 0.810000 \
input in_vol2 0.140000 \
input in_vol3 0.000000 \
input in_vol_final 1.000000 \
module vco 3 pos 660 -173 \
input phase 0.629575 \
input in_v0 235.600113 \
input_buffer in_voct > quantizer 7 out_cv \
module clock 4 pos -143 -20 \
input phase 0.202133 \
input in_hz 6.508700 \
module seq8 5 pos 10 -115 \
input step 5 \
input trig 1 \
input_buffer in_step > clock 4 out_gate \
input in_cv_0 0.280000 \
input in_cv_1 0.180000 \
input in_cv_2 0.000000 \
input in_cv_3 0.000000 \
input in_cv_4 0.360000 \
input in_cv_5 0.000000 \
input in_cv_6 0.610000 \
input in_cv_7 0.000000 \
module seq8 6 pos 9 -8 \
input step 0 \
input trig 1 \
input_buffer in_step > clockdiv 17 out_1 \
input in_cv_0 0.070000 \
input in_cv_1 0.280000 \
input in_cv_2 0.470000 \
input in_cv_3 0.620000 \
input in_cv_4 0.000000 \
input in_cv_5 0.270000 \
input in_cv_6 0.470000 \
input in_cv_7 0.690000 \
module quantizer 7 pos 437 -138 \
input in_mode 3 \
input_buffer in_cv > seq8 5 out_cv \
module quantizer 8 pos 439 -15 \
input in_mode 3 \
input_buffer in_cv > seq8 6 out_cv \
module adsr 9 pos 796 252 \
input value 0.988310 \
input gate 1 \
input state 1 \
input in_attack 0.310690 \
input in_decay 0.210790 \
input in_sustain 0.410000 \
input in_release 0.870129 \
input_buffer in_gate > clock 4 out_gate \
module adsr 10 pos 1658 337 \
input value 0.000000 \
input gate 0 \
input state 0 \
input in_attack 0.000000 \
input in_decay 0.000000 \
input in_sustain 0.000000 \
input in_release 0.000000 \
module lp 11 pos 779 9 \
input value 0.000000 \
input z 0.538930 \
input_buffer in_audio > vco 1 out_saw \
input_buffer in_cut > adsr 9 out_env \
input in_cut0 0.000000 \
input in_cut_mul 0.460000 \
module vca 12 pos 1655 452 \
module lp 13 pos 779 -111 \
input value 0.000000 \
input z 0.094695 \
input_buffer in_audio > vco 3 out_saw \
input_buffer in_cut > adsr 9 out_env \
input in_cut0 0.000000 \
input in_cut_mul 0.480000 \
module scope 14 pos 1098 87 \
input_buffer in_0 > lp 11 out_audio \
module scope 15 pos 1475 -97 \
input_buffer in_0 > mixer 2 out_mix \
module scope 16 pos 1099 -288 \
input_buffer in_0 > lp 13 out_audio \
module clockdiv 17 pos 10 -223 \
input gate 0.000000 \
input state 70 \
input_buffer in_gate > clock 4 out_gate \
module adsr 18 pos 789 373 \
input value 0.567364 \
input gate 1 \
input state 0 \
input in_attack 0.770230 \
input in_decay 0.001000 \
input in_sustain 1.000000 \
input in_release 0.520480 \
input_buffer in_gate > clockdiv 17 out_2 \
module vco 19 pos 659 184 \
input phase 5.398239 \
input in_v0 118.000015 \
input_buffer in_voct > quantizer 22 out_cv \
module lp 20 pos 777 126 \
input value 0.000000 \
input z 0.603262 \
input_buffer in_audio > vco 19 out_saw \
input_buffer in_cut > adsr 18 out_env \
input in_cut0 0.140000 \
input in_cut_mul 0.780000 \
module seq8 21 pos 9 104 \
input step 0 \
input trig 1 \
input_buffer in_step > clockdiv 17 out_2 \
input in_cv_0 0.000000 \
input in_cv_1 0.480000 \
input in_cv_2 0.000000 \
input in_cv_3 1.000000 \
input in_cv_4 0.000000 \
input in_cv_5 0.480000 \
input in_cv_6 0.680000 \
input in_cv_7 1.000000 \
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
        tr_gui_module_t* speaker0 = tr_rack_create_module(app->rack, TR_SPEAKER);
        speaker0->x = 1280 / 2;
        speaker0->y = 720 / 2;
    }
#else
    tr_rack_deserialize(rack, TEST, strlen(TEST));
    const Rectangle bounds = ComputePatchBounds(rack);
    g_input.camera.target.x = bounds.x + bounds.width * 0.5f;
    g_input.camera.target.y = bounds.y + bounds.height * 0.5f;
#endif

    int config_flags = FLAG_MSAA_4X_HINT;
    config_flags |= FLAG_WINDOW_RESIZABLE;
#ifdef __EMSCRIPTEN__
    config_flags |= FLAG_WINDOW_MAXIMIZED;
    //config_flags |= FLAG_WINDOW_HIGHDPI;
#endif
    SetConfigFlags(config_flags);
    InitWindow(1280, 720, "rack.fm");

    g_font = LoadFontFromMemory(".ttf", berkeley_mono, (int)sizeof(berkeley_mono), 22, NULL, 0);
    SetTextureFilter(g_font.texture, TEXTURE_FILTER_BILINEAR);

    InitAudioDevice();
    assert(IsAudioDeviceReady());
    SetAudioStreamBufferSizeDefault(TR_SAMPLE_COUNT);

    app->stream = LoadAudioStream(TR_SAMPLE_RATE, 16, 1);
#ifdef TR_AUDIO_CALLBACK
    SetAudioStreamCallback(app->stream, tr_audio_callback);
#endif
    PlayAudioStream(app->stream);

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
