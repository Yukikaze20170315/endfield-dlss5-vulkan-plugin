"""Package the unified Throttle=2 default, preserving all previous candidates."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011j-diagnostic'
OUT = ROOT / 'dist/unified-20261011k-candidate'
BUILD = ROOT / 'builds/unified-20261011/throttle-default1k'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


assert not OUT.exists(), 'Keep previous packages intact'
manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
overrides = {
    'dlss5-bridge.addon64': BUILD / 'dlss5-bridge.addon64',
    'DLSS5-使用与恢复 Usage and Recovery.md': ROOT / 'docs/统一候选使用与恢复-20261011.md',
}
for item in manifest['files']:
    source = BASE / item['path']
    assert source.resolve().is_relative_to(BASE.resolve())
    assert digest(source) == item['sha256'], item['path']
    assert overrides.get(item['path'], source).is_file()
entries = []
for item in manifest['files']:
    name = item['path']
    source = overrides.get(name, BASE / name)
    target = OUT / name
    assert target.resolve().is_relative_to(OUT.resolve())
    target.parent.mkdir(parents=True, exist_ok=True)
    expected = digest(source)
    shutil.copy2(source, target)
    assert digest(target) == expected
    entries.append({'path': name, 'sha256': expected, 'size': target.stat().st_size})
(OUT / 'release-manifest.json').write_text(json.dumps({
    'version': 'unified1k-20261011',
    'status': 'candidate; built-in Throttle=2; cross-GPU and in-game acceptance pending; intermittent MFG hook failure unresolved',
    'files': entries,
}, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
(OUT / 'SHA256SUMS.txt').write_text(
    ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
    + f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n", encoding='utf-8')
assert (OUT / 'DLSS5-Clean-Recovery.ps1').read_bytes().startswith(b'\xef\xbb\xbf')
assert len(entries) == 20 and sum(p.is_file() for p in OUT.rglob('*')) == 22
changed = [e['path'] for e in entries if digest(BASE / e['path']) != e['sha256']]
assert set(changed) == set(overrides)
result = {'version': 'unified1k', 'payload_files': len(entries), 'total_files': 22,
          'changed_payloads_from_1j': changed,
          'bridge_sha256': digest(OUT / 'dlss5-bridge.addon64')}
(BUILD / 'package-verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
