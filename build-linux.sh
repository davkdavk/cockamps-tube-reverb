#!/bin/bash
set -e
JUCE_PATH=$(realpath ../JUCE)
if [ ! -d "$JUCE_PATH" ]; then
    echo "ERROR: JUCE not found at $JUCE_PATH"
    echo "Run: git clone --branch 7.0.9 --depth 1 https://github.com/juce-framework/JUCE.git ../JUCE"
    exit 1
fi
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH="$JUCE_PATH"
cmake --build build --parallel $(nproc)
echo "Build complete."
