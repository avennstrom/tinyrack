#pragma once
#include <stdint.h>

typedef struct timer
{
#ifdef __EMSCRIPTEN__
    double last;
#else
    int64_t freq;
    int64_t last;
#endif
} timer_t;

void timer_start(timer_t* timer);
double timer_reset(timer_t* timer);