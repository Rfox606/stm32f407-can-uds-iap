#!/usr/bin/env python3
"""Print a build-time image manifest. This does not communicate with a MCU."""
import argparse
import hashlib
import json
import pathlib
import struct
import zlib

APP_BASE = 0x08020000
APP_MAX = 896 * 1024


def main():
    parser = argparse.ArgumentParser(description="Compute STM32F407 IAP image manifest")
    parser.add_argument("bin", type=pathlib.Path)
    parser.add_argument("--version", default="0.0.0", help="Human-readable version")
    args = parser.parse_args()
    data = args.bin.read_bytes()
    if not 8 <= len(data) <= APP_MAX:
        parser.error(f"image must be 8..{APP_MAX} bytes")
    msp, reset = struct.unpack_from("<II", data)
    if not (0x20000000 <= msp <= 0x20020000 and msp % 4 == 0):
        parser.error("invalid initial MSP for standard STM32F407 SRAM")
    if reset % 2 != 1 or not (APP_BASE <= (reset & ~1) < APP_BASE + len(data)):
        parser.error("reset handler does not point into app image in Thumb mode")
    result = {
        "file": str(args.bin),
        "version": args.version,
        "application_base": f"0x{APP_BASE:08X}",
        "size_bytes": len(data),
        "msp": f"0x{msp:08X}",
        "reset_handler": f"0x{reset:08X}",
        "crc32_zlib": f"0x{zlib.crc32(data):08X}",
        "sha256_for_build_evidence": hashlib.sha256(data).hexdigest(),
    }
    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
