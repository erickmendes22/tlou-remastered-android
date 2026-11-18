#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
root=$PWD
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH LIBRARY_PATH
ndk=${TLOU_NDK:-${ANDROID_NDK_HOME:-$root/.local-deps/android/sdk/ndk/android-ndk-r29}}
api=${TLOU_ANDROID_API:-30}
src=$root/.local-deps/android/orbis-pkg
if [[ ! -d $src/.git ]]; then git clone --no-checkout https://github.com/videobitva/orbis.git "$src"; fi
git -C "$src" fetch --depth=1 origin 13f1294a87a53d54f82feec56acbfd03f50667a9
git -C "$src" checkout --detach 13f1294a87a53d54f82feec56acbfd03f50667a9
rustup target add aarch64-linux-android
export CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android$api-clang"
export CC_aarch64_linux_android="$CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER"
export AR_aarch64_linux_android="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
cargo build --manifest-path "$src/Cargo.toml" --locked --release --target aarch64-linux-android \
    --target-dir "$root/out/android-bionic/orbis-cargo" -p orbis-pkg-util
mkdir -p "$root/out/android-bionic/bin"
install -m 755 "$root/out/android-bionic/orbis-cargo/aarch64-linux-android/release/orbis-pkg-util" "$root/out/android-bionic/bin/orbis-pkg-util"
