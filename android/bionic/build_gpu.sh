#!/usr/bin/env bash
set -Eeuo pipefail
trap 'printf "Bionic GPU build failed at line %s: %s\n" "$LINENO" "$BASH_COMMAND" >&2' ERR
cd -- "$(dirname -- "$0")/../.."
ndk=${TLOU_NDK:-${ANDROID_NDK_HOME:-}}
: "${ndk:?Set TLOU_NDK or ANDROID_NDK_HOME}"
api=${TLOU_ANDROID_API:-30}
root=$PWD
prefix=$root/.local-deps/android/bionic-prefix
headers=${TLOU_BIONIC_HEADERS:-$root/.local-deps/root/usr}
out=$root/out/android-bionic
build=${TLOU_BIONIC_GPU_BUILD:-$out/gpu}
atomic=$("$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android$api-clang" -print-file-name=libatomic.a)
[[ -f $atomic ]] || { echo 'NDK ARM64 libatomic.a is missing' >&2; exit 1; }
[[ -f $prefix/.done-ffmpeg ]] || { echo 'Run android/bionic/build_deps.sh first' >&2; exit 1; }
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH LIBRARY_PATH CMAKE_PREFIX_PATH
export PATH="$headers/bin:$PATH"
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig:$prefix/share/pkgconfig"
export PKG_CONFIG_PATH=$PKG_CONFIG_LIBDIR
unset PKG_CONFIG_SYSROOT_DIR
for patch in gow3_pc/gpu/patches/fsr-vulkan/*.patch; do
    if ! git -C gow3_pc/gpu/third_party/fsr-vulkan apply --reverse --check "$root/$patch" 2>/dev/null; then
        git -C gow3_pc/gpu/third_party/fsr-vulkan apply "$root/$patch"
    fi
done
# TLOU PC gains ~10% from trained PGO+LTO; an x86 profile cannot train ARM64.
# Reuse its LTO setup for the Android GPU only. Can disable at build time with
# TLOU_ANDROID_LTO=OFF; this is a binary setting, not a launcher experiment.
# Let CMake discover include/vma as well as include; discard the old forced cache path.
mkdir -p "$build/.cmake/api/v1/query"
touch "$build/.cmake/api/v1/query/codemodel-v2"
cmake -S gow3_pc/gpu -B "$build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM="android-$api" -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGOW3_PGO=off -DGOW3_LTO="${TLOU_ANDROID_LTO:-ON}" \
    -DCMAKE_FIND_ROOT_PATH="$prefix" -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -Dpkgcfg_lib_FFMPEG_atomic:FILEPATH="$atomic" \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH "-DCMAKE_PREFIX_PATH=$prefix;$headers" \
    -UVMA_INCLUDE_DIR
python3 android/bionic/audit_link.py --build "$build" --prefix "$prefix" --ndk "$ndk"
if [[ ${1:-} == --configure-only ]]; then exit 0; fi
cmake --build "$build" --target gow3gpu --parallel "${TLOU_JOBS:-2}" -- -k 0
mkdir -p "$out/lib"
cp "$build/libgow3gpu.so" "$out/lib/"
echo "TLOU-specific GPU shared library built: $out/lib/libgow3gpu.so"
