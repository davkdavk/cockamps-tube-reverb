param(
    [ValidateSet("Release","Debug")]
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

$jucePath = "..\JUCE"

cmake -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH="$jucePath"
cmake --build build --config $BuildType --parallel
