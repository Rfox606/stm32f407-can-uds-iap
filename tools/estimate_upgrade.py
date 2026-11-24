#!/usr/bin/env python3
"""理论CAN帧数与链路占用下界（不是实测升级时长）。

前提：经典CAN，11位ID、DLC=8（上位机和Bootloader均填充至8B）；
每帧不计位填充的111个总线位，含帧间隔。仅计算各个0x36请求
及其1个FC+1个0x76响应；不计0x10/0x34/0x37/0x31/0x11、
Flash擦写、校验、软件调度、CAN重发、STmin和诊断等待。
"""
from __future__ import annotations
import argparse
from dataclasses import dataclass

BITS_PER_CLASSIC_CAN8_FRAME = 111
MAX_IMAGE_BYTES = 896 * 1024

@dataclass(frozen=True)
class Estimate:
    image_bytes: int
    blocks: int
    frames: int
    bus_bits: int
    bus_only_ms: float
    ideal_bytes_per_s: float

def frames_for_block(data_bytes: int) -> int:
    if not 1 <= data_bytes <= 256:
        raise ValueError("0x36单块载荷必须为1～256字节")
    pdu = data_bytes + 2  # SID=0x36, blockSequenceCounter
    if pdu <= 7:
        return 2  # UDS Single Frame + 0x76 Single Frame
    remaining = pdu - 6  # ISO-TP First Frame already carries 6 bytes
    cf_count = (remaining + 6) // 7
    return 1 + cf_count + 1 + 1  # FF + CFs + FC + 0x76

def estimate(image_bytes: int, *, bitrate: int = 500_000, block_size: int = 256) -> Estimate:
    if not 8 <= image_bytes <= MAX_IMAGE_BYTES:
        raise ValueError("APP二进制长度超出8～896KiB约束")
    if bitrate <= 0 or not 1 <= block_size <= 256:
        raise ValueError("位速率和块大小不合法")
    blocks = (image_bytes + block_size - 1) // block_size
    frames = sum(frames_for_block(min(block_size, image_bytes - i * block_size))
                 for i in range(blocks))
    bits = frames * BITS_PER_CLASSIC_CAN8_FRAME
    sec = bits / bitrate
    return Estimate(image_bytes, blocks, frames, bits,
                    1000.0 * sec, image_bytes / sec)

def main() -> int:
    p = argparse.ArgumentParser(description="估算0x36阶段的经典CAN总线时间下界，不代表实际升级时长")
    p.add_argument("bytes", type=int, help="APP二进制大小（字节）")
    p.add_argument("--bitrate", type=int, default=500_000)
    p.add_argument("--chunk", type=int, default=256)
    a = p.parse_args()
    result = estimate(a.bytes, bitrate=a.bitrate, block_size=a.chunk)
    print(f"APP: {result.image_bytes}B；0x36数据块: {result.blocks}")
    print(f"0x36相关经典CAN帧: {result.frames}；假定总线位数: {result.bus_bits}")
    print(f"500kbit/s链路时间下界（只计算0x36相关帧）：{result.bus_only_ms:.3f}ms" if a.bitrate==500_000
          else f"{a.bitrate}bit/s链路时间下界：{result.bus_only_ms:.3f}ms")
    print(f"理想阶段吞吐上限：{result.ideal_bytes_per_s:.1f}字节/秒")
    print("注意：不含Flash操作、其它UDS服务、位填充、仲裁及调度；禁止表述为实测速度")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
