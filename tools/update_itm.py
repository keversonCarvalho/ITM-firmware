"""Select the inactive slot and update the ITM over CANopen SDO + XCP on CAN."""

import argparse
import hashlib
import json
from pathlib import Path

import can

from firmware_update import Client, inspect


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DRIVER = ROOT / ".tools/canalystii/ControlCAN.dll"


def read_bundle(directory):
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("format") != 1 or manifest.get("target") != "ITM-100 Rev00 STM32H723":
        raise ValueError("Bundle target or format does not match ITM-100 Rev00")
    version = manifest.get("version")
    if not isinstance(version, int) or not 1 <= version <= 0xFFFFFFFF:
        raise ValueError("Invalid bundle version")
    packages = {}
    for slot, letter in enumerate(("a", "b")):
        name = f"itm_app_{letter}.itmf"
        data = (directory / name).read_bytes()
        item = manifest["files"][name]
        if len(data) != item["bytes"] or hashlib.sha256(data).hexdigest() != item["sha256"]:
            raise ValueError(f"Bundle file changed: {name}")
        if inspect(data) != (slot, version):
            raise ValueError(f"Wrong version or slot in {name}")
        packages[slot] = data
    return version, packages, bool(manifest.get("bootloader_migration_required", False))


def boot_state(client, node):
    return tuple(client.sdo(node, index) for index in (0x2104, 0x2105, 0x2106))


def select_inactive(state, version, allow_non_incremental=False):
    active, running_version, confirmed = state
    if active not in (0, 1) or confirmed != 1:
        raise RuntimeError("No confirmed running image; use the bootloader recovery procedure")
    if version <= running_version and not allow_non_incremental:
        raise RuntimeError(f"Bundle version {version} is not newer than running version {running_version}")
    return 1 - active


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", nargs="?", type=Path,
                        help="Directory created by build_itm_update.py")
    parser.add_argument("--interface", default="canalystii",
                        help="python-can interface; default: canalystii")
    parser.add_argument("--channel", default="2", help="CAN channel; default: 2")
    parser.add_argument("--driver", type=Path, default=DEFAULT_DRIVER,
                        help="Path to ControlCAN.dll for CANalyst-II")
    parser.add_argument("--node", type=int, default=10, help="CANopen node ID")
    parser.add_argument("--status", action="store_true", help="Read current slot without updating")
    parser.add_argument("--dry-run", action="store_true", help="Select and check the image only")
    parser.add_argument("--allow-non-incremental", action="store_true",
                        help="Permit installing a version no greater than the running one")
    parser.add_argument("--bootloader-migrated", action="store_true",
                        help="Confirm the bundled bootloader was installed by SWD before updating the application")
    args = parser.parse_args()
    if not 1 <= args.node <= 127:
        parser.error("--node must be in 1..127")
    if args.bundle is None and not args.status:
        parser.error("bundle is required unless --status is used")
    bundle = args.bundle.resolve() if args.bundle else None
    version, packages, bootloader_required = read_bundle(bundle) if bundle else (None, None, False)
    if bootloader_required and not args.bootloader_migrated and not args.dry_run and not args.status:
        parser.error("this bundle requires the new protected bootloader to be installed by SWD first; "
                     "then pass --bootloader-migrated")

    if args.interface == "canalystii":
        from canalystii_bus import CanalystBus
        if not args.driver.is_file():
            parser.error(f"ControlCAN.dll not found at {args.driver}; pass --driver PATH")
        bus_context = CanalystBus(args.driver, int(args.channel))
    else:
        bus_context = can.Bus(interface=args.interface, channel=args.channel, bitrate=500000)
    with bus_context as bus:
        client = Client(bus, can.Message)
        try:
            state = boot_state(client, args.node)
        except TimeoutError as exc:
            raise SystemExit("ITM did not answer CANopen SDO; check power, channel, "
                             "CAN wiring and node ID") from exc
        active, running_version, confirmed = state
        print(f"ITM: slot={active}, version={running_version}, confirmed={confirmed}")
        if args.status:
            return
        inactive = select_inactive(state, version, args.allow_non_incremental)
        print(f"Selected inactive slot {inactive} for version {version}")
        if args.dry_run:
            print("Dry run complete; no reset or flash write performed")
            return
        try:
            result = client.update(packages[inactive], args.node, enter=True)
        except Exception:
            try:
                current = boot_state(client, args.node)
                print(f"Update failed; current CANopen boot state: {current}")
            except Exception:
                print("Update failed; ITM is not currently responding to CANopen status")
            raise
        print(f"Update confirmed: slot={result[0]}, version={result[1]}")


if __name__ == "__main__":
    main()
