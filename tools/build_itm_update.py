"""Compile and package both STM32H723 application slots from current sources."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess

from firmware_update import inspect, pack


ROOT = Path(__file__).resolve().parents[1]


def rooted(path):
    return path if path.is_absolute() else ROOT / path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", type=int, required=True,
                        help="Unsigned firmware version, greater than the running version")
    parser.add_argument("--build-dir", type=Path, default=Path("build-update"))
    parser.add_argument("--output", type=Path,
                        help="Bundle directory; default: out/firmware/v<VERSION>")
    parser.add_argument("--replace", action="store_true",
                        help="Replace an existing bundle for this version")
    args = parser.parse_args()
    if not 1 <= args.version <= 0xFFFFFFFF:
        parser.error("--version must be in 1..4294967295")
    build_dir = rooted(args.build_dir).resolve()
    output = rooted(args.output or Path("out/firmware") / f"v{args.version}").resolve()
    if output.exists() and any(output.iterdir()) and not args.replace:
        parser.error(f"Bundle already exists: {output}; use a new version or --replace")

    configure = [
        "cmake", "-S", str(ROOT), "-B", str(build_dir), "-G", "Ninja",
        "-DCMAKE_TOOLCHAIN_FILE=" + str(ROOT / "cmake/toolchains/arm-none-eabi-gcc.cmake"),
        "-DITM_HARDWARE=ITM_REV00", "-DITM_BUILD_TESTS=OFF",
        "-DITM_BUILD_UPDATE_BENCH=ON", "-DCMAKE_BUILD_TYPE=Release",
    ]
    subprocess.run(configure, cwd=ROOT, check=True)
    subprocess.run(["cmake", "--build", str(build_dir), "--target",
                    "itm_boot.elf", "itm_app_a.elf", "itm_app_b.elf"], cwd=ROOT, check=True)

    images = {}
    source = build_dir / "platform/stm32h723"
    for slot, letter in enumerate(("a", "b")):
        binary = (source / f"itm_app_{letter}.bin").read_bytes()
        package = pack(binary, slot, args.version)
        if inspect(package) != (slot, args.version):
            raise RuntimeError(f"Package validation failed for slot {letter.upper()}")
        images[letter] = (binary, package)

    output.mkdir(parents=True, exist_ok=True)
    files = {}
    for letter, (binary, package) in images.items():
        for suffix, contents in (("bin", binary), ("itmf", package)):
            name = f"itm_app_{letter}.{suffix}"
            (output / name).write_bytes(contents)
            files[name] = {"bytes": len(contents), "sha256": hashlib.sha256(contents).hexdigest()}
    for name in ("itm_boot.bin", "itm_boot.hex"):
        contents = (source / name).read_bytes()
        (output / name).write_bytes(contents)
        files[name] = {"bytes": len(contents), "sha256": hashlib.sha256(contents).hexdigest()}
    manifest = {
        "format": 1, "target": "ITM-100 Rev00 STM32H723", "version": args.version,
        "policy": "bench CRC-32 only", "created_utc": datetime.now(timezone.utc).isoformat(),
        "bootloader_migration_required": True,
        "files": files,
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Bundle ready: {output}")
    print(f"Version {args.version}; A and B binaries and XCP packages generated")


if __name__ == "__main__":
    main()
