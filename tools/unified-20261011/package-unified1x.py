"""Package unified1x: the verified unified1w package with a third-review correction.

Changed against unified1w: the Bridge add-on (the crash-handler chain reads a
module's PE headers only when they are readable and under an exception guard;
a handler whose module hides or erases its headers is matched by its base
address alone, so the Bridge cannot fault there and the game's own crash
reporter is not skipped for that reason), the Generic add-on (version only)
and the usage document. MFG Unlock, the recovery menu, the clean-up script, the
recovery helper and all runtime files are taken unchanged from unified1w.
Existing candidates and archives are preserved; copies, versions, file counts
and ZIP contents are verified. This script does not deploy to a game or change
any driver/system configuration.
"""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261031w-candidate'
OUT = ROOT / 'dist/unified-20261031x-candidate'
ZIP = ROOT / 'dist/unified-20261031x-candidate.zip'
BUILD = ROOT / 'builds/unified-20261011/unified1x'
EVIDENCE = ROOT / 'builds/unified-20261011/package-1x'

DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'
BRIDGE = 'dlss5-bridge.addon64'
GENERIC = 'renodx-dlss5.addon64'
MFG = 'renodx-mfgunlock.addon64'
RECOVERY = 'DLSS5-Recovery 故障恢复.cmd'
CLEAN = 'DLSS5-Clean-Recovery.ps1'
RESTORE = 'DLSS5-RestoreDriver.exe'

overrides = {
    BRIDGE: BUILD / 'bridge' / BRIDGE,
    GENERIC: BUILD / 'generic' / GENERIC,
    DOC: ROOT / 'docs/统一候选使用与恢复-20261031x.md',
}
expected_versions = {
    BRIDGE: b'1.4.13-pre8-unified1x-20261031',
    GENERIC: b'v8.5.0-rc10-unified1x-20261031',
}
unchanged_sources = {
    RECOVERY: ROOT / 'tools/unified-20261011/DLSS5-Recovery.cmd',
    CLEAN: ROOT / 'tools/unified-20261011/DLSS5-Clean-Recovery.ps1',
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
source_snapshot = json.loads((BUILD / 'source-manifest.json').read_text(encoding='utf-8'))
assert source_snapshot['version'] == 'unified1x-20261031'
for item in source_snapshot['files']:
    source = ROOT / item['path']
    assert source.resolve().is_relative_to((ROOT / 'src/unified-20261011').resolve())
    assert digest(source) == item['sha256'], f"source changed after build snapshot: {item['path']}"
for name, source in overrides.items():
    assert source.is_file(), f'missing build output {source}'
for name, marker in expected_versions.items():
    assert marker in overrides[name].read_bytes(), f'{name}: version marker {marker!r} not found'

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified1w-20261031', manifest['version']
assert '0.11+unified1t-20261031'.encode('utf-16-le') in (BASE / MFG).read_bytes(), 'MFG from 1t'
for name, source in unchanged_sources.items():
    assert digest(source) == digest(BASE / name), f'{name} changed after unified1w; package it explicitly'

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
    'version': 'unified1x-20261031',
    'status': 'candidate; unified1w plus third-review correction: crash-handler chain reads module headers only '
              'when readable and under an exception guard and keeps handlers whose headers are hidden or erased; '
              'built and offline-tested; not deployed or accepted in game or across GPUs',
    'files': entries,
}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

sums = ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
sums += f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n"
(OUT / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')

assert (OUT / CLEAN).read_bytes().startswith(b'\xef\xbb\xbf'), 'PowerShell 5 needs the UTF-8 BOM'
for name in (RECOVERY, CLEAN):
    data = (OUT / name).read_bytes()
    assert b'\r\n' in data and data.count(b'\n') == data.count(b'\r\n'), f'{name} must be CRLF only'
recovery_bytes = (OUT / RECOVERY).read_bytes()
assert b'helper_both_failed' in recovery_bytes and b'WindowStyle Hidden' not in recovery_bytes
assert b'Windows VRAM budget' in (OUT / RESTORE).read_bytes()
assert len(entries) == 21, len(entries)
assert sum(p.is_file() for p in OUT.rglob('*')) == 23

changed = sorted(e['path'] for e in entries if digest(BASE / e['path']) != e['sha256'])
assert set(changed) == set(overrides), changed

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
    'version': 'unified1x',
    'payload_files': len(entries),
    'total_files': 23,
    'changed_payloads_from_1w': changed,
    'bridge_build': '1.4.13-pre8-unified1x-20261031',
    'generic_build': 'v8.5.0-rc10-unified1x-20261031',
    'mfg_build': '0.11+unified1t-20261031 (unchanged)',
    'zip': {'path': ZIP.name, 'size': ZIP.stat().st_size, 'sha256': digest(ZIP)},
    'source_manifest_sha256': digest(BUILD / 'source-manifest.json'),
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
