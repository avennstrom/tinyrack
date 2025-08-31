#include "platform.h"
#include "font.h"

#include <raylib.h>
#include <assert.h>
#include <stdio.h>

Font g_font;
AudioStream g_stream;
platform_audio_callback g_audio_callback;

#ifdef __EMSCRIPTEN__
void emscripten_audio_callback(void* data, unsigned int frames)
{
    g_audio_callback(data, frames);
}
#endif

void platform_init(size_t sample_rate, size_t sample_count, platform_audio_callback audio_callback)
{
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
    SetAudioStreamBufferSizeDefault((int)sample_count);

    g_audio_callback = audio_callback;
    g_stream = LoadAudioStream((unsigned int)sample_rate, 16, 1);
#ifdef __EMSCRIPTEN__
    SetAudioStreamCallback(g_stream, emscripten_audio_callback);
#endif
    PlayAudioStream(g_stream);
}

bool platform_running(void)
{
    return !WindowShouldClose();
}

void platform_render(const render_buffer_t* rb)
{
    BeginDrawing();
    ClearBackground(*(Color*)&rb->clear_color);

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
                BeginMode2D(*(Camera2D*)&cmd->camera);
                break;
            }
            case CMD_CAMERA_END:
            {
                EndMode2D();
                break;
            }
            case CMD_DRAW_CIRCLE:
            {
                cmd_draw_circle_t* cmd = (cmd_draw_circle_t*)ptr;
                ptr += sizeof(cmd_draw_circle_t);
                DrawCircleV(*(Vector2*)&cmd->position, cmd->radius, *(Color*)&cmd->color);
                break;
            }
            case CMD_DRAW_RECTANGLE:
            {
                cmd_draw_rectangle_t* cmd = (cmd_draw_rectangle_t*)ptr;
                ptr += sizeof(cmd_draw_rectangle_t);
                Rectangle rect = {cmd->position.x, cmd->position.y, cmd->size.x, cmd->size.y};
                DrawRectanglePro(rect, *(Vector2*)&cmd->origin, cmd->rotation * RAD2DEG, *(Color*)&cmd->color);
                break;
            }
            case CMD_DRAW_RECTANGLE_ROUNDED:
            {
                cmd_draw_rectangle_rounded_t* cmd = (cmd_draw_rectangle_rounded_t*)ptr;
                ptr += sizeof(cmd_draw_rectangle_rounded_t);
                Rectangle rect = {cmd->position.x, cmd->position.y, cmd->size.x, cmd->size.y};
                DrawRectangleRounded(rect, cmd->roundness, 8, *(Color*)&cmd->color);
                break;
            }
            case CMD_DRAW_TEXT:
            {
                cmd_draw_text_t* cmd = (cmd_draw_text_t*)ptr;
                ptr += sizeof(cmd_draw_text_t);
                ptr += cmd->text_len;
                DrawTextEx(g_font, cmd->text, *(Vector2*)&cmd->position, cmd->font_size, 0.0f, *(Color*)&cmd->color);
                break;
            }
            case CMD_DRAW_SPLINE:
            {
                cmd_draw_spline_t* cmd = (cmd_draw_spline_t*)ptr;
                ptr += sizeof(cmd_draw_spline_t);
                DrawSplineSegmentBezierQuadratic(*(Vector2*)&cmd->p1, *(Vector2*)&cmd->c2, *(Vector2*)&cmd->p3, cmd->thickness, *(Color*)&cmd->color);
                break;
            }
            case CMD_DRAW_LINE_STRIP:
            {
                cmd_draw_line_strip_t* cmd = (cmd_draw_line_strip_t*)ptr;
                ptr += sizeof(cmd_draw_line_strip_t);
                ptr += cmd->vertex_count * sizeof(float2);
                DrawLineStrip((Vector2*)cmd->vertices, cmd->vertex_count, *(Color*)&cmd->color);
                break;
            }
            default: assert(0);
        }
    }

    EndDrawing();

#ifndef __EMSCRIPTEN__
    if (IsAudioStreamProcessed(g_stream))
    {
        int16_t samples[1024];
        g_audio_callback(samples, 1024);
        UpdateAudioStream(g_stream, samples, 1024);
    }
#endif
}

float2 get_screen_size(void)
{
    return (float2){(float)GetScreenWidth(), (float)GetScreenHeight()};
}

float2 get_screen_to_world(float2 position, camera_t camera)
{
    Vector2 v = GetScreenToWorld2D(*(Vector2*)&position, *(Camera2D*)&camera);
    return (float2){v.x, v.y};
}

float2 get_mouse_position(void)
{
    Vector2 v = GetMousePosition();
    return (float2){v.x, v.y};
}

float2 measure_text(font_t font, const char *text, float fontSize, float spacing)
{
    (void)font;
    Vector2 v = MeasureTextEx(g_font, text, fontSize, spacing);
    return (float2){v.x, v.y};
}

int get_raylib_key(keyboard_key_t key)
{
    switch (key)
    {
        case PL_KEY_LEFT_CONTROL: return KEY_LEFT_CONTROL;
        case PL_KEY_TAB: return KEY_TAB;
        case PL_KEY_SPACE: return KEY_SPACE;
        case PL_KEY_S: return KEY_S;
        default: assert(0);
    }
    return -1;
}

bool is_key_pressed(keyboard_key_t key)
{
    return IsKeyPressed(get_raylib_key(key));
}

bool is_key_down(keyboard_key_t key)
{
    return IsKeyDown(get_raylib_key(key));
}

int get_raylib_mouse_button(mouse_button_t button)
{
    switch (button)
    {
        case PL_MOUSE_BUTTON_LEFT: return MOUSE_BUTTON_LEFT;
        case PL_MOUSE_BUTTON_RIGHT: return MOUSE_BUTTON_RIGHT;
        default: assert(0);
    }
    return -1;
}

bool is_mouse_button_pressed(mouse_button_t button)
{
    return IsMouseButtonPressed(get_raylib_mouse_button(button));
}

bool is_mouse_button_released(mouse_button_t button)
{
    return IsMouseButtonReleased(get_raylib_mouse_button(button));
}

bool is_mouse_button_down(mouse_button_t button)
{
    return IsMouseButtonDown(get_raylib_mouse_button(button));
}

float2 get_mouse_delta(void)
{
    Vector2 v = GetMouseDelta();
    return (float2){v.x, v.y};
}

float2 get_mouse_wheel_move(void)
{
    Vector2 v = GetMouseWheelMoveV();
    return (float2){v.x, v.y};
}