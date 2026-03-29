@echo off
echo === Ember - Dependency Setup ===
echo.

if not exist vendor mkdir vendor

echo [1/3] Cloning ImGui (docking branch)...
if not exist vendor\imgui (
    git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git vendor/imgui
) else (
    echo Already exists, skipping.
)

echo [2/3] Cloning LZMA SDK...
if not exist vendor\lzma (
    git clone --depth 1 https://github.com/jljusten/LZMA-SDK.git vendor/lzma
) else (
    echo Already exists, skipping.
)

echo [3/3] Cloning Native File Dialog Extended...
if not exist vendor\nfd (
    git clone --depth 1 https://github.com/btzy/nativefiledialog-extended.git vendor/nfd
) else (
    echo Already exists, skipping.
)

echo.
echo === Setup complete ===
echo Run: xmake f -p windows ^&^& xmake build
