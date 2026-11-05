"""Package unified1p: the 1o package with MFG Unlock unload-pin1p.

1o could not start the game on the development machine: MFG Unlock was unloaded
at a Vulkan instance teardown while Unity's Direct3D 9 probe thread was inside
its LoadLibraryExW detour, and that thread returned into the unmapped image
(DEP at MFG+0x31183). unload-pin1p keeps the add-on resident once it hooks and
repeats the bootstrap scan from AddonInit on later ReShade add-on cycles.

Only `renodx-mfgunlock.addon64` and the bilingual usage document change; the two
manifests are regenerated. Everything else is carried over byte-for-byte from
1o. Previous packages are preserved.
"""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011o-candidate'
OUT = ROOT / 'dist/unified-20261011p-candidate'
BUILD = ROOT / 'builds/unified-20261011/mfg-unload-pin1p'
EVIDENCE = ROOT / 'builds/unified-20261011/package-1p'

DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'
MFG = 'renodx-mfgunlock.addon64'

expected_hashes = {
    MFG: '270b38dd8db32cb477f1d8720a028edad613ed55bf76dd7d58a7439641516aac',
}
overrides = {
    MFG: BUILD / MFG,
    DOC: ROOT / 'docs/统一候选使用与恢复-20261011.md',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


for path in (BASE, OUT, EVIDENCE, BUILD, *BASE.parents, *OUT.parents, *EVIDENCE.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
assert not OUT.exists(), 'Keep previous packages intact'
assert not EVIDENCE.exists(), 'Keep previous packaging evidence intact'

for name, expected in expected_hashes.items():
    actual = digest(overrides[name])
    assert actual == expected, f'{name}: expected {expected}, found {actual}'

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified1o-20261012', manifest['version']

entries = []


def add(name, source):
    target = OUT / name
    assert target.resolve().is_relative_to(OUT.resolve()), name
    target.parent.mkdir(parents=True, exist_ok=True)
    expected = digest(source)
    shutil.copy2(source, target)
    assert digest(target) == expected, name
    entries.append({'path': name, 'sha256': expected, 'size': target.stat().st_size})


for item in manifest['files']:
    name = item['path']
    source = BASE / name
    assert source.resolve().is_relative_to(BASE.resolve()), name
    assert digest(source) == item['sha256'], name
    add(name, overrides.get(name, source))

paths = {entry['path'] for entry in entries}
assert len(paths) == len(entries), 'duplicate payload path'
assert set(overrides) <= paths, 'override target missing from the base package'

EVIDENCE.mkdir(parents=True)
(OUT / 'release-manifest.json').write_text(json.dumps({
    'version': 'unified1p-20261012',
    'status': 'candidate; MFG Unlock stays resident once hooked (startup unload race fixed); '
              'offline race fixture and build verified; no in-game or cross-GPU acceptance yet',
    'files': entries,
}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

sums = ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
sums += f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n"
(OUT / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')

assert (OUT / 'DLSS5-Clean-Recovery.ps1').read_bytes().startswith(b'\xef\xbb\xbf')
assert b'\r\n' in (OUT / 'DLSS5-Recovery 故障恢复.cmd').read_bytes()
assert len(entries) == 21, len(entries)
assert sum(p.is_file() for p in OUT.rglob('*')) == 23

changed = sorted(e['path'] for e in entries if digest(BASE / e['path']) != e['sha256'])
assert set(changed) == {MFG, DOC}, changed

result = {
    'version': 'unified1p',
    'payload_files': len(entries),
    'total_files': 23,
    'changed_payloads_from_1o': changed,
    'mfg_build': 'unload-pin1p (0.11.2026.1014)',
    'bridge_build': 'nr-runtime-select1o (unchanged from 1o)',
    'generic_build': 'unified1i (unchanged from 1o)',
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
