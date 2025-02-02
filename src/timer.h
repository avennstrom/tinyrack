#pragma once
#include <stdint.h>

typedef struct timer
{
    int64_t freq;
    int64_t last;
} timer_t;

void timer_start(timer_t* timer);
double timer_reset(timer_t* timer);