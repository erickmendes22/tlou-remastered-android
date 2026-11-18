#!/usr/bin/env python3
"""Reject a host CPP/SOABI/pkg-config before compiling Android CPython modules."""
import argparse
from pathlib import Path
import re
import shlex


def audit(makefile, ndk, config_log=""):
    # Horizontal whitespace only: an empty variable must not consume the next line.
    values = dict(re.findall(r'^([A-Za-z_][A-Za-z_0-9]*)[ \t]*=[ \t]*(.*)$', makefile, re.M))
    for key in ['SOABI', 'EXT_SUFFIX', 'MULTIARCH']:
        value = values.get(key, '')
        if 'aarch64' not in value or 'android' not in value:
            raise ValueError(f'Python target {key} is not Android ARM64: {value}')
    # CPython does not export CPP to the generated Makefile; configure records it here.
    if config_log:
        match = re.search(r'^CPP=(.*)$', config_log, re.M)
        values['CPP'] = shlex.split(match.group(1))[0] if match else ''
    ndk = Path(ndk).resolve()
    for key in ['CC', 'CPP']:
        command = shlex.split(values.get(key, ''))
        if not command or not Path(command[0]).resolve().is_relative_to(ndk):
            raise ValueError(f'Python target {key} is not from the NDK: {values.get(key)}')
    for key, value in values.items():
        if key.endswith(('_CFLAGS', '_LDFLAGS', '_LIBS')) or key in {'CPPFLAGS', 'LDFLAGS', 'CFLAGS', 'LIBS'}:
            for token in shlex.split(value):
                if token.startswith(('-I/', '-L/')) and not Path(token[2:]).resolve().is_relative_to(ndk):
                    raise ValueError(f'Host dependency in Python target {key}: {token}')
    print('Python configuration audit: Android ARM64 ABI, NDK CC/CPP and no host dependency paths')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('makefile', type=Path)
    p.add_argument('--ndk', type=Path, required=True)
    p.add_argument('--config-log', type=Path, required=True)
    a = p.parse_args()
    audit(a.makefile.read_text(), a.ndk, a.config_log.read_text())


if __name__ == '__main__':
    main()
