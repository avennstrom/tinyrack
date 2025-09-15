#pragma once

#include "types.h"
#include "renderbuf.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

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

typedef enum cursor
{
    PL_CURSOR_DEFAULT,
    PL_CURSOR_POINTER,
    PL_CURSOR_CROSSHAIR,
    PL_CURSOR_ARROW,
    PL_CURSOR_GRAB,
    PL_CURSOR_GRABBING,
    PL_CURSOR_NO_DROP,
    PL_CURSOR_MOVE,
} cursor_t;

void platform_set_cursor(cursor_t cursor);

// input
bool is_key_pressed(keyboard_key_t key);
bool is_key_down(keyboard_key_t key);
bool is_mouse_button_pressed(mouse_button_t button);
bool is_mouse_button_released(mouse_button_t button);
bool is_mouse_button_down(mouse_button_t button);
float2 get_mouse_delta(void);
float2 get_mouse_wheel_move(void);