#pragma once

#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

void write_wav_file(FILE* f, const int16_t* samples, size_t sample_count, uint32_t sample_rate);