"""Inspect the ITM CANopen OD and persist its configurable parameters by SDO.

Management objects are intentionally read-only. Firmware updates use
update_itm.py; this tool only changes Node-ID and producer heartbeat.
"""

import argparse
import configparser
from contextlib import contextmanager
from pathlib import Path
import re
import struct
import time


ROOT = Path(__file__).resolve().parents[1]
EDS = ROOT / "config/canopen/itm-100.eds"
DEFAULT_DRIVER = ROOT / ".tools/canalystii/ControlCAN.dll"
OBJECT_SECTION = re.compile(r"^([0-9A-Fa-f]{4})(?:sub([0-9]+))?$")


class SDOError(RuntimeError):
    pass


class SDOClient:
    def __init__(self, bus, message_class, node_id):
        self.bus = bus
        self.message_class = message_class
        self.node_id = node_id

    def exchange(self, payload, index=None, subindex=None, timeout=3):
        request = self.message_class(arbitration_id=0x600 + self.node_id,
                                     is_extended_id=False, data=payload)
        self.bus.send(request, timeout=2)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            response = self.bus.recv(max(0, deadline - time.monotonic()))
            if (response is None or response.is_extended_id or
                    response.is_remote_frame or response.is_error_frame or
                    response.arbitration_id != 0x580 + self.node_id or
                    len(response.data) != 8):
                continue
            data = bytes(response.data)
            if index is not None and data[1:4] != struct.pack("<HB", index, subindex):
                continue
            if data[0] == 0x80:
                code = struct.unpack_from("<I", data, 4)[0]
                raise SDOError(f"SDO {index:#06x}:{subindex} abort {code:#010x}")
            return data
        raise TimeoutError(f"ITM node {self.node_id} did not answer SDO")

    def read(self, index, subindex=0):
        response = self.exchange(b"\x40" + struct.pack("<HB", index, subindex) + bytes(4),
                                 index, subindex)
        command = response[0]
        if command & 0xE0 != 0x40:
            raise SDOError(f"Unexpected upload response: {response.hex()}")
        if command & 2:
            size = 4 - ((command >> 2) & 3) if command & 1 else 4
            return response[4:4 + size]
        expected = struct.unpack_from("<I", response, 4)[0] if command & 1 else None
        output = bytearray()
        toggle = 0
        while True:
            segment = self.exchange(bytes([0x60 | (toggle << 4)]) + bytes(7))
            command = segment[0]
            if command & 0xE0 or ((command >> 4) & 1) != toggle:
                raise SDOError(f"Unexpected upload segment: {segment.hex()}")
            output.extend(segment[1:8 - ((command >> 1) & 7)])
            if command & 1:
                if expected is not None and len(output) != expected:
                    raise SDOError("Segmented upload length mismatch")
                return bytes(output)
            toggle ^= 1

    def write(self, index, subindex, value, timeout=3):
        if not 1 <= len(value) <= 4:
            raise ValueError("Only 1-to-4-byte expedited writes are supported")
        command = 0x23 | ((4 - len(value)) << 2)
        response = self.exchange(bytes([command]) + struct.pack("<HB", index, subindex) +
                                 value.ljust(4, b"\0"), index, subindex, timeout)
        if response[0] != 0x60:
            raise SDOError(f"Unexpected download response: {response.hex()}")


def object_entries():
    eds = configparser.ConfigParser(interpolation=None)
    with EDS.open(encoding="utf-8") as handle:
        eds.read_file(handle)
    entries = []
    for section in eds.sections():
        match = OBJECT_SECTION.fullmatch(section)
        if not match or not eds.has_option(section, "DataType"):
            continue
        entries.append((int(match.group(1), 16), int(match.group(2) or 0),
                        int(eds.get(section, "DataType"), 0),
                        eds.get(section, "AccessType", fallback="?"),
                        eds.get(section, "ParameterName", fallback="")))
    return sorted(entries)


def parse_object(text):
    parts = text.split(":")
    if len(parts) > 2:
        raise argparse.ArgumentTypeError("Use INDEX or INDEX:SUB, for example 0x1018:4")
    try:
        index = int(parts[0], 16)
        subindex = int(parts[1], 0) if len(parts) == 2 else 0
    except ValueError as error:
        raise argparse.ArgumentTypeError("Invalid OD address") from error
    if not 0 <= index <= 0xFFFF or not 0 <= subindex <= 0xFF:
        raise argparse.ArgumentTypeError("OD address out of range")
    return index, subindex


def display(value, data_type):
    if data_type == 9:
        return value.rstrip(b"\0").decode("ascii", errors="replace")
    if data_type in (5, 6, 7):
        return str(int.from_bytes(value, "little"))
    if data_type == 10:
        return f"<{len(value)} binary bytes; use the snapshot command>"
    return value.hex()


@contextmanager
def open_bus(args):
    import can
    if args.interface == "canalystii":
        from canalystii_bus import CanalystBus
        if not args.driver.is_file():
            raise FileNotFoundError(f"ControlCAN.dll not found: {args.driver}")
        bus = CanalystBus(args.driver, int(args.channel))
    else:
        bus = can.Bus(interface=args.interface, channel=args.channel, bitrate=500000)
    with bus:
        yield bus, can.Message


def show(client, entries):
    for index, subindex, data_type, access, name in entries:
        try:
            value = display(client.read(index, subindex), data_type)
        except SDOError as error:
            value = f"<unavailable: {error}>"
        print(f"0x{index:04X}:{subindex:<2} {access:<5} {name:<54} {value}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--interface", default="canalystii")
    parser.add_argument("--channel", default="2")
    parser.add_argument("--driver", type=Path, default=DEFAULT_DRIVER)
    parser.add_argument("--node", type=int, default=10, help="Currently active Node-ID")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("list", help="List local EDS object definitions; no CAN access")
    commands.add_parser("show", help="Read every EDS leaf object from the ITM")
    commands.add_parser("snapshot", help="Read and decode all 75 telemetry signals via SDO")
    read = commands.add_parser("read", help="Read one OD entry, e.g. 0x1018:4")
    read.add_argument("object", type=parse_object)
    configure = commands.add_parser("configure", help="Write and persist allowed parameters")
    configure.add_argument("--node-id", type=int)
    configure.add_argument("--heartbeat-ms", type=int)
    configure.add_argument("--dry-run", action="store_true")
    commands.add_parser("save", help="Issue 0x1010:1 'save' for pending SDO changes")
    # Accept connection options on either side of the subcommand, matching the
    # existing update_itm.py command style.
    for command in commands.choices.values():
        command.add_argument("--interface", default=argparse.SUPPRESS)
        command.add_argument("--channel", default=argparse.SUPPRESS)
        command.add_argument("--driver", type=Path, default=argparse.SUPPRESS)
        command.add_argument("--node", type=int, default=argparse.SUPPRESS)
    args = parser.parse_args()
    if not 1 <= args.node <= 127:
        parser.error("--node must be 1..127")
    entries = object_entries()
    if args.command == "list":
        for index, subindex, _, access, name in entries:
            print(f"0x{index:04X}:{subindex:<2} {access:<5} {name}")
        return
    if args.command == "configure":
        if args.node_id is None and args.heartbeat_ms is None:
            parser.error("configure needs --node-id and/or --heartbeat-ms")
        if args.node_id is not None and not 1 <= args.node_id <= 127:
            parser.error("--node-id must be 1..127")
        if args.heartbeat_ms is not None and not 0 <= args.heartbeat_ms <= 65535:
            parser.error("--heartbeat-ms must be 0..65535")
        if args.dry_run:
            print(f"Would SDO-write Node-ID={args.node_id}, heartbeat_ms={args.heartbeat_ms}, "
                  "then store by 0x1010:1; no changes made")
            return
    try:
        with open_bus(args) as (bus, message_class):
            client = SDOClient(bus, message_class, args.node)
            if args.command == "show":
                show(client, entries)
            elif args.command == "snapshot":
                try:
                    from .telemetry_snapshot import format_snapshot
                except ImportError:
                    from telemetry_snapshot import format_snapshot
                print(format_snapshot(client.read(0x2112, 0)))
            elif args.command == "read":
                index, subindex = args.object
                entry = next((item for item in entries if item[:2] == args.object), None)
                value = client.read(index, subindex)
                kind = entry[2] if entry else 0
                print(f"0x{index:04X}:{subindex} = {display(value, kind)} (raw {value.hex()})")
            elif args.command in ("configure", "save"):
                before = int.from_bytes(client.read(0x2100), "little")
                if before == 0xFFFFFFFF:
                    raise SDOError("Configuration version is exhausted")
                if args.command == "configure":
                    if args.node_id is not None:
                        client.write(0x2101, 0, bytes([args.node_id]))
                    if args.heartbeat_ms is not None:
                        client.write(0x1017, 0, struct.pack("<H", args.heartbeat_ms))
                client.write(0x1010, 1, b"save", timeout=15)
                after = int.from_bytes(client.read(0x2100), "little")
                if after != before + 1:
                    raise SDOError(f"Save version mismatch: {before} -> {after}")
                print(f"Configuration saved in FLASH: version {before} -> {after}")
                print(f"Configured Node-ID: {int.from_bytes(client.read(0x2101), 'little')}; "
                      f"heartbeat: {int.from_bytes(client.read(0x1017), 'little')} ms")
                if args.command == "configure" and args.node_id not in (None, args.node):
                    print(f"Node-ID {args.node_id} becomes active on the next MCU restart; "
                          f"the current SDO session still uses {args.node}")
    except (OSError, RuntimeError, TimeoutError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
