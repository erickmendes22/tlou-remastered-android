#pragma once
#include <stdint.h>

static inline void gow3_display_size(int w, int h, uint32_t width, uint32_t height,
                                    int fullscreen, int* dw, int* dh) {
    *dw = w;
    *dh = h;
    if (fullscreen) return;
    *dh = (int)((int64_t)w * height / (width ? width : 1));
    if (*dh > h) { *dh = h; *dw = (int)((int64_t)h * width / (height ? height : 1)); }
}
