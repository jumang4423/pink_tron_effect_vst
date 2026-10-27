#!/bin/sh
# Fetch JUCE (once), build the plugin and install it for the current user.
set -eu

cd "$(dirname "$0")/.."

if [ ! -d libs/JUCE ]; then
    echo "== cloning JUCE 8.0.15 =="
    git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git libs/JUCE
fi

echo "== configuring =="
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja

echo "== building VST3 + AU =="
cmake --build build --target PinkTron_VST3 PinkTron_AU -j"$(sysctl -n hw.ncpu)"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

echo "== installing =="
rsync -a --delete "build/PinkTron_artefacts/Release/VST3/Pink Tron.vst3" "$VST3_DIR/"
rsync -a --delete "build/PinkTron_artefacts/Release/AU/Pink Tron.component" "$AU_DIR/"

# make the AU visible to hosts that were already running
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "installed:"
echo "  $VST3_DIR/Pink Tron.vst3"
echo "  $AU_DIR/Pink Tron.component"
