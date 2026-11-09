// YARG PS3 — Standard MIDI File (SMF) reader.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
// Byte-level parser: endian-independent (works the same on x86 and big-endian PPU).
// No exceptions, no dynamic polymorphism — returns false + error text on bad input.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace yarg {

enum class MidiEventKind : uint8_t {
    NoteOn,      // velocity > 0
    NoteOff,     // explicit note-off or note-on with velocity 0
    Text,        // meta FF 01..0F (text, lyrics, markers...)
    TrackName,   // meta FF 03
    Tempo,       // meta FF 51, value = microseconds per quarter note
    TimeSig,     // meta FF 58, numerator/denominator(power of 2)
    SysEx,       // F0 / F7, payload in `data`
    Other        // controllers, pitch bend, other meta — kept only as a tick marker
};

struct MidiEvent {
    uint32_t tick = 0;          // absolute tick
    MidiEventKind kind = MidiEventKind::Other;
    uint8_t channel = 0;        // for channel events
    uint8_t key = 0;            // note number
    uint8_t velocity = 0;
    uint32_t value = 0;         // tempo (us/qn) or time-sig numerator
    uint8_t value2 = 0;         // time-sig denominator power
    uint8_t metaType = 0;       // for meta events
    std::string data;           // text or sysex payload
};

struct MidiTrack {
    std::string name;
    std::vector<MidiEvent> events;  // in file order (ticks non-decreasing)
};

struct MidiFile {
    uint16_t format = 0;
    uint16_t ticksPerQuarter = 480;
    std::vector<MidiTrack> tracks;
};

// Parse an in-memory .mid. Returns false and fills `error` on malformed input.
// Never reads past `size`.
bool parseMidi(const uint8_t* bytes, size_t size, MidiFile& out, std::string& error);

}  // namespace yarg
