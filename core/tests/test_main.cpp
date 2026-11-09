// YARG PS3 — host unit tests for the portable core.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "yarg/guitar_chart.h"
#include "yarg/midi.h"
#include "yarg/selftest.h"
#include "yarg/smf_writer.h"
#include "yarg/song_ini.h"
#include "yarg/tempo.h"

using namespace yarg;

static int g_failed = 0, g_passed = 0;

#define EXPECT(cond)                                                          \
    do {                                                                      \
        if (cond) {                                                           \
            ++g_passed;                                                       \
        } else {                                                              \
            ++g_failed;                                                       \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                     \
    } while (0)

static bool load(const std::vector<uint8_t>& smf, MidiFile& midi, TempoMap& tempo) {
    std::string err;
    if (!parseMidi(smf.data(), smf.size(), midi, err)) {
        std::printf("  parse error: %s\n", err.c_str());
        return false;
    }
    tempo.build(midi);
    return true;
}

static void testSelfTest() {
    std::printf("[selftest]\n");
    SelfTestResult r = runCoreSelfTest();
    std::printf("  %s\n", r.message.c_str());
    EXPECT(r.ok);
}

static void testEnhancedOpens() {
    std::printf("[enhanced opens]\n");
    for (int withFlag = 0; withFlag < 2; ++withFlag) {
        SmfTrackBuilder sync, g;
        sync.tempo(0, 500000);
        g.name("PART GUITAR");
        g.note(0, 1, 95);     // Expert base-1
        g.note(480, 1, 96);   // G
        if (withFlag) g.text(960, "[ENHANCED_OPENS]");  // placed after the notes on purpose
        MidiFile m;
        TempoMap t;
        GuitarChart c;
        std::string err;
        EXPECT(load(buildSmf(480, {sync.build(), g.build()}), m, t));
        EXPECT(buildGuitarChart(m, t, c, err));
        const auto& ex = c.get(Difficulty::Expert);
        if (withFlag) {
            EXPECT(c.enhancedOpens);
            EXPECT(ex.size() == 2 && ex[0].mask == kOpen);
        } else {
            EXPECT(!c.enhancedOpens);
            EXPECT(ex.size() == 1 && ex[0].mask == kGreen);
        }
    }
}

static void testHopoFrequencySetting() {
    std::printf("[hopo_frequency]\n");
    SmfTrackBuilder sync, g;
    sync.tempo(0, 500000);
    g.name("PART GUITAR");
    g.note(0, 1, 96);
    g.note(240, 1, 97);  // 240 ticks apart: strum by default (threshold 161)
    MidiFile m;
    TempoMap t;
    std::string err;
    EXPECT(load(buildSmf(480, {sync.build(), g.build()}), m, t));
    GuitarChart def, wide;
    EXPECT(buildGuitarChart(m, t, def, err));
    EXPECT(def.get(Difficulty::Expert)[1].type == NoteType::Strum);
    GuitarParseSettings s;
    s.hopoFrequency = 240;  // threshold becomes 241
    EXPECT(buildGuitarChart(m, t, wide, err, s));
    EXPECT(wide.hopoThreshold == 241);
    EXPECT(wide.get(Difficulty::Expert)[1].type == NoteType::Hopo);
}

static void testErrors() {
    std::printf("[errors]\n");
    MidiFile m;
    std::string err;
    const uint8_t junk[] = {'R', 'I', 'F', 'F', 0, 0, 0, 0};
    EXPECT(!parseMidi(junk, sizeof(junk), m, err));
    EXPECT(!err.empty());
    EXPECT(!parseMidi(nullptr, 0, m, err));

    SmfTrackBuilder sync;
    sync.tempo(0, 500000);
    TempoMap t;
    EXPECT(load(buildSmf(480, {sync.build()}), m, t));
    GuitarChart c;
    EXPECT(!buildGuitarChart(m, t, c, err));
    EXPECT(err.find("PART GUITAR") != std::string::npos);

    // Bit-flip fuzz: parser must never crash or hang on corrupted input.
    SmfTrackBuilder g;
    g.name("PART GUITAR");
    for (int i = 0; i < 64; ++i) g.note(uint32_t(i * 120), 60, uint8_t(96 + i % 5));
    std::vector<uint8_t> smf = buildSmf(480, {sync.build(), g.build(true)});
    uint32_t seed = 12345;
    int parsed = 0;
    for (int iter = 0; iter < 20000; ++iter) {
        std::vector<uint8_t> copy = smf;
        for (int k = 0; k < 4; ++k) {
            seed = seed * 1664525u + 1013904223u;
            copy[seed % copy.size()] ^= uint8_t(1u << ((seed >> 16) % 8));
        }
        MidiFile mm;
        if (parseMidi(copy.data(), copy.size(), mm, err)) {
            ++parsed;
            TempoMap tt;
            tt.build(mm);
            GuitarChart cc;
            buildGuitarChart(mm, tt, cc, err);
        }
    }
    std::printf("  fuzz: %d/20000 corrupted files still parsed, no crash\n", parsed);
    EXPECT(true);
}

static void testTempoRoundTrip() {
    std::printf("[tempo]\n");
    SmfTrackBuilder sync;
    sync.tempo(0, 600000);
    sync.tempo(1000, 300000);
    sync.tempo(1000, 400000);  // same tick: last wins
    sync.tempo(5000, 750000);
    MidiFile m;
    TempoMap t;
    EXPECT(load(buildSmf(192, {sync.build()}), m, t));
    EXPECT(t.tempos().size() == 3);
    EXPECT(t.tempos()[1].usPerQuarter == 400000);
    bool roundTrip = true;
    for (uint32_t tick = 0; tick < 10000; tick += 7)
        if (t.secondsToTick(t.tickToSeconds(tick)) != tick) roundTrip = false;
    EXPECT(roundTrip);
}

int main() {
    testSelfTest();
    testEnhancedOpens();
    testHopoFrequencySetting();
    testErrors();
    testTempoRoundTrip();
    std::printf("\n%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
