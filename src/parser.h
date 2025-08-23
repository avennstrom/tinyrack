#pragma once

#include <stddef.h>
#include <stdint.h>

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

int tr_parse_memory(tr_parser_t* parser, const char* input, size_t len);