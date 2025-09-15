#pragma once

#include "types.h"

#include <stddef.h>

typedef struct camera {
    float2 offset;         // Camera offset (displacement from target)
    float2 target;         // Camera target (rotation and zoom origin)
    float rotation;        // Camera rotation in degrees
    float zoom;            // Camera zoom (scaling), should be 1.0f by default
} camera_t;

typedef enum font {
    FONT_BERKELY_MONO,
} font_t;

typedef enum cmd_type
{
    CMD_CAMERA_BEGIN,
    CMD_CAMERA_END,
    CMD_DRAW_RECTANGLE,
    CMD_DRAW_RECTANGLE_ROUNDED,
    CMD_DRAW_CIRCLE,
    CMD_DRAW_TEXT,
    CMD_DRAW_SPLINE,
    CMD_DRAW_LINE_STRIP,
    CMD_EOF = 0xff,
} cmd_type_t;

typedef struct cmd_header
{
    uint8_t type;
} cmd_header_t;

typedef struct cmd_camera_begin
{
    camera_t camera;
} cmd_camera_begin_t;

typedef struct cmd_draw_rectangle
{
    float2 position;
    float2 size;
    float2 origin;
    float rotation;
    color_t color;
} cmd_draw_rectangle_t;

typedef struct cmd_draw_rectangle_rounded
{
    float2 position;
    float2 size;
    color_t color;
    float roundness;
} cmd_draw_rectangle_rounded_t;

typedef struct cmd_draw_circle
{
    float2 position;
    float radius;
    color_t color;
} cmd_draw_circle_t;

typedef struct cmd_draw_text
{
    float2 position;
    color_t color;
    float font_size;
    uint32_t text_len;
    char text[];
} cmd_draw_text_t;

// DrawSplineSegmentBezierQuadratic in Raylib
typedef struct cmd_draw_spline
{
    float2 p1;
    float2 c2;
    float2 p3;
    float thickness;
    color_t color;
} cmd_draw_spline_t;

typedef struct cmd_draw_line_strip
{
    color_t color;
    uint32_t vertex_count;
    float2 vertices[];
} cmd_draw_line_strip_t;

typedef struct render_buffer
{
    uint8_t* data;
    size_t head;
    color_t clear_color;
} render_buffer_t;

void rb_begin(render_buffer_t* rb, color_t color);
void rb_end(render_buffer_t* rb);
cmd_camera_begin_t* rb_camera_begin(render_buffer_t* rb);
void rb_camera_end(render_buffer_t* rb);
cmd_draw_circle_t* rb_draw_circle(render_buffer_t* rb);
cmd_draw_rectangle_t* rb_draw_rectangle(render_buffer_t* rb);
cmd_draw_rectangle_rounded_t* rb_draw_rectangle_rounded(render_buffer_t* rb);
void rb_draw_text(render_buffer_t* rb, const char* text, float2 position, float font_size, color_t color);
cmd_draw_spline_t* rb_draw_spline(render_buffer_t* rb);
float2* rb_draw_line_strip(render_buffer_t* rb, size_t vertex_count, color_t color);