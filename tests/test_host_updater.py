"""M3上位机实际软件测试：C固件UDS核心桥接，不涉及实物CAN/Flash。"""
from __future__ import annotations
from pathlib import Path
import os
import struct
import tempfile
import unittest

from host.bridge_link import CBridgeLink
from host.uds_client import (
    APP_BASE, CHUNK_SIZE, Firmware, RequestTimeout, Timeouts, UdsClient,
    UdsNegativeResponse, UpgradeError
)

BRIDGE = os.environ.get("IAP_UDS_BRIDGE")


def image_file(directory: Path, length: int) -> Path:
    data = bytearray(((i * 71 + 19) & 0xFF) for i in range(length))
    struct.pack_into("<II", data, 0, 0x2001FFC0, APP_BASE + 0x21)
    p = directory / f"app_{length}.bin"
    p.write_bytes(data)
    return p


class Stub:
    """Only checks Python request/response logic; the C bridge tests real C code."""
    def __init__(self, replies: list[bytes | None]) -> None:
        self.replies = list(replies)
        self.sent: list[bytes] = []

    def send(self, payload: bytes) -> None:
        self.sent.append(payload)

    def recv(self, timeout_s: float) -> bytes | None:
        if not self.replies:
            return None
        return self.replies.pop(0)


class FirmwareTests(unittest.TestCase):
    def test_manifest_and_invalid_vectors(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            p = image_file(Path(d), 530)
            f = Firmware.open(p)
            self.assertEqual(len(f.content), 530)
            self.assertEqual(f.initial_msp, 0x2001FFC0)
            self.assertEqual(f.reset_handler, APP_BASE + 0x21)
            self.assertEqual(len(f.sha256), 64)
            data = bytearray(p.read_bytes())
            struct.pack_into("<I", data, 4, 0x08000001)
            p.write_bytes(data)
            with self.assertRaises(UpgradeError):
                Firmware.open(p)

    def test_deferred_responses_and_negative(self) -> None:
        l = Stub([b"\x7F\x31\x78", b"\x7F\x31\x78", b"\x71\x01\xFF\x01\x00"])
        c = UdsClient(l, Timeouts(1, 1, 5))
        self.assertEqual(c.request(b"\x31\x01\xFF\x01"), b"\x71\x01\xFF\x01\x00")
        self.assertEqual(l.sent, [b"\x31\x01\xFF\x01"])
        l2 = Stub([b"\x7F\x36\x73"])
        with self.assertRaises(UdsNegativeResponse) as ctx:
            UdsClient(l2).request(b"\x36\x05\xAA")
        self.assertEqual(ctx.exception.nrc, 0x73)

    def test_no_blind_retry_on_download_timeout(self) -> None:
        link = Stub([b"\x62\xF1\x92\x00\x00\x00\x01",
                     b"\x50\x02\x00\x32\x01\xF4", None])
        with tempfile.TemporaryDirectory() as d:
            f = Firmware.open(image_file(Path(d), 256))
            with self.assertRaises(RequestTimeout):
                UdsClient(link).upgrade(f, version=1, retries=3)
        self.assertEqual(len(link.sent), 3)
        self.assertEqual(link.sent[0], b"\x22\xF1\x92")
        self.assertEqual(link.sent[1], b"\x10\x02")
        self.assertEqual(link.sent[2][:3], b"\x34\x00\x44")


@unittest.skipUnless(BRIDGE, "需要C协议桥接程序，CTest配置IAP_UDS_BRIDGE")
class RealCBridgeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.directory = Path(self.tmp.name)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_complete_upgrade_and_version(self) -> None:
        f = Firmware.open(image_file(self.directory, 530))
        with CBridgeLink(BRIDGE) as link:
            c = UdsClient(link)
            initial = c.get_version()
            self.assertEqual(initial, 0)
            progress = []
            actual = c.upgrade(f, version=0x01020304, reset=True,
                               progress=lambda done, total: progress.append((done, total)))
            self.assertEqual(actual, 0x01020304)
            self.assertEqual(progress, [(256,530),(512,530),(530,530)])

    def test_confirmation_lost_same_block_retry(self) -> None:
        f = Firmware.open(image_file(self.directory, 530))
        with CBridgeLink(BRIDGE, "--drop-36-once") as link:
            c = UdsClient(link, Timeouts(0.2, 0.3, 5))
            self.assertEqual(c.upgrade(f, version=0x010002, retries=2), 0x010002)

    def test_corrupt_flash_rejected_before_commit(self) -> None:
        f = Firmware.open(image_file(self.directory, 530))
        with CBridgeLink(BRIDGE, "--corrupt-before-finish") as link:
            c = UdsClient(link)
            with self.assertRaises(UdsNegativeResponse) as ctx:
                c.upgrade(f, version=7)
            self.assertEqual(ctx.exception.sid, 0x37)
            self.assertEqual(ctx.exception.nrc, 0x72)
            self.assertEqual(c.get_version(), 0)

    def test_volatile_state_reset_after_two_blocks_then_reflash(self) -> None:
        f = Firmware.open(image_file(self.directory, 780))
        with CBridgeLink(BRIDGE, "--reset-after-second-block") as link:
            c = UdsClient(link)
            with self.assertRaises(UdsNegativeResponse):
                c.upgrade(f, version=2)
            self.assertEqual(c.get_version(), 0)
            link.reset()
            self.assertEqual(c.upgrade(f, version=3), 3)

    def test_block_sequence_rollover_255_to_0(self) -> None:
        # 257 x 256 byte requests exercise 0xFF -> 0x00 -> 0x01.
        f = Firmware.open(image_file(self.directory, CHUNK_SIZE * 257))
        with CBridgeLink(BRIDGE) as link:
            c = UdsClient(link)
            self.assertEqual(c.upgrade(f, version=0x11223344), 0x11223344)


if __name__ == "__main__":
    unittest.main()
