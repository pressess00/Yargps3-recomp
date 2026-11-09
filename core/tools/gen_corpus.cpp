// YARG PS3 — deterministic generator of test .mid files for the golden comparison with YARG.Core.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
//   gen_corpus <output_dir> [count]
//
// Each file is a format-1 MIDI with a tempo track and "PART GUITAR" containing random notes,
// chords, gaps clustered around the HOPO threshold (resolution/3 +- a few ticks), sustains around
// the cutoff, forced HOPO/strum markers, tap and star power phrases, all four difficulties.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "yarg/smf_writer.h"

namespace {

struct Rng {
    uint32_t s;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    int range(int lo, int hi) { return lo + int(next() % uint32_t(hi - lo + 1)); }
    bool chance(int percent) { return range(0, 99) < percent; }
};

std::vector<uint8_t> makeSong(uint32_t seed) {
    Rng r{seed * 2654435761u + 1};
    static const uint16_t kResolutions[] = {192, 480, 960};
    const uint16_t res = kResolutions[seed % 3];
    const uint32_t hopo = res / 3;

    yarg::SmfTrackBuilder sync, g;
    sync.name("tempo");
    sync.timeSig(0, 4, 2);
    sync.tempo(0, uint32_t(r.range(300000, 800000)));
    g.name("PART GUITAR");
    const bool opens = r.chance(30);
    if (opens) g.text(0, "[ENHANCED_OPENS]");

    static const int kBase[] = {60, 72, 84, 96};
    const int kCount = 160;
    // Note positions first, so every length can be capped to the gap before the next note:
    // real charts never overlap two notes of the same key, and such overlaps would make
    // note-on/off pairing ambiguous between parsers.
    std::vector<uint32_t> ticks(kCount + 1);
    uint32_t tick = res;  // start after one beat
    for (int i = 0; i <= kCount; ++i) {
        // Gaps: mostly around the HOPO threshold to probe the boundary.
        int gapKind = r.range(0, 9);
        uint32_t gap;
        if (gapKind < 6)
            gap = hopo + 1 + uint32_t(r.range(-3, 3));  // threshold is hopo+1 (inclusive)
        else if (gapKind < 8)
            gap = res / 4;
        else
            gap = res * uint32_t(r.range(1, 2));
        tick += gap;
        ticks[i] = tick;
    }

    uint32_t tapFree = 0, spFree = 0;  // phrases of one kind never overlap
    for (int i = 0; i < kCount; ++i) {
        tick = ticks[i];
        const uint32_t room = ticks[i + 1] - tick - 1;  // max length without overlap
        auto capped = [room](uint32_t len) { return len > room ? room : (len ? len : 1); };
        if (r.chance(3)) sync.tempo(tick, uint32_t(r.range(300000, 800000)));

        for (int d = 0; d < 4; ++d) {
            if (d < 3 && r.chance(40)) continue;  // lower difficulties are sparser
            const int base = kBase[d];
            int fretCount = r.chance(25) ? r.range(2, 3) : 1;
            uint32_t used = 0;
            for (int k = 0; k < fretCount; ++k) {
                int fret = (opens && fretCount == 1 && r.chance(10)) ? -1 : r.range(0, 4);
                if (fret >= 0 && (used & (1u << fret))) continue;
                if (fret >= 0) used |= 1u << fret;
                uint32_t len = r.chance(30) ? uint32_t(r.range(int(hopo) - 3, int(res) * 2)) : 1;
                g.note(tick, capped(len), uint8_t(base + fret));
            }
            if (r.chance(8)) g.note(tick, capped(uint32_t(r.range(1, int(res)))), uint8_t(base + 5));  // force HOPO
            if (r.chance(8)) g.note(tick, capped(uint32_t(r.range(1, int(res)))), uint8_t(base + 6));  // force strum
        }
        if (tick >= tapFree && r.chance(4)) {  // tap phrase over several notes
            uint32_t len = uint32_t(r.range(1, int(res) * 2));
            g.note(tick, len, 104);
            tapFree = tick + len + 1;
        }
        if (tick >= spFree && r.chance(5)) {  // star power phrase
            uint32_t len = uint32_t(r.range(int(res), int(res) * 8));
            g.note(tick, len, 116);
            spFree = tick + len + 1;
        }
    }
    return yarg::buildSmf(res, {sync.build(), g.build(r.chance(50))});
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <output_dir> [count]\n", argv[0]);
        return 2;
    }
    const int count = argc > 2 ? std::atoi(argv[2]) : 40;
    for (int i = 0; i < count; ++i) {
        std::vector<uint8_t> smf = makeSong(uint32_t(i + 1));
        char path[1024];
        std::snprintf(path, sizeof(path), "%s/song%03d.mid", argv[1], i);
        FILE* f = std::fopen(path, "wb");
        if (!f) {
            std::fprintf(stderr, "cannot write %s\n", path);
            return 1;
        }
        std::fwrite(smf.data(), 1, smf.size(), f);
        std::fclose(f);
    }
    std::printf("generated %d files in %s\n", count, argv[1]);
    return 0;
}
