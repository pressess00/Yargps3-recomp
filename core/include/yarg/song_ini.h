// YARG PS3 — song.ini reader (Clone Hero / YARG format).
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
// Keys are case-insensitive; only the [song] section is read (also [Song]).
// Key names follow YARG.Core IO/Ini/SongIniHandler.cs.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace yarg {

class SongIni {
public:
    bool parse(const char* text, size_t size);  // false only if no [song] section found

    bool has(const char* key) const;
    std::string getString(const char* key, const char* fallback = "") const;
    int64_t getInt(const char* key, int64_t fallback) const;
    bool getBool(const char* key, bool fallback) const;

    // Convenience accessors used by the song browser.
    std::string name() const { return getString("name"); }
    std::string artist() const { return getString("artist"); }
    std::string album() const { return getString("album"); }
    std::string genre() const { return getString("genre"); }
    std::string charter() const;  // "charter", falls back to "frets"
    int64_t songLengthMs() const { return getInt("song_length", -1); }
    int64_t delayMs() const { return getInt("delay", 0); }
    int64_t hopoFrequency() const { return getInt("hopo_frequency", -1); }
    int64_t sustainCutoff() const { return getInt("sustain_cutoff_threshold", -1); }
    int64_t guitarDifficulty() const { return getInt("diff_guitar", -1); }

    size_t size() const { return entries_.size(); }

private:
    struct Entry {
        std::string key;  // lower-case
        std::string value;
    };
    const Entry* find(const char* key) const;
    std::vector<Entry> entries_;
};

}  // namespace yarg
