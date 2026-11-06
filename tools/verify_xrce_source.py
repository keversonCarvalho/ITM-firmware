"""Offline integrity check for the pinned eProsima source selections."""
import hashlib
import json
from pathlib import Path

project = Path(__file__).resolve().parents[1]
manifest = json.loads((project / "config/xrce-upstream.json").read_text())
for component, expected in manifest.items():
    root = project / "third_party" / component
    paths = sorted(
        [path for directory in ("include", "src")
         for path in (root / directory).rglob("*") if path.is_file()]
        + [root / "LICENSE"]
    )
    if len(paths) != expected["selected_files"]:
        raise SystemExit(f"{component}: source file selection changed")
    digest = hashlib.sha256()
    for path in paths:
        digest.update(path.relative_to(root).as_posix().encode() + b"\0")
        digest.update(path.read_bytes())
    if digest.hexdigest() != expected["sha256"]:
        raise SystemExit(f"{component}: pinned source digest differs")
    print(f"{component}: {len(paths)} files verified at {expected['tag']}")
