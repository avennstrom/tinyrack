#pragma once

#include <stdint.h>

typedef struct float2 {
    float x;
    float y;
} float2;

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

#define COLOR_FROM_RGBA_HEX(hex) ((color_t){((hex) >> 24) & 0xff, ((hex) >> 16) & 0xff, ((hex) >> 8) & 0xff, ((hex) >> 0) & 0xff}) // 0xRRGGBBAA
#define COLOR_ALPHA(color, alpha) ((color_t){color.r, color.g, color.b, (uint8_t)(alpha * 255.0f)})