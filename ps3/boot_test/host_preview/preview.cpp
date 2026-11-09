// YARG PS3 — renders the BOOT TEST screen on a PC into a PPM image (layout preview / CI check).
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
//   preview out.ppm [width height]
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "boot_screen.h"
#include "yarg/selftest.h"

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "boot_test_preview.ppm";
    const int w = argc > 3 ? std::atoi(argv[2]) : 1280;
    const int h = argc > 3 ? std::atoi(argv[3]) : 720;

    std::vector<uint32_t> fb(size_t(w) * h);
    Canvas c{fb.data(), w, h, w};

    BootStatus s;
    initBootStatus(s);
    s.checks[kPpu] = {"PPU", CheckState::Ok, "big-endian, timebase 79.8 MHz"};
    s.checks[kRsx] = {"RSX", CheckState::Ok, "1280x720 XRGB, 2 buffers"};
    s.checks[kMemory] = {"MEMORY", CheckState::Ok, "io 32 MB, VRAM fb 7.0 MB"};
    s.checks[kHdd] = {"HDD", CheckState::Ok, "write/read/list OK"};
    s.checks[kController] = {"CONTROLLER", CheckState::Ok, "port 0 12BA:0100 GUITAR"};
    yarg::SelfTestResult st = yarg::runCoreSelfTest();
    s.checks[kCore] = {"CORE", st.ok ? CheckState::Ok : CheckState::Fail, st.message};
    s.resolution = "1280x720";
    s.padLine = "PAD0: X O [] /\\ L1 . . UP . STA .  R(128,128)";
    s.guitarLine = "GUITAR: [G][R][ ][ ][O] STRUM:UP WHAMMY:128";
    s.songsLine = "SONGS: 3 folders, first: Song1";
    s.dataRoot = "/dev_hdd0/game/YARGPS300/USRDIR";
    s.fps = 59.9f;
    s.frameMs = 4.21f;
    s.frame = 0;
    drawBootScreen(c, s);

    FILE* f = std::fopen(out, "wb");
    if (!f) return 1;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (uint32_t px : fb) {
        unsigned char rgb[3] = {uint8_t(px >> 16), uint8_t(px >> 8), uint8_t(px)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    std::printf("wrote %s (%dx%d), core self-test: %s\n", out, w, h, st.message.c_str());
    return st.ok ? 0 : 1;
}
