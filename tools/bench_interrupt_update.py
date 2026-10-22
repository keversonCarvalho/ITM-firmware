"""Bench-only test: drop CAN during a partial XCP update and check fallback."""

import argparse
from pathlib import Path
import struct
import time

import can

from canalystii_bus import CanalystBus
from firmware_update import Client, SLOTS, inspect


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--channel", type=int, choices=(1, 2), required=True)
    parser.add_argument("--node", type=int, default=10)
    parser.add_argument("--bytes", type=int, default=12000)
    args = parser.parse_args()
    image = args.image.read_bytes()
    slot, version = inspect(image)
    if not 32 <= args.bytes < len(image) or args.bytes % 32:
        parser.error("--bytes must be a 32-byte multiple smaller than the image")

    with CanalystBus(args.driver, args.channel) as bus:
        client = Client(bus, can.Message)
        before = tuple(client.sdo(args.node, index) for index in (0x2104, 0x2105, 0x2106))
        if before[2] != 1 or before[0] == slot:
            raise RuntimeError(f"No confirmed image outside destination slot: {before}")
        print(f"Confirmed image before test: {before}", flush=True)
        client.sdo(args.node, 0x2103, b"UPDT")
        deadline = time.monotonic() + 12
        while True:
            try:
                client.xcp(b"\xff\x00", timeout=0.3)
                break
            except TimeoutError:
                if time.monotonic() >= deadline:
                    raise
        status = client.xcp(b"\xf1\x00")
        if len(status) != 5 or status[1] != slot or status[2] != before[0] or status[4]:
            raise RuntimeError(f"Unexpected bootloader state: {status.hex()}")
        client.xcp(b"\xd2")
        client.xcp(b"\xf6\0\0\0" + struct.pack("<I", SLOTS[slot]))
        client.xcp(b"\xd1\0\0\0" + struct.pack("<I", len(image)))
        for offset in range(0, args.bytes, 6):
            chunk = image[offset:min(offset + 6, args.bytes)]
            client.xcp(bytes([0xd0, len(chunk)]) + chunk)
        print(f"Interrupted after {args.bytes}/{len(image)} bytes; no final PROGRAM sent", flush=True)

    # The bootloader has a 10 s XCP inactivity timeout before booting the
    # confirmed image. Allow the timeout and application startup to finish.
    time.sleep(11)
    with CanalystBus(args.driver, args.channel) as bus:
        client = Client(bus, can.Message)
        deadline = time.monotonic() + 15
        while True:
            try:
                after = tuple(client.sdo(args.node, index, timeout=1)
                              for index in (0x2104, 0x2105, 0x2106))
                break
            except TimeoutError:
                if time.monotonic() >= deadline:
                    raise
                time.sleep(0.5)
    print(f"Image after interruption: {after}", flush=True)
    if after != before:
        raise RuntimeError("Confirmed image did not survive interrupted update")
    print(f"Recovery passed; incomplete slot {slot}, version {version} was not activated", flush=True)


if __name__ == "__main__":
    main()
