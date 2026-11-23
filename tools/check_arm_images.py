#!/usr/bin/env python3
"""Check linked ARM image addresses and first vector words (not MCU execution)."""
from pathlib import Path
import argparse
import struct
import sys

REGIONS = {'boot': (0x08000000, 0x10000), 'app': (0x08020000, 0xE0000)}

def check(root: Path) -> None:
    for name, (base, size) in REGIONS.items():
        path = root / f'{name}.bin'
        data = path.read_bytes()
        if len(data) < 8 or len(data) > size:
            raise ValueError(f'{name}: illegal size {len(data)} (max {size})')
        msp, reset = struct.unpack_from('<II', data)
        if msp != 0x20020000:
            raise ValueError(f'{name}: unexpected initial MSP 0x{msp:08X}')
        if (reset & 1) != 1 or not base <= (reset & ~1) < base + len(data):
            raise ValueError(f'{name}: reset PC out of image 0x{reset:08X}')
        print(f'{name}: {len(data)} bytes, MSP=0x{msp:08X}, Reset=0x{reset:08X}; PASS')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('build_dir', type=Path)
    args = parser.parse_args()
    try:
        check(args.build_dir)
    except (OSError, ValueError) as exc:
        print(f'FAILED: {exc}', file=sys.stderr)
        sys.exit(1)
