#!/usr/bin/env bash
# V Spoofer Zygisk build - by @Xverno
set -euo pipefail
cd "$(dirname "$0")"
NDK="${ANDROID_NDK_LATEST_HOME:-${ANDROID_NDK_HOME:-}}"
[ -d "$NDK" ] || { echo "Set ANDROID_NDK_HOME to your NDK path"; exit 1; }

HDR=native/zygisk.hpp
[ -f "$HDR" ] || curl -fsSL https://raw.githubusercontent.com/topjohnwu/zygisk-module-sample/master/module/jni/zygisk.hpp -o "$HDR"

FLAGS=""
if grep -q 'const char \*regex' "$HDR"; then FLAGS="-DVS_PLT_REGEX=1"; fi
echo "Zygisk header flags: ${FLAGS:-none (dev/inode API)}"

rm -rf out build && mkdir -p out/module
cp -r module/. out/module/
mkdir -p out/module/zygisk

for abi in arm64-v8a armeabi-v7a x86_64 x86; do
  cmake -S native -B "build/$abi" -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-26 -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="$FLAGS"
  cmake --build "build/$abi" -j
  cp "build/$abi/libvspoofer.so" "out/module/zygisk/$abi.so"
done

(cd out/module && zip -r -q ../V_Spoofer.zip .)
echo "Built out/V_Spoofer.zip"
