#include "strbuf.h"

void sb_append_cstring(tr_strbuf_t* sb, const char* str)
{
    const char* c = str;
    while (*c != '\0')
    {
        sb->buf[sb->pos++] = *c++;
    }
}

void sb_append_string(tr_strbuf_t* sb, const char* str, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        sb->buf[sb->pos++] = str[i];
    }
}

static void sb_append_hex_pair(tr_strbuf_t* sb, uint8_t byte)
{
    static const char hex[] = "0123456789abcdef";
    sb->buf[sb->pos++] = hex[(byte >> 4)];
    sb->buf[sb->pos++] = hex[byte & 0xf];
}

void sb_append_hex_float(tr_strbuf_t* sb, float x)
{
    sb->buf[sb->pos++] = '0';
    sb->buf[sb->pos++] = 'x';

    const uint8_t* bytes = (uint8_t*)&x;
    for (size_t i = 0; i < 4; ++i)
    {
        sb_append_hex_pair(sb, bytes[3 - i]);
    }
}

void sb_append_int(tr_strbuf_t* sb, int x)
{
    if (x < 0)
    {
        sb_append_cstring(sb, "-");
        x = -x;
    }

    if (x == 0)
    {
        sb_append_cstring(sb, "0");
    }
    else
    {
        char buffer[64];
        size_t count = 0;

        while (x > 0)
        {
            buffer[count++] = (x % 10) + '0';
            x /= 10;
        }

        for (size_t i = 0; i < count / 2; ++i)
        {
            char t = buffer[i];
            buffer[i] = buffer[count - i - 1];
            buffer[count - i - 1] = t;
        }

        sb_append_string(sb, buffer, count);
    }
}

static int is_nan_or_inf(float x)
{
    unsigned int mask = (1u << 8) - 1u; // 0xFF
    unsigned int bits = *(unsigned int *)&x;
    return ((bits >> 23) & mask) == mask;
}

void sb_append_float(tr_strbuf_t* sb, float x)
{
    int precision = 6;

    if (is_nan_or_inf(x))
    {
        sb_append_cstring(sb, "0.0");
    }
    else
    {
        sb_append_int(sb, (int)x);

        x -= (float)(int)x;
        while (precision-- > 0) {
            x *= 10.0;
        }

        sb_append_cstring(sb, ".");

        long long int y = (long long int) x;
        if (y < 0)
        {
            y = -y;
        }

        sb_append_int(sb, y);
    }
}

void sb_terminate(tr_strbuf_t* sb)
{
    sb->buf[sb->pos] = '\0';
}