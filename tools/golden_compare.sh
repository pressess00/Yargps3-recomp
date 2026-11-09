#!/usr/bin/env bash
# YARG PS3 — compare the C++ chart port against the real YARG.Core on a generated corpus.
# Copyright (C) 2026 YARG PS3 contributors. LGPL-3.0-or-later.
#
# Requirements: cmake + C++17 compiler, .NET SDK 10, a YARG.Core checkout in ext/YARG.Core
# (or YARG_CORE_DIR pointing to one).
#
#   tools/golden_compare.sh [count]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
COUNT="${1:-40}"
YARG_CORE_DIR="${YARG_CORE_DIR:-$ROOT/ext/YARG.Core}"
WORK="$ROOT/golden_out"

rm -rf "$WORK" && mkdir -p "$WORK/mid" "$WORK/cpp" "$WORK/yarg"

cmake -S "$ROOT/core" -B "$ROOT/core/build-golden" -DCMAKE_BUILD_TYPE=Release -DYARG_SANITIZE=OFF >/dev/null
cmake --build "$ROOT/core/build-golden" -j >/dev/null
"$ROOT/core/build-golden/gen_corpus" "$WORK/mid" "$COUNT"

dotnet build "$ROOT/tools/golden_cs/YargGolden.csproj" -c Release -p:YargCoreDir="$YARG_CORE_DIR" -o "$WORK/bin" >/dev/null

fail=0
total=0
for mid in "$WORK"/mid/*.mid; do
  name="$(basename "$mid" .mid)"
  for diff in Expert Hard Medium Easy; do
    total=$((total + 1))
    "$ROOT/core/build-golden/chartdump" --golden "$mid" "$diff" > "$WORK/cpp/$name.$diff.txt"
    dotnet "$WORK/bin/YargGolden.dll" "$mid" "$WORK/yarg/$name.$diff.txt" "$diff"
    if ! diff -u "$WORK/yarg/$name.$diff.txt" "$WORK/cpp/$name.$diff.txt" > "$WORK/$name.$diff.diff"; then
      fail=$((fail + 1))
      echo "MISMATCH $name $diff (first lines; '-' = YARG.Core, '+' = C++ port):"
      head -20 "$WORK/$name.$diff.diff"
    else
      rm "$WORK/$name.$diff.diff"
    fi
  done
done

notes=$(cat "$WORK"/yarg/*.txt | wc -l)
echo "golden: $((total - fail))/$total charts identical ($notes notes/chords from YARG.Core)"
[ "$fail" -eq 0 ]
