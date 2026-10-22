"""Wire-format checks for the PC CEB telemetry sender."""

import struct
import unittest

from tools import simulate_ceb_tlm as ceb


class CebSenderTests(unittest.TestCase):
    def test_crc_standard_vector(self):
        self.assertEqual(0x29B1, ceb.crc16_ccitt_false(b"123456789"))

    def test_default_frame_layout_and_values(self):
        values = ceb.parse_overrides([])
        frame = ceb.build_frame(0x1234, values)
        self.assertEqual(44, len(frame))
        self.assertEqual(b"\x5a\x5a\x12\x34", frame[:4])
        self.assertEqual(b"\x5a\x5a", frame[-2:])
        self.assertEqual(16384, struct.unpack_from(">h", frame, 4)[0])
        self.assertEqual(2048, struct.unpack_from(">H", frame, 12)[0])
        self.assertEqual(8992, struct.unpack_from(">h", frame, 16)[0])
        self.assertEqual(2730, struct.unpack_from(">H", frame, 26)[0])
        self.assertEqual(0, struct.unpack_from(">H", frame, 36)[0])
        self.assertEqual(0x000A, struct.unpack_from(">H", frame, 38)[0])
        self.assertEqual(ceb.crc16_ccitt_false(frame[2:40]),
                         int.from_bytes(frame[40:42], "big"))

    def test_override_and_signed_bounds(self):
        values = ceb.parse_overrides(["pitch_command=-16384", "ceb_health=0x101"])
        frame = ceb.build_frame(65535, values)
        self.assertEqual(65535, struct.unpack_from(">H", frame, 2)[0])
        self.assertEqual(-16384, struct.unpack_from(">h", frame, 28)[0])
        self.assertEqual(0x101, struct.unpack_from(">H", frame, 36)[0])
        values["pitch_command"] = 32768
        with self.assertRaises(ValueError):
            ceb.build_frame(0, values)

    def test_dry_run_without_serial_driver(self):
        self.assertEqual(0, ceb.main(["--dry-run", "--counter-start", "0xFFFF"]))


if __name__ == "__main__":
    unittest.main()
