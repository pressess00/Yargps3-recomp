// YARG PS3 — BOOT TEST (milestone 1).
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
// Proves on real hardware / RPCS3 that: the EBOOT starts, RSX shows a framebuffer, the heap
// works, the HDD can be written/read/listed, pads (and a GH guitar dongle) are readable, and the
// portable YARG core gives identical results on the big-endian PPU. Exits cleanly on START or
// "Quit Game" from the XMB. Writes a log to <USRDIR>/boot_test.log.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <io/pad.h>
#include <lv2/systime.h>
#include <sys/file.h>
#include <sys/process.h>
#include <sys/systime.h>
#include <sysutil/sysutil.h>

#include "boot_screen.h"
#include "display.h"
#include "yarg/selftest.h"
#include "yarg/song_ini.h"

SYS_PROCESS_PARAM(1001, 0x100000);

namespace {

const char* kFallbackRoot = "/dev_hdd0/game/YARGPS300/USRDIR";
const int kMaxPorts = 7;

volatile bool g_running = true;

void onSysutilEvent(u64 status, u64 /*param*/, void* /*user*/) {
    if (status == SYSUTIL_EXIT_GAME) g_running = false;
}

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return buf;
}

// ---------- PPU ----------
void checkPpu(CheckLine& l) {
    const uint32_t probe = 0x01020304;
    const bool bigEndian = *reinterpret_cast<const uint8_t*>(&probe) == 0x01;
    const u64 tb = sysGetTimebaseFrequency();
    l.state = bigEndian ? CheckState::Ok : CheckState::Fail;
    l.detail = fmt("%s, timebase %.1f MHz", bigEndian ? "big-endian" : "LITTLE-ENDIAN?!", double(tb) / 1e6);
}

// ---------- MEMORY ----------
void checkMemory(CheckLine& l, const DisplayInfo& d) {
    const size_t kHeapTest = 64u * 1024 * 1024;
    uint8_t* p = static_cast<uint8_t*>(malloc(kHeapTest));
    bool ok = p != nullptr;
    if (ok) {
        for (size_t i = 0; i < kHeapTest; i += 4096) p[i] = uint8_t(i >> 12);
        for (size_t i = 0; i < kHeapTest && ok; i += 4096) ok = p[i] == uint8_t(i >> 12);
        free(p);
    }
    l.state = ok ? CheckState::Ok : CheckState::Fail;
    l.detail = fmt("heap 64 MB %s, io %u MB, fb 2x%.1f MB", ok ? "ok" : "FAILED", d.hostBytes >> 20,
                   d.framebufferBytes / 1048576.0);
}

// ---------- HDD ----------
std::string dataRootFrom(const char* argv0) {
    // When started from the XMB, argv[0] is the path of EBOOT.BIN inside USRDIR.
    if (argv0 && strncmp(argv0, "/dev_hdd0/game/", 15) == 0) {
        std::string p(argv0);
        size_t slash = p.rfind('/');
        if (slash != std::string::npos) return p.substr(0, slash);
    }
    return kFallbackRoot;
}

bool isDirectory(const std::string& path) {
    s32 fd;
    if (sysLv2FsOpenDir(path.c_str(), &fd) != 0) return false;
    sysLv2FsCloseDir(fd);
    return true;
}

bool fileExists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

void checkHdd(CheckLine& l, const std::string& root, std::string& songsLine) {
    static const char* dirs[] = {"config", "scores", "cache", "songs"};
    for (const char* d : dirs) sysLv2FsMkdir((root + "/" + d).c_str(), 0755);  // EEXIST is fine

    const std::string probePath = root + "/config/boot_test.tmp";
    const std::string payload = fmt("YARGPS3 boot test %lld", (long long)sysGetSystemTime());
    bool writeOk = false, readOk = false;
    if (FILE* f = fopen(probePath.c_str(), "wb")) {
        writeOk = fwrite(payload.data(), 1, payload.size(), f) == payload.size();
        fclose(f);
    }
    if (FILE* f = fopen(probePath.c_str(), "rb")) {
        char buf[128] = {};
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
        readOk = n == payload.size() && memcmp(buf, payload.data(), n) == 0;
    }
    sysLv2FsUnlink(probePath.c_str());

    // Directory iteration through the lv2 syscalls (PSL1GHT README: not available via newlib).
    int folders = 0, charts = 0;
    std::string first, firstTitle;
    bool listOk = false;
    s32 fd;
    const std::string songs = root + "/songs";
    if (sysLv2FsOpenDir(songs.c_str(), &fd) == 0) {
        listOk = true;
        sysFSDirent ent;
        u64 read = 0;
        while (sysLv2FsReadDir(fd, &ent, &read) == 0 && read > 0) {
            std::string name(ent.d_name, ent.d_namlen);
            if (name == "." || name == "..") continue;
            const std::string path = songs + "/" + name;
            if (!isDirectory(path)) continue;
            ++folders;
            if (fileExists(path + "/notes.mid") || fileExists(path + "/notes.chart")) ++charts;
            if (first.empty()) {
                first = name;
                if (FILE* f = fopen((path + "/song.ini").c_str(), "rb")) {
                    char ini[4096];
                    size_t n = fread(ini, 1, sizeof(ini), f);
                    fclose(f);
                    yarg::SongIni s;
                    if (s.parse(ini, n)) firstTitle = s.artist() + " - " + s.name();
                }
            }
        }
        sysLv2FsCloseDir(fd);
    }

    const bool ok = writeOk && readOk && listOk;
    l.state = ok ? CheckState::Ok : CheckState::Fail;
    l.detail = fmt("write %s, read %s, list %s", writeOk ? "ok" : "FAIL", readOk ? "ok" : "FAIL",
                   listOk ? "ok" : "FAIL");
    songsLine = fmt("SONGS: %d folders, %d with notes.mid/.chart", folders, charts);
    if (!first.empty()) songsLine += ", first: " + (firstTitle.empty() ? first : firstTitle);
}

// ---------- PADS ----------
struct PadState {
    bool connected = false;
    u16 vid = 0, pid = 0;
    padData data;
};

bool isGhGuitar(const PadState& p) { return p.vid == 0x12BA && p.pid == 0x0100; }  // PlasticBand docs

void pollPads(PadState (&pads)[kMaxPorts], padInfo& info) {
    ioPadGetInfo(&info);
    for (int i = 0; i < kMaxPorts; ++i) {
        PadState& p = pads[i];
        p.connected = info.status[i] != 0;
        if (!p.connected) {
            memset(&p.data, 0, sizeof(p.data));
            continue;
        }
        p.vid = info.vendor_id[i];
        p.pid = info.product_id[i];
        padData d;
        // ioPadGetData only fills `d` when the input changed; len == 0 means "no change".
        if (ioPadGetData(i, &d) == 0 && d.len > 0) p.data = d;
    }
}

std::string padLine(int port, const padData& d) {
    return fmt("PAD%d: %s %s %s %s %s %s %s %s %s %s R(%u,%u)", port, d.BTN_CROSS ? "X" : ".",
               d.BTN_CIRCLE ? "O" : ".", d.BTN_SQUARE ? "[]" : ".", d.BTN_TRIANGLE ? "/\\" : ".",
               d.BTN_L1 ? "L1" : ".", d.BTN_R1 ? "R1" : ".", d.BTN_UP ? "UP" : ".", d.BTN_DOWN ? "DN" : ".",
               d.BTN_START ? "STA" : ".", d.BTN_SELECT ? "SEL" : ".", unsigned(d.ANA_R_H), unsigned(d.ANA_R_V));
}

// Mapping from PlasticBand "PS3 Guitar Hero Guitars": G=cross R=circle Y=square B=triangle O=L1,
// strum = d-pad up/down, whammy = right stick X, star power pedal = R1. TO VERIFY on hardware.
std::string guitarLine(const padData& d) {
    return fmt("GUITAR: [%c][%c][%c][%c][%c] STRUM:%s WHAMMY:%u TILT:%u", d.BTN_CROSS ? 'G' : ' ',
               d.BTN_CIRCLE ? 'R' : ' ', d.BTN_SQUARE ? 'Y' : ' ', d.BTN_TRIANGLE ? 'B' : ' ',
               d.BTN_L1 ? 'O' : ' ', d.BTN_UP ? "UP" : d.BTN_DOWN ? "DN" : "--", unsigned(d.ANA_R_H),
               unsigned(d.SENSOR_X));
}

void writeLog(const std::string& root, const BootStatus& s, const char* phase) {
    FILE* f = fopen((root + "/boot_test.log").c_str(), "a");
    if (!f) return;
    fprintf(f, "=== YARG PS3 BOOT TEST: %s (t=%lld us) ===\n", phase, (long long)sysGetSystemTime());
    for (const CheckLine& l : s.checks) {
        const char* st = l.state == CheckState::Ok ? "OK" : l.state == CheckState::Fail ? "FAIL" : "WAIT";
        fprintf(f, "%s: %s  %s\n", l.label, st, l.detail.c_str());
    }
    fprintf(f, "%s\n%s\n%s\nvideo %s, %.1f fps, draw %.2f ms, frames %u\n\n", s.padLine.c_str(),
            s.guitarLine.c_str(), s.songsLine.c_str(), s.resolution.c_str(), s.fps, s.frameMs, s.frame);
    fclose(f);
}

}  // namespace

int main(int argc, const char* argv[]) {
    printf("YARG PS3 BOOT TEST starting\n");  // goes to the lv2 TTY

    BootStatus s;
    initBootStatus(s);
    checkPpu(s.checks[kPpu]);

    DisplayInfo disp;
    const char* err = "";
    if (!displayInit(disp, &err)) {
        // Nothing can be shown without RSX; leave a trace on the TTY and quit cleanly.
        printf("RSX init failed: %s\n", err);
        return 1;
    }
    s.checks[kRsx].state = CheckState::Ok;
    s.checks[kRsx].detail = fmt("%ux%u XRGB, 2 buffers, vsync", disp.width, disp.height);
    s.resolution = fmt("%ux%u", disp.width, disp.height);

    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, onSysutilEvent, NULL);
    ioPadInit(kMaxPorts);

    s.checks[kController].state = CheckState::Wait;
    s.checks[kController].detail = "connect a controller";
    s.checks[kCore].detail = "running...";
    s.dataRoot = dataRootFrom(argc > 0 ? argv[0] : nullptr);

    PadState pads[kMaxPorts];
    padInfo info;
    memset(&info, 0, sizeof(info));
    s64 fpsWindowStart = sysGetSystemTime();
    unsigned fpsFrames = 0;
    bool slowChecksDone = false, startWasDown = false, startArmed = false;

    while (g_running) {
        sysUtilCheckCallback();

        pollPads(pads, info);
        int firstPort = -1, guitarPort = -1;
        for (int i = 0; i < kMaxPorts; ++i) {
            if (!pads[i].connected) continue;
            if (firstPort < 0) firstPort = i;
            if (guitarPort < 0 && isGhGuitar(pads[i])) guitarPort = i;
        }
        if (firstPort >= 0) {
            const PadState& p = pads[guitarPort >= 0 ? guitarPort : firstPort];
            const int port = guitarPort >= 0 ? guitarPort : firstPort;
            s.checks[kController].state = CheckState::Ok;
            s.checks[kController].detail =
                fmt("%u connected, port %d %04X:%04X%s", unsigned(info.connected), port, p.vid, p.pid,
                    guitarPort >= 0 ? " GUITAR" : "");
            s.padLine = padLine(port, p.data);
            s.guitarLine = guitarPort >= 0 ? guitarLine(p.data) : "";
        } else {
            s.checks[kController].state = CheckState::Wait;
            s.checks[kController].detail = "connect a controller";
            s.padLine.clear();
            s.guitarLine.clear();
        }

        // START on any pad exits. Require a release first so a START held at launch is ignored.
        bool startDown = false;
        for (const PadState& p : pads)
            if (p.connected && p.data.BTN_START) startDown = true;
        if (!startDown) startArmed = true;
        s.startHeld = startDown;
        if (startArmed && startWasDown && !startDown) g_running = false;  // exit on release
        startWasDown = startDown;

        const s64 t0 = sysGetSystemTime();
        Canvas c = displayBackBuffer();
        drawBootScreen(c, s);
        s.frameMs = float(sysGetSystemTime() - t0) / 1000.0f;
        displayPresent();

        // Slow checks run after the first frame is on screen, so the user sees progress.
        if (!slowChecksDone && s.frame == 1) {
            checkMemory(s.checks[kMemory], disp);
            checkHdd(s.checks[kHdd], s.dataRoot, s.songsLine);
            yarg::SelfTestResult r = yarg::runCoreSelfTest();
            s.checks[kCore].state = r.ok ? CheckState::Ok : CheckState::Fail;
            s.checks[kCore].detail = r.message;
            slowChecksDone = true;
            writeLog(s.dataRoot, s, "startup");
            printf("checks done: core %s\n", r.message.c_str());
        }

        ++s.frame;
        ++fpsFrames;
        const s64 now = sysGetSystemTime();
        if (now - fpsWindowStart >= 1000000) {
            s.fps = float(fpsFrames) * 1e6f / float(now - fpsWindowStart);
            fpsFrames = 0;
            fpsWindowStart = now;
        }
    }

    writeLog(s.dataRoot, s, "exit");
    printf("YARG PS3 BOOT TEST exiting cleanly after %u frames\n", s.frame);
    ioPadEnd();
    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
    displayShutdown();
    return 0;
}
