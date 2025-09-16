#include "timer.h"

#ifdef PLATFORM_WEB

__attribute__((import_module("env"), import_name("js_now")))
extern double js_now(void);

void timer_start(timer_t* timer)
{
    timer->last = js_now();
}

double timer_reset(timer_t* timer)
{
    const double now = js_now();
    const double elapsed = now - timer->last;
    timer->last = now;
    return elapsed;
}

#else
#include <Windows.h>

void timer_start(timer_t* timer)
{
    _STATIC_ASSERT(sizeof(LARGE_INTEGER) == sizeof(int64_t));
    QueryPerformanceFrequency((LARGE_INTEGER*)&timer->freq);
    QueryPerformanceCounter((LARGE_INTEGER*)&timer->last);
}

// milliseconds
double timer_reset(timer_t* timer)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    
    const double elapsed = (double)(now.QuadPart - timer->last) / timer->freq;

    timer->last = now.QuadPart;

    return elapsed * 1000.0;
}

#endif