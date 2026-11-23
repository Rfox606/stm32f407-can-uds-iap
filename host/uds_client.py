"""UDS上位机：仅实现本项目Bootloader的编程协议子集。

单APP槽、非认证升级：不得用于未经授权或安全关键的生产系统。
链路仅需 send(bytes) 和 recv(timeout_seconds)->bytes|None 两个方法。
"""
from __future__ import annotations

from dataclasses import dataclass
import hashlib
import math
import struct
import time
from pathlib import Path
from typing import Callable, Protocol
import zlib

APP_BASE = 0x08020000
APP_LIMIT = 896 * 1024
SRAM_MIN = 0x20000000
SRAM_MAX = 0x20020000
CHUNK_SIZE = 256
DID_VERSION = b"\x22\xF1\x90"
ROUTINE_VERIFY = b"\x31\x01\xFF\x01"


class Link(Protocol):
    def send(self, payload: bytes) -> None: ...
    def recv(self, timeout_s: float) -> bytes | None: ...


class UpgradeError(RuntimeError):
    """固件或设备状态错误；没有确认状态时不得假装升级成功。"""


class RequestTimeout(UpgradeError):
    """超时意味着设备可能已处理请求，但上位机尚未获得结果。"""


class UdsNegativeResponse(UpgradeError):
    def __init__(self, sid: int, nrc: int) -> None:
        self.sid, self.nrc = sid, nrc
        super().__init__(f"UDS 0x{sid:02X}负响应：NRC=0x{nrc:02X}")


@dataclass(frozen=True)
class Firmware:
    path: Path
    content: bytes
    crc32: int
    sha256: str
    initial_msp: int
    reset_handler: int

    @staticmethod
    def open(path: str | Path) -> "Firmware":
        p = Path(path)
        content = p.read_bytes()
        if not 8 <= len(content) <= APP_LIMIT:
            raise UpgradeError(f"固件长度必须在8～{APP_LIMIT}字节之间")
        msp, handler = struct.unpack_from("<II", content)
        if not (SRAM_MIN <= msp <= SRAM_MAX and msp % 4 == 0):
            raise UpgradeError(f"APP初始MSP无效：0x{msp:08X}")
        if not (handler & 1 and APP_BASE <= (handler & ~1) < APP_BASE + len(content)):
            raise UpgradeError(f"APP复位入口不在有效范围：0x{handler:08X}")
        return Firmware(p, content, zlib.crc32(content) & 0xFFFFFFFF,
                        hashlib.sha256(content).hexdigest(), msp, handler)

    def summary(self, version: int) -> dict[str, str | int]:
        return {"文件": str(self.path), "大小（字节）": len(self.content),
                "目标地址": f"0x{APP_BASE:08X}", "CRC32": f"0x{self.crc32:08X}",
                "SHA256": self.sha256, "MSP": f"0x{self.initial_msp:08X}",
                "复位入口": f"0x{self.reset_handler:08X}",
                "准备写入的版本": f"0x{version:08X}"}


@dataclass(frozen=True)
class Timeouts:
    response_s: float = 2.0
    pending_s: float = 6.0
    total_pending_s: float = 90.0

    def __post_init__(self) -> None:
        if min(self.response_s, self.pending_s, self.total_pending_s) <= 0:
            raise ValueError("所有超时参数必须为正数")


class UdsClient:
    def __init__(self, link: Link, timeouts: Timeouts | None = None,
                 log: Callable[[str], None] | None = None) -> None:
        self.link = link
        self.timeouts = timeouts or Timeouts()
        self.log = log if log is not None else (lambda _: None)

    def request(self, payload: bytes) -> bytes:
        if not payload:
            raise ValueError("UDS请求不能为空")
        sid = payload[0]
        self.link.send(payload)
        start = time.monotonic()
        deadline = start + self.timeouts.response_s
        pending_count = 0
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RequestTimeout(f"服务0x{sid:02X}等待响应超时；设备可能已经执行，不能盲目重复所有请求")
            response = self.link.recv(remaining)
            if response is None:
                raise RequestTimeout(f"服务0x{sid:02X}在{remaining:.2f}s内未收到响应")
            response = bytes(response)
            if len(response) >= 3 and response[0] == 0x7F:
                if response[1] != sid:
                    raise UpgradeError(f"收到不对应服务0x{sid:02X}的负响应：{response.hex(' ')}")
                if response[2] == 0x78 and len(response) == 3:
                    pending_count += 1
                    if time.monotonic() - start > self.timeouts.total_pending_s:
                        raise RequestTimeout(f"0x{sid:02X}持续收到0x78，已超出总等待预算")
                    deadline = min(time.monotonic() + self.timeouts.pending_s,
                                   start + self.timeouts.total_pending_s)
                    self.log(f"0x{sid:02X}等待处理中（第{pending_count}次0x78）")
                    continue
                raise UdsNegativeResponse(sid, response[2])
            if not response or response[0] != (sid + 0x40) & 0xFF:
                raise UpgradeError(f"0x{sid:02X}收到非预期响应：{response.hex(' ')}")
            return response

    def get_version(self) -> int:
        response = self.request(DID_VERSION)
        if len(response) != 7 or response[:3] != b"\x62\xF1\x90":
            raise UpgradeError(f"版本查询响应格式不正确：{response.hex(' ')}")
        return int.from_bytes(response[3:7], "big")

    def upgrade(self, firmware: Firmware, *, version: int,
                retries: int = 2, reset: bool = False,
                progress: Callable[[int, int], None] | None = None) -> int:
        """执行一次刷写。仅0x36具有受约束的超时重试能力。

        返回设备在复位前读回的版本。复位后的APP目前没有UDS服务，
        因此不在这里虚构复位后在线查询的结果。
        """
        if not 0 <= version <= 0xFFFFFFFF:
            raise ValueError("版本号必须为32位无符号整数")
        if not 0 <= retries <= 10:
            raise ValueError("重试次数须在0～10之间")
        total = len(firmware.content)
        r = self.request(b"\x10\x02")
        if len(r) != 6 or r[:2] != b"\x50\x02":
            raise UpgradeError("进入编程会话失败或响应长度错误")

        req = b"\x34\x00\x44" + APP_BASE.to_bytes(4, "big") + total.to_bytes(4, "big")
        r = self.request(req)
        if len(r) != 4 or r[:2] != b"\x74\x20" or int.from_bytes(r[2:4], "big") < CHUNK_SIZE + 2:
            raise UpgradeError(f"0x34返回的传输块长度不支持256字节分块：{r.hex(' ')}")

        for offset in range(0, total, CHUNK_SIZE):
            seq = (offset // CHUNK_SIZE + 1) & 0xFF
            payload = b"\x36" + bytes([seq]) + firmware.content[offset:offset + CHUNK_SIZE]
            for attempt in range(retries + 1):
                try:
                    r = self.request(payload)
                    if r != bytes([0x76, seq]):
                        raise UpgradeError(f"0x36块序号确认异常：{r.hex(' ')}")
                    break
                except RequestTimeout:
                    if attempt == retries:
                        raise
                    self.log(f"块{offset // CHUNK_SIZE + 1}确认超时；重发相同块（{attempt + 1}/{retries}）")
            if progress:
                progress(min(offset + CHUNK_SIZE, total), total)

        # OEM扩展：0x37后是完整固件CRC32和4字节版本号，必须与设备端匹配。
        r = self.request(b"\x37" + firmware.crc32.to_bytes(4, "big") +
                         version.to_bytes(4, "big"))
        if r != b"\x77":
            raise UpgradeError("0x37结束传输响应不正确")

        r = self.request(ROUTINE_VERIFY)
        if len(r) != 5 or r[:4] != b"\x71\x01\xFF\x01" or r[4] != 0:
            raise UpgradeError(f"0x31镜像校验未通过：{r.hex(' ')}")

        actual_version = self.get_version()
        if actual_version != version:
            raise UpgradeError(f"版本回读不符：预期0x{version:08X}，实际0x{actual_version:08X}")

        if reset:
            r = self.request(b"\x11\x01")
            if r != b"\x51\x01":
                raise UpgradeError("0x11复位确认异常")
            self.log("已收到复位确认；尚不能证明APP在真实硬件上成功启动")
        return actual_version
