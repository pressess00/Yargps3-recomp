// YARG PS3 — song.ini reader.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#include "yarg/song_ini.h"

#include <cstdlib>
#include <cstring>

namespace yarg {
namespace {

char lower(char c) { return (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c; }

std::string toLower(std::string s) {
    for (char& c : s) c = lower(c);
    return s;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return s.substr(b, e - b);
}

}  // namespace

bool SongIni::parse(const char* text, size_t size) {
    entries_.clear();
    size_t pos = 0;
    if (size >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        pos = 3;  // UTF-8 BOM

    bool inSong = false, sawSong = false;
    while (pos < size) {
        size_t end = pos;
        while (end < size && text[end] != '\n') ++end;
        std::string line = trim(std::string(text + pos, end - pos));
        pos = end + 1;

        if (line.empty() || line[0] == ';' || line[0] == '#' ||
            (line.size() >= 2 && line[0] == '/' && line[1] == '/'))
            continue;
        if (line[0] == '[') {
            size_t close = line.find(']');
            std::string section = toLower(trim(line.substr(1, close == std::string::npos ? std::string::npos : close - 1)));
            inSong = (section == "song");
            sawSong = sawSong || inSong;
            continue;
        }
        if (!inSong) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        Entry e{toLower(trim(line.substr(0, eq))), trim(line.substr(eq + 1))};
        if (e.key.empty()) continue;
        // Later duplicates override earlier ones.
        bool replaced = false;
        for (Entry& x : entries_)
            if (x.key == e.key) {
                x.value = e.value;
                replaced = true;
                break;
            }
        if (!replaced) entries_.push_back(std::move(e));
    }
    return sawSong;
}

const SongIni::Entry* SongIni::find(const char* key) const {
    std::string k = toLower(key);
    for (const Entry& e : entries_)
        if (e.key == k) return &e;
    return nullptr;
}

bool SongIni::has(const char* key) const { return find(key) != nullptr; }

std::string SongIni::getString(const char* key, const char* fallback) const {
    const Entry* e = find(key);
    return e ? e->value : std::string(fallback);
}

int64_t SongIni::getInt(const char* key, int64_t fallback) const {
    const Entry* e = find(key);
    if (!e || e->value.empty()) return fallback;
    char* endp = nullptr;
    long long v = std::strtoll(e->value.c_str(), &endp, 10);
    if (endp == e->value.c_str()) return fallback;
    return v;
}

bool SongIni::getBool(const char* key, bool fallback) const {
    const Entry* e = find(key);
    if (!e) return fallback;
    std::string v = toLower(e->value);
    if (v == "1" || v == "true" || v == "yes") return true;
    if (v == "0" || v == "false" || v == "no") return false;
    return fallback;
}

std::string SongIni::charter() const {
    std::string c = getString("charter");
    return c.empty() ? getString("frets") : c;
}

}  // namespace yarg
