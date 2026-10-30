"""Check the bundled library against the verified import's byte hashes."""
import hashlib
import sys
from pathlib import Path

root = Path(sys.argv[1])
entries = (root / 'SHA256SUMS').read_text().splitlines()
if len(entries) != 44:
    raise SystemExit('Expected 44 library files')
for entry in entries:
    expected, name = entry.split('  ', 1)
    if Path(name).name != name:
        raise SystemExit('Invalid manifest path')
    data = (root / name).read_bytes()
    if data[:19] != b'Phoenix -Cure'.ljust(19, b' '):
        raise SystemExit(f'Unexpected content branding: {name}')
    if hashlib.sha256(data).hexdigest() != expected:
        raise SystemExit(f'Content hash mismatch: {name}')
print('Verified all 44 sound library files')
