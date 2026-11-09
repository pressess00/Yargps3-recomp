// YARG PS3 — minimal CPU canvas.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include "canvas.h"

#include <cstring>

#include "font8x8_basic.h"

void Canvas::clear(uint32_t color) { fillRect(0, 0, width, height, color); }

void Canvas::fillRect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > width) w = width - x;
    if (y + h > height) h = height - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; ++row) {
        uint32_t* p = pixels + (y + row) * pitchPixels + x;
        for (int col = 0; col < w; ++col) p[col] = color;
    }
}

int Canvas::textWidth(const char* text, int scale) { return int(std::strlen(text)) * 8 * scale; }

int Canvas::drawText(int x, int y, const char* text, uint32_t color, int scale) {
    for (const char* c = text; *c; ++c, x += 8 * scale) {
        const unsigned char ch = static_cast<unsigned char>(*c);
        if (ch == ' ' || ch >= 128) continue;
        const unsigned char* glyph = kFont8x8[ch];
        for (int gy = 0; gy < 8; ++gy) {
            const unsigned char bits = glyph[gy];
            if (!bits) continue;
            for (int gx = 0; gx < 8; ++gx)
                if (bits & (1u << gx)) fillRect(x + gx * scale, y + gy * scale, scale, scale, color);
        }
    }
    return x;
}
