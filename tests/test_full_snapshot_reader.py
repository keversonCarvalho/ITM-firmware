"""The host SDO reader must match the firmware's fixed 16-byte records."""

import struct
import unittest

from tools.telemetry_snapshot import catalog_metadata, decode_snapshot, format_snapshot


class FullSnapshotReaderTests(unittest.TestCase):
    def test_all_sources_and_quality(self):
        catalog = catalog_metadata()
        self.assertEqual(71, len(catalog))
        names = {name: index for index, (name, *_rest) in enumerate(catalog)}
        payload = bytearray()
        for signal_id, (_, kind, _, _, _) in enumerate(catalog):
            value = {names["AV_BATT_VOLTAGE"]: 28500,
                     names["ACT_BATT_VOLTAGE"]: 150000,
                     names["CEB_PITCH_MAIN_LVDT"]: 5000}.get(signal_id, 0)
            quality = 1 if value else 0
            payload.extend(struct.pack("<HBBIq", signal_id, kind, quality, 10, value))
        decoded = decode_snapshot(payload)
        self.assertEqual(71, len(decoded))
        output = format_snapshot(payload)
        self.assertIn("AV_BATT_VOLTAGE: 28.500 V (valid", output)
        self.assertIn("ACT_BATT_VOLTAGE: 150.000 V (valid", output)
        self.assertIn("CEB_PITCH_MAIN_LVDT: 5.000 V (valid", output)
        self.assertIn("CEB_CAN_HEALTH: <no sample> (invalid", output)

    def test_rejects_truncation_and_wrong_record_id(self):
        catalog = catalog_metadata()
        payload = bytearray().join(
            struct.pack("<HBBIq", index, metadata[1], 0, 0, 0)
            for index, metadata in enumerate(catalog))
        with self.assertRaises(ValueError):
            decode_snapshot(payload[:-1])
        payload[16] = 99
        with self.assertRaises(ValueError):
            decode_snapshot(payload)
