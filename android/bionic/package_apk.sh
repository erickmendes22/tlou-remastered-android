#!/usr/bin/env bash
# Package the real TLOU ARM64/FEX executable and GPU, not a placeholder APK.
set -Eeuo pipefail
trap 'echo "TLOU APK packaging failed at line $LINENO: $BASH_COMMAND" >&2' ERR
cd "$(dirname "$0")/../.."
root=$PWD
out=$root/out/android-bionic
ndk="$TLOU_NDK"
if [[ -z "$ndk" ]]; then ndk="$ANDROID_NDK_HOME"; fi
[[ -n "$ndk" ]] || { echo "Set TLOU_NDK or ANDROID_NDK_HOME" >&2; exit 2; }
llvm=$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin
sdk="$ANDROID_HOME"
if [[ -z "$sdk" ]]; then sdk="$ANDROID_SDK_ROOT"; fi
[[ -n "$sdk" ]] || { echo "Set ANDROID_HOME or ANDROID_SDK_ROOT" >&2; exit 2; }
build_tools=35.0.0
android_tools="$sdk/build-tools/$build_tools"
[[ -s "$out/bin/gow3-probe" ]] || { echo "Missing actual TLOU FEX loader" >&2; exit 2; }
for library in libgow3cpu.so libgow3gpu.so libgow3present.so libc++_shared.so; do
  [[ -s "$out/lib/$library" ]] || { echo "Missing native $library" >&2; exit 2; }
done
"$llvm/llvm-readelf" -h "$out/bin/gow3-probe" | grep -q 'Machine:.*AArch64'
"$llvm/llvm-readelf" -l "$out/bin/gow3-probe" | grep -q '/system/bin/linker64'
for library in libgow3cpu.so libgow3gpu.so libgow3present.so libc++_shared.so; do
  "$llvm/llvm-readelf" -h "$out/lib/$library" | grep -q 'Machine:.*AArch64'
done
stage="$out/apk-jni/arm64-v8a"
assets="$out/apk-assets/runtime"
python3 android/bionic/stage_tools.py
mkdir -p "$stage" "$assets" "$root/out/release"
cp "$out/bin/gow3-probe" "$assets/gow3-probe"
for lib in libgow3cpu.so libgow3gpu.so libgow3present.so libc++_shared.so; do
  cp "$out/lib/$lib" "$stage/$lib"
done
gradle -p "$root/android-launcher" :app:assembleRelease --no-daemon --max-workers=2
unsigned="$root/android-launcher/app/build/outputs/apk/release/app-release-unsigned.apk"
[[ -s "$unsigned" ]] || { echo "Gradle did not produce an unsigned APK" >&2; exit 2; }
aligned="$out/tlou-release-aligned.apk"
apk="$root/out/release/TheLastOfUsRemastered-Android-Bionic-v0.31-arm64.apk"
"$android_tools/zipalign" -f -p 4 "$unsigned" "$aligned"
key="$out/experimental-development.jks"
if [[ ! -f $key ]]; then
  keytool -genkeypair -keystore "$key" -storepass 'TLOU-Bionic-development' \
    -alias tlou-bionic -keypass 'TLOU-Bionic-development' -keyalg RSA -keysize 3072 \
    -validity 3650 -dname 'CN=TLOU Experimental Android,O=Daedalus' -noprompt
fi
"$android_tools/apksigner" sign --ks "$key" --ks-key-alias tlou-bionic \
  --ks-pass pass:TLOU-Bionic-development --key-pass pass:TLOU-Bionic-development \
  --out "$apk" "$aligned"
"$android_tools/apksigner" verify --verbose "$apk"
"$android_tools/aapt" dump badging "$apk" | grep -q "package: name='com.daedalus.tlou.remastered'"
unzip -Z -1 "$apk" | grep -q 'lib/arm64-v8a/libgow3gpu.so'
unzip -Z -1 "$apk" | grep -q 'lib/arm64-v8a/libgow3cpu.so'
unzip -Z -1 "$apk" | grep -q 'lib/arm64-v8a/libgow3present.so'
unzip -Z -1 "$apk" | grep -q 'assets/runtime/gow3-probe'
unzip -Z -1 "$apk" | grep -q 'assets/tools.zip'
unzip -Z -1 "$apk" | grep -q 'lib/arm64-v8a/libpython3.13.so'
sha256sum "$apk" > "$apk.sha256"
echo "Experimental TLOU APK signed and validated: $apk"
