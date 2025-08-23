#pragma once

#define TR_SAMPLE_RATE  44100
#define TR_SAMPLE_COUNT (1024) // 1024/44100 ~= 23ms

#define TR_PI 3.141592f
#define TR_TWOPI (2.0f * TR_PI)

#define tr_countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))