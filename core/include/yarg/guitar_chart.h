// YARG PS3 — 5-fret guitar chart built from a .mid file.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
// Rules ported from YARG.Core (LGPL-3.0), MoonscraperChartParser:
//   - MidIOHelper.GUITAR_DIFF_START_LOOKUP: Easy 60, Medium 72, Hard 84, Expert 96
//   - frets: base+0..4 = Green..Orange, base+5 forced HOPO, base+6 forced strum
//   - base-1 = open note only when the "[ENHANCED_OPENS]"/"ENHANCED_OPENS" text event is present
//   - note 104 = tap, note 116 = star power, phrase end tick excluded
//   - MidReader: hopoThreshold = settings > default ? value+1 : resolution/3 + 1
//   - MidReader: sustain shorter than cutoff (default resolution/3) becomes 0
//   - MoonNote.IsNaturalHopo with UsesMidiNaturalHopoSubsetException = true
// Not ported yet: Phase Shift sysex opens/taps, solos, GH1 "T1 GEMS" quirks.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "yarg/midi.h"
#include "yarg/tempo.h"

namespace yarg {

enum class Difficulty : uint8_t { Easy = 0, Medium = 1, Hard = 2, Expert = 3 };
constexpr int kDifficultyCount = 4;

enum class NoteType : uint8_t { Strum, Hopo, Tap };

// Fret bit layout matches MoonNote.GuitarFret: Green=0 ... Orange=4, Open=5.
enum FretBit : uint8_t {
    kGreen = 1 << 0,
    kRed = 1 << 1,
    kYellow = 1 << 2,
    kBlue = 1 << 3,
    kOrange = 1 << 4,
    kOpen = 1 << 5,
};

struct GuitarNote {
    uint32_t tick = 0;
    uint32_t length = 0;          // longest sustain in the chord (ticks, after cutoff)
    uint32_t fretLength[6] = {};  // per-fret sustain (disjoint chords)
    uint8_t mask = 0;             // FretBit set
    NoteType type = NoteType::Strum;
    bool starPower = false;
    double time = 0.0;     // seconds
    double timeEnd = 0.0;  // seconds (== time when no sustain)

    bool isChord() const;
};

struct StarPowerPhrase {
    uint32_t startTick;
    uint32_t endTick;  // exclusive
};

struct GuitarParseSettings {
    int64_t hopoFrequency = -1;           // song.ini hopo_frequency (ticks), -1 = default
    int64_t sustainCutoffThreshold = -1;  // song.ini sustain_cutoff_threshold, -1 = default
};

struct GuitarChart {
    std::string trackName;
    uint16_t resolution = 480;
    uint32_t hopoThreshold = 0;
    uint32_t sustainCutoff = 0;
    bool enhancedOpens = false;
    std::vector<GuitarNote> notes[kDifficultyCount];
    std::vector<StarPowerPhrase> starPower;

    const std::vector<GuitarNote>& get(Difficulty d) const { return notes[int(d)]; }
};

// Builds the chart for `trackName` (default "PART GUITAR").
// Returns false if the track is missing.
bool buildGuitarChart(const MidiFile& midi, const TempoMap& tempo, GuitarChart& out,
                      std::string& error, const GuitarParseSettings& settings = {},
                      const char* trackName = "PART GUITAR");

const char* difficultyName(Difficulty d);
const char* noteTypeName(NoteType t);

}  // namespace yarg
