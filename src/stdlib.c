#include "stdlib.h"

#include <stdint.h>

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len] != '\0') ++len;
    return len;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *p = (uint8_t *)dst;
    uint8_t v = (uint8_t)c;
    for (size_t i = 0; i < n; i++) {
        p[i] = v;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }

    return 0;
}

static uint32_t rand_state = 1;

int rand(void)
{
    rand_state = rand_state * 1664525u + 1013904223u;
    return (int)(rand_state >> 1);
}

void srand(unsigned int seed)
{
    rand_state = seed ? seed : 1;
}

static void swap_bytes(uint8_t *a, uint8_t *b, size_t size) {
    while (size--) {
        uint8_t tmp = *a;
        *a++ = *b;
        *b++ = tmp;
    }
}

static void qsort_recursive(uint8_t *base, size_t nitems, size_t size,
                            int (*compar)(const void *, const void *)) {
    if (nitems < 2) return;

    // Pick pivot = middle element
    uint8_t *pivot = base + (nitems / 2) * size;
    size_t i = 0, j = nitems - 1;

    while (i <= j) {
        while (compar(base + i * size, pivot) < 0) i++;
        while (compar(base + j * size, pivot) > 0) j--;

        if (i <= j) {
            swap_bytes(base + i * size, base + j * size, size);
            i++;
            if (j > 0) j--; // avoid underflow
        }
    }

    if (j > 0)
        qsort_recursive(base, j + 1, size, compar);
    if (i < nitems)
        qsort_recursive(base + i * size, nitems - i, size, compar);
}

void qsort(void *base, size_t nitems, size_t size,
           int (*compar)(const void *, const void *)) {
    qsort_recursive((uint8_t *)base, nitems, size, compar);
}