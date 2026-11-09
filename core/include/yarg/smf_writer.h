// YARG PS3 — tiny SMF writer used by tests and the on-console self-test.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace yarg {

class SmfTrackBuilder {
public:
    void name(const std::string& n) { meta(0, 0x03, n); }
    void text(uint32_t tick, const std::string& t) { meta(tick, 0x01, t); }
    void tempo(uint32_t tick, uint32_t usPerQuarter);
    void timeSig(uint32_t tick, uint8_t num, uint8_t denomPow2);
    void note(uint32_t tick, uint32_t length, uint8_t key, uint8_t velocity = 100, uint8_t channel = 0);

    // Serialize the MTrk chunk. `useRunningStatus` exercises the reader's running-status path.
    std::vector<uint8_t> build(bool useRunningStatus = false) const;

private:
    struct Ev {
        uint32_t tick;
        int order;  // note-offs before note-ons on the same tick
        std::vector<uint8_t> bytes;
    };
    void meta(uint32_t tick, uint8_t type, const std::string& payload);
    std::vector<Ev> events_;
};

std::vector<uint8_t> buildSmf(uint16_t ticksPerQuarter, const std::vector<std::vector<uint8_t>>& tracks);

}  // namespace yarg
