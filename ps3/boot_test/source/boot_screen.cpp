// YARG PS3 — BOOT TEST screen layout.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include "boot_screen.h"

#include <cstdio>

namespace {

const uint32_t kBg = rgb(10, 8, 22);
const uint32_t kText = rgb(230, 230, 240);
const uint32_t kDim = rgb(140, 140, 160);
const uint32_t kAccent = rgb(255, 196, 0);
const uint32_t kOk = rgb(80, 220, 120);
const uint32_t kFail = rgb(255, 80, 80);
const uint32_t kWait = rgb(255, 196, 0);

void centered(Canvas& c, int y, const char* text, uint32_t color, int scale) {
    c.drawText((c.width - Canvas::textWidth(text, scale)) / 2, y, text, color, scale);
}

}  // namespace

void initBootStatus(BootStatus& s) {
    static const char* labels[kCheckCount] = {"PPU", "RSX", "MEMORY", "HDD", "CONTROLLER", "CORE"};
    for (int i = 0; i < kCheckCount; ++i) s.checks[i].label = labels[i];
}

void drawBootScreen(Canvas& c, const BootStatus& s) {
    int scale = c.height / 240;  // 480p -> 2, 720p -> 3, 1080p -> 4
    if (scale < 2) scale = 2;
    if (scale > 4) scale = 4;
    const int small = scale > 2 ? scale - 1 : 1;
    const int line = 11 * scale;
    const int smallLine = 11 * small;
    const int big = scale * 2;

    c.clear(kBg);

    // Header — exactly the reference screen from the master prompt.
    int y = line;
    const char* rule = "========================";
    centered(c, y, rule, kAccent, scale);
    y += line;
    centered(c, y, "YARG PS3", kText, big);
    y += 10 * big;
    centered(c, y, "BOOT TEST", kText, big);
    y += 9 * big;
    centered(c, y, rule, kAccent, scale);
    y += line * 2;

    bool allOk = true;
    for (const CheckLine& l : s.checks)
        if (l.state != CheckState::Ok) allOk = false;
    centered(c, y, allOk ? "PS3 INITIALIZED" : "PS3 INITIALIZING...", allOk ? kOk : kWait, scale);
    y += line * 2;

    // Fixed columns: "CONTROLLER:" | status | detail
    const int labelW = Canvas::textWidth("CONTROLLER: ", scale);
    const int statusW = Canvas::textWidth("FAIL  ", scale);
    const int detailW = Canvas::textWidth("port 0 12BA:0100 GUITAR HERO", small);
    int left = (c.width - (labelW + statusW + detailW)) / 2;
    if (left < 2 * scale) left = 2 * scale;

    for (const CheckLine& l : s.checks) {
        char label[32];
        std::snprintf(label, sizeof(label), "%s:", l.label);
        c.drawText(left, y, label, kText, scale);
        const char* st = l.state == CheckState::Ok ? "OK" : l.state == CheckState::Fail ? "FAIL" : "WAIT";
        const uint32_t col = l.state == CheckState::Ok ? kOk : l.state == CheckState::Fail ? kFail : kWait;
        c.drawText(left + labelW, y, st, col, scale);
        if (!l.detail.empty())
            c.drawText(left + labelW + statusW, y + (8 * scale - 8 * small) / 2, l.detail.c_str(), kDim, small);
        y += line;
    }

    // Live info block
    y += line / 2;
    if (!s.padLine.empty()) { c.drawText(left, y, s.padLine.c_str(), kDim, small); y += smallLine; }
    if (!s.guitarLine.empty()) { c.drawText(left, y, s.guitarLine.c_str(), kAccent, small); y += smallLine; }
    if (!s.songsLine.empty()) { c.drawText(left, y, s.songsLine.c_str(), kDim, small); y += smallLine; }

    // Blinking prompt (~1 Hz at 60 fps), always below the info block.
    y += line;
    if ((s.frame / 30) % 2 == 0 || s.startHeld)
        centered(c, y, "PRESS START", s.startHeld ? kOk : kText, scale);

    // Footer (two short lines so it fits 480p too).
    char footer[96];
    std::snprintf(footer, sizeof(footer), "%s  %.1f FPS  draw %.2f ms  #%u", s.resolution.c_str(), s.fps,
                  s.frameMs, s.frame);
    // Keep inside the TV title-safe area (~5% overscan on each edge).
    const int fx = c.width / 20;
    const int fy = c.height - c.height / 20 - 2 * smallLine;
    c.drawText(fx, fy, footer, kDim, small);
    c.drawText(fx, fy + smallLine, s.dataRoot.c_str(), kDim, small);
}
