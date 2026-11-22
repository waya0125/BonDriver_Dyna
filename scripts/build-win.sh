#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/dist"
mkdir -p "$OUT/x64" "$OUT/x86"

build_local() {
    cmake -S "$ROOT" -B "$ROOT/build-x64" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchain-mingw64.cmake" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$ROOT/build-x64" -j
    cp "$ROOT/build-x64/BonDriver_Dyna.dll" "$OUT/x64/BonDriver_Dyna.dll"

    cmake -S "$ROOT" -B "$ROOT/build-x86" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/toolchain-mingw32.cmake" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$ROOT/build-x86" -j
    cp "$ROOT/build-x86/BonDriver_Dyna.dll" "$OUT/x86/BonDriver_Dyna.dll"
}

if command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1 && command -v i686-w64-mingw32-g++ >/dev/null 2>&1; then
    build_local
else
    docker run --rm -v "$ROOT:/src" -w /src debian:bookworm-slim bash -lc '
        set -euo pipefail
        apt-get update -qq
        DEBIAN_FRONTEND=noninteractive apt-get install -y -qq cmake g++-mingw-w64-x86-64 g++-mingw-w64-i686 make >/dev/null
        cmake -S /src -B /tmp/build-x64 -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mingw64.cmake -DCMAKE_BUILD_TYPE=Release
        cmake --build /tmp/build-x64 -j
        cp /tmp/build-x64/BonDriver_Dyna.dll /src/dist/x64/BonDriver_Dyna.dll
        cp /tmp/build-x64/bon_test.exe /src/dist/x64/bon_test.exe
        cmake -S /src -B /tmp/build-x86 -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchain-mingw32.cmake -DCMAKE_BUILD_TYPE=Release
        cmake --build /tmp/build-x86 -j
        cp /tmp/build-x86/BonDriver_Dyna.dll /src/dist/x86/BonDriver_Dyna.dll
        cp /tmp/build-x86/bon_test.exe /src/dist/x86/bon_test.exe
    '
fi

echo "Built:"
ls -lh "$OUT/x64/BonDriver_Dyna.dll" "$OUT/x86/BonDriver_Dyna.dll"
