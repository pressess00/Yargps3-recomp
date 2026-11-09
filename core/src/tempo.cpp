// YARG PS3 — tempo map.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include "yarg/tempo.h"

#include <algorithm>

namespace yarg {

void TempoMap::build(const MidiFile& midi) {
    resolution_ = midi.ticksPerQuarter ? midi.ticksPerQuarter : 480;
    tempos_.clear();
    timeSigs_.clear();

    std::vector<std::pair<uint32_t, uint32_t>> raw;  // tick, us/qn
    if (!midi.tracks.empty()) {
        for (const MidiEvent& ev : midi.tracks[0].events) {
            if (ev.kind == MidiEventKind::Tempo && ev.value > 0) {
                raw.emplace_back(ev.tick, ev.value);
            } else if (ev.kind == MidiEventKind::TimeSig && ev.value > 0 && ev.value2 < 8) {
                timeSigs_.push_back({ev.tick, static_cast<uint8_t>(ev.value),
                                     static_cast<uint8_t>(1u << ev.value2)});
            }
        }
    }
    // Stable: for several tempos on one tick, the last one wins.
    std::stable_sort(raw.begin(), raw.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

    tempos_.push_back({0, 500000, 0.0});  // SMF default: 120 BPM
    for (const auto& t : raw) {
        TempoChange& last = tempos_.back();
        if (t.first == last.tick) {
            last.usPerQuarter = t.second;
            continue;
        }
        const double secs =
            last.seconds + double(t.first - last.tick) * last.usPerQuarter / (1e6 * resolution_);
        tempos_.push_back({t.first, t.second, secs});
    }
    if (timeSigs_.empty()) timeSigs_.push_back({0, 4, 4});
}

double TempoMap::tickToSeconds(uint32_t tick) const {
    if (tempos_.empty()) return double(tick) * 0.5 / resolution_;
    auto it = std::upper_bound(tempos_.begin(), tempos_.end(), tick,
                               [](uint32_t t, const TempoChange& c) { return t < c.tick; });
    const TempoChange& seg = *(it - 1);
    return seg.seconds + double(tick - seg.tick) * seg.usPerQuarter / (1e6 * resolution_);
}

uint32_t TempoMap::secondsToTick(double seconds) const {
    if (seconds <= 0.0 || tempos_.empty()) return 0;
    auto it = std::upper_bound(tempos_.begin(), tempos_.end(), seconds,
                               [](double s, const TempoChange& c) { return s < c.seconds; });
    const TempoChange& seg = *(it - 1);
    const double ticks = (seconds - seg.seconds) * 1e6 * resolution_ / seg.usPerQuarter;
    return seg.tick + static_cast<uint32_t>(ticks + 1e-6);
}

}  // namespace yarg
