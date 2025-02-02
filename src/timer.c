#include "timer.h"

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

    return elapsed;
}