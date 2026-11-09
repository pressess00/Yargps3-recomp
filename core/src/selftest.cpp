// YARG PS3 — deterministic core self-test.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include "yarg/selftest.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "yarg/guitar_chart.h"
#include "yarg/midi.h"
#include "yarg/smf_writer.h"
#include "yarg/song_ini.h"
#include "yarg/tempo.h"

namespace yarg {
namespace {

struct Checker {
    SelfTestResult& r;
    bool check(bool cond, const char* what, long got = 0, long want = 0) {
        ++r.checks;
        if (!cond && r.ok) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "FAIL #%d %s (got %ld, want %ld)", r.checks, what, got, want);
            r.message = buf;
            r.ok = false;
        }
        return cond;
    }
};

}  // namespace

SelfTestResult runCoreSelfTest() {
    SelfTestResult r;
    r.ok = true;
    Checker c{r};

    // ---- Build a known song in memory (480 ticks per quarter note) ----
    SmfTrackBuilder sync;
    sync.name("selftest");
    sync.tempo(0, 500000);      // 120 BPM
    sync.timeSig(0, 4, 2);      // 4/4
    sync.tempo(1920, 1000000);  // 60 BPM from tick 1920 (= 2.0 s)

    SmfTrackBuilder gtr;
    gtr.name("PART GUITAR");
    const int X = 96;           // Expert base note
    gtr.note(0, 480, X + 0);    // 1  G, sustain kept (>= 160)
    gtr.note(160, 60, X + 1);   // 2  R, sustain cut -> 0, natural HOPO
    gtr.note(320, 60, X + 1);   // 3  R again -> strum (same fret)
    gtr.note(480, 1, X + 0);    // 4  G+Y chord -> strum
    gtr.note(480, 1, X + 2);
    gtr.note(640, 1, X + 2);    // 5  Y after chord containing Y -> strum (subset exception)
    gtr.note(800, 1, X + 3);    // 6  B -> natural HOPO
    gtr.note(960, 1, X + 4);    // 7  O natural HOPO, but forced strum
    gtr.note(960, 1, X + 6);
    gtr.note(1440, 1, X + 0);   // 8  G too far for HOPO, forced HOPO
    gtr.note(1440, 120, X + 5);
    gtr.note(1920, 1, X + 1);   // 9  R tap
    gtr.note(1920, 240, 104);
    gtr.note(2400, 1, X + 2);   // 10 Y in star power
    gtr.note(2400, 480, 116);
    gtr.note(2880, 1, X + 3);   // 11 B star power phrase already ended (end tick excluded)
    gtr.note(0, 1, 84);         // Hard: one green note

    const std::vector<uint8_t> smf = buildSmf(480, {sync.build(false), gtr.build(true)});

    // ---- Parse ----
    MidiFile midi;
    std::string err;
    if (!c.check(parseMidi(smf.data(), smf.size(), midi, err), "parseMidi")) {
        r.message += " " + err;
        return r;
    }
    c.check(midi.ticksPerQuarter == 480, "resolution", midi.ticksPerQuarter, 480);
    c.check(midi.tracks.size() == 2, "track count", long(midi.tracks.size()), 2);

    TempoMap tempo;
    tempo.build(midi);
    c.check(std::fabs(tempo.tickToSeconds(1920) - 2.0) < 1e-9, "tick 1920 = 2.0s",
            long(tempo.tickToSeconds(1920) * 1000), 2000);
    c.check(std::fabs(tempo.tickToSeconds(2400) - 3.0) < 1e-9, "tick 2400 = 3.0s",
            long(tempo.tickToSeconds(2400) * 1000), 3000);
    c.check(tempo.secondsToTick(4.0) == 2880, "4.0s = tick 2880", long(tempo.secondsToTick(4.0)), 2880);

    GuitarChart chart;
    if (!c.check(buildGuitarChart(midi, tempo, chart, err), "buildGuitarChart")) {
        r.message += " " + err;
        return r;
    }
    c.check(chart.hopoThreshold == 161, "hopo threshold", chart.hopoThreshold, 161);
    c.check(chart.sustainCutoff == 160, "sustain cutoff", chart.sustainCutoff, 160);

    const auto& ex = chart.get(Difficulty::Expert);
    const uint8_t wantMask[] = {kGreen, kRed, kRed, kGreen | kYellow, kYellow, kBlue,
                                kOrange, kGreen, kRed, kYellow, kBlue};
    const NoteType S = NoteType::Strum, H = NoteType::Hopo, T = NoteType::Tap;
    const NoteType wantType[] = {S, H, S, S, S, H, S, H, T, S, S};
    const bool wantSp[] = {false, false, false, false, false, false, false, false, false, true, false};
    const size_t n = sizeof(wantMask);

    if (c.check(ex.size() == n, "expert note count", long(ex.size()), long(n))) {
        for (size_t i = 0; i < n; ++i) {
            char what[48];
            std::snprintf(what, sizeof(what), "expert note %d mask", int(i + 1));
            c.check(ex[i].mask == wantMask[i], what, ex[i].mask, wantMask[i]);
            std::snprintf(what, sizeof(what), "expert note %d type", int(i + 1));
            c.check(ex[i].type == wantType[i], what, long(ex[i].type), long(wantType[i]));
            std::snprintf(what, sizeof(what), "expert note %d star power", int(i + 1));
            c.check(ex[i].starPower == wantSp[i], what, ex[i].starPower, wantSp[i]);
        }
        c.check(ex[0].length == 480, "note 1 sustain", ex[0].length, 480);
        c.check(ex[1].length == 0, "note 2 sustain cut", ex[1].length, 0);
        c.check(std::fabs(ex[0].timeEnd - 0.5) < 1e-9, "note 1 end = 0.5s", long(ex[0].timeEnd * 1000), 500);
        c.check(std::fabs(ex[9].time - 3.0) < 1e-9, "note 10 time = 3.0s", long(ex[9].time * 1000), 3000);
    }
    c.check(chart.get(Difficulty::Hard).size() == 1, "hard note count",
            long(chart.get(Difficulty::Hard).size()), 1);
    c.check(chart.get(Difficulty::Easy).empty(), "easy empty", long(chart.get(Difficulty::Easy).size()), 0);
    c.check(chart.starPower.size() == 1, "star power phrases", long(chart.starPower.size()), 1);

    // ---- song.ini ----
    const char ini[] =
        "\xEF\xBB\xBF[Song]\r\nName = Test Song\r\nartist=YARG PS3\r\n; comment\r\n"
        "HOPO_FREQUENCY = 170\r\ndelay=-25\r\n[other]\r\nname=ignored\r\n";
    SongIni s;
    c.check(s.parse(ini, sizeof(ini) - 1), "song.ini [song] section");
    c.check(s.name() == "Test Song", "song.ini name");
    c.check(s.artist() == "YARG PS3", "song.ini artist");
    c.check(s.hopoFrequency() == 170, "song.ini hopo_frequency", long(s.hopoFrequency()), 170);
    c.check(s.delayMs() == -25, "song.ini delay", long(s.delayMs()), -25);

    // ---- Robustness: every truncated prefix must fail cleanly, never crash ----
    int truncatedAccepted = 0;
    for (size_t len = 0; len < smf.size(); ++len) {
        MidiFile m;
        std::string e;
        if (parseMidi(smf.data(), len, m, e)) ++truncatedAccepted;
    }
    // A prefix can be "valid" only if it ends exactly after a complete earlier chunk.
    c.check(truncatedAccepted <= 1, "truncated files rejected", truncatedAccepted, 1);

    if (r.ok) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "OK (%d checks, %d expert notes)", r.checks, int(ex.size()));
        r.message = buf;
    }
    return r;
}

}  // namespace yarg
