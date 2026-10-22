"""Checks for the CEB SDO snapshot format used by the combined probe."""

import unittest

from tools import simulate_battery_can as probe


class CebSnapshotReaderTests(unittest.TestCase):
    def test_reads_all_cebs_in_catalog_order(self):
        encoded = b"1;" + b",".join(
            f"{index - 9}:{index % 3}".encode("ascii") for index in range(19))
        values = probe.parse_ceb_snapshot(encoded)
        self.assertEqual(19, len(values))
        self.assertEqual((-9, 0), values[0])
        self.assertEqual((9, 0), values[-1])
        ids = probe.catalog_ids()
        first = ids["ITM_SIGNAL_CEB_PITCH_MAIN_LVDT"]
        self.assertEqual(list(range(first, first + 19)), [
            ids[f"ITM_SIGNAL_CEB_{suffix}"]
            for suffix, _, _ in probe.CEB_FIELDS])

    def test_rejects_truncated_or_unknown_snapshot(self):
        with self.assertRaises(ValueError):
            probe.parse_ceb_snapshot(b"1;42:1")
        with self.assertRaises(ValueError):
            probe.parse_ceb_snapshot(b"2;" + b",".join([b"0:1"] * 19))
        with self.assertRaises(ValueError):
            probe.parse_ceb_snapshot(b"1;" + b",".join([b"0:3"] * 19))


if __name__ == "__main__":
    unittest.main()
