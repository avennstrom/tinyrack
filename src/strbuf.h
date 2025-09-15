#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct tr_strbuf
{
    char* buf;
    size_t pos;
} tr_strbuf_t;

void sb_append_cstring(tr_strbuf_t* sb, const char* str);
void sb_append_string(tr_strbuf_t* sb, const char* str, size_t len);
void sb_append_hex_float(tr_strbuf_t* sb, float x);
void sb_append_int(tr_strbuf_t* sb, int x);
void sb_append_float(tr_strbuf_t* sb, float x);
void sb_terminate(tr_strbuf_t* sb);