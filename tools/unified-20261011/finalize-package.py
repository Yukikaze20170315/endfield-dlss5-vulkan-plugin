"""Finish the unified1f1 candidate; refuse different existing package files."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011f-candidate'
OUT = ROOT / 'dist/unified-20261011f1-candidate'

# The external recovery CMD uses Windows PowerShell 5.1, whose script parser
# requires a BOM to recognize UTF-8 containing non-ASCII strings.
recovery_script = (ROOT / 'tools/unified-20261011/DLSS5-Clean-Recovery.ps1').read_bytes()
assert recovery_script.startswith(b'\xef\xbb\xbf'), 'Recovery script must retain its UTF-8 BOM'
assert b'\r\n' in (ROOT / 'tools/unified-20261011/DLSS5-Recovery.cmd').read_bytes(), 'Recovery CMD requires CRLF'

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def copy_verified(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        if digest(source) != digest(target):
            raise RuntimeError(f'Existing package file differs: {target}')
    else:
        shutil.copy2(source, target)
    assert digest(source) == digest(target), target

copy_verified(BASE / 'payload-manifest.json', OUT / 'payload-manifest.json')
payload = json.loads((OUT / 'payload-manifest.json').read_text(encoding='utf-8-sig'))
for entry in payload['files']:
    copy_verified(BASE / entry['path'], OUT / entry['path'])
    assert digest(OUT / entry['path']) == entry['sha256'], entry['path']

for source, name in [
    ('src/unified-20261011/bridge/src/dlss5-bridge.addon64', 'dlss5-bridge.addon64'),
    ('builds/unified-20261011/generic/renodx-dlss5.addon64', 'renodx-dlss5.addon64'),
    ('builds/unified-20261011/mfg/renodx-mfgunlock.addon64', 'renodx-mfgunlock.addon64'),
    ('tools/unified-20261011/DLSS5-Recovery.cmd', 'DLSS5-Recovery 故障恢复.cmd'),
    ('tools/unified-20261011/DLSS5-Clean-Recovery.ps1', 'DLSS5-Clean-Recovery.ps1'),
    ('builds/unified-20261011/recovery/DLSS5-RestoreDriver.exe', 'DLSS5-RestoreDriver.exe'),
    ('docs/统一候选使用与恢复-20261011.md', 'DLSS5-使用与恢复 Usage and Recovery.md'),
]:
    copy_verified(ROOT / source, OUT / name)

manifest_path = OUT / 'release-manifest.json'
entries = [{'path': p.relative_to(OUT).as_posix(), 'sha256': digest(p), 'size': p.stat().st_size}
           for p in sorted(OUT.rglob('*')) if p.is_file() and p.name not in ('release-manifest.json', 'SHA256SUMS.txt')]
manifest = {'version': 'unified1f1-20261011', 'status': 'candidate; hardware acceptance pending', 'files': entries}
content = json.dumps(manifest, ensure_ascii=False, indent=2) + '\n'
if manifest_path.exists():
    assert manifest_path.read_text(encoding='utf-8') == content, 'Existing manifest differs'
else:
    manifest_path.write_text(content, encoding='utf-8')
sums = ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
sums += f'{digest(manifest_path)}  release-manifest.json\n'
sum_path = OUT / 'SHA256SUMS.txt'
if sum_path.exists():
    assert sum_path.read_text(encoding='utf-8') == sums
else:
    sum_path.write_text(sums, encoding='utf-8')
print(json.dumps({'files': len(entries), 'bytes': sum(e['size'] for e in entries),
                  'manifest_sha256': digest(manifest_path)}, indent=2))
