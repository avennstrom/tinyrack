#include "platform.h"
#include "config.h"

#include <emscripten.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define WEBGL_LINE_STRIP 0x0003
#define WEBGL_TRIANGLES 0x0004
#define WEBGL_TRIANGLE_STRIP 0x0005

typedef struct vertex
{
    float2 pos;
    uint32_t color;
} vertex_t;

typedef struct draw
{
    float projection[16];
    int topology;
    uint32_t vertex_offset;
    uint32_t vertex_count;
} draw_t;

_Static_assert(sizeof(vertex_t) == 12, "");

static vertex_t* g_vertices = NULL;
static draw_t* g_draws = NULL;

extern void js_init(void);
extern void js_render(const draw_t* draws, uint32_t draw_count, const vertex_t* vertex_data, uint32_t vertex_count);

void platform_init(size_t sample_rate, size_t sample_count, platform_audio_callback audio_callback)
{
    g_vertices = calloc(1 * 1024 * 1024, sizeof(vertex_t));
    g_draws = calloc(64 * 1024, sizeof(draw_t));
    js_init();
}

struct 
{
    int mouse_x;
    int mouse_y;
    int mouse_prev_x;
    int mouse_prev_y;
    int mouse_delta_x;
    int mouse_delta_y;
    int mouse_buttons[2];
    int mouse_buttons_prev[2];
} input;

mouse_button_t js_mouse_button(int js_button)
{
    switch (js_button)
    {
        case 0: return PL_MOUSE_BUTTON_LEFT;
        case 2: return PL_MOUSE_BUTTON_RIGHT;
        default: assert(0);
    }
}

static int canvas_w = 240;
static int canvas_h = 240;

EMSCRIPTEN_KEEPALIVE
void js_canvas_size(int w, int h)
{
    canvas_w = w;
    canvas_h = h;
    printf("CANVAS RESIZE %d, %d\n", w, h);
}

EMSCRIPTEN_KEEPALIVE
void js_mousedown(int button)
{
    input.mouse_buttons[js_mouse_button(button)] = 1;
}

EMSCRIPTEN_KEEPALIVE
void js_mouseup(int button)
{
    input.mouse_buttons[js_mouse_button(button)] = 0;
}

EMSCRIPTEN_KEEPALIVE
void js_mousemove(int x, int y)
{
    input.mouse_x = x;
    input.mouse_y = y;
}

typedef struct draw_context
{
    vertex_t* vertices;
    uint32_t vertex_count;
    draw_t* draws;
    uint32_t draw_count;

    int batch_topology;
    uint32_t batch_vertex_offset;
    float batch_projection[16];
} draw_context_t;

typedef struct Matrix {
    float m0, m4, m8, m12;  // Matrix first row (4 components)
    float m1, m5, m9, m13;  // Matrix second row (4 components)
    float m2, m6, m10, m14; // Matrix third row (4 components)
    float m3, m7, m11, m15; // Matrix fourth row (4 components)
} Matrix;

_Static_assert(sizeof(Matrix) == 64, "");

// Get two matrix multiplication
// NOTE: When multiplying matrices... the order matters!
Matrix MatrixMultiply(Matrix left, Matrix right)
{
    Matrix result = { 0 };

    result.m0 = left.m0*right.m0 + left.m1*right.m4 + left.m2*right.m8 + left.m3*right.m12;
    result.m1 = left.m0*right.m1 + left.m1*right.m5 + left.m2*right.m9 + left.m3*right.m13;
    result.m2 = left.m0*right.m2 + left.m1*right.m6 + left.m2*right.m10 + left.m3*right.m14;
    result.m3 = left.m0*right.m3 + left.m1*right.m7 + left.m2*right.m11 + left.m3*right.m15;
    result.m4 = left.m4*right.m0 + left.m5*right.m4 + left.m6*right.m8 + left.m7*right.m12;
    result.m5 = left.m4*right.m1 + left.m5*right.m5 + left.m6*right.m9 + left.m7*right.m13;
    result.m6 = left.m4*right.m2 + left.m5*right.m6 + left.m6*right.m10 + left.m7*right.m14;
    result.m7 = left.m4*right.m3 + left.m5*right.m7 + left.m6*right.m11 + left.m7*right.m15;
    result.m8 = left.m8*right.m0 + left.m9*right.m4 + left.m10*right.m8 + left.m11*right.m12;
    result.m9 = left.m8*right.m1 + left.m9*right.m5 + left.m10*right.m9 + left.m11*right.m13;
    result.m10 = left.m8*right.m2 + left.m9*right.m6 + left.m10*right.m10 + left.m11*right.m14;
    result.m11 = left.m8*right.m3 + left.m9*right.m7 + left.m10*right.m11 + left.m11*right.m15;
    result.m12 = left.m12*right.m0 + left.m13*right.m4 + left.m14*right.m8 + left.m15*right.m12;
    result.m13 = left.m12*right.m1 + left.m13*right.m5 + left.m14*right.m9 + left.m15*right.m13;
    result.m14 = left.m12*right.m2 + left.m13*right.m6 + left.m14*right.m10 + left.m15*right.m14;
    result.m15 = left.m12*right.m3 + left.m13*right.m7 + left.m14*right.m11 + left.m15*right.m15;

    return result;
}

// Get translation matrix
Matrix MatrixTranslate(float x, float y, float z)
{
    Matrix result = { 1.0f, 0.0f, 0.0f, x,
                      0.0f, 1.0f, 0.0f, y,
                      0.0f, 0.0f, 1.0f, z,
                      0.0f, 0.0f, 0.0f, 1.0f };

    return result;
}

Matrix MatrixScale(float x, float y, float z)
{
    Matrix result = { x, 0.0f, 0.0f, 0.0f,
                      0.0f, y, 0.0f, 0.0f,
                      0.0f, 0.0f, z, 0.0f,
                      0.0f, 0.0f, 0.0f, 1.0f };

    return result;
}

// void MakePixelOrthoMatrix(float out[16], float width, float height)
// {
//     out[0]  =  2.0f / width;  out[4]  = 0.0f;          out[8]  = 0.0f;  out[12] = -1.0f;
//     out[1]  =  0.0f;          out[5]  = -2.0f / height;out[9]  = 0.0f;  out[13] =  1.0f;
//     out[2]  =  0.0f;          out[6]  = 0.0f;          out[10] = -1.0f; out[14] =  0.0f;
//     out[3]  =  0.0f;          out[7]  = 0.0f;          out[11] = 0.0f;  out[15] =  1.0f;
// }

void identity_matrix(float m[16])
{
    m[0]  = 1.0f; m[1]  = 0.0f; m[2]  = 0.0f; m[3]  = 0.0f;
    m[4]  = 0.0f; m[5]  = 1.0f; m[6]  = 0.0f; m[7]  = 0.0f;
    m[8]  = 0.0f; m[9]  = 0.0f; m[10] = 1.0f; m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}

void GetCameraMatrix2D(float out[16], float2 target, float zoom, float2 offset)
{
    // Having camera transform in world-space, inverse of it gives the modelview transform
    // Since (A*B*C)' = C'*B'*A', the modelview is
    //   1. Move to offset
    //   2. Rotate and Scale
    //   3. Move by -target
    Matrix matOrigin = MatrixTranslate(-target.x, -target.y, 0.0f);
    Matrix matScale = MatrixScale(zoom, zoom, 1.0f);
    Matrix matTranslation = MatrixTranslate(offset.x, offset.y, 0.0f);
    Matrix matView = MatrixMultiply(MatrixMultiply(matOrigin, matScale), matTranslation);
    
    memcpy(out, &matView, sizeof(Matrix));
}

static void draw_context_init(draw_context_t* dc)
{
    dc->vertices = g_vertices;
    dc->vertex_count = 0;
    dc->draws = g_draws;
    dc->draw_count = 0;
    dc->batch_topology = -1;
    dc->batch_vertex_offset = 0;
    //MakePixelOrthoMatrix(dc->batch_projection, canvas_w, canvas_h);
    identity_matrix(dc->batch_projection);
}

static void finish_batch(draw_context_t* dc)
{
    const uint32_t batch_vertex_count = dc->vertex_count - dc->batch_vertex_offset;
    if (batch_vertex_count == 0)
    {
        return;
    }

    draw_t* draw = &dc->draws[dc->draw_count++];
    for (int i = 0; i < 16; ++i)
    {
        draw->projection[i] = dc->batch_projection[i];
    }
    draw->topology = dc->batch_topology;
    draw->vertex_offset = dc->batch_vertex_offset;
    draw->vertex_count = batch_vertex_count;

    dc->batch_vertex_offset = dc->vertex_count;
}

static void start_batch(draw_context_t* dc, int topology)
{
    // first batch
    if (dc->batch_topology == -1)
    {
        assert(dc->batch_vertex_offset == 0);
        assert(dc->draw_count == 0);
        dc->batch_topology = topology;
        return;
    }

    if (dc->batch_topology != topology || 
        topology == WEBGL_TRIANGLE_STRIP ||
        topology == WEBGL_LINE_STRIP)
    {
        finish_batch(dc);
        dc->batch_topology = topology;
    }
}

static void append_triangle(draw_context_t* dc, float2 a, float2 b, float2 c, uint32_t color)
{
    dc->vertices[dc->vertex_count++] = (vertex_t){a, color};
    dc->vertices[dc->vertex_count++] = (vertex_t){b, color};
    dc->vertices[dc->vertex_count++] = (vertex_t){c, color};
}

// a      b
// +------+
// |      |
// |      |
// +------+
// c      d
static void append_quad(draw_context_t* dc, float2 a, float2 b, float2 c, float2 d, uint32_t color)
{
    append_triangle(dc, a, b, c, color);
    append_triangle(dc, c, b, d, color);
}

static void draw_circle_arc(draw_context_t* dc, float2 pos, float radius, float a00, float a11, int N, uint32_t color)
{
    start_batch(dc, WEBGL_TRIANGLES);

    for (int i = 0; i < N; ++i)
    {
        const float a0 = float_lerp(a00, a11, (i * (1.0f / N)));
        const float a1 = float_lerp(a00, a11, ((i + 1) * (1.0f / N)));
        const float2 v0 = {pos.x + cosf(a0) * radius, pos.y + sinf(a0) * radius};
        const float2 v1 = {pos.x + cosf(a1) * radius, pos.y + sinf(a1) * radius};
        
        dc->vertices[dc->vertex_count++] = (vertex_t){pos, color};
        dc->vertices[dc->vertex_count++] = (vertex_t){v0, color};
        dc->vertices[dc->vertex_count++] = (vertex_t){v1, color};
    }
}

static void draw_rectangle_rounded(draw_context_t* dc, float2 pos, float2 size, float roundness, uint32_t color)
{
    if (roundness >= 1.0f) roundness = 1.0f;
    const float radius = (size.x > size.y)? (size.y*roundness)/2 : (size.x*roundness)/2;
    if (radius <= 0.0f) return;

    start_batch(dc, WEBGL_TRIANGLES);
    
    // center rect
    const float2 c00 = {pos.x + radius, pos.y + radius};
    const float2 c01 = {pos.x + radius, pos.y + size.y - radius};
    const float2 c10 = {pos.x + size.x - radius, pos.y + radius};
    const float2 c11 = {pos.x + size.x - radius, pos.y + size.y - radius};
    append_quad(dc, c00, c01, c10, c11, color);

    // left
    const float2 l00 = {pos.x, c00.y};
    const float2 l01 = {pos.x, c01.y};
    append_quad(dc, l00, c00, l01, c01, color);

    // right
    const float2 r10 = {pos.x + size.x, c10.y};
    const float2 r11 = {pos.x + size.x, c11.y};
    append_quad(dc, r10, c10, r11, c11, color);

    // top
    const float2 t00 = {c00.x, pos.y};
    const float2 t10 = {c10.x, pos.y};
    append_quad(dc, t00, c00, t10, c10, color);

    // bottom
    const float2 b01 = {c01.x, pos.y + size.y};
    const float2 b11 = {c11.x, pos.y + size.y};
    append_quad(dc, b01, c01, b11, c11, color);
    
    draw_circle_arc(dc, c00, radius, TR_PI * 1.0f, TR_PI * 1.5f, 4, color);
    draw_circle_arc(dc, c01, radius, TR_PI * 0.5f, TR_PI * 1.0f, 4, color);
    draw_circle_arc(dc, c10, radius, TR_PI * 1.5f, TR_PI * 2.0f, 4, color);
    draw_circle_arc(dc, c11, radius, TR_PI * 0.0f, TR_PI * 0.5f, 4, color);
}

float2 rotate(float2 v, float angle)
{
    const float c = cosf(angle);
    const float s = sinf(angle);
    return (float2){
        v.x * c - v.y * s,
        v.x * s + v.y * c,
    };
}

static void draw_rectangle_rotated(draw_context_t* dc, float2 pos, float2 size, float2 origin, float angle, uint32_t color)
{
    start_batch(dc, WEBGL_TRIANGLES);

    const float2 o00 = {-origin.x, -origin.y};
    const float2 o01 = {-origin.x, size.y - origin.y};
    const float2 o10 = {size.x - origin.x, -origin.y};
    const float2 o11 = {size.x - origin.x, size.y - origin.y};

    const float2 r00 = rotate(o00, angle);
    const float2 r01 = rotate(o01, angle);
    const float2 r10 = rotate(o10, angle);
    const float2 r11 = rotate(o11, angle);

    const float2 v00 = {pos.x + r00.x, pos.y + r00.y};
    const float2 v01 = {pos.x + r01.x, pos.y + r01.y};
    const float2 v10 = {pos.x + r10.x, pos.y + r10.y};
    const float2 v11 = {pos.x + r11.x, pos.y + r11.y};

    dc->vertices[dc->vertex_count++] = (vertex_t){v00, color};
    dc->vertices[dc->vertex_count++] = (vertex_t){v01, color};
    dc->vertices[dc->vertex_count++] = (vertex_t){v10, color};

    dc->vertices[dc->vertex_count++] = (vertex_t){v10, color};
    dc->vertices[dc->vertex_count++] = (vertex_t){v01, color};
    dc->vertices[dc->vertex_count++] = (vertex_t){v11, color};
}

static void draw_circle(draw_context_t* dc, float2 pos, float radius, uint32_t color)
{
    start_batch(dc, WEBGL_TRIANGLES);

    const int N = 16;
    for (int i = 0; i < N; ++i)
    {
        const float a0 = (i * (1.0f / N)) * TR_TWOPI;
        const float a1 = ((i + 1) * (1.0f / N)) * TR_TWOPI;
        const float2 v0 = {pos.x + cosf(a0) * radius, pos.y + sinf(a0) * radius};
        const float2 v1 = {pos.x + cosf(a1) * radius, pos.y + sinf(a1) * radius};
        
        dc->vertices[dc->vertex_count++] = (vertex_t){pos, color};
        dc->vertices[dc->vertex_count++] = (vertex_t){v0, color};
        dc->vertices[dc->vertex_count++] = (vertex_t){v1, color};
    }
}

static void draw_spline_segment_bezier_quadratic(draw_context_t* dc, float2 p1, float2 c2, float2 p3, float thick, uint32_t color)
{
    start_batch(dc, WEBGL_TRIANGLE_STRIP);

#define SPLINE_SEGMENT_DIVISIONS 16
    const float step = 1.0f/SPLINE_SEGMENT_DIVISIONS;

    float2 previous = p1;
    float2 current = { 0 };
    float t = 0.0f;

    vertex_t* vertices = dc->vertices + dc->vertex_count;

    for (int i = 1; i <= SPLINE_SEGMENT_DIVISIONS; i++)
    {
        t = step*(float)i;

        float a = powf(1.0f - t, 2);
        float b = 2.0f*(1.0f - t)*t;
        float c = powf(t, 2);

        // NOTE: The easing functions aren't suitable here because they don't take a control point
        current.y = a*p1.y + b*c2.y + c*p3.y;
        current.x = a*p1.x + b*c2.x + c*p3.x;

        float dy = current.y - previous.y;
        float dx = current.x - previous.x;
        float size = 0.5f*thick/sqrtf(dx*dx+dy*dy);

        if (i == 1)
        {
            vertices[0].pos.x = previous.x + dy*size;
            vertices[0].pos.y = previous.y - dx*size;
            vertices[0].color = color;
            vertices[1].pos.x = previous.x - dy*size;
            vertices[1].pos.y = previous.y + dx*size;
            vertices[1].color = color;
        }

        vertices[2*i + 1].pos.x = current.x - dy*size;
        vertices[2*i + 1].pos.y = current.y + dx*size;
        vertices[2*i + 1].color = color;
        vertices[2*i].pos.x = current.x + dy*size;
        vertices[2*i].pos.y = current.y - dx*size;
        vertices[2*i].color = color;

        previous = current;
    }

    dc->vertex_count += 2*SPLINE_SEGMENT_DIVISIONS + 2;
}

static void draw_line_strip(draw_context_t* dc, const float2* vertices, size_t vertex_count, uint32_t color)
{
    start_batch(dc, WEBGL_LINE_STRIP);

    for (size_t i = 0; i < vertex_count; ++i)
    {
        dc->vertices[dc->vertex_count++] = (vertex_t){vertices[i], color};
    }
}

void platform_render(const render_buffer_t* rb)
{
    draw_context_t dc;
    draw_context_init(&dc);

    const uint8_t* ptr = rb->data;
    for (;;)
    {
        cmd_header_t* header = (cmd_header_t*)ptr;
        if (header->type == CMD_EOF)
        {
            break;
        }
        
        ptr += sizeof(cmd_header_t);

        switch (header->type)
        {
            case CMD_CAMERA_BEGIN:
            {
                cmd_camera_begin_t* cmd = (cmd_camera_begin_t*)ptr;
                ptr += sizeof(cmd_camera_begin_t);
                finish_batch(&dc);
                GetCameraMatrix2D(dc.batch_projection, cmd->camera.target, cmd->camera.zoom, cmd->camera.offset);
                break;
            }
            case CMD_CAMERA_END:
            {
                finish_batch(&dc);
                identity_matrix(dc.batch_projection);
                break;
            }
            case CMD_DRAW_CIRCLE:
            {
                cmd_draw_circle_t* cmd = (cmd_draw_circle_t*)ptr;
                ptr += sizeof(cmd_draw_circle_t);
                draw_circle(&dc, cmd->position, cmd->radius, *(uint32_t*)&cmd->color);
                break;
            }
            case CMD_DRAW_RECTANGLE:
            {
                cmd_draw_rectangle_t* cmd = (cmd_draw_rectangle_t*)ptr;
                ptr += sizeof(cmd_draw_rectangle_t);
                draw_rectangle_rotated(&dc, cmd->position, cmd->size, cmd->origin, cmd->rotation, *(uint32_t*)&cmd->color);
                break;
            }
            case CMD_DRAW_RECTANGLE_ROUNDED:
            {
                cmd_draw_rectangle_rounded_t* cmd = (cmd_draw_rectangle_rounded_t*)ptr;
                ptr += sizeof(cmd_draw_rectangle_rounded_t);
                draw_rectangle_rounded(&dc, cmd->position, cmd->size, cmd->roundness, *(uint32_t*)&cmd->color);
                break;
            }
            case CMD_DRAW_TEXT:
            {
                cmd_draw_text_t* cmd = (cmd_draw_text_t*)ptr;
                ptr += sizeof(cmd_draw_text_t);
                ptr += cmd->text_len;
                //DrawTextEx(g_font, cmd->text, *(Vector2*)&cmd->position, cmd->font_size, 0.0f, *(Color*)&cmd->color);
                break;
            }
            case CMD_DRAW_SPLINE:
            {
                cmd_draw_spline_t* cmd = (cmd_draw_spline_t*)ptr;
                ptr += sizeof(cmd_draw_spline_t);
                draw_spline_segment_bezier_quadratic(&dc, cmd->p1, cmd->c2, cmd->p3, cmd->thickness, *(uint32_t*)&cmd->color);
                break;
            }
            case CMD_DRAW_LINE_STRIP:
            {
                cmd_draw_line_strip_t* cmd = (cmd_draw_line_strip_t*)ptr;
                ptr += sizeof(cmd_draw_line_strip_t);
                ptr += cmd->vertex_count * sizeof(float2);
                draw_line_strip(&dc, cmd->vertices, cmd->vertex_count, *(uint32_t*)&cmd->color);
                break;
            }
            default: assert(0);
        }
    }
    
    finish_batch(&dc);
    js_render(dc.draws, dc.draw_count, dc.vertices, dc.vertex_count);

    input.mouse_delta_x = input.mouse_x - input.mouse_prev_x;
    input.mouse_delta_y = input.mouse_y - input.mouse_prev_y;
    input.mouse_prev_x = input.mouse_x;
    input.mouse_prev_y = input.mouse_y;
    input.mouse_buttons_prev[0] = input.mouse_buttons[0];
    input.mouse_buttons_prev[1] = input.mouse_buttons[1];
}

float2 get_screen_size(void)
{
    return (float2){(float)canvas_w, (float)canvas_h};
}

float2 get_screen_to_world(float2 position, camera_t camera)
{
    float2 w;
    w.x = position.x + camera.target.x - camera.offset.x;
    w.y = position.y + camera.target.y - camera.offset.y;
    return w;
}

float2 get_mouse_position(void)
{
    return (float2){(float)input.mouse_x, (float)input.mouse_y};
}

float2 measure_text(font_t font, const char *text, float fontSize, float spacing)
{
    return (float2){0.0f, 0.0f};
}

// input
bool is_key_pressed(keyboard_key_t key)
{
    return false;
}

bool is_key_down(keyboard_key_t key)
{
    return false;
}

bool is_mouse_button_pressed(mouse_button_t button)
{
    return input.mouse_buttons[button] && !input.mouse_buttons_prev[button];
}

bool is_mouse_button_released(mouse_button_t button)
{
    return !input.mouse_buttons[button] && input.mouse_buttons_prev[button];
}

bool is_mouse_button_down(mouse_button_t button)
{
    return input.mouse_buttons[button];
}

float2 get_mouse_delta(void)
{
    return (float2){
        (float)(input.mouse_delta_x),
        (float)(input.mouse_delta_y),
    };
}

float2 get_mouse_wheel_move(void)
{
    return (float2){0.0f, 0.0f};
}