"""Build the 1g full package from verified 1f1 payloads and the two new addons."""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011f1-candidate'
OUT = ROOT / 'dist/unified-20261011g-candidate'

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
overrides = {
    'dlss5-bridge.addon64': ROOT / 'src/unified-20261011/bridge/src/dlss5-bridge.addon64',
    'renodx-dlss5.addon64': ROOT / 'builds/unified-20261011/generic/renodx-dlss5.addon64',
    'DLSS5-使用与恢复 Usage and Recovery.md': ROOT / 'docs/统一候选使用与恢复-20261011.md',
}
entries = []
for item in manifest['files']:
    name = item['path']
    assert digest(BASE / name) == item['sha256'], name
    source = overrides.get(name, BASE / name)
    target = OUT / name
    assert target.resolve().is_relative_to(OUT.resolve()), name
    target.parent.mkdir(parents=True, exist_ok=True)
    expected = digest(source)
    if target.exists():
        assert digest(target) == expected, f'Existing candidate differs: {target}'
    else:
        shutil.copy2(source, target)
    assert digest(target) == expected
    entries.append({'path': name, 'sha256': expected, 'size': target.stat().st_size})

def save_new_or_equal(name, text):
    path = OUT / name
    if path.exists():
        assert path.read_text(encoding='utf-8') == text, name
    else:
        path.write_text(text, encoding='utf-8')

save_new_or_equal('release-manifest.json', json.dumps({
    'version': 'unified1g-20261011', 'status': 'candidate; hardware acceptance pending', 'files': entries,
}, ensure_ascii=False, indent=2) + '\n')
save_new_or_equal('SHA256SUMS.txt', ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
    + f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n")
assert (OUT / 'DLSS5-Clean-Recovery.ps1').read_bytes().startswith(b'\xef\xbb\xbf')
print(json.dumps({'version': 'unified1g', 'payload_files': len(entries),
                  'total_files': sum(p.is_file() for p in OUT.rglob('*')), 'changed_payloads': list(overrides)}, ensure_ascii=False))
