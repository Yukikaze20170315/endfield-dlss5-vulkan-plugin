"""Package unified1q: the 1p package with three rebuilt add-ons and the usage doc.

Changes against 1p, all built from the production tree by build-unified1q.cmd:

* dlss5-bridge.addon64 (1.4.13-pre8-unified1q-20261015): accepts the game's
  post-2026-10-15 sl.interposer.dll (Streamline 2.10.3, NVIDIA build) besides
  the earlier Hypergryph build, and reports an unknown interposer separately
  from a damaged package; restores the pq-wait-fix2 wait chain that the 1o/1p
  bridge was built without; moves every log into dlss5/dlss5_log; adds the
  anomaly event log (bounded by default, full mode behind a confirmed switch at
  the bottom of the advanced settings).
* renodx-dlss5.addon64 (v8.5.0-rc10-unified1q): crash log and trace CSVs in
  dlss5/dlss5_log; log lines forwarded to the event log.
* renodx-mfgunlock.addon64 (0.11+unified1q-20261015): the 1p behaviour, with log
  lines and every unapplied multiplier decision forwarded to the event log.
* The bilingual usage document.

Everything else is carried over byte-for-byte from 1p, and a zip of the folder
is written beside it. Previous packages are preserved.
"""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011p-candidate'
OUT = ROOT / 'dist/unified-20261015q-candidate'
ZIP = ROOT / 'dist/unified-20261015q-candidate.zip'
BUILD = ROOT / 'builds/unified-20261011/unified1q'
EVIDENCE = ROOT / 'builds/unified-20261011/package-1q'

DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'
BRIDGE = 'dlss5-bridge.addon64'
GENERIC = 'renodx-dlss5.addon64'
MFG = 'renodx-mfgunlock.addon64'

overrides = {
    BRIDGE: BUILD / 'bridge' / BRIDGE,
    GENERIC: BUILD / 'generic' / GENERIC,
    MFG: BUILD / 'mfg' / MFG,
    DOC: ROOT / 'docs/统一候选使用与恢复-20261011.md',
}
expected_versions = {
    BRIDGE: b'1.4.13-pre8-unified1q-20261015',
    GENERIC: b'v8.5.0-rc10-unified1q',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


for path in (BASE, OUT, ZIP, EVIDENCE, BUILD, *BASE.parents, *OUT.parents, *EVIDENCE.parents, *BUILD.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
assert not OUT.exists(), 'Keep previous packages intact'
assert not ZIP.exists(), 'Keep previous archives intact'
assert not EVIDENCE.exists(), 'Keep previous packaging evidence intact'
for name, source in overrides.items():
    assert source.is_file(), f'missing build output {source}'
for name, marker in expected_versions.items():
    assert marker in overrides[name].read_bytes(), f'{name}: version marker {marker!r} not found'
assert b'0.11+unified1q-20261015'.decode('ascii').encode('utf-16-le') in overrides[MFG].read_bytes(), 'MFG version resource'

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified1p-20261012', manifest['version']

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
    'version': 'unified1q-20261015',
    'status': 'candidate; accepts the 2026-10-15 game interposer, restores pq-wait-fix2, logs in dlss5/dlss5_log, '
              'anomaly event log; builds and offline fixtures verified; no in-game or cross-GPU acceptance yet',
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
assert set(changed) == {BRIDGE, GENERIC, MFG, DOC}, changed

# Zip with forward slashes and UTF-8 names, every file of the folder.
with zipfile.ZipFile(ZIP, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for path in sorted(p for p in OUT.rglob('*') if p.is_file()):
        archive.write(path, path.relative_to(OUT).as_posix())
with zipfile.ZipFile(ZIP) as archive:
    names = archive.namelist()
    assert len(names) == 23, len(names)
    for entry in entries:
        assert hashlib.sha256(archive.read(entry['path'])).hexdigest() == entry['sha256'], entry['path']
    assert archive.testzip() is None

result = {
    'version': 'unified1q',
    'payload_files': len(entries),
    'total_files': 23,
    'changed_payloads_from_1p': changed,
    'bridge_build': '1.4.13-pre8-unified1q-20261015',
    'generic_build': 'v8.5.0-rc10-unified1q',
    'mfg_build': '0.11+unified1q-20261015',
    'zip': {'path': ZIP.name, 'size': ZIP.stat().st_size, 'sha256': digest(ZIP)},
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
