"""Write a pre-build SHA256 snapshot of the production source tree.

    python snapshot-sources.py <version> <build-subdirectory>

Writes builds/unified-20261011/<build-subdirectory>/source-manifest.json. The
package script re-hashes every listed file and refuses to package if any of
them changed after the snapshot, so the shipped add-ons match the recorded
sources. Refuses to overwrite an existing snapshot.
"""
from pathlib import Path
import hashlib
import json
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'src/unified-20261011'
EXTENSIONS = {'.h', '.hpp', '.inc', '.c', '.cpp', '.asm', '.rc', '.def', '.lib',
              '.hlsl', '.slang', '.cso', '.spv', '.cmd'}

version, sub = sys.argv[1], sys.argv[2]
out = ROOT / 'builds/unified-20261011' / sub / 'source-manifest.json'
assert not out.exists(), f'snapshot exists: {out}'
files = []
for path in sorted(p for p in SRC.rglob('*') if p.is_file() and p.suffix.lower() in EXTENSIONS):
    with path.open('rb') as stream:
        files.append({'path': path.relative_to(ROOT).as_posix(),
                      'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()})
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({
    'version': version,
    'scope': 'production source and in-tree build dependencies; pre-build snapshot',
    'files': files,
}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'{len(files)} files -> {out}')
