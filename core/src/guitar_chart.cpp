// YARG PS3 — 5-fret guitar chart builder (port of YARG.Core MidReader rules).
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include "yarg/guitar_chart.h"

#include <algorithm>
#include <deque>
#include <map>

namespace yarg {
namespace {

constexpr int kDiffStart[kDifficultyCount] = {60, 72, 84, 96};  // GUITAR_DIFF_START_LOOKUP
constexpr int kTapNote = 104;                                     // MidIOHelper.TAP_NOTE_CH
constexpr int kStarPowerNote = 116;                               // MidIOHelper.STARPOWER_NOTE

struct Span {
    uint32_t start;
    uint32_t end;
};

struct RawNote {
    uint32_t tick;
    uint8_t fret;  // 0..5
    uint32_t length;
};

struct Range {  // inclusive range, phrase end tick excluded like YARG
    uint32_t first;
    uint32_t last;
    bool contains(uint32_t t) const { return t >= first && t <= last; }
};

Range toRange(const Span& s) {
    uint32_t end = s.end;
    if (end > s.start) --end;  // "Exclude the last tick of the phrase"
    return {s.start, end};
}

bool inAny(const std::vector<Range>& ranges, uint32_t tick) {
    for (const Range& r : ranges)
        if (r.contains(tick)) return true;
    return false;
}

bool isEnhancedOpensText(const std::string& s) {
    return s == "ENHANCED_OPENS" || s == "[ENHANCED_OPENS]";
}

}  // namespace

bool GuitarNote::isChord() const {
    uint8_t m = mask;
    return m != 0 && (m & (m - 1)) != 0;
}

const char* difficultyName(Difficulty d) {
    switch (d) {
        case Difficulty::Easy: return "Easy";
        case Difficulty::Medium: return "Medium";
        case Difficulty::Hard: return "Hard";
        case Difficulty::Expert: return "Expert";
    }
    return "?";
}

const char* noteTypeName(NoteType t) {
    switch (t) {
        case NoteType::Strum: return "strum";
        case NoteType::Hopo: return "hopo";
        case NoteType::Tap: return "tap";
    }
    return "?";
}

bool buildGuitarChart(const MidiFile& midi, const TempoMap& tempo, GuitarChart& out,
                      std::string& error, const GuitarParseSettings& settings,
                      const char* trackName) {
    out = GuitarChart{};
    const MidiTrack* track = nullptr;
    for (size_t i = 1; i < midi.tracks.size(); ++i) {  // track 0 is the sync track
        if (midi.tracks[i].name == trackName) {
            track = &midi.tracks[i];
            break;
        }
    }
    if (!track) {
        error = std::string("track not found: ") + trackName;
        return false;
    }

    const uint16_t res = midi.ticksPerQuarter;
    out.trackName = trackName;
    out.resolution = res;
    out.hopoThreshold = settings.hopoFrequency >= 0 ? uint32_t(settings.hopoFrequency) + 1
                                                    : uint32_t(res / 3) + 1;
    if (settings.sustainCutoffThreshold < 0)
        out.sustainCutoff = res / 3;
    else if (settings.sustainCutoffThreshold == 0)
        out.sustainCutoff = 1;
    else
        out.sustainCutoff = uint32_t(settings.sustainCutoffThreshold);

    // Pair note-on/note-off per (channel, key), first-in first-out.
    std::map<int, std::deque<uint32_t>> open;
    std::vector<std::pair<int, Span>> spans;  // key, span
    for (const MidiEvent& ev : track->events) {
        if (ev.kind == MidiEventKind::Text && isEnhancedOpensText(ev.data)) out.enhancedOpens = true;
        const int id = ev.channel * 128 + ev.key;
        if (ev.kind == MidiEventKind::NoteOn) {
            open[id].push_back(ev.tick);
        } else if (ev.kind == MidiEventKind::NoteOff) {
            auto it = open.find(id);
            if (it == open.end() || it->second.empty()) continue;  // stray note-off
            spans.push_back({ev.key, {it->second.front(), ev.tick}});
            it->second.pop_front();
        }
    }
    for (auto& kv : open)  // unterminated notes: zero length
        for (uint32_t start : kv.second) spans.push_back({kv.first % 128, {start, start}});

    std::vector<RawNote> raw[kDifficultyCount];
    std::vector<Range> forceHopo[kDifficultyCount], forceStrum[kDifficultyCount];
    std::vector<Range> taps, sp;

    for (const auto& ks : spans) {
        const int key = ks.first;
        const Span& s = ks.second;
        if (key == kTapNote) {
            taps.push_back(toRange(s));
            continue;
        }
        if (key == kStarPowerNote) {
            out.starPower.push_back({s.start, s.end});
            sp.push_back(toRange(s));
            continue;
        }
        for (int d = 0; d < kDifficultyCount; ++d) {
            const int offset = key - kDiffStart[d];
            int fret = -1;
            if (offset >= 0 && offset <= 4)
                fret = offset;
            else if (offset == -1 && out.enhancedOpens)
                fret = 5;
            else if (offset == 5)
                forceHopo[d].push_back(toRange(s));
            else if (offset == 6)
                forceStrum[d].push_back(toRange(s));
            if (fret < 0) continue;
            uint32_t len = s.end - s.start;
            if (len < out.sustainCutoff) len = 0;
            raw[d].push_back({s.start, uint8_t(fret), len});
        }
    }
    // Note: the ENHANCED_OPENS flag is collected during the event pass above, before any span is
    // turned into a fret, so its position in the track does not matter.

    std::sort(out.starPower.begin(), out.starPower.end(),
              [](const StarPowerPhrase& a, const StarPowerPhrase& b) { return a.startTick < b.startTick; });

    for (int d = 0; d < kDifficultyCount; ++d) {
        std::vector<RawNote>& rn = raw[d];
        std::sort(rn.begin(), rn.end(), [](const RawNote& a, const RawNote& b) {
            return a.tick != b.tick ? a.tick < b.tick : a.fret < b.fret;
        });

        std::vector<GuitarNote>& notes = out.notes[d];
        for (size_t i = 0; i < rn.size();) {
            GuitarNote n;
            n.tick = rn[i].tick;
            size_t j = i;
            for (; j < rn.size() && rn[j].tick == n.tick; ++j) {
                const RawNote& r = rn[j];
                n.mask |= uint8_t(1u << r.fret);
                n.fretLength[r.fret] = std::max(n.fretLength[r.fret], r.length);
                n.length = std::max(n.length, r.length);
            }
            notes.push_back(n);
            i = j;
        }

        for (size_t i = 0; i < notes.size(); ++i) {
            GuitarNote& n = notes[i];
            // Natural HOPO — MoonNote.IsNaturalHopo(threshold, usesMidiNaturalHopoSubsetException=true).
            bool natural = false;
            if (!n.isChord() && i > 0) {
                const GuitarNote& prev = notes[i - 1];
                bool fretOk;
                if (!prev.isChord()) {
                    fretOk = prev.mask != n.mask;
                } else {
                    fretOk = (prev.mask & n.mask) == 0;  // subset exception
                }
                natural = fretOk && (n.tick - prev.tick) <= out.hopoThreshold;
            }

            if (inAny(taps, n.tick))
                n.type = NoteType::Tap;  // tap overrides all other forcing
            else if (inAny(forceHopo[d], n.tick))
                n.type = NoteType::Hopo;  // forced HOPO wins over forced strum (YARG skips strum then)
            else if (inAny(forceStrum[d], n.tick))
                n.type = NoteType::Strum;
            else
                n.type = natural ? NoteType::Hopo : NoteType::Strum;

            n.starPower = inAny(sp, n.tick);
            n.time = tempo.tickToSeconds(n.tick);
            n.timeEnd = tempo.tickToSeconds(n.tick + n.length);
        }
    }
    return true;
}

}  // namespace yarg
