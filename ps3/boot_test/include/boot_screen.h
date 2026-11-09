// YARG PS3 — BOOT TEST screen layout (platform independent).
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#pragma once

#include <string>

#include "canvas.h"

enum class CheckState { Wait, Ok, Fail };

struct CheckLine {
    const char* label = "";
    CheckState state = CheckState::Wait;
    std::string detail;
};

enum CheckId { kPpu, kRsx, kMemory, kHdd, kController, kCore, kCheckCount };

struct BootStatus {
    CheckLine checks[kCheckCount];
    std::string resolution;   // e.g. "1280x720"
    std::string padLine;      // live buttons of the first pad
    std::string guitarLine;   // fret view when a guitar is detected
    std::string songsLine;    // songs folder summary
    std::string dataRoot;     // USRDIR used for data
    float fps = 0.0f;
    float frameMs = 0.0f;     // CPU time spent drawing one frame
    unsigned frame = 0;
    bool startHeld = false;
};

void initBootStatus(BootStatus& s);
void drawBootScreen(Canvas& c, const BootStatus& s);
