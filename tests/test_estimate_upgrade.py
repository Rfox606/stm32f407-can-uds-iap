"""校验理论计算所采用的CAN/ISO-TP帧数，非硬件计时。"""
import unittest
from tools.estimate_upgrade import estimate, frames_for_block

class TheoreticalTimingTests(unittest.TestCase):
    def test_full_256_byte_block(self):
        self.assertEqual(frames_for_block(256), 39)
    def test_short_block(self):
        self.assertEqual(frames_for_block(1), 2)
    def test_compiled_app_size(self):
        r = estimate(3188)
        self.assertEqual(r.blocks, 13)
        self.assertEqual(r.frames, 487)
        self.assertAlmostEqual(r.bus_only_ms, 108.114)
    def test_invalid_size(self):
        with self.assertRaises(ValueError):
            estimate(896*1024+1)

if __name__ == "__main__":
    unittest.main()
