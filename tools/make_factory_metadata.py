#!/usr/bin/env python3
"""Build the initial 20-byte metadata record for a known, linked APP image.

Only needed to program APP and metadata externally (e.g., SWD) for first installation; online CAN/UDS updating is now available in M2/M3. The file is NOT an app image.
"""
from pathlib import Path
import argparse
import struct
import zlib

MAGIC = 0x49504131
APP_BASE = 0x08020000
APP_LIMIT = 896 * 1024

def generate(image: bytes, version: int) -> bytes:
    if not 8 <= len(image) <= APP_LIMIT:
        raise ValueError("unexpected APP size")
    msp, reset = struct.unpack_from("<II", image)
    if msp != 0x2001FFC0 or (reset & 1) == 0 or not APP_BASE <= (reset & ~1) < APP_BASE + len(image):
        raise ValueError("invalid APP vector table")
    header = struct.pack("<III", len(image), zlib.crc32(image), version)
    return header + struct.pack("<II", zlib.crc32(header), MAGIC)

def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("app_bin", type=Path)
    p.add_argument("metadata_bin", type=Path)
    p.add_argument("--version", type=lambda x: int(x, 0), default=0x010000)
    a = p.parse_args()
    data = generate(a.app_bin.read_bytes(), a.version)
    a.metadata_bin.write_bytes(data)
    print(f"Factory metadata: {len(data)} bytes, target=0x08010000, version=0x{a.version:08X}")

if __name__ == "__main__":
    main()
