// YARG PS3 — deterministic core self-test.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
// Runs on the host (unit tests) AND on the PS3 boot screen. On the console it proves that the
// MIDI parser, tempo map and HOPO rules give identical results on the big-endian Cell PPU.
#pragma once

#include <string>

namespace yarg {

struct SelfTestResult {
    bool ok = false;
    int checks = 0;        // number of assertions evaluated
    std::string message;   // first failure, or a short summary on success
};

SelfTestResult runCoreSelfTest();

}  // namespace yarg
