"""Offline integrity check for the vendored N7 Space release files."""
import hashlib
import json
from pathlib import Path

project = Path(__file__).resolve().parents[1]
root = project / 'third_party/n7space'
files = [root / name for name in
         (project / 'config/n7space-files.txt').read_text(encoding='utf-8').splitlines()]
actual = sorted([path for directory in ('include', 'lib/co', 'lib/can',
                                        'lib/compat', 'lib/util')
                 for path in (root / directory).rglob('*') if path.is_file()] +
                [root / 'LICENSE', root / 'NOTICE', root / 'configure.ac'])
if sorted(files) != actual:
    raise SystemExit('N7 Space file selection differs from the pinned release')
digest = hashlib.sha256()
for path in files:
    digest.update(path.relative_to(root).as_posix().encode() + b'\0')
    digest.update(path.read_bytes())
expected = json.loads((project / 'config/n7space-upstream.json').read_text())['source_tree_sha256']
if digest.hexdigest() != expected:
    raise SystemExit('N7 Space source tree differs from the pinned release')
print(f'N7 Space source verified: {len(files)} files')
