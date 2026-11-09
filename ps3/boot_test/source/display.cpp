// YARG PS3 — double-buffered RSX framebuffer for CPU drawing.
// Based on PSL1GHT samples/graphics/blitting/source/rsxutil.cpp
// (Copyright (c) 2011 PSL1GHT Development Team, MIT license — see THIRD_PARTY.md).
// Modifications: no depth buffer / render surface (CPU drawing only), present() waits for the
// flip so the back buffer is never written while visible, error reporting.
#include "display.h"

#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include <rsx/rsx.h>
#include <sysutil/video.h>

namespace {

const u32 kCommandBufferSize = 0x100000;       // 1 MB, as in the PSL1GHT samples
const u32 kHostSize = 32 * 1024 * 1024;        // RSX io area in main memory
const u32 kLabelIndex = 255;

gcmContextData* g_context = nullptr;
void* g_hostAddr = nullptr;
u32* g_color[2] = {nullptr, nullptr};
u32 g_offset[2] = {0, 0};
u32 g_back = 0;  // index of the buffer we draw into
DisplayInfo g_info;
u32 g_label = 1;

void waitFinish() {
    rsxSetWriteBackendLabel(g_context, kLabelIndex, g_label);
    rsxFlushBuffer(g_context);
    while (*(vu32*)gcmGetLabelAddress(kLabelIndex) != g_label) usleep(30);
    ++g_label;
}

void waitRsxIdle() {
    rsxSetWriteBackendLabel(g_context, kLabelIndex, g_label);
    rsxSetWaitLabel(g_context, kLabelIndex, g_label);
    ++g_label;
    waitFinish();
}

}  // namespace

bool displayInit(DisplayInfo& info, const char** error) {
    g_hostAddr = memalign(1024 * 1024, kHostSize);
    if (!g_hostAddr) {
        *error = "memalign host area failed";
        return false;
    }
    if (rsxInit(&g_context, kCommandBufferSize, kHostSize, g_hostAddr) != 0 || !g_context) {
        *error = "rsxInit failed";
        return false;
    }

    videoState state;
    if (videoGetState(0, 0, &state) != 0) {
        *error = "videoGetState failed";
        return false;
    }
    videoResolution res;
    if (videoGetResolution(state.displayMode.resolution, &res) != 0) {
        *error = "videoGetResolution failed";
        return false;
    }

    videoConfiguration vconfig;
    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = res.width * sizeof(u32);

    waitRsxIdle();
    if (videoConfigure(0, &vconfig, NULL, 0) != 0) {
        *error = "videoConfigure failed";
        return false;
    }
    videoGetState(0, 0, &state);
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    g_info.width = res.width;
    g_info.height = res.height;
    g_info.pitch = res.width * sizeof(u32);
    g_info.framebufferBytes = g_info.pitch * res.height;
    g_info.hostBytes = kHostSize;

    for (int i = 0; i < 2; ++i) {
        g_color[i] = (u32*)rsxMemalign(64, g_info.framebufferBytes);
        if (!g_color[i]) {
            *error = "rsxMemalign framebuffer failed";
            return false;
        }
        rsxAddressToOffset(g_color[i], &g_offset[i]);
        gcmSetDisplayBuffer(i, g_offset[i], g_info.pitch, g_info.width, g_info.height);
    }
    g_back = 0;
    gcmResetFlipStatus();
    info = g_info;
    return true;
}

Canvas displayBackBuffer() {
    Canvas c;
    c.pixels = g_color[g_back];
    c.width = int(g_info.width);
    c.height = int(g_info.height);
    c.pitchPixels = int(g_info.pitch / sizeof(u32));
    return c;
}

void displayPresent() {
    gcmResetFlipStatus();
    gcmSetFlip(g_context, g_back);
    rsxFlushBuffer(g_context);
    gcmSetWaitFlip(g_context);
    // Block until the flip happened (vsync): afterwards the other buffer is off-screen.
    while (gcmGetFlipStatus() != 0) usleep(200);
    g_back ^= 1;
}

void displayShutdown() {
    if (!g_context) return;
    gcmSetWaitFlip(g_context);
    rsxFinish(g_context, 1);
}
