"""Transmit CEB chapter-3 telemetry frames to the ITM over USB/RS-485.

No reply is defined on this telemetry link; successful writes prove only that
the PC handed frames to the converter, not that the ITM accepted them.
"""

from __future__ import annotations

import argparse
import struct
import sys
import time

BAUDRATE = 460_800
PERIOD_SECONDS = 0.010
FRAME_SIZE = 44

# Payload offset, signedness, representative raw ADC/count value.
# All wire fields are 16-bit big-endian, including the two health bitfields.
FIELDS: dict[str, tuple[int, bool, int]] = {
    "pitch_main_lvdt": (0, True, 16384),       # 5.0 V
    "pitch_mirror_lvdt": (2, True, 16384),     # 5.0 V
    "yaw_main_lvdt": (4, True, 8192),          # 2.5 V
    "yaw_mirror_lvdt": (6, True, 8192),        # 2.5 V
    "pitch_current": (8, False, 2048),        # 0 A
    "yaw_current": (10, False, 2048),         # 0 A
    "pitch_actuator_temp": (12, True, 8992), # 25 C
    "yaw_actuator_temp": (14, True, 8992),   # 25 C
    "pitch_driver_temp": (16, True, 8992),  # 25 C
    "yaw_driver_temp": (18, True, 8992),    # 25 C
    "battery_150v": (20, False, 2048),       # ~150.04 V
    "supply_28v": (22, False, 2730),         # 28 V
    "pitch_command": (24, True, 0),         # 0 mm
    "yaw_command": (26, True, 0),           # 0 mm
    "pitch_position": (28, True, 0),        # 0 mm
    "yaw_position": (30, True, 0),          # 0 mm
    "ceb_health": (32, False, 0),           # no CEB faults
    "can_health": (34, False, 0x000A),      # CEB CAN A/B NMT operational
}


def crc16_ccitt_false(data: bytes) -> int:
    """ICD polynomial/init/xorout; provisional non-reflected convention."""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ (0x1021 if crc & 0x8000 else 0)) & 0xFFFF
    return crc


def build_frame(counter: int, values: dict[str, int]) -> bytes:
    if not 0 <= counter <= 0xFFFF:
        raise ValueError("frame counter must be 0..65535")
    if set(values) != set(FIELDS):
        raise ValueError("frame values must contain each of the 18 CEB fields")
    frame = bytearray(FRAME_SIZE)
    frame[:2] = b"\x5a\x5a"
    struct.pack_into(">H", frame, 2, counter)
    for name, (offset, signed, _) in FIELDS.items():
        value = values[name]
        minimum, maximum = (-32768, 32767) if signed else (0, 65535)
        if not minimum <= value <= maximum:
            raise ValueError(f"{name} raw value must be {minimum}..{maximum}")
        struct.pack_into(">h" if signed else ">H", frame, 4 + offset, value)
    # The agreed range is frame offsets 2..39 (counter + 36-byte payload).
    struct.pack_into(">H", frame, 40, crc16_ccitt_false(frame[2:40]))
    frame[42:] = b"\x5a\x5a"
    return bytes(frame)


def parse_overrides(overrides: list[str]) -> dict[str, int]:
    values = {name: default for name, (_, _, default) in FIELDS.items()}
    for override in overrides:
        name, separator, text = override.partition("=")
        if not separator or name not in FIELDS:
            raise ValueError(f"invalid --raw {override!r}; use FIELD=VALUE and --list-fields")
        try:
            values[name] = int(text, 0)
        except ValueError as exc:
            raise ValueError(f"invalid integer in --raw {override!r}") from exc
    return values


def send_frames(port: str, count: int, counter: int,
                values: dict[str, int], verbose: bool) -> None:
    try:
        import serial
    except ImportError as exc:
        raise SystemExit("pyserial is missing; install with: "
                         "python -m pip install -r tools/requirements-ceb-serial.txt") from exc

    sent = 0
    late = 0
    try:
        with serial.Serial(port=port, baudrate=BAUDRATE,
                           bytesize=serial.EIGHTBITS, parity=serial.PARITY_NONE,
                           stopbits=serial.STOPBITS_ONE, timeout=0,
                           write_timeout=1) as link:
            print(f"Sending CEB telemetry on {port}: {BAUDRATE} 8N1, "
                  f"{PERIOD_SECONDS * 1000:g} ms, {FRAME_SIZE} bytes/frame")
            deadline = time.perf_counter()
            while count == 0 or sent < count:
                remaining = deadline - time.perf_counter()
                if remaining > 0:
                    time.sleep(remaining)
                frame = build_frame(counter, values)
                written = link.write(frame)
                if written != FRAME_SIZE:
                    raise OSError(f"short serial write: {written}/{FRAME_SIZE} bytes")
                sent += 1
                if verbose or sent == 1 or sent % 100 == 0:
                    print(f"frame={sent} counter={counter} crc={frame[40:42].hex()}"
                          + (f" data={frame.hex(' ')}" if verbose else ""))
                counter = (counter + 1) & 0xFFFF
                deadline += PERIOD_SECONDS
                now = time.perf_counter()
                if now > deadline:
                    late += 1
                    deadline = now + PERIOD_SECONDS  # do not burst to catch up
    except KeyboardInterrupt:
        print("Stopped by user")
    except (OSError, serial.SerialException) as exc:
        raise SystemExit(f"Cannot use {port}: {exc}") from exc
    finally:
        print(f"Sent {sent} frame(s); scheduling overruns: {late}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="COM3", help="USB/RS-485 port (default: COM3)")
    parser.add_argument("--count", type=int, default=100,
                        help="number of frames; 0 = continuous until Ctrl+C (default: 100)")
    parser.add_argument("--counter-start", type=lambda value: int(value, 0), default=0,
                        help="first 16-bit frame counter (default: 0)")
    parser.add_argument("--raw", action="append", default=[], metavar="FIELD=VALUE",
                        help="override a raw 16-bit field; repeat as needed")
    parser.add_argument("--list-fields", action="store_true",
                        help="show payload fields and representative raw values")
    parser.add_argument("--dry-run", action="store_true",
                        help="print one encoded frame without opening the serial port")
    parser.add_argument("--verbose", action="store_true", help="print every frame in hex")
    args = parser.parse_args(argv)
    if args.list_fields:
        for name, (offset, signed, default) in FIELDS.items():
            print(f"{name:23} payload={offset:2}-{offset + 1:2} "
                  f"{'int16' if signed else 'uint16':6} default={default}")
        return 0
    if args.count < 0 or not 0 <= args.counter_start <= 0xFFFF:
        parser.error("--count must be >= 0 and --counter-start must be 0..65535")
    try:
        values = parse_overrides(args.raw)
        frame = build_frame(args.counter_start, values)
    except ValueError as exc:
        parser.error(str(exc))
    if args.dry_run:
        print(f"{FRAME_SIZE} bytes: {frame.hex(' ')}")
        print(f"counter={args.counter_start} crc=0x{int.from_bytes(frame[40:42], 'big'):04X}")
        return 0
    send_frames(args.port, args.count, args.counter_start, values, args.verbose)
    return 0


if __name__ == "__main__":
    sys.exit(main())
