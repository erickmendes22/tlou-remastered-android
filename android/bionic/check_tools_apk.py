#!/usr/bin/env python3
"""Check that the installable APK really contains executable ARM64 preparation tools."""
import io
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zipfile


def arm64(data, name):
    if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError(f'{name} is not an ARM64 ELF')
    if b'/lib/ld-linux' in data or b'GLIBC_' in data:
        raise ValueError(f'{name} contains glibc references')


with zipfile.ZipFile(sys.argv[1]) as apk:
    with tempfile.TemporaryDirectory(prefix='tlou-apk-native-') as temp:
        root = Path(temp)
        names = ['assets/runtime/gow3-probe', *[f'lib/arm64-v8a/{lib}' for lib in
            ('libgow3cpu.so', 'libgow3gpu.so', 'libgow3present.so', 'libc++_shared.so')]]
        if b'g3PNjYKWqnQ#libSceRemoteplay' not in apk.read('assets/runtime/gow3-probe'):
            raise ValueError('Stale runtime: base TLOU Remote Play service is absent')
        allowed = {'libc.so', 'libm.so', 'libdl.so', 'liblog.so', 'libandroid.so', 'libnativewindow.so',
                   'libc++_shared.so', 'libgow3cpu.so', 'libgow3gpu.so', 'libvulkan.so'}
        for name in names:
            data = apk.read(name)
            arm64(data, name)
            path = root / Path(name).name
            path.write_bytes(data)
            dynamic = subprocess.check_output(['readelf', '-dW', str(path)], text=True)
            needed = set(re.findall(r'NEEDED.*?\[(.*?)\]', dynamic))
            if needed - allowed:
                raise ValueError(f'{name} needs unpackaged or non-Android libraries: {needed-allowed}')
        interp = subprocess.check_output(['readelf', '-lW', str(root/'gow3-probe')], text=True)
        if '/system/bin/linker64' not in interp:
            raise ValueError('Runtime is not an Android executable')
        symbols = subprocess.check_output(['readelf', '--dyn-syms', '--wide', str(root/'libgow3present.so')], text=True)
        for method in ('nativeStart', 'nativeSetSurface', 'nativeFrames', 'nativeSetFullscreen', 'nativeShutdown'):
            if 'Java_ro_cobrabm_fexdroid_Gow3Present_'+method not in symbols:
                raise ValueError(f'Packaged presenter lacks JNI {method}')
    arm64(apk.read('lib/arm64-v8a/libpython3.13.so'), 'libpython')
    with zipfile.ZipFile(io.BytesIO(apk.read('assets/tools.zip'))) as tools:
        for name in ('python/bin/python3.13', 'bin/orbis-pkg-util'):
            arm64(tools.read(name), name)
        for name in ('python/lib/python3.13/encodings/__init__.py',
                     'source/android/device_prepare.py', 'source/android/tlou_bionic.py',
                     'source/android/tlou_patches.py',
                     'source/gow3_pc/scripts/prepare.py', 'source/gow3_pc/scripts/link_modules.py',
                     'source/gow3_pc/scripts/link_libc.py', 'source/gow3_pc/scripts/content_profile.py',
                     'source/tools/probe_tlou.py'):
            if not tools.read(name):
                raise ValueError(f'Missing preparation component {name}')
        for module in ('_struct', '_posixsubprocess', '_sha2'):
            if not any('/lib-dynload/'+module+'.' in name for name in tools.namelist()):
                raise ValueError(f'Missing target Python extension {module}')
print('APK: Android ARM64 loader/dependencies, presenter JNI, PKG extractor, CPython and TLOU preparation verified')
