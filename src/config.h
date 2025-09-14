#pragma once

#define TR_SAMPLE_RATE  48000
#define TR_SAMPLE_COUNT (512) // 512/48000 ~= 10ms

#define TR_PI 3.141592f
#define TR_TWOPI (2.0f * TR_PI)

#define tr_countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))