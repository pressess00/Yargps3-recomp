// YARG PS3 — minimal CPU canvas over a 32-bit XRGB framebuffer.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
// Platform independent: on PS3 `pixels` points into RSX memory, on the host into a plain buffer
// (used to render preview images). Only WRITES to the buffer — reading RSX memory from the PPU
// is very slow.
#pragma once

#include <cstdint>

struct Canvas {
    uint32_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    int pitchPixels = 0;  // pixels per row (pitch in bytes / 4)

    void clear(uint32_t color);
    void fillRect(int x, int y, int w, int h, uint32_t color);
    // Draws ASCII text with the 8x8 font scaled by `scale`. Returns the x after the last glyph.
    int drawText(int x, int y, const char* text, uint32_t color, int scale);
    static int textWidth(const char* text, int scale);
};

// XRGB8888 helpers
constexpr uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}
