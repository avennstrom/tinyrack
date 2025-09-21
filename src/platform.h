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
    PL_KEY_BACKSPACE = 8,
    PL_KEY_TAB = 9,
    PL_KEY_ENTER = 13,
    PL_KEY_SHIFT = 16,
    PL_KEY_CTRL = 17,
    PL_KEY_ALT = 18,
    PL_KEY_PAUSE = 19,
    PL_KEY_CAPSLOCK = 20,
    PL_KEY_ESCAPE = 27,
    PL_KEY_SPACE = 32,
    PL_KEY_PAGE_UP = 33,
    PL_KEY_PAGE_DOWN = 34,
    PL_KEY_END = 35,
    PL_KEY_HOME = 36,
    PL_KEY_LEFT = 37,
    PL_KEY_UP = 38,
    PL_KEY_RIGHT = 39,
    PL_KEY_DOWN = 40,
    PL_KEY_INSERT = 45,
    PL_KEY_DELETE = 46,
    // Digits
    PL_KEY_0 = 48,
    PL_KEY_1 = 49,
    PL_KEY_2 = 50,
    PL_KEY_3 = 51,
    PL_KEY_4 = 52,
    PL_KEY_5 = 53,
    PL_KEY_6 = 54,
    PL_KEY_7 = 55,
    PL_KEY_8 = 56,
    PL_KEY_9 = 57,

    // Letters
    PL_KEY_A = 65,
    PL_KEY_B = 66,
    PL_KEY_C = 67,
    PL_KEY_D = 68,
    PL_KEY_E = 69,
    PL_KEY_F = 70,
    PL_KEY_G = 71,
    PL_KEY_H = 72,
    PL_KEY_I = 73,
    PL_KEY_J = 74,
    PL_KEY_K = 75,
    PL_KEY_L = 76,
    PL_KEY_M = 77,
    PL_KEY_N = 78,
    PL_KEY_O = 79,
    PL_KEY_P = 80,
    PL_KEY_Q = 81,
    PL_KEY_R = 82,
    PL_KEY_S = 83,
    PL_KEY_T = 84,
    PL_KEY_U = 85,
    PL_KEY_V = 86,
    PL_KEY_W = 87,
    PL_KEY_X = 88,
    PL_KEY_Y = 89,
    PL_KEY_Z = 90,

    // Function keys
    PL_KEY_F1 = 112,
    PL_KEY_F2 = 113,
    PL_KEY_F3 = 114,
    PL_KEY_F4 = 115,
    PL_KEY_F5 = 116,
    PL_KEY_F6 = 117,
    PL_KEY_F7 = 118,
    PL_KEY_F8 = 119,
    PL_KEY_F9 = 120,
    PL_KEY_F10 = 121,
    PL_KEY_F11 = 122,
    PL_KEY_F12 = 123,

    // Symbols
    PL_KEY_SEMICOLON = 186,
    PL_KEY_EQUAL = 187,
    PL_KEY_COMMA = 188,
    PL_KEY_MINUS = 189,
    PL_KEY_PERIOD = 190,
    PL_KEY_SLASH = 191,
    PL_KEY_BACKQUOTE = 192,
    PL_KEY_BRACKET_LEFT = 219,
    PL_KEY_BACKSLASH = 220,
    PL_KEY_BRACKET_RIGHT = 221,
    PL_KEY_QUOTE = 222
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

__attribute__((import_module("env"), import_name("console_log")))
extern void console_log(const char* text);