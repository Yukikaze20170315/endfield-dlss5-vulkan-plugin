"""Package the NGX path consistency candidate without replacing earlier releases."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011g-candidate'
OUT = ROOT / 'dist/unified-20261011h-candidate'

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

assert not OUT.exists(), 'Keep any previous candidate intact'
manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
overrides = {
    'dlss5-bridge.addon64': ROOT / 'src/unified-20261011/bridge/src/dlss5-bridge.addon64',
    'DLSS5-使用与恢复 Usage and Recovery.md': ROOT / 'docs/统一候选使用与恢复-20261011.md',
}
for item in manifest['files']:
    assert digest(BASE / item['path']) == item['sha256'], item['path']
entries = []
for item in manifest['files']:
    name = item['path']
    source = overrides.get(name, BASE / name)
    target = OUT / name
    assert target.resolve().is_relative_to(OUT.resolve()), name
    target.parent.mkdir(parents=True, exist_ok=True)
    expected = digest(source)
    shutil.copy2(source, target)
    assert digest(target) == expected
    entries.append({'path': name, 'sha256': expected, 'size': target.stat().st_size})
(OUT / 'release-manifest.json').write_text(json.dumps({
    'version': 'unified1h-20261011',
    'status': 'candidate; NGX path fix awaits in-game and cross-GPU acceptance',
    'files': entries,
}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
(OUT / 'SHA256SUMS.txt').write_text(
    ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
    + f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n", encoding='utf-8')
assert (OUT / 'DLSS5-Clean-Recovery.ps1').read_bytes().startswith(b'\xef\xbb\xbf')
print(json.dumps({'version': 'unified1h', 'payload_files': len(entries),
    'total_files': sum(p.is_file() for p in OUT.rglob('*')),
    'changed_payloads': list(overrides)}, ensure_ascii=False))
