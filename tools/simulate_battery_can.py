"""Send Battery.dbc frames or read the CEB telemetry snapshot from the ITM.

The 0x7A0-0x7A2 query is available only in CRC-only bench application builds.
It is ordinary classic CAN, never CANopen or a firmware-update transport.
"""

import argparse
from decimal import Decimal
import importlib
import math
from pathlib import Path
import re
import secrets
import struct
import sys
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
DBC = ROOT / "config/can/Battery.dbc"
DRIVER = ROOT / ".tools/canalystii/ControlCAN.dll"
CATALOG = ROOT / "include/itm/app/telemetry_catalog.h"

import can

try:
    from .canalystii_bus import CanalystBus
except ImportError:
    from canalystii_bus import CanalystBus

MESSAGES = (
    "CAN_PACKET_BMS_TEMPS", "CAN_PACKET_BMS_V_TOT", "CAN_PACKET_BMS_I",
    "CAN_PACKET_BMS_AH_WH", "CAN_PACKET_BMS_V_CELL", "CAN_PACKET_BMS_BAL",
    "CAN_PACKET_BMS_SOC_SOH_TEMP_STAT", "CAN_PACKET_BMS_HUM",
)
SNAPSHOT_FIELDS = {
    "CAN_PACKET_BMS_TEMPS": (
        ("TEMP_CELL_COUNT", 1), ("AUX_VOLTAGE_1", 100),
        ("AUX_VOLTAGE_2", 100), ("AUX_VOLTAGE_3", 100)),
    "CAN_PACKET_BMS_V_TOT": (("VOLTAGE", 1000), ("CHARGER_VOLTAGE", 1000)),
    "CAN_PACKET_BMS_I": (("CURRENT_1", 1000), ("CURRENT_2", 1000)),
    "CAN_PACKET_BMS_AH_WH": (("AMPERE_HOURS", 1000), ("WATT_HOURS", 1000)),
    "CAN_PACKET_BMS_V_CELL": (
        ("CELL_POINT", 1), ("CELL_COUNT", 1), ("CELL_VOLTAGE_10", 1000),
        ("CELL_VOLTAGE_11", 1000), ("CELL_VOLTAGE_12", 1000)),
    "CAN_PACKET_BMS_BAL": (("BAL_CELL_COUNT", 1), ("BALANCE_STATE", 1)),
    "CAN_PACKET_BMS_SOC_SOH_TEMP_STAT": (
        ("CELL_VOLTAGE_LOW", 1000), ("CELL_VOLTAGE_HIGH", 1000),
        ("SOC", None), ("SOH", None),
        ("BATTERY_HIGH_TEMPERATURE", 1), ("STATUS_FLAGS", 1)),
    "CAN_PACKET_BMS_HUM": (("TEMP_0", 100), ("HUMIDITY", 100),
                           ("TEMP_1", 100)),
}

CEB_FIELDS = (
    ("PITCH_MAIN_LVDT", "V", -3),
    ("PITCH_MIRROR_LVDT", "V", -3),
    ("YAW_MAIN_LVDT", "V", -3),
    ("YAW_MIRROR_LVDT", "V", -3),
    ("PITCH_CURRENT", "A", -3),
    ("YAW_CURRENT", "A", -3),
    ("PITCH_ACTUATOR_TEMPERATURE", "C", -2),
    ("YAW_ACTUATOR_TEMPERATURE", "C", -2),
    ("PITCH_DRIVER_TEMPERATURE", "C", -2),
    ("YAW_DRIVER_TEMPERATURE", "C", -2),
    ("BATTERY_150V_VOLTAGE", "V", -3),
    ("SUPPLY_28V_VOLTAGE", "V", -3),
    ("PITCH_COMMAND", "mm", -3),
    ("YAW_COMMAND", "mm", -3),
    ("PITCH_POSITION", "mm", -3),
    ("YAW_POSITION", "mm", -3),
    ("HEALTH", "", 0),
    ("CAN_HEALTH", "", 0),
    ("FRAME_COUNTER", "", 0),
)


def load_dbc():
    try:
        cantools_database = importlib.import_module("cantools.database")
    except (ModuleNotFoundError, ImportError, AttributeError) as exc:
        raise SystemExit("Cannot load the cantools DBC encoder. Install it with "
                         "python -m pip install -r tools/requirements-dbc.txt "
                         f"({exc})") from exc
    database = cantools_database.load_file(str(DBC), strict=True)
    messages = {message.name: message for message in database.messages}
    if set(messages) != set(MESSAGES) or not all(message.is_extended_frame
                                                  for message in messages.values()):
        raise ValueError("Battery.dbc messages or frame format changed; review simulator")
    for name, data_hex in (
        ("CAN_PACKET_BMS_V_TOT", "41e400000000001e"),
        ("CAN_PACKET_BMS_SOC_SOH_TEMP_STAT", "0ce4100480ff2aa5"),
    ):
        message = messages[name]
        data = bytes.fromhex(data_hex)
        decoded = message.decode(data, decode_choices=False)
        if message.encode(decoded, strict=False) != data:
            raise ValueError(f"Installed cantools does not match the DBC reference: {name}")
    return messages


def sample_signals(voltage, soc, strict_limits=False):
    """Representative nonzero signals; Battery.dbc has provisional min/max.

    In particular SOC, cell voltage, temperature and humidity have [0|1]
    bounds incompatible with their scales and normal physical values. We keep
    the DBC's bit positions, types, endianness and scales, but bypass bounds.
    """
    samples = {
        "CAN_PACKET_BMS_TEMPS": {
            "NoOfCells": 48,
            "auxVoltagesIndividual1": 3.30,
            "auxVoltagesIndividual2": 3.31,
            "auxVoltagesIndividual3": 3.29,
        },
        "CAN_PACKET_BMS_V_TOT": {
            "packVoltage": voltage,
            "chargerVoltage": round(voltage),
        },
        "CAN_PACKET_BMS_I": {
            "packCurrent1": 0.70,
            "packCurrent2": 0.60,
        },
        "CAN_PACKET_BMS_AH_WH": {
            "Ah_Counter": 0.50,
            "Wh_Counter": 0.80,
        },
        "CAN_PACKET_BMS_V_CELL": {
            "cellPoint": 3,
            "NoOfCells": 48,
            "cellVoltage10": 3.300,
            "cellVoltage11": 3.310,
            "cellVoltage12": 3.290,
        },
        "CAN_PACKET_BMS_BAL": {
            "NoOfCells": 48,
            "bal_state": 1,
        },
        "CAN_PACKET_BMS_SOC_SOH_TEMP_STAT": {
            "cellVoltageLow": 3.250,
            "cellVoltageHigh": 3.400,
            "SOC": soc,
            "SOH": 96.5,
            "tBattHi": 25,
            "BitF": 1,
        },
        "CAN_PACKET_BMS_HUM": {
            "CAN_PACKET_BMS_TEMP0": 24.75,
            "CAN_PACKET_BMS_HUM_HUM": 45.20,
            "CAN_PACKET_BMS_HUM_TEMP1": 25.10,
        },
    }
    if strict_limits:
        samples["CAN_PACKET_BMS_TEMPS"].update(
            NoOfCells=1, auxVoltagesIndividual1=0,
            auxVoltagesIndividual2=0, auxVoltagesIndividual3=0)
        samples["CAN_PACKET_BMS_V_TOT"]["chargerVoltage"] = 1
        samples["CAN_PACKET_BMS_V_CELL"].update(
            cellVoltage10=0.8, cellVoltage11=0.9, cellVoltage12=0.85)
        samples["CAN_PACKET_BMS_BAL"]["NoOfCells"] = 0
        samples["CAN_PACKET_BMS_SOC_SOH_TEMP_STAT"].update(
            cellVoltageLow=0.8, cellVoltageHigh=0.9, tBattHi=1)
        samples["CAN_PACKET_BMS_HUM"].update(
            CAN_PACKET_BMS_TEMP0=0.6, CAN_PACKET_BMS_HUM_HUM=0.7,
            CAN_PACKET_BMS_HUM_TEMP1=0.8)
    return samples


def build_frames(messages, voltage, soc, strict_limits=False):
    samples = sample_signals(voltage, soc, strict_limits)
    frames = []
    for name in MESSAGES:
        message = messages[name]
        values = samples[name]
        if set(values) != {signal.name for signal in message.signals}:
            raise ValueError(f"Signals changed in Battery.dbc: {name}")
        data = message.encode(values, strict=strict_limits)
        decoded = message.decode(data, decode_choices=False)
        if len(data) != message.length:
            raise ValueError(f"Wrong DBC length: {name}")
        for signal in message.signals:
            expected = values[signal.name]
            actual = decoded[signal.name]
            tolerance = (max(abs(signal.scale) / 2 + 1e-8, 1e-5)
                         if not signal.is_float else max(abs(expected) * 1e-6, 1e-5))
            if not math.isclose(actual, expected, rel_tol=0, abs_tol=tolerance):
                raise ValueError(f"DBC round-trip mismatch: {name}.{signal.name} "
                                 f"expected={expected} decoded={actual}")
        frames.append((message, data, decoded))
    return frames


def out_of_range_signals(messages, samples):
    result = []
    for name in MESSAGES:
        for signal in messages[name].signals:
            value = samples[name][signal.name]
            if ((signal.minimum is not None and value < signal.minimum) or
                    (signal.maximum is not None and value > signal.maximum)):
                result.append(f"{name}.{signal.name}")
    return result


def catalog_ids():
    enum = CATALOG.read_text(encoding="utf-8").split("typedef enum {", 1)[1]
    enum = enum.split("ITM_SIGNAL_COUNT", 1)[0]
    names = re.findall(r"\bITM_SIGNAL_[A-Z0-9_]+\b", enum)
    if len(names) != 71 or len(set(names)) != len(names):
        raise ValueError("Firmware telemetry catalog changed; review snapshot probe")
    return {name: index for index, name in enumerate(names)}


def parse_ceb_snapshot(encoded):
    """Decode atomic SDO 0x2111:0, retaining invalid/stale quality per signal."""
    try:
        version, payload = encoded.decode("ascii").split(";", 1)
        if version != "1":
            raise ValueError(f"unsupported CEB snapshot version {version!r}")
        entries = payload.split(",")
        if len(entries) != len(CEB_FIELDS):
            raise ValueError(f"expected {len(CEB_FIELDS)} CEB signals, got {len(entries)}")
        values = []
        for entry in entries:
            value_text, quality_text = entry.split(":", 1)
            value, quality = int(value_text), int(quality_text)
            if quality not in (0, 1, 2):
                raise ValueError(f"invalid CEB signal quality {quality}")
            values.append((value, quality))
        return values
    except (UnicodeDecodeError, ValueError) as exc:
        raise ValueError(f"invalid CEB snapshot SDO payload: {exc}") from exc


def show_ceb_snapshot(driver, channel, node=10):
    try:
        from .itm_od import SDOClient
    except ImportError:
        from itm_od import SDOClient

    ids = catalog_ids()
    first = ids["ITM_SIGNAL_CEB_PITCH_MAIN_LVDT"]
    if any(ids[f"ITM_SIGNAL_CEB_{suffix}"] != first + index
           for index, (suffix, _, _) in enumerate(CEB_FIELDS)):
        raise ValueError("CEB snapshot order differs from firmware catalog")
    with CanalystBus(driver, channel) as bus:
        client = SDOClient(bus, can.Message, node)
        values = parse_ceb_snapshot(client.read(0x2111, 0))
    print(f"CEB snapshot via CANopen RET, converter channel {channel}: "
          f"{len(values)} signals")
    for (suffix, unit, exponent), (value, quality) in zip(CEB_FIELDS, values):
        physical = Decimal(value).scaleb(exponent)
        status = ("invalid", "valid", "stale")[quality]
        print(f"  CEB.{suffix}: {physical:f}{(' ' + unit) if unit else ''} ({status})")
    return values


def simulate_and_show_ceb(driver, channel, node, port, frames):
    try:
        from . import simulate_ceb_tlm
    except ImportError:
        import simulate_ceb_tlm

    failures = []

    def transmit():
        try:
            simulate_ceb_tlm.send_frames(port, frames, 0,
                simulate_ceb_tlm.parse_overrides([]), False)
        except BaseException as exc:
            failures.append(exc)

    worker = threading.Thread(target=transmit, name="ceb-rs485-simulator")
    worker.start()
    try:
        time.sleep(0.2)
        values = show_ceb_snapshot(driver, channel, node)
    finally:
        worker.join()
    if failures:
        raise RuntimeError(f"CEB serial transmission failed: {failures[0]}")
    if any(quality != 1 for _, quality in values):
        raise RuntimeError("CEB simulator sent frames, but one or more snapshot "
                           "signals were not valid during the CANopen read")


def round_away(value):
    return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)


def expected_snapshot(frames, battery_bus):
    ids = catalog_ids()
    prefix = "AV_BATT" if battery_bus == "av" else "ACT_BATT"
    expected = []
    for message, data, decoded in frames:
        fields = SNAPSHOT_FIELDS[message.name]
        if len(fields) != len(message.signals):
            raise ValueError(f"Snapshot mapping incomplete: {message.name}")
        for index, (signal, (suffix, factor)) in enumerate(zip(message.signals, fields)):
            name = f"ITM_SIGNAL_{prefix}_{suffix}"
            if name not in ids:
                raise ValueError(f"Snapshot signal missing: {name}")
            if factor is None:
                raw = data[4 if suffix == "SOC" else 5]
                value = ((raw * 10000 + 127) // 255 if suffix == "SOC"
                         else (raw * 3922 + 50) // 100)
            else:
                value = round_away(decoded[signal.name] * factor)
            exponent = -2 if factor is None else {1: 0, 100: -2, 1000: -3}[factor]
            expected.append((ids[name], name, value, message.name,
                             signal.name, signal.unit or "", exponent))
    if len(expected) != 26 or len({item[0] for item in expected}) != 26:
        raise ValueError("Expected 26 distinct battery snapshot signals")
    return expected


def query_signal(bus, signal_id):
    nonce = secrets.token_bytes(2)
    header = struct.pack("<H", signal_id) + nonce
    bus.send(can.Message(arbitration_id=0x7A0, is_extended_id=False,
                         data=b"ITMS" + header))
    deadline = time.monotonic() + 3.0
    metadata = None
    value = None
    while time.monotonic() < deadline and (metadata is None or value is None):
        message = bus.recv(max(0.01, deadline - time.monotonic()))
        if message is None or message.is_extended_id or len(message.data) != 8:
            continue
        if message.arbitration_id == 0x7A3 and message.data[:4] == header:
            metadata = message.data[4:]
        elif message.arbitration_id == 0x7A4:
            value = struct.unpack("<q", message.data)[0]
    if metadata is None or value is None:
        raise TimeoutError(f"ITM did not return snapshot signal {signal_id}")
    return metadata, value


def query(bus):
    nonce = secrets.token_bytes(4)
    bus.send(can.Message(arbitration_id=0x7A0, is_extended_id=False,
                         data=b"ITM?" + nonce))
    deadline = time.monotonic() + 3.0
    count = None
    sample = None
    while time.monotonic() < deadline and (count is None or sample is None):
        message = bus.recv(max(0.01, deadline - time.monotonic()))
        if message is None or message.is_extended_id or len(message.data) != 8:
            continue
        if message.arbitration_id == 0x7A1 and message.data[:4] == nonce:
            count = struct.unpack_from("<I", message.data, 4)[0]
        elif message.arbitration_id == 0x7A2:
            sample = struct.unpack_from("<IHBB", message.data)
    if count is None or sample is None:
        raise TimeoutError("ITM did not answer bench query on this battery CAN bus")
    return count, sample


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bus", choices=("av", "acc", "ceb"), required=True)
    parser.add_argument("--channel", type=int, choices=(1, 2),
                        help="CANalyst-II channel; defaults to 2 for AV, 1 for ACC")
    parser.add_argument("--driver", type=Path, default=DRIVER)
    parser.add_argument("--voltage", type=float)
    parser.add_argument("--soc", type=float)
    parser.add_argument("--cycles", type=int, default=1)
    parser.add_argument("--node", type=int, default=10,
                        help="CANopen Node-ID for CEB snapshot (default: 10)")
    parser.add_argument("--include-ceb", action="store_true",
                        help="after battery test, also read CEB snapshot via CANopen RET")
    parser.add_argument("--ret-channel", type=int, choices=(1, 2), default=2,
                        help="CANalyst-II channel carrying CAN RET (default: 2)")
    parser.add_argument("--simulate-ceb", action="store_true",
                        help="with --bus ceb, transmit sample RS-485 frames while reading")
    parser.add_argument("--ceb-port", default="COM3",
                        help="USB/RS-485 port for --simulate-ceb (default: COM3)")
    parser.add_argument("--ceb-frames", type=int, default=200,
                        help="frames sent by --simulate-ceb (default: 200)")
    parser.add_argument("--strict-dbc-limits", action="store_true",
                        help="Use the literal DBC min/max limits, even where they are unrealistic")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show all DBC frames and decoded signals without CAN transmission")
    args = parser.parse_args()
    if not 1 <= args.node <= 127:
        parser.error("--node must be 1..127")
    if args.ceb_frames < 1 or (args.simulate_ceb and args.bus != "ceb"):
        parser.error("--ceb-frames must be positive and --simulate-ceb needs --bus ceb")
    if args.channel is None:
        args.channel = 2 if args.bus in ("av", "ceb") else 1
    if args.bus == "ceb":
        if args.dry_run:
            for suffix, unit, exponent in CEB_FIELDS:
                print(f"CEB.{suffix}: unit={unit or '-'} scale=10^{exponent}")
            return
        try:
            if args.simulate_ceb:
                simulate_and_show_ceb(args.driver, args.channel, args.node,
                                      args.ceb_port, args.ceb_frames)
            else:
                show_ceb_snapshot(args.driver, args.channel, args.node)
        except (OSError, ValueError, TimeoutError, RuntimeError) as exc:
            raise SystemExit(f"CEB snapshot read failed: {exc}") from exc
        return
    if args.voltage is None:
        args.voltage = 28.5 if args.bus == "av" else 150.0
    if args.soc is None:
        args.soc = 0.5 if args.strict_dbc_limits else 55.0
    if not 0 <= args.soc <= 100 or args.cycles < 1:
        parser.error("SOC must be 0..100 and cycles must be positive")
    if args.strict_dbc_limits and args.soc > 1:
        parser.error("the DBC limits SOC to 0..1%; use --soc <= 1 or omit it")
    if not 0 <= args.voltage <= (40 if args.bus == "av" else 200):
        parser.error("voltage is outside the firmware telemetry catalog range")
    messages = load_dbc()
    frames = build_frames(messages, args.voltage, args.soc, args.strict_dbc_limits)
    expected_signals = expected_snapshot(frames, args.bus)
    if not args.strict_dbc_limits:
        outside = out_of_range_signals(messages, sample_signals(args.voltage, args.soc))
        if outside:
            print(f"Note: {len(outside)} physical sample values exceed provisional DBC "
                  "min/max; use --strict-dbc-limits for literal bounds", file=sys.stderr)
    if args.dry_run:
        for message, data, decoded in frames:
            print(f"{message.name}: ID=0x{message.frame_id:X} extended=1 "
                  f"DLC={message.length} data={data.hex()} signals={decoded}")
        return
    with CanalystBus(args.driver, args.channel) as bus:
        before, _ = query(bus)
        for _ in range(args.cycles):
            for message, data, _ in frames:
                bus.send(can.Message(arbitration_id=message.frame_id,
                                     is_extended_id=message.is_extended_frame,
                                     data=data))
                time.sleep(0.01)
        time.sleep(0.05)
        after, sample = query(bus)
        verified = 0
        stale = 0
        snapshot_rows = []
        expected_bus = 0 if args.bus == "av" else 1
        for (signal_id, name, expected_value, message_name,
             signal_name, unit, expected_exponent) in expected_signals:
            metadata, actual_value = query_signal(bus, signal_id)
            quality, exponent_byte, signal_type, source = metadata
            exponent = struct.unpack("b", bytes((exponent_byte,)))[0]
            if quality not in (1, 2) or signal_type == 0xFF or source != expected_bus:
                raise SystemExit(f"Snapshot signal unavailable: {name} "
                                 f"quality={quality} source={source}")
            if exponent != expected_exponent:
                raise SystemExit(f"Snapshot scale mismatch: {name} "
                                 f"expected=10^{expected_exponent} actual=10^{exponent}")
            if actual_value != expected_value:
                raise SystemExit(f"Snapshot value mismatch: {name} "
                                 f"expected={expected_value} actual={actual_value}")
            snapshot_rows.append((message_name, signal_name, actual_value,
                                  exponent, unit, quality))
            verified += 1
            stale += quality == 2
    voltage_mv, soc_centi, flags, reported_bus = sample
    status_data = next(data for message, data, _ in frames
                       if message.name == "CAN_PACKET_BMS_SOC_SOH_TEMP_STAT")
    expected_soc = (status_data[4] * 10000 + 127) // 255
    expected_count = args.cycles * len(frames)
    if reported_bus != expected_bus or after - before < expected_count:
        raise SystemExit(f"Receive/DBC check failed: count {before}->{after}, bus={reported_bus}")
    encoded_voltage = next(decoded["packVoltage"] for message, _, decoded in frames
                           if message.name == "CAN_PACKET_BMS_V_TOT")
    if flags != 3 or abs(voltage_mv - round(encoded_voltage * 1000)) > 2 or soc_centi != expected_soc:
        raise SystemExit(f"Telemetry check failed: voltage={voltage_mv} mV, "
                         f"SOC={soc_centi / 100:.2f}%, flags={flags}")
    print(f"{args.bus.upper()} CAN OK on converter channel {args.channel}: "
          f"{after - before} DBC frames decoded; voltage={voltage_mv} mV, "
          f"SOC={soc_centi / 100:.2f}%; snapshot={verified} signals "
          f"({stale} stale)")
    previous_message = None
    for message_name, signal_name, value, exponent, unit, quality in snapshot_rows:
        if message_name != previous_message:
            print(f"  {message_name}:")
            previous_message = message_name
        physical = Decimal(value).scaleb(exponent)
        suffix = f" {unit}" if unit else ""
        freshness = " (stale)" if quality == 2 else ""
        print(f"    {signal_name}: {physical:f}{suffix}{freshness}")
    if args.include_ceb:
        show_ceb_snapshot(args.driver, args.ret_channel, args.node)


if __name__ == "__main__":
    main()
