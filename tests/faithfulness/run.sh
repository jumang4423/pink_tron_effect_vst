#!/bin/sh
# Numerically compares the C++ port against the original Pink Trombone JS.
# Requires Node.js and reference/pink-trombone (git clone of zakaton/Pink-Trombone).
set -eu

cd "$(dirname "$0")/../.."

if [ ! -d reference/pink-trombone ]; then
    git clone --depth 1 https://github.com/zakaton/Pink-Trombone.git reference/pink-trombone
fi

c++ -std=c++20 -O2 tests/faithfulness/port.cpp Source/dsp/Tract.cpp -ISource/dsp -o build/pinktron-faithfulness-cmp

node tests/faithfulness/reference.mjs > build/reference.txt
./build/pinktron-faithfulness-cmp > build/port.txt

# The engine halves the 2x-oversampled sum, so compare against 2 * port.
paste -d' ' build/reference.txt build/port.txt | awk '
{
    a = $1; b = $2 * 2; d = b - a; if (d < 0) d = -d;
    if (d > maxd) { maxd = d; line = NR }
    if (a != 0) { r = (b - a) / a; if (r < 0) r = -r; if (r > maxr) maxr = r }
}
END {
    printf "samples: %d   max abs diff: %.3e   max rel diff: %.3e (line %d)\n", NR, maxd, maxr, line;
    if (maxd > 1e-9) { print "MISMATCH"; exit 1 }
    print "MATCH (double precision)";
}'
