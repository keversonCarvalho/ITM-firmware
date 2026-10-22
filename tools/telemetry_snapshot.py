"""Decode the ITM's read-only complete snapshot at CANopen SDO 0x2112:0."""

from decimal import Decimal
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
CATALOG_HEADER = ROOT / "include/itm/app/telemetry_catalog.h"
CATALOG_SOURCE = ROOT / "src/app/telemetry_catalog.c"
RECORD = struct.Struct("<HBBIq")
UNIT_LABELS = {
    "VOLT": "V", "PERCENT": "%", "CELSIUS": "C", "STATE": "",
    "AMPERE": "A", "AMPERE_HOUR": "Ah", "WATT_HOUR": "Wh",
    "COUNT": "", "MILLIMETER": "mm", "NONE": "",
}
TYPE_IDS = {"I32": 0, "U32": 1, "BOOL": 2, "I64": 3}
QUALITY = ("invalid", "valid", "stale")


def catalog_metadata():
    """Resolve catalog order and display scales from the same source as firmware."""
    header = CATALOG_HEADER.read_text(encoding="utf-8")
    enum = header.split("typedef enum {", 1)[1].split("ITM_SIGNAL_COUNT", 1)[0]
    names = re.findall(r"\bITM_SIGNAL_[A-Z0-9_]+\b", enum)
    source = CATALOG_SOURCE.read_text(encoding="utf-8")
    metadata = {}
    explicit = re.compile(
        r"\{(ITM_SIGNAL_[A-Z0-9_]+),\s*ITM_SIGNAL_TYPE_([A-Z0-9_]+),\s*"
        r"ITM_UNIT_([A-Z0-9_]+),\s*ITM_SOURCE_([A-Z0-9_]+),\s*(-?\d+)\s*,")
    for name, kind, unit, origin, exponent in explicit.findall(source):
        metadata[name] = (TYPE_IDS[kind], UNIT_LABELS[unit], origin, int(exponent))
    battery = re.compile(
        r"BATT_META\(PREFIX, SOURCE, ([A-Z0-9_]+), ([A-Z0-9_]+), "
        r"([A-Z0-9_]+), (-?\d+),")
    for suffix, kind, unit, exponent in battery.findall(source):
        for prefix in ("AV_BATT", "ACT_BATT"):
            metadata[f"ITM_SIGNAL_{prefix}_{suffix}"] = (
                TYPE_IDS[kind], UNIT_LABELS[unit], prefix, int(exponent))
    if len(names) != 71 or len(set(names)) != len(names) or set(names) != set(metadata):
        raise ValueError("Local telemetry catalog differs from the 71-signal SDO format")
    return [(name.removeprefix("ITM_SIGNAL_"), *metadata[name]) for name in names]


def decode_snapshot(payload, catalog=None):
    catalog = catalog if catalog is not None else catalog_metadata()
    if len(payload) != len(catalog) * RECORD.size:
        raise ValueError(f"Expected {len(catalog) * RECORD.size} snapshot bytes, "
                         f"received {len(payload)}")
    decoded = []
    for expected_id, metadata in enumerate(catalog):
        signal_id, kind, quality, timestamp_ms, value = RECORD.unpack_from(
            payload, expected_id * RECORD.size)
        if signal_id != expected_id or kind != metadata[1] or quality >= len(QUALITY):
            raise ValueError(f"Invalid snapshot record {expected_id}: "
                             f"id={signal_id}, type={kind}, quality={quality}")
        decoded.append((metadata, value, quality, timestamp_ms))
    return decoded


def format_snapshot(payload):
    rows = decode_snapshot(payload)
    lines = [f"ITM snapshot via CANopen SDO 0x2112:0: {len(rows)} signals"]
    for (name, _, unit, origin, exponent), value, quality, timestamp_ms in rows:
        physical = Decimal(value).scaleb(exponent)
        formatted = (f"{physical:f}{(' ' + unit) if unit else ''}"
                     if quality else "<no sample>")
        lines.append(f"  {name}: {formatted} ({QUALITY[quality]}, "
                     f"source={origin}, last_ms={timestamp_ms})")
    return "\n".join(lines)
