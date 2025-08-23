#include "parser.h"
#include "modules.reflection.h"

#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

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
            //.color = tr_random_cable_color(),
        };
    }

    return 0;
}

int tr_parse_memory(tr_parser_t* p, const char* input, size_t len)
{
    p->module_count = 0;
    p->value_count = 0;

    tr_tokenizer_t tokenizer = {input, len};
    for (;;)
    {
        if (tr_parse_line(p, &tokenizer))
        {
            break;
        }
    }

    return 0;
}