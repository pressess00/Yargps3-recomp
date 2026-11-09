// YARG PS3 — tiny SMF writer.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include "yarg/smf_writer.h"

#include <algorithm>

namespace yarg {
namespace {

void putVlq(std::vector<uint8_t>& out, uint32_t v) {
    uint8_t tmp[5];
    int n = 0;
    tmp[n++] = v & 0x7F;
    while (v >>= 7) tmp[n++] = uint8_t(0x80 | (v & 0x7F));
    while (n--) out.push_back(tmp[n]);
}

void putBe32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(uint8_t(v >> 24));
    out.push_back(uint8_t(v >> 16));
    out.push_back(uint8_t(v >> 8));
    out.push_back(uint8_t(v));
}

}  // namespace

void SmfTrackBuilder::meta(uint32_t tick, uint8_t type, const std::string& payload) {
    Ev e{tick, 1, {0xFF, type}};
    putVlq(e.bytes, uint32_t(payload.size()));
    e.bytes.insert(e.bytes.end(), payload.begin(), payload.end());
    events_.push_back(std::move(e));
}

void SmfTrackBuilder::tempo(uint32_t tick, uint32_t us) {
    events_.push_back({tick, 1, {0xFF, 0x51, 0x03, uint8_t(us >> 16), uint8_t(us >> 8), uint8_t(us)}});
}

void SmfTrackBuilder::timeSig(uint32_t tick, uint8_t num, uint8_t denomPow2) {
    events_.push_back({tick, 1, {0xFF, 0x58, 0x04, num, denomPow2, 24, 8}});
}

void SmfTrackBuilder::note(uint32_t tick, uint32_t length, uint8_t key, uint8_t velocity, uint8_t ch) {
    events_.push_back({tick, 2, {uint8_t(0x90 | ch), key, velocity}});
    // Alternate note-off encodings: real 0x80 and note-on velocity 0.
    if (key % 2)
        events_.push_back({tick + length, 0, {uint8_t(0x80 | ch), key, 0x40}});
    else
        events_.push_back({tick + length, 0, {uint8_t(0x90 | ch), key, 0}});
}

std::vector<uint8_t> SmfTrackBuilder::build(bool useRunningStatus) const {
    std::vector<Ev> evs = events_;
    std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
        return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });
    std::vector<uint8_t> body;
    uint32_t last = 0;
    uint8_t running = 0;
    for (const Ev& e : evs) {
        putVlq(body, e.tick - last);
        last = e.tick;
        const uint8_t status = e.bytes[0];
        if (useRunningStatus && status < 0xF0 && status == running) {
            body.insert(body.end(), e.bytes.begin() + 1, e.bytes.end());
        } else {
            body.insert(body.end(), e.bytes.begin(), e.bytes.end());
            running = status < 0xF0 ? status : 0;
        }
    }
    body.insert(body.end(), {0x00, 0xFF, 0x2F, 0x00});

    std::vector<uint8_t> chunk = {'M', 'T', 'r', 'k'};
    putBe32(chunk, uint32_t(body.size()));
    chunk.insert(chunk.end(), body.begin(), body.end());
    return chunk;
}

std::vector<uint8_t> buildSmf(uint16_t tpq, const std::vector<std::vector<uint8_t>>& tracks) {
    std::vector<uint8_t> out = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1};
    out.push_back(uint8_t(tracks.size() >> 8));
    out.push_back(uint8_t(tracks.size()));
    out.push_back(uint8_t(tpq >> 8));
    out.push_back(uint8_t(tpq));
    for (const auto& t : tracks) out.insert(out.end(), t.begin(), t.end());
    return out;
}

}  // namespace yarg
