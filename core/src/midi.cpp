// YARG PS3 — Standard MIDI File reader.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include "yarg/midi.h"

#include <cstdio>

namespace yarg {
namespace {

struct Reader {
    const uint8_t* p;
    size_t size;
    size_t pos = 0;

    bool has(size_t n) const { return pos <= size && size - pos >= n; }
    bool u8(uint8_t& v) {
        if (!has(1)) return false;
        v = p[pos++];
        return true;
    }
    bool be16(uint16_t& v) {
        if (!has(2)) return false;
        v = static_cast<uint16_t>((p[pos] << 8) | p[pos + 1]);
        pos += 2;
        return true;
    }
    bool be32(uint32_t& v) {
        if (!has(4)) return false;
        v = (uint32_t(p[pos]) << 24) | (uint32_t(p[pos + 1]) << 16) | (uint32_t(p[pos + 2]) << 8) |
            uint32_t(p[pos + 3]);
        pos += 4;
        return true;
    }
    // MIDI variable-length quantity, max 4 bytes (28 bits).
    bool vlq(uint32_t& v) {
        v = 0;
        for (int i = 0; i < 4; ++i) {
            uint8_t b;
            if (!u8(b)) return false;
            v = (v << 7) | (b & 0x7F);
            if ((b & 0x80) == 0) return true;
        }
        return false;  // too long
    }
};

bool fail(std::string& error, const char* msg, size_t offset) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s (offset %lu)", msg, static_cast<unsigned long>(offset));
    error = buf;
    return false;
}

bool parseTrack(const uint8_t* data, size_t len, size_t baseOffset, MidiTrack& track,
                std::string& error) {
    Reader r{data, len};
    uint32_t tick = 0;
    uint8_t running = 0;

    while (r.pos < len) {
        uint32_t delta;
        if (!r.vlq(delta)) return fail(error, "bad delta time", baseOffset + r.pos);
        tick += delta;

        uint8_t status;
        if (!r.u8(status)) return fail(error, "truncated event", baseOffset + r.pos);

        MidiEvent ev;
        ev.tick = tick;

        if (status == 0xFF) {  // meta
            uint8_t type;
            uint32_t mlen;
            if (!r.u8(type) || !r.vlq(mlen) || !r.has(mlen))
                return fail(error, "truncated meta event", baseOffset + r.pos);
            const uint8_t* m = data + r.pos;
            ev.metaType = type;
            if (type == 0x2F) {  // end of track
                r.pos += mlen;
                break;
            }
            if (type == 0x51 && mlen == 3) {
                ev.kind = MidiEventKind::Tempo;
                ev.value = (uint32_t(m[0]) << 16) | (uint32_t(m[1]) << 8) | m[2];
            } else if (type == 0x58 && mlen >= 2) {
                ev.kind = MidiEventKind::TimeSig;
                ev.value = m[0];
                ev.value2 = m[1];
            } else if (type >= 0x01 && type <= 0x0F) {
                ev.kind = (type == 0x03) ? MidiEventKind::TrackName : MidiEventKind::Text;
                ev.data.assign(reinterpret_cast<const char*>(m), mlen);
                if (type == 0x03 && track.name.empty()) track.name = ev.data;
            } else {
                ev.kind = MidiEventKind::Other;
            }
            r.pos += mlen;
            // Meta and sysex events cancel running status per SMF spec (most readers allow it).
            track.events.push_back(std::move(ev));
            continue;
        }

        if (status == 0xF0 || status == 0xF7) {  // sysex
            uint32_t slen;
            if (!r.vlq(slen) || !r.has(slen))
                return fail(error, "truncated sysex", baseOffset + r.pos);
            ev.kind = MidiEventKind::SysEx;
            ev.data.assign(reinterpret_cast<const char*>(data + r.pos), slen);
            r.pos += slen;
            running = 0;
            track.events.push_back(std::move(ev));
            continue;
        }

        uint8_t d1;
        if (status & 0x80) {
            if (status >= 0xF0) return fail(error, "unsupported system event", baseOffset + r.pos);
            running = status;
            if (!r.u8(d1)) return fail(error, "truncated channel event", baseOffset + r.pos);
        } else {
            if (running == 0) return fail(error, "running status without status", baseOffset + r.pos);
            d1 = status;  // first data byte already read
            status = running;
        }

        const uint8_t hi = status & 0xF0;
        ev.channel = status & 0x0F;
        const bool twoBytes = !(hi == 0xC0 || hi == 0xD0);
        uint8_t d2 = 0;
        if (twoBytes && !r.u8(d2)) return fail(error, "truncated channel event", baseOffset + r.pos);

        if (hi == 0x90 && d2 > 0) {
            ev.kind = MidiEventKind::NoteOn;
            ev.key = d1 & 0x7F;
            ev.velocity = d2 & 0x7F;
        } else if (hi == 0x80 || hi == 0x90) {
            ev.kind = MidiEventKind::NoteOff;
            ev.key = d1 & 0x7F;
            ev.velocity = d2 & 0x7F;
        } else {
            ev.kind = MidiEventKind::Other;
        }
        track.events.push_back(std::move(ev));
    }
    return true;
}

}  // namespace

bool parseMidi(const uint8_t* bytes, size_t size, MidiFile& out, std::string& error) {
    out = MidiFile{};
    Reader r{bytes, size};

    uint32_t magic, hlen;
    if (!r.be32(magic) || magic != 0x4D546864u)  // "MThd"
        return fail(error, "not a MIDI file (missing MThd)", 0);
    if (!r.be32(hlen) || hlen < 6 || !r.has(hlen)) return fail(error, "bad header length", 4);

    uint16_t format, ntracks, division;
    r.be16(format);
    r.be16(ntracks);
    r.be16(division);
    r.pos += hlen - 6;

    if (division & 0x8000) return fail(error, "SMPTE time division not supported", 12);
    if (division == 0) return fail(error, "zero ticks per quarter note", 12);
    out.format = format;
    out.ticksPerQuarter = division;

    while (r.pos < size && out.tracks.size() < ntracks) {
        uint32_t id, clen;
        if (!r.be32(id) || !r.be32(clen)) return fail(error, "truncated chunk header", r.pos);
        if (!r.has(clen)) return fail(error, "truncated chunk", r.pos);
        if (id == 0x4D54726Bu) {  // "MTrk"
            MidiTrack track;
            if (!parseTrack(bytes + r.pos, clen, r.pos, track, error)) return false;
            out.tracks.push_back(std::move(track));
        }
        r.pos += clen;  // unknown chunks are skipped
    }
    if (out.tracks.empty()) return fail(error, "MIDI file has no tracks", r.pos);
    return true;
}

}  // namespace yarg
