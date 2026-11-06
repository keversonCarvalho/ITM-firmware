"""Read analog inputs or test digital outputs over CANopen RET.

The analog command only reads EM1/EM2 SDOs; it never writes a command.
The outputs command renews the one-second firmware lease while running and
always requests OFF when it exits.
"""

import argparse
from pathlib import Path
import sys
import time

from itm_od import DEFAULT_DRIVER, SDOClient, SDOError, open_bus


STATE = ("OFF", "CEB_STARTING", "CEB_ON", "CB_STARTING", "BOTH_ON",
         "STOPPING", "FAULT", "CB_ON")
CONDITION = ("PENDING", "NORMAL", "BATTERY_DISCONNECTED", "MONITOR_FAULT",
             "STUCK_ON", "STUCK_OFF", "INVALID")


def read_uint(client, index):
    data = client.read(index)
    if len(data) not in (1, 2, 4):
        raise RuntimeError(f"Unexpected size for 0x{index:04X}: {len(data)}")
    return int.from_bytes(data, "little")


def write_uint(client, index, value, size):
    client.write(index, 0, value.to_bytes(size, "little"))


def format_voltage(mv):
    if mv == 0xFFFFFFFF:
        return "invalid (0xFFFFFFFF)"
    return f"{mv / 1000:.3f} V ({mv} mV)"


def format_adc(raw):
    if raw == 0xFFFFFFFF:
        return "ADC=conversion failed"
    input_v = (raw - 32768) * 6.6 / 65535
    note = ""
    if raw < 32128:
        note = "; negative ADC differential: check AMC fail-safe/polarity"
    elif raw < 32768:
        note = "; small negative zero offset clamped to 0 V by firmware"
    elif input_v > 2.0:
        note = "; above AMC3311 specified linear range"
    return f"ADC={raw} (0x{raw:04X}, ADC pin differential ~{input_v:.3f} V nominal{note})"


def read_analog(client):
    em1, em2 = read_uint(client, 0x2125), read_uint(client, 0x2126)
    try:
        raw1, raw2 = read_uint(client, 0x2128), read_uint(client, 0x2129)
    except SDOError as exc:
        raise SystemExit("The running ITM firmware does not expose raw ADC "
                         "objects 0x2128/0x2129; install the diagnostic image first") from exc
    return em1, em2, raw1, raw2


def monitor_analog(client, seconds, interval):
    started = time.monotonic()
    deadline = started + seconds
    request = read_uint(client, 0x2120)
    print("Passive SDO reading of EM1/E_ALIM and EM2/S_ALIM; "
          "this command does not change the outputs.")
    if request:
        print(f"Warning: output command {request} was already active before monitoring.")
    while True:
        em1, em2, raw1, raw2 = read_analog(client)
        print(f"t={time.monotonic() - started:.2f} s  "
              f"EM1/E_ALIM={format_voltage(em1)}; {format_adc(raw1)}  "
              f"EM2/S_ALIM={format_voltage(em2)}; {format_adc(raw2)}", flush=True)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return
        time.sleep(min(interval, remaining))


def show(client):
    version = read_uint(client, 0x2105)
    request = read_uint(client, 0x2120)
    state = read_uint(client, 0x2121)
    debounce = read_uint(client, 0x2122)
    ceb_delay = read_uint(client, 0x2123)
    cb_timeout = read_uint(client, 0x2124)
    em1, em2, raw1, raw2 = read_analog(client)
    condition = read_uint(client, 0x2127)
    print(f"firmware=v{version} request={request} "
          f"state={STATE[state] if state < len(STATE) else state} "
          f"EM1={format_voltage(em1)} [{format_adc(raw1)}] "
          f"EM2={format_voltage(em2)} [{format_adc(raw2)}] "
          f"CB={CONDITION[condition] if condition < len(CONDITION) else condition}")
    print(f"diagnostic debounce={debounce} ms, CEB status delay={ceb_delay} ms, "
          f"CB status delay={cb_timeout} ms")
    return state


def test_outputs(client, requested, seconds):
    version = read_uint(client, 0x2105)
    if version < 17:
        raise SystemExit(f"ITM firmware v{version} still couples CB to CEB/EM1/EM2. "
                         "Install v17 before testing independent outputs")
    deadline = time.monotonic() + seconds
    failure = None
    failure_phase = ""
    off_failure = None
    first_command = True
    try:
        while time.monotonic() < deadline:
            phase = "writing SDO 0x2120:0"
            write_uint(client, 0x2120, requested, 1)
            if first_command:
                print(f"SDO command {requested} acknowledged by ITM", flush=True)
                first_command = False
            phase = "reading power state SDO 0x2121:0"
            state = read_uint(client, 0x2121)
            phase = "reading CB diagnostic SDO 0x2127:0"
            condition = read_uint(client, 0x2127)
            print(f"state={STATE[state] if state < len(STATE) else state} "
                  f"CB={CONDITION[condition] if condition < len(CONDITION) else condition}",
                  flush=True)
            if state == 6:
                print("ITM entered FAULT; ending the test.", flush=True)
                break
            time.sleep(min(0.25, max(0.0, deadline - time.monotonic())))
    except (TimeoutError, SDOError, OSError) as exc:
        failure = exc
        failure_phase = phase
    finally:
        try:
            write_uint(client, 0x2120, 0, 1)
        except (TimeoutError, SDOError, OSError) as exc:
            off_failure = exc

    if off_failure is not None:
        print(f"OFF was not confirmed by SDO: {off_failure}. "
              "The firmware's one-second command lease should switch the outputs off "
              "if it remains running; verify the physical outputs before proceeding.",
              file=sys.stderr)
    else:
        time.sleep(0.15)
        print("Final state after OFF:")
        try:
            show(client)
        except (TimeoutError, SDOError, OSError) as exc:
            print(f"OFF was acknowledged, but final status could not be read: {exc}",
                  file=sys.stderr)
    if failure is not None:
        raise SystemExit(f"CANopen communication failed while {failure_phase}: {failure}")
    if off_failure is not None:
        raise SystemExit(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("analog", "outputs", "status", "off",
                                            "ceb", "on", "config"))
    parser.add_argument("--interface", default="canalystii")
    parser.add_argument("--channel", default="2")
    parser.add_argument("--driver", type=Path, default=DEFAULT_DRIVER)
    parser.add_argument("--node", type=int, default=10)
    parser.add_argument("--seconds", type=float, default=5.0,
                        help="Duration for analog/outputs/ceb/on; 0 < seconds <= 600")
    parser.add_argument("--interval", type=float,
                        help="Analog reading interval in seconds (default: 0.5)")
    parser.add_argument("--target", choices=("ceb", "cb", "both"),
                        help="Required only for outputs: test CEB, CB, or both independently")
    parser.add_argument("--debounce-ms", type=int)
    parser.add_argument("--ceb-delay-ms", type=int)
    parser.add_argument("--cb-timeout-ms", type=int)
    args = parser.parse_args()
    if args.command in ("analog", "outputs", "ceb", "on") and not 0 < args.seconds <= 600:
        parser.error("--seconds must be greater than 0 and at most 600")
    if args.command == "outputs" and args.target is None:
        parser.error("outputs requires --target ceb, cb, or both")
    if args.command != "outputs" and args.target is not None:
        parser.error("--target is only valid with outputs")
    if args.command != "analog" and args.interval is not None:
        parser.error("--interval is only valid with analog")
    interval = 0.5 if args.interval is None else args.interval
    if args.command == "analog" and not 0.05 <= interval <= 5:
        parser.error("--interval must be between 0.05 and 5 seconds")
    if args.command != "config" and any(x is not None for x in
        (args.debounce_ms, args.ceb_delay_ms, args.cb_timeout_ms)):
        parser.error("timing options require the config command")

    with open_bus(args) as (bus, message_class):
        client = SDOClient(bus, message_class, args.node)
        if args.command == "analog":
            monitor_analog(client, args.seconds, interval)
            return
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

        requested = (1 if (args.command == "ceb" or args.target == "ceb")
                     else 3 if args.target == "cb" else 2)
        test_outputs(client, requested, args.seconds)


if __name__ == "__main__":
    main()
