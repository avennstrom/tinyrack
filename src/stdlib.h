#pragma once

#include <stddef.h>

size_t strlen(const char* str);
void *memset(void *dst, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);