#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;
}

int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int a = tolower((unsigned char)*s1);
        int b = tolower((unsigned char)*s2);
        if (a != b) return a - b;
        ++s1;
        ++s2;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n--) {
        int a = tolower((unsigned char)*s1);
        int b = tolower((unsigned char)*s2);
        if (a != b) return a - b;
        if (!*s1) return 0;
        ++s1;
        ++s2;
    }
    return 0;
}

static const float PIO2 = 1.5707963267948966f;

float fmodf(float x, float y) {
    if (y == 0.0f) return 0.0f;
    return x - (float)((int)(x / y)) * y;
}

float asinf(float x) {
    if (x >= 1.0f) return PIO2;
    if (x <= -1.0f) return -PIO2;
    return atan2f(x, sqrtf(1.0f - x * x));
}

double asin(double x) {
    return (double)asinf((float)x);
}

float expf(float x) {
    if (x < -20.0f) return 0.0f;
    if (x > 20.0f) x = 20.0f;
    float sum = 1.0f;
    float term = 1.0f;
    for (int i = 1; i <= 12; ++i) {
        term *= x / (float)i;
        sum += term;
    }
    return sum;
}

double exp(double x) {
    return (double)expf((float)x);
}

float logf(float x) {
    if (x <= 0.0f) return -20.0f;
    int e = 0;
    while (x >= 2.0f) { x *= 0.5f; ++e; }
    while (x < 1.0f) { x *= 2.0f; --e; }
    float y = (x - 1.0f) / (x + 1.0f);
    float y2 = y * y;
    float r = 2.0f * y * (1.0f + y2 * (1.0f / 3.0f + y2 * (1.0f / 5.0f + y2 / 7.0f)));
    return r + (float)e * 0.69314718f;
}
