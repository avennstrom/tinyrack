#pragma once
#include <stdint.h>

typedef struct audio audio_t;

audio_t* audio_create(uint32_t sample_rate, uint32_t sample_count);
int16_t* audio_fill(audio_t* audio);