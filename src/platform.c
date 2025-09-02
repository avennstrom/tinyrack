#include "platform.h"

#include <math.h>
#include <string.h>

float float_lerp(float a, float b, float t)
{
    return a + t*(b - a);
}

float float_remap(float value, float inputStart, float inputEnd, float outputStart, float outputEnd)
{
    return (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;
}

float float_clamp(float value, float min, float max)
{
    float result = (value < min)? min : value;

    if (result > max) result = max;

    return result;
}

float float2_distance(float2 v1, float2 v2)
{
    return sqrtf((v1.x - v2.x)*(v1.x - v2.x) + (v1.y - v2.y)*(v1.y - v2.y));
}

float2 float2_scale(float2 v, float s)
{
    return (float2){v.x * s, v.y * s};
}

float2 float2_lerp(float2 v1, float2 v2, float amount)
{
    float2 result = { 0 };

    result.x = v1.x + amount*(v2.x - v1.x);
    result.y = v1.y + amount*(v2.y - v1.y);

    return result;
}

float2 float2_normalize(float2 v)
{
    float2 result = { 0 };
    float length = sqrtf((v.x*v.x) + (v.y*v.y));

    if (length > 0)
    {
        float ilength = 1.0f/length;
        result.x = v.x*ilength;
        result.y = v.y*ilength;
    }

    return result;
}

static void* rb_alloc(render_buffer_t* rb, size_t size)
{
    void* mem = rb->data + rb->head;
    rb->head += size;
    return mem;
}

static void* rb_alloc_cmd(render_buffer_t* rb, cmd_type_t type, size_t size)
{
    void* mem = rb_alloc(rb, sizeof(cmd_header_t) + size);
    cmd_header_t* header = mem;
    header->type = (uint8_t)type;

    return (uint8_t*)mem + sizeof(cmd_header_t);
}

void rb_begin(render_buffer_t* rb, color_t color)
{
    rb->head = 0;
    rb->clear_color = color;
}

void rb_end(render_buffer_t* rb)
{
    rb_alloc_cmd(rb, CMD_EOF, 0);
}

cmd_camera_begin_t* rb_camera_begin(render_buffer_t* rb)
{
    return rb_alloc_cmd(rb, CMD_CAMERA_BEGIN, sizeof(cmd_camera_begin_t));
}

void rb_camera_end(render_buffer_t* rb)
{
    rb_alloc_cmd(rb, CMD_CAMERA_END, 0);
}

cmd_draw_circle_t* rb_draw_circle(render_buffer_t* rb)
{
    return rb_alloc_cmd(rb, CMD_DRAW_CIRCLE, sizeof(cmd_draw_circle_t));
}

cmd_draw_rectangle_t* rb_draw_rectangle(render_buffer_t* rb)
{
    return rb_alloc_cmd(rb, CMD_DRAW_RECTANGLE, sizeof(cmd_draw_rectangle_t));
}

cmd_draw_rectangle_rounded_t* rb_draw_rectangle_rounded(render_buffer_t* rb)
{
    return rb_alloc_cmd(rb, CMD_DRAW_RECTANGLE_ROUNDED, sizeof(cmd_draw_rectangle_rounded_t));
}

void rb_draw_text(render_buffer_t* rb, const char* text, float2 position, float font_size, color_t color)
{
    const size_t text_len = strlen(text);

    cmd_draw_text_t* cmd = rb_alloc_cmd(rb, CMD_DRAW_TEXT, sizeof(cmd_draw_text_t));
    cmd->position = position;
    cmd->color = color;
    cmd->font_size = font_size;
    cmd->text_len = (uint32_t)text_len + 1;
    
    char* cmd_text = rb_alloc(rb, text_len + 1);
    strcpy(cmd_text, text);
}

cmd_draw_spline_t* rb_draw_spline(render_buffer_t* rb)
{
    return rb_alloc_cmd(rb, CMD_DRAW_SPLINE, sizeof(cmd_draw_spline_t));
}

float2* rb_draw_line_strip(render_buffer_t* rb, size_t vertex_count, color_t color)
{
    cmd_draw_line_strip_t* cmd = rb_alloc_cmd(rb, CMD_DRAW_LINE_STRIP, sizeof(cmd_draw_line_strip_t));
    cmd->color = color;
    cmd->vertex_count = (uint32_t)vertex_count;
    
    rb_alloc(rb, vertex_count * sizeof(float2));
    return cmd->vertices;
}