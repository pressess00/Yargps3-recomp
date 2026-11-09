# Audyt YARG → PS3 (2026-11-09)

## Repozytoria

- **YARG**: Unity 6000.3.5f2, LGPL-3.0-or-later. Branch domyślny `master`, rozwój na `dev`.
  YARG.Core jako submoduł (przypięty do `3beb94e`). Audio: BASS (własnościowy, brak wersji PS3).
- **YARG.Core**: `netstandard2.1`, C# 9, `unsafe`, LGPL-3.0-or-later.
  Zależności: Melanchall.DryWetMidi.Nativeless 7.0.0, Newtonsoft.Json 13.0.3, ZString 2.5.1,
  System.Runtime.CompilerServices.Unsafe 6.0.0. Parser `.chart` z Moonscrapera (BSD-3).
  Katalogi: Audio, Chart, Engine, Game, IO, Input, Logging, MoonscraperChartParser, Parsing,
  Replays, Song, Utility, Venue.

**Wniosek:** brak środowiska .NET/Mono dla PS3 GameOS → logika YARG.Core jest przenoszona
do C++, a YARG.Core na PC służy jako wyrocznia (porównanie wyników nuta po nucie).

## Tabela

| Komponent | Technologia w YARG | Niezależny od Unity? | Możliwy na PS3? | Wymaga portu? |
|---|---|---|---|---|
| Chart parser | C# (Chart, Parsing, Moonscraper, DryWetMidi) | Tak | Tak | Tak — C++ (`core/`, zaczęte) |
| Gameplay (Engine) | C# | Tak | Tak | Tak — C++ |
| Scoring / combo / SP | C# (Engine) | Tak | Tak | Tak — C++ |
| Song metadata / skan | C# (Song, IO), song.ini | Tak | Tak | Tak — C++ (`song_ini` gotowe) |
| Replay | C# (Replays) | Tak | Tak | Później (uwaga: big-endian) |
| Audio | abstrakcja w Core, BASS w Unity | Częściowo | Tak: libaudio (PSL1GHT), libvorbis 1.3.5, SDL2_mixer 2.0.4 | Napisać |
| Renderer | Unity | Nie | Tak: RSX (libgcm), tiny3d, SDL2 | Napisać |
| Input | Unity Input System + PlasticBand-Unity | Nie | Tak: libpad | Napisać |
| File system | .NET IO | — | Tak: newlib + syscalle lv2 | Napisać (warstwa platformy) |
| UI | Unity UI, TextMeshPro | Nie | Tak, prostsze | Napisać |

## Środowisko PS3

- Toolchain: `ps3dev/ps3dev` (PPU GCC 13.2.0, newlib 1.20.0, SPU GCC 9.5.0), gotowe wydania.
- PSL1GHT: RSX/gcm, libpad, sysutil, lv2 FS (`sysLv2FsOpenDir/ReadDir` do listowania katalogów).
- ps3libraries: zlib, libpng, freetype, libogg, libvorbis, libmad, flac, SDL2_PSL1GHT,
  SDL2_mixer, tiny3d+libfont, NoRSX, ps3soundlib, debugnet. **Brak Opusa.**
- Gitara GH PS3 (dongle `12BA:0100`): raport w formacie pada PS3, frety na ×○□△L1 (PlasticBand).
- Pamięć: 256 MB XDR + 256 MB GDDR3 → indeks metadanych, streaming audio, lazy loading.
