// YARG PS3 — tempo map (ticks <-> seconds).
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#pragma once

#include <cstdint>
#include <vector>

#include "yarg/midi.h"

namespace yarg {

struct TempoChange {
    uint32_t tick;
    uint32_t usPerQuarter;  // microseconds per quarter note
    double seconds;         // absolute time at `tick`
};

struct TimeSignature {
    uint32_t tick;
    uint8_t numerator;
    uint8_t denominator;  // actual denominator (4, 8, ...)
};

class TempoMap {
public:
    // Builds from the first track (SMF format 1 sync track); default 120 BPM.
    void build(const MidiFile& midi);

    double tickToSeconds(uint32_t tick) const;
    uint32_t secondsToTick(double seconds) const;

    uint16_t resolution() const { return resolution_; }
    const std::vector<TempoChange>& tempos() const { return tempos_; }
    const std::vector<TimeSignature>& timeSignatures() const { return timeSigs_; }

private:
    uint16_t resolution_ = 480;
    std::vector<TempoChange> tempos_;
    std::vector<TimeSignature> timeSigs_;
};

}  // namespace yarg
