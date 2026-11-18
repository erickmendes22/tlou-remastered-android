#!/usr/bin/env bash
# CPython Android build: host and target commands must not inherit Conda's compiler/pkg-config.
set -Eeuo pipefail
cd -- "$(dirname -- "$0")/../.."
root=$PWD
ndk=${TLOU_NDK:-${ANDROID_NDK_HOME:-$root/.local-deps/android/sdk/ndk/android-ndk-r29}}
api=${TLOU_ANDROID_API:-30}
src=$root/.local-deps/android/bionic-src/python-3.13.7
# Do not reuse Makefiles generated before the host/target environment was isolated.
build=$root/.local-deps/android/bionic-build/python-host-isolated-v1
cross=$root/.local-deps/android/bionic-build/python-android-isolated-v1
out=$root/out/android-bionic/python
j=${TLOU_JOBS:-4}
llvm=$ndk/toolchains/llvm/prebuilt/linux-x86_64
clean=(env -i "PATH=/usr/bin:/bin:$llvm/bin" "HOME=$HOME" LC_ALL=C CONFIG_SITE=/dev/null)
if [[ ! -f $src/configure ]]; then
    mkdir -p "$src"
    curl -fsSL --retry 3 https://www.python.org/ftp/python/3.13.7/Python-3.13.7.tar.xz -o "$src/source.tar.xz"
    tar --no-same-owner -xf "$src/source.tar.xz" -C "$src" --strip-components=1
fi
mkdir -p "$build" "$cross" "$out"
if [[ ! -f $build/Makefile ]]; then
    (cd "$build"
     "${clean[@]}" CC=/usr/bin/cc CXX=/usr/bin/c++ CPP='/usr/bin/cc -E' \
        "$src/configure" --with-pkg-config=no --without-ensurepip --disable-test-modules > configure.log 2>&1)
fi
"${clean[@]}" make -C "$build" -j"$j" > "$build/build.log" 2>&1
if [[ ! -f $cross/Makefile ]]; then
    build_type=$("${clean[@]}" "$build/python" -c 'import sysconfig; print(sysconfig.get_config_var("BUILD_GNU_TYPE"))')
    (cd "$cross"
     "${clean[@]}" CC="$llvm/bin/aarch64-linux-android$api-clang" \
     CPP="$llvm/bin/aarch64-linux-android$api-clang -E" \
     CXX="$llvm/bin/aarch64-linux-android$api-clang++" \
     AR="$llvm/bin/llvm-ar" RANLIB="$llvm/bin/llvm-ranlib" READELF="$llvm/bin/llvm-readelf" \
     PKG_CONFIG=/usr/bin/false PKG_CONFIG_LIBDIR=/nonexistent PKG_CONFIG_PATH= \
     CFLAGS=-D__BIONIC_NO_PAGE_SIZE_MACRO \
     LDFLAGS="-Wl,-rpath,'\$\$ORIGIN/../lib' -Wl,-z,max-page-size=16384" \
     "$src/configure" --host=aarch64-linux-android --build="$build_type" \
       --with-build-python="$build/python" --prefix="$out" --enable-shared --without-static-libpython \
       --with-pkg-config=no --without-ensurepip --disable-test-modules > configure.log 2>&1)
fi
"${clean[@]}" "$build/python" "$root/android/bionic/audit_python.py" "$cross/Makefile" --ndk "$ndk" --config-log "$cross/config.log"
"${clean[@]}" make -C "$cross" -j"$j" > "$cross/build.log" 2>&1
"${clean[@]}" make -C "$cross" install > "$cross/install.log" 2>&1
cp "$src/LICENSE" "$out/LICENSE-Python.txt"
echo "Bionic Python built: $out"
