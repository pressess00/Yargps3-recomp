// YARG PS3 — double-buffered RSX framebuffer for CPU drawing.
// Based on PSL1GHT samples/graphics/blitting/source/rsxutil.cpp
// (Copyright (c) 2011 PSL1GHT Development Team, MIT license — see THIRD_PARTY.md).
#pragma once

#include <ppu-types.h>

#include "canvas.h"

struct DisplayInfo {
    u32 width = 0;
    u32 height = 0;
    u32 pitch = 0;          // bytes per row
    u32 framebufferBytes = 0;
    u32 hostBytes = 0;      // RSX io (command buffer) area in main memory
};

// Returns false if RSX or video setup failed. `error` points to a static string.
bool displayInit(DisplayInfo& info, const char** error);

// Canvas for the back buffer (the one not on screen).
Canvas displayBackBuffer();

// Queue a flip of the back buffer on vsync and wait until it is shown.
void displayPresent();

void displayShutdown();
