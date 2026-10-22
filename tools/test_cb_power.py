"""Bench control of ITM CEB and contactor-box outputs over CANopen RET.

The active command is leased for one second by the firmware. This program
renews it while running and always requests OFF when it exits.
"""

import argparse
from pathlib import Path
import time

from itm_od import DEFAULT_DRIVER, SDOClient, open_bus


STATE = ("OFF", "CEB_STARTING", "CEB_ON", "CB_STARTING", "OPERATIONAL",
         "STOPPING", "FAULT")
CONDITION = ("PENDING", "NORMAL", "BATTERY_DISCONNECTED", "MONITOR_FAULT",
             "STUCK_ON", "STUCK_OFF", "INVALID")


def read_uint(client, index):
    data = client.read(index)
    if len(data) not in (1, 2, 4):
        raise RuntimeError(f"Unexpected size for 0x{index:04X}: {len(data)}")
    return int.from_bytes(data, "little")


def write_uint(client, index, value, size):
    client.write(index, 0, value.to_bytes(size, "little"))


def show(client):
    request = read_uint(client, 0x2120)
    state = read_uint(client, 0x2121)
    debounce = read_uint(client, 0x2122)
    ceb_delay = read_uint(client, 0x2123)
    cb_timeout = read_uint(client, 0x2124)
    em1 = read_uint(client, 0x2125)
    em2 = read_uint(client, 0x2126)
    condition = read_uint(client, 0x2127)
    fmt = lambda mv: "invalid" if mv == 0xFFFFFFFF else f"{mv / 1000:.3f} V"
    print(f"request={request} state={STATE[state] if state < len(STATE) else state} "
          f"EM1={fmt(em1)} EM2={fmt(em2)} "
          f"CB={CONDITION[condition] if condition < len(CONDITION) else condition}")
    print(f"debounce={debounce} ms, CEB delay={ceb_delay} ms, "
          f"CB timeout={cb_timeout} ms")
    return state


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("status", "off", "ceb", "on", "config"))
    parser.add_argument("--interface", default="canalystii")
    parser.add_argument("--channel", default="2")
    parser.add_argument("--driver", type=Path, default=DEFAULT_DRIVER)
    parser.add_argument("--node", type=int, default=10)
    parser.add_argument("--seconds", type=float, default=5.0,
                        help="Duration for ceb/on; 0 < seconds <= 600")
    parser.add_argument("--debounce-ms", type=int)
    parser.add_argument("--ceb-delay-ms", type=int)
    parser.add_argument("--cb-timeout-ms", type=int)
    args = parser.parse_args()
    if args.command in ("ceb", "on") and not 0 < args.seconds <= 600:
        parser.error("--seconds must be greater than 0 and at most 600")
    if args.command != "config" and any(x is not None for x in
        (args.debounce_ms, args.ceb_delay_ms, args.cb_timeout_ms)):
        parser.error("timing options require the config command")

    with open_bus(args) as (bus, message_class):
        client = SDOClient(bus, message_class, args.node)
        if args.command == "status":
            show(client)
            return
        if args.command == "off":
            write_uint(client, 0x2120, 0, 1)
            time.sleep(0.15)
            show(client)
            return
        if args.command == "config":
            settings = ((0x2122, args.debounce_ms),
                        (0x2123, args.ceb_delay_ms),
                        (0x2124, args.cb_timeout_ms))
            if not any(value is not None for _, value in settings):
                parser.error("provide at least one timing parameter")
            for index, value in settings:
                if value is not None:
                    if not 1 <= value <= 5000:
                        parser.error("timings must be between 1 and 5000 ms")
                    write_uint(client, index, value, 2)
            show(client)
            return

        requested = 1 if args.command == "ceb" else 2
        deadline = time.monotonic() + args.seconds
        try:
            while time.monotonic() < deadline:
                write_uint(client, 0x2120, requested, 1)
                show(client)
                time.sleep(min(0.25, max(0.0, deadline - time.monotonic())))
        finally:
            write_uint(client, 0x2120, 0, 1)
            time.sleep(0.15)
            print("Final state after OFF:")
            show(client)


if __name__ == "__main__":
    main()
