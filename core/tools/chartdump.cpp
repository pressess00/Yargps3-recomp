// YARG PS3 — chartdump: print the parsed guitar chart of a .mid as plain text.
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
//   chartdump notes.mid [song.ini|-] [Expert|Hard|Medium|Easy]   human-readable dump
//   chartdump --golden notes.mid [Expert|Hard|Medium|Easy]       format shared with
//                                                                tools/golden_cs (YARG.Core)
// Golden format, one note/chord per line: tick<TAB>frets<TAB>type<TAB>length<TAB>sp
//   frets  = letters in order G R Y B O o (o = open), type = strum|hopo|tap, sp = 0|1
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "yarg/guitar_chart.h"
#include "yarg/midi.h"
#include "yarg/song_ini.h"
#include "yarg/tempo.h"

static bool readFile(const char* path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

static yarg::Difficulty parseDiff(const char* s) {
    for (int d = 0; d < yarg::kDifficultyCount; ++d)
        if (std::strcmp(s, yarg::difficultyName(yarg::Difficulty(d))) == 0) return yarg::Difficulty(d);
    return yarg::Difficulty::Expert;
}

int main(int argc, char** argv) {
    bool golden = argc > 1 && std::strcmp(argv[1], "--golden") == 0;
    int a = golden ? 2 : 1;
    if (argc <= a) {
        std::fprintf(stderr,
                     "usage: %s notes.mid [song.ini|-] [Expert|Hard|Medium|Easy]\n"
                     "       %s --golden notes.mid [Expert|Hard|Medium|Easy]\n",
                     argv[0], argv[0]);
        return 2;
    }
    const char* midPath = argv[a];
    std::vector<uint8_t> data;
    if (!readFile(midPath, data)) {
        std::fprintf(stderr, "cannot read %s\n", midPath);
        return 1;
    }

    yarg::GuitarParseSettings settings;
    yarg::Difficulty diff = yarg::Difficulty::Expert;
    if (golden) {
        if (argc > a + 1) diff = parseDiff(argv[a + 1]);
    } else {
        if (argc > a + 1 && std::strcmp(argv[a + 1], "-") != 0) {
            std::vector<uint8_t> ini;
            yarg::SongIni s;
            if (readFile(argv[a + 1], ini) && s.parse(reinterpret_cast<const char*>(ini.data()), ini.size())) {
                settings.hopoFrequency = s.hopoFrequency();
                settings.sustainCutoffThreshold = s.sustainCutoff();
                std::printf("# song: %s - %s\n", s.artist().c_str(), s.name().c_str());
            }
        }
        if (argc > a + 2) diff = parseDiff(argv[a + 2]);
    }

    yarg::MidiFile midi;
    std::string err;
    if (!yarg::parseMidi(data.data(), data.size(), midi, err)) {
        std::fprintf(stderr, "MIDI error: %s\n", err.c_str());
        return 1;
    }
    yarg::TempoMap tempo;
    tempo.build(midi);
    yarg::GuitarChart chart;
    if (!yarg::buildGuitarChart(midi, tempo, chart, err, settings)) {
        std::fprintf(stderr, "chart error: %s\n", err.c_str());
        return 1;
    }

    static const char* names = "GRYBOo";
    if (!golden) {
        std::printf("# resolution=%u hopo_threshold=%u sustain_cutoff=%u difficulty=%s\n", chart.resolution,
                    chart.hopoThreshold, chart.sustainCutoff, yarg::difficultyName(diff));
        std::printf("# tick\ttime_ms\tfrets\ttype\tlength\tsp\n");
    }
    for (const auto& n : chart.get(diff)) {
        char frets[8] = {};
        int k = 0;
        for (int b = 0; b < 6; ++b)
            if (n.mask & (1 << b)) frets[k++] = names[b];
        if (golden)
            std::printf("%u\t%s\t%s\t%u\t%d\n", n.tick, frets, yarg::noteTypeName(n.type), n.length,
                        n.starPower ? 1 : 0);
        else
            std::printf("%u\t%.3f\t%s\t%s\t%u\t%d\n", n.tick, n.time * 1000.0, frets, yarg::noteTypeName(n.type),
                        n.length, n.starPower ? 1 : 0);
    }
    return 0;
}
