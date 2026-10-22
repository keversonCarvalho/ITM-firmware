"""ITM bench packager and CANopen-entry/XCP-on-CAN updater.

CRC-only packages are for controlled bench evaluation, not authenticated flight.
No retransmission of PROGRAM: a lost response requires reconnect and full restart.
"""
import argparse
from pathlib import Path
import struct
import time
import zlib

SLOTS = (0x08060000, 0x080A0000)
TARGET = 0x07230001
HEADER = 512
CAPACITY = 256 * 1024
RX_ID, TX_ID = 0x6F0, 0x6F1


def pack(binary, slot, version):
    if slot not in (0, 1) or not 8 <= len(binary) <= CAPACITY - HEADER:
        raise ValueError('Invalid slot or application size')
    base = SLOTS[slot] + HEADER
    stack, reset = struct.unpack_from('<II', binary)
    if stack & 7 or not 0x20000000 < stack <= 0x20020000:
        raise ValueError('Invalid DTCM stack pointer')
    if not reset & 1 or not base <= (reset & ~1) < base + len(binary):
        raise ValueError('Binary is not linked for selected slot')
    manifest = struct.pack('<4s6I', b'ITMF', 1, TARGET, base, len(binary), version,
                           zlib.crc32(binary))
    manifest += struct.pack('<I', zlib.crc32(manifest))
    return manifest.ljust(HEADER, b'\0') + binary


def inspect(package):
    if len(package) < HEADER + 8:
        raise ValueError('Truncated package')
    magic, schema, target, base, size, version, crc, header_crc = struct.unpack_from('<4s7I', package)
    if magic != b'ITMF' or schema != 1 or target != TARGET:
        raise ValueError('Wrong package format or target')
    if len(package) != HEADER + size or not 8 <= size <= CAPACITY - HEADER:
        raise ValueError('Wrong package length')
    if crc != zlib.crc32(package[HEADER:]) or header_crc != zlib.crc32(package[:28]):
        raise ValueError('Package CRC mismatch')
    slot = next((i for i, address in enumerate(SLOTS) if address + HEADER == base), None)
    if slot is None or pack(package[HEADER:], slot, version) != package:
        raise ValueError('Invalid vectors or unsupported manifest extension')
    return slot, version


class Client:
    def __init__(self, bus, message_type):
        self.bus = bus
        self.message_type = message_type

    def send(self, identifier, data):
        self.bus.send(self.message_type(arbitration_id=identifier, data=data,
                                      is_extended_id=False), timeout=2)

    def wait(self, identifier, timeout, match=lambda _: True):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            frame = self.bus.recv(max(0, deadline - time.monotonic()))
            if frame is not None and not frame.is_extended_id and not frame.is_error_frame and \
                    not frame.is_remote_frame and frame.arbitration_id == identifier and match(bytes(frame.data)):
                return bytes(frame.data)
        raise TimeoutError(f'No response on CAN ID {identifier:#x}; restart the update from CONNECT')

    def xcp(self, payload, timeout=30):
        self.send(RX_ID, payload)
        reply = self.wait(TX_ID, timeout)
        if not reply or reply[0] != 0xFF:
            raise RuntimeError(f'XCP rejected command {payload[0]:#x}: {reply.hex()}')
        return reply

    def sdo(self, node, index, value=None, timeout=2):
        if not 1 <= node <= 127:
            raise ValueError('Node-ID must be in 1..127')
        prefix = struct.pack('<HB', index, 0)
        data = bytes([0x40 if value is None else 0x23]) + prefix
        data += bytes(4) if value is None else value
        self.send(0x600 + node, data)
        reply = self.wait(0x580 + node, timeout, lambda p: len(p) == 8 and p[1:4] == prefix)
        if reply[0] == 0x80:
            raise RuntimeError(f'SDO abort: {struct.unpack_from("<I", reply, 4)[0]:#x}')
        if reply[0] != (0x43 if value is None else 0x60):
            raise RuntimeError(f'Unexpected SDO reply: {reply.hex()}')
        return struct.unpack_from('<I', reply, 4)[0]

    def update(self, image, node=10, enter=True):
        slot, version = inspect(image)
        if enter:
            self.sdo(node, 0x2103, b'UPDT')
        deadline = time.monotonic() + 12
        while True:
            try:
                reply = self.xcp(b'\xff\x00', timeout=0.3)
                break
            except TimeoutError:
                if time.monotonic() > deadline:
                    raise
        if len(reply) != 8 or reply[1] != 0x10 or reply[2] != 0 or reply[3] != 8:
            raise RuntimeError('Incompatible XCP programming endpoint')
        status = self.xcp(b'\xf1\x00')
        if len(status) != 5 or status[1] != slot or status[4]:
            raise RuntimeError('Image does not match inactive slot, or a trial boot is still pending')
        self.xcp(b'\xd2')
        self.xcp(b'\xf6\0\0\0' + struct.pack('<I', SLOTS[slot]))
        self.xcp(b'\xd1\0\0\0' + struct.pack('<I', len(image)))
        for offset in range(0, len(image), 6):
            chunk = image[offset:offset + 6]
            self.xcp(bytes([0xD0, len(chunk)]) + chunk)
            if offset and offset % 6000 == 0:
                print(f'Transferred {offset}/{len(image)} bytes', flush=True)
        self.xcp(b'\xd0\0')  # validates flash and commits candidate
        self.send(RX_ID, b'\xcf')  # PROGRAM_RESET has no positive response
        # Allow the application watchdog and bootloader discovery window to
        # complete if a candidate fails and the old image must be selected.
        deadline = time.monotonic() + 60
        state = None
        while time.monotonic() < deadline:
            try:
                state = (self.sdo(node, 0x2104, timeout=1),
                         self.sdo(node, 0x2105, timeout=1),
                         self.sdo(node, 0x2106, timeout=1))
            except TimeoutError:
                time.sleep(0.5)
                continue
            if state == (slot, version, 1):
                return slot, version
            time.sleep(0.5)
        if state is not None and state[0] == status[2] and state[2] == 1:
            raise RuntimeError(f'Candidate did not confirm; previous slot {state[0]} is active (rollback)')
        raise RuntimeError(f'Image was not confirmed; last boot state: {state}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    package = sub.add_parser('pack')
    package.add_argument('binary', type=Path)
    package.add_argument('output', type=Path)
    package.add_argument('--slot', type=int, choices=(0, 1), required=True)
    package.add_argument('--version', type=int, required=True)
    send = sub.add_parser('send')
    send.add_argument('image', type=Path)
    send.add_argument('--interface', required=True)
    send.add_argument('--channel', required=True)
    send.add_argument('--node', type=int, default=10)
    send.add_argument('--already-in-bootloader', action='store_true')
    send.add_argument('--driver', type=Path, help='ControlCAN.dll for --interface canalystii')
    args = parser.parse_args()
    if args.command == 'pack':
        result = pack(args.binary.read_bytes(), args.slot, args.version)
        args.output.write_bytes(result)
        print(f'Created {args.output}: {len(result)} bytes, CRC-only BENCH image')
    else:
        import can  # optional dependency; only required for real CAN hardware
        image = args.image.read_bytes()
        inspect(image)  # validate BEFORE requesting reset
        if args.interface == 'canalystii':
            from canalystii_bus import CanalystBus
            if args.driver is None:
                parser.error('--interface canalystii requires --driver ControlCAN.dll')
            bus_context = CanalystBus(args.driver, int(args.channel))
        else:
            bus_context = can.Bus(interface=args.interface, channel=args.channel, bitrate=500000)
        with bus_context as bus:
            result = Client(bus, can.Message).update(image, args.node, not args.already_in_bootloader)
        print(f'Confirmed running slot {result[0]}, version {result[1]}')


if __name__ == '__main__':
    main()
