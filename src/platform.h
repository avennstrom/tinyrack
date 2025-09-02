#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct float2 {
    float x;
    float y;
} float2;

float float_lerp(float a, float b, float t);
// Remap input value within input range to output range
float float_remap(float value, float inputStart, float inputEnd, float outputStart, float outputEnd);
float float_clamp(float value, float min, float max);

float float2_distance(float2 v1, float2 v2);
float2 float2_scale(float2 v, float s);
float2 float2_lerp(float2 v1, float2 v2, float amount);
float2 float2_normalize(float2 v);

typedef struct rectangle {
    float x;
    float y;
    float width;
    float height;
} rectangle_t;

typedef struct color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} color_t;

#define COLOR_FROM_RGBA_HEX(hex) ((color_t){((hex) >> 24) & 0xff, ((hex) >> 16) & 0xff, ((hex) >> 8) & 0xff, ((hex) >> 0) & 0xff})
#define COLOR_ALPHA(color, alpha) ((color_t){color.r, color.g, color.b, (uint8_t)(alpha * 255.0f)})

typedef struct camera {
    float2 offset;         // Camera offset (displacement from target)
    float2 target;         // Camera target (rotation and zoom origin)
    float rotation;         // Camera rotation in degrees
    float zoom;             // Camera zoom (scaling), should be 1.0f by default
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

typedef enum mouse_button
{
    PL_MOUSE_BUTTON_LEFT,
    PL_MOUSE_BUTTON_RIGHT,
} mouse_button_t;

typedef enum keyboard_key
{
    PL_KEY_LEFT_CONTROL,
    PL_KEY_TAB,
    PL_KEY_SPACE,
    PL_KEY_S,
} keyboard_key_t;

typedef void(*platform_audio_callback)(int16_t*, size_t);

void platform_init(size_t sample_rate, size_t sample_count, platform_audio_callback audio_callback);
#ifndef PLATFORM_WEB
bool platform_running(void);
#endif
void platform_render(const render_buffer_t* rb);

float2 get_screen_size(void);
float2 get_screen_to_world(float2 position, camera_t camera);
float2 get_mouse_position(void);
float2 measure_text(font_t font, const char *text, float fontSize, float spacing);

// input
bool is_key_pressed(keyboard_key_t key);
bool is_key_down(keyboard_key_t key);
bool is_mouse_button_pressed(mouse_button_t button);
bool is_mouse_button_released(mouse_button_t button);
bool is_mouse_button_down(mouse_button_t button);
float2 get_mouse_delta(void);
float2 get_mouse_wheel_move(void);