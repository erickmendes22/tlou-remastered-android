#!/usr/bin/env bash
# Adapted from Bloodborne-Android build_bionic_deps.sh, 2e472653.
# Third-party libraries for the Bionic (Android NDK) build, into .local-deps/android/bionic-prefix.
# Pinned Bionic dependencies; fmt 12.1.0 supports the NDK r29 C++23 toolchain.
# Sources are downloaded to .local-deps/android/bionic-src, builds go to bionic-build.
# Host-side prerequisites: curl, tar, cmake, ninja, make, python3.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
root=$PWD/.local-deps/android
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH LIBRARY_PATH CMAKE_PREFIX_PATH PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
ndk=${TLOU_NDK:-${ANDROID_NDK_HOME:-$root/sdk/ndk/29.0.14206865}}
api=${TLOU_ANDROID_API:-30}
prefix=$root/bionic-prefix src=$root/bionic-src bld=$root/bionic-build
[[ -f $ndk/build/cmake/android.toolchain.cmake ]] || { echo "No NDK: $ndk" >&2; exit 1; }
mkdir -p "$prefix" "$src" "$bld"
J=${TLOU_JOBS:-2}
run() { "$@"; }

fetch() { # name url archive-extension
    local dir=$src/$1
    [[ -d $dir ]] && return
    rm -rf -- "$dir.tmp"
    local url="$2"
    if [[ "$1" == "ffmpeg-7.1.5" ]]; then
        # ffmpeg.org timed out in GitHub Actions; the same upstream n7.1.5 source tag.
        url="https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n7.1.5.tar.gz"
    fi
    curl --retry 5 --retry-delay 3 --connect-timeout 12 --max-time 180 -fsSL "$url" -o "$src/$1.tar" && mkdir -p "$dir.tmp" && tar --no-same-owner -xf "$src/$1.tar" -C "$dir.tmp" --strip-components=1 \
        && mv "$dir.tmp" "$dir" && rm "$src/$1.tar"
}
cm() { # name source-dir extra cmake args...
    local name=$1 sdir=$2; shift 2
    [[ -f $prefix/.done-$name ]] && return
    cmake -S "$sdir" -B "$bld/$name" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-$api -DANDROID_STL=c++_shared \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_FIND_ROOT_PATH="$prefix" "$@" >/dev/null
    run cmake --build "$bld/$name" -j "$J" >"$bld/$name.log" 2>&1 || { tail -30 "$bld/$name.log" >&2; exit 1; }
    cmake --install "$bld/$name" >/dev/null
    touch "$prefix/.done-$name"
}

fetch fmt-12.1.0 https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz
fetch zycore-1.5.2 https://github.com/zyantific/zycore-c/archive/refs/tags/v1.5.2.tar.gz
fetch zydis-4.1.1 https://github.com/zyantific/zydis/archive/refs/tags/v4.1.1.tar.gz
fetch xxhash-0.8.3 https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.8.3.tar.gz
fetch miniz-3.1.2 https://github.com/richgel999/miniz/archive/refs/tags/3.1.2.tar.gz
fetch Vulkan-Headers-1.4.363 https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/v1.4.363.tar.gz
fetch SPIRV-Headers-1.4.309 https://github.com/KhronosGroup/SPIRV-Headers/archive/refs/tags/vulkan-sdk-1.4.309.0.tar.gz
fetch ffmpeg-7.1.5 https://ffmpeg.org/releases/ffmpeg-7.1.5.tar.xz

# Header-only: vulkan-hpp (the NDK ships Vulkan 1.3.275 C headers without it) and SPIR-V.
cm vulkan-headers "$src/Vulkan-Headers-1.4.363"
cm spirv-headers "$src/SPIRV-Headers-1.4.309"
cm fmt-12.1.0 "$src/fmt-12.1.0" -DFMT_TEST=OFF -DFMT_DOC=OFF
cm zycore "$src/zycore-1.5.2" -DBUILD_SHARED_LIBS=OFF
# The release tarball lacks the zycore submodule: build against the same Zycore source tree.
[[ -f $src/zydis-4.1.1/dependencies/zycore/CMakeLists.txt ]] || cp -a "$src/zycore-1.5.2/." "$src/zydis-4.1.1/dependencies/zycore/"
cm zydis "$src/zydis-4.1.1" -DBUILD_SHARED_LIBS=OFF -DZYDIS_BUILD_EXAMPLES=OFF -DZYDIS_BUILD_TOOLS=OFF \
    -DZYDIS_BUILD_DOXYGEN=OFF
cm xxhash "$src/xxhash-0.8.3/cmake_unofficial" -DBUILD_SHARED_LIBS=OFF -DXXHASH_BUILD_XXHSUM=OFF
cm miniz "$src/miniz-3.1.2" -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTS=OFF -DINSTALL_PROJECT=ON
# FFmpeg (avplayer/videodec): static, PIC, no programs, only built-in codecs/demuxers.
if [[ ! -f $prefix/.done-ffmpeg ]]; then
    tc=$ndk/toolchains/llvm/prebuilt/linux-x86_64
    mkdir -p "$bld/ffmpeg" && pushd "$bld/ffmpeg" >/dev/null
    "$src/ffmpeg-7.1.5/configure" --prefix="$prefix" --enable-cross-compile --target-os=android --arch=aarch64 \
        --cc="$tc/bin/aarch64-linux-android$api-clang" --cxx="$tc/bin/aarch64-linux-android$api-clang++" \
        --ar="$tc/bin/llvm-ar" --ranlib="$tc/bin/llvm-ranlib" --strip="$tc/bin/llvm-strip" --nm="$tc/bin/llvm-nm" \
        --enable-static --disable-shared --enable-pic --disable-programs --disable-doc --disable-debug \
        --disable-avdevice --disable-avfilter --disable-network --disable-autodetect --disable-mediacodec --disable-jni >"$bld/ffmpeg-configure.log" 2>&1 \
        || { tail -30 "$bld/ffmpeg-configure.log" >&2; exit 1; }
    run make -j"$J" >"$bld/ffmpeg.log" 2>&1 || { tail -30 "$bld/ffmpeg.log" >&2; exit 1; }
    make install >/dev/null
    # configure's Android target links libandroid/libmediandk for MediaCodec/JNI, both disabled here.
    sed -i 's/ -landroid -lmediandk//' "$prefix"/lib/pkgconfig/libavutil.pc
    popd >/dev/null
    touch "$prefix/.done-ffmpeg"
fi
echo "Bionic deps in $prefix"
