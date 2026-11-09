// YARG PS3 — golden reference dumper built on the real YARG.Core (C#).
// Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
//
// Writes the 5-fret guitar chart of a .mid in the same text format as
// `chartdump --golden` (core/tools/chartdump.cpp) so the C++ port can be diffed against YARG.Core.
//
//   YargGolden <notes.mid> <out.txt> [Expert|Hard|Medium|Easy]
//
// Format, one note/chord per line: tick<TAB>frets<TAB>type<TAB>length<TAB>sp
//   frets  = letters in order G R Y B O o (o = open), type = strum|hopo|tap, sp = 0|1
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using YARG.Core;
using YARG.Core.Chart;

internal static class Program
{
    private static int Main(string[] args)
    {
        if (args.Length < 2)
        {
            Console.Error.WriteLine("usage: YargGolden <notes.mid> <out.txt> [Expert|Hard|Medium|Easy]");
            return 2;
        }

        var difficulty = args.Length > 2 ? Enum.Parse<Difficulty>(args[2]) : Difficulty.Expert;
        var settings = ParseSettings.Default_Midi;
        var chart = SongChart.FromFile(in settings, args[0]);
        var notes = chart.FiveFretGuitar.GetDifficulty(difficulty).Notes;

        const string letters = "GRYBOo";
        var sb = new StringBuilder();
        foreach (var parent in notes)
        {
            var chord = new List<GuitarNote> { parent };
            chord.AddRange(parent.ChildNotes);

            var present = new bool[6];
            uint length = 0;
            foreach (var n in chord)
            {
                // FiveFretGuitarFret: Green = 1 .. Orange = 5, Open = 7
                int index = n.Fret switch
                {
                    >= 1 and <= 5 => n.Fret - 1,
                    7 => 5,
                    _ => -1,
                };
                if (index < 0)
                {
                    Console.Error.WriteLine($"unexpected fret {n.Fret} at tick {n.Tick}");
                    return 1;
                }
                present[index] = true;
                length = Math.Max(length, n.TickLength);
            }

            var frets = new StringBuilder();
            for (int i = 0; i < 6; i++)
                if (present[i]) frets.Append(letters[i]);

            sb.Append(parent.Tick).Append('\t')
              .Append(frets).Append('\t')
              .Append(parent.Type.ToString().ToLowerInvariant()).Append('\t')
              .Append(length).Append('\t')
              .Append(parent.IsStarPower ? 1 : 0).Append('\n');
        }

        File.WriteAllText(args[1], sb.ToString());
        return 0;
    }
}
