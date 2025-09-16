#pragma once

#include <stddef.h>

#define assert(x)

size_t strlen(const char* str);
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
int rand(void);
void srand(unsigned int seed);
void qsort(void *base, size_t nitems, size_t size, int (*compar)(const void *, const void *));