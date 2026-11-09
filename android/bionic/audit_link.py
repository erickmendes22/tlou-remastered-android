#!/usr/bin/env python3
"""Check CMake's actual link inputs before compiling the Bionic GPU."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess


def audit(fragments, build, roots, readelf, ndk=None):
    build=Path(build).resolve()
    roots=[Path(p).resolve() for p in roots]
    checked=set()
    for fragment in fragments:
        for token in shlex.split(fragment['fragment']):
            search=token.startswith('-L')
            if search:
                path=Path(token[2:])
            elif fragment['role']=='libraries' and not token.startswith('-'):
                path=Path(token)
            else:
                continue
            path=(path if path.is_absolute() else build/path).resolve()
            if path.is_relative_to(build):
                continue  # generated targets are checked after compilation by verify_bionic.py
            if not any(path.is_relative_to(root) for root in roots):
                raise ValueError(f'Host/outside-target library path selected by CMake: {path}')
            if search or path in checked:
                continue
            if not path.is_file():
                raise ValueError(f'Missing imported Android library: {path}')
            # NDK r29's ARM64 libatomic.a is a comment-only linker placeholder;
            # __atomic functions are supplied by the compiler's builtins archive.
            if (ndk is not None and path.is_relative_to(Path(ndk).resolve()) and
                    path.name=='libatomic.a' and path.parent.name=='aarch64' and path.stat().st_size<4096 and
                    path.read_bytes().startswith(b'/*') and
                    not re.sub(r'/\*.*?\*/','',path.read_text(),flags=re.S).strip()):
                checked.add(path)
                continue
            header=subprocess.check_output([str(readelf),'-h',str(path)],text=True)
            machines=set(re.findall(r'Machine:\s*([^\n]+)',header))
            if machines != {'AArch64'}:
                raise ValueError(f'Non-ARM64 library selected by CMake: {path}: {sorted(machines)}')
            checked.add(path)
    if not checked:
        raise ValueError('No imported ARM64 libraries found in the CMake link model')
    return len(checked)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--prefix',type=Path,required=True)
    p.add_argument('--ndk',type=Path,required=True)
    a=p.parse_args()
    reply=a.build/'.cmake/api/v1/reply'
    index=json.loads(max(reply.glob('index-*.json'),key=lambda p:p.stat().st_mtime_ns).read_text())
    model=json.loads((reply/index['reply']['codemodel-v2']['jsonFile']).read_text())
    target=next(t for t in model['configurations'][0]['targets'] if t['name']=='gow3gpu')
    data=json.loads((reply/target['jsonFile']).read_text())
    n=audit(data['link']['commandFragments'],a.build,[a.prefix,a.ndk],
            a.ndk/'toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf',a.ndk)
    print(f'CMake link audit: {n} target libraries (ARM64/NDK); no host library/search path')

if __name__=='__main__':main()
