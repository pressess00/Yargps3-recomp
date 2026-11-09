# Third-party code and references

| What | Where in this repo | License | Source |
|---|---|---|---|
| RSX framebuffer setup (adapted) | `ps3/boot_test/source/display.cpp` | MIT, © 2011 PSL1GHT Development Team | PSL1GHT `samples/graphics/blitting/source/rsxutil.cpp` |
| Build rules (Makefile layout) | `ps3/boot_test/Makefile` | MIT, © 2011 PSL1GHT Development Team | PSL1GHT `samples/graphics/blitting/Makefile` |
| 8x8 bitmap font | `ps3/boot_test/include/font8x8_basic.h` | Public domain | https://github.com/dhepper/font8x8 |
| Guitar chart rules (ported, not copied) | `core/src/guitar_chart.cpp` | LGPL-3.0-or-later | YARG.Core `MoonscraperChartParser` (MidReader, MidIOHelper, MoonNote) |
| song.ini key names | `core/include/yarg/song_ini.h` | LGPL-3.0-or-later | YARG.Core `IO/Ini/SongIniHandler.cs` |
| PS3 Guitar Hero guitar mapping (documentation) | `ps3/boot_test/source/main.cpp` | reference only | PlasticBand `Docs/Instruments/5-Fret Guitar/Guitar Hero/PS3.md` |

Reference commits used for this port (checked 2026-11-09):

- YARG.Core `master` @ `2f3900cf91aa141149d60cb122e88876e1709abb`
- PSL1GHT `master` @ `dd4e8c9be744e3273d665c393fa6d97a87402e2e`
- ps3toolchain: PPU GCC 13.2.0 + newlib 1.20.0

## MIT license text (PSL1GHT)

```
Copyright (c) 2011 PSL1GHT Development Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
