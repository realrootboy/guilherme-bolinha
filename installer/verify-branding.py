"""Audit distributed files for historical labels in UTF-8 and UTF-16."""
from pathlib import Path
import re
import sys

# Encoded test fixtures keep historical labels out of project text and product files.
fixtures = [bytes.fromhex('6c75786f6e6978'), bytes.fromhex('707572697479')]
patterns = [re.compile(word.decode('ascii').encode(encoding), re.I)
            for word in fixtures for encoding in ('utf-8', 'utf-16le', 'utf-16be')]
if len(sys.argv) < 2:
    raise SystemExit('usage: verify-branding.py <distributed-folder> [...]')
count = 0
for argument in sys.argv[1:]:
    root = Path(argument)
    if not root.is_dir():
        raise SystemExit(f'Missing installation directory: {root}')
    for path in root.rglob('*'):
        if any(pattern.search(path.name.encode('utf-8')) for pattern in patterns):
            raise SystemExit(f'Unexpected historical filename: {path}')
        if path.is_file():
            data = path.read_bytes()
            if any(pattern.search(data) for pattern in patterns):
                raise SystemExit(f'Historical label found in {path}')
            count += 1
print(f'Branding audit passed for {count} distributed files')
