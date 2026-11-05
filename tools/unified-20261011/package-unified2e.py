"""Package unified2e: the unified2d package with one review fix.

Changed against unified2d: the MFG Unlock add-on (the safe-mode page names the
DLSS5Bridge page instead of the removed Recovery heading;
0.11+unified2e-20261101) and the usage document. Bridge, Generic, the
uninstaller, the restore helper and all runtime files are taken unchanged from
unified2d. Existing candidates and archives are preserved; copies, versions,
file counts and ZIP contents are verified. This script does not deploy to a
game or change any driver/system configuration.
"""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261101d-candidate'
OUT = ROOT / 'dist/unified-20261101e-candidate'
ZIP = ROOT / 'dist/unified-20261101e-candidate.zip'
EVIDENCE = ROOT / 'builds/unified-20261011/package-2e'
BUILD = ROOT / 'builds/unified-20261011/unified2e'

DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'
BRIDGE = 'dlss5-bridge.addon64'
GENERIC = 'renodx-dlss5.addon64'
MFG = 'renodx-mfgunlock.addon64'
RECOVERY = 'DLSS5-Recovery 故障恢复.cmd'
CLEAN = 'DLSS5-Clean-Recovery.ps1'
RESTORE = 'DLSS5-RestoreDriver.exe'

overrides = {
    MFG: BUILD / 'mfg' / MFG,
    DOC: ROOT / 'docs/统一候选使用与恢复-20261101-2e.md',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


for path in (BASE, OUT, ZIP, EVIDENCE, *BASE.parents, *OUT.parents, *EVIDENCE.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
assert not OUT.exists(), 'Keep previous packages intact'
assert not ZIP.exists(), 'Keep previous archives intact'
assert not EVIDENCE.exists(), 'Keep previous packaging evidence intact'
for name, source in overrides.items():
    assert source.is_file(), f'missing source {source}'

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified2d-20261101', manifest['version']
assert b'1.4.13-pre8-unified2d-20261101' in (BASE / BRIDGE).read_bytes(), 'Bridge 2d build'
assert b'v8.5.0-rc10-unified2d-20261101' in (BASE / GENERIC).read_bytes(), 'Generic 2d build'
mfg_bytes = overrides[MFG].read_bytes()
assert '0.11+unified2e-20261101'.encode('utf-16-le') in mfg_bytes, 'MFG 2e build'
assert '在 DLSS5Bridge 页取消勾选'.encode('utf-8') in mfg_bytes and '故障与恢复'.encode('utf-8') not in mfg_bytes, 'MFG safe-mode text'
snapshot = json.loads((BUILD / 'source-manifest.json').read_text(encoding='utf-8'))
assert snapshot['version'] == 'unified2e-20261101'
for item in snapshot['files']:
    assert digest(ROOT / item['path']) == item['sha256'], f"source changed after build snapshot: {item['path']}"
for name in (MFG,):
    assert overrides[name].stat().st_mtime > (BUILD / 'source-manifest.json').stat().st_mtime, f'{name} built before the snapshot'

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
    'version': 'unified2e-20261101',
    'status': 'candidate; unified2d plus one review fix: the MFG Unlock safe-mode page names the DLSS5Bridge page '
              'instead of the removed Recovery heading; Bridge, Generic, uninstaller, restore helper and runtime files '
              'unchanged from unified2d; offline-tested; not deployed or accepted in game or across GPUs',
    'files': entries,
}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

sums = ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
sums += f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n"
(OUT / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')

clean_bytes = (OUT / CLEAN).read_bytes()
assert clean_bytes.startswith(b'\xef\xbb\xbf'), 'PowerShell 5 needs the UTF-8 BOM'
for name in (RECOVERY, CLEAN):
    data = (OUT / name).read_bytes()
    assert b'\r\n' in data and data.count(b'\n') == data.count(b'\r\n'), f'{name} must be CRLF only'
recovery_bytes = (OUT / RECOVERY).read_bytes()
assert max(recovery_bytes) < 128, 'recovery launcher must stay ASCII-only'
assert b'-Action Uninstall' in recovery_bytes and b'(goto) 2>nul' in recovery_bytes
for marker in (b'Move-CleanTools', b'Move-CleanLeftovers', b'exit 9', b'ForEach-Object { Write-Host $_ }', 'DLSS5-上次启动异常-说明.txt'.encode('utf-8')):
    assert marker in clean_bytes, marker
assert b'Windows VRAM budget' in (OUT / RESTORE).read_bytes()
assert 'unified2e'.encode('utf-8') in (OUT / DOC).read_bytes()
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
    'version': 'unified2e',
    'payload_files': len(entries),
    'total_files': 23,
    'changed_payloads_from_2d': changed,
    'bridge_build': '1.4.13-pre8-unified2d-20261101',
    'generic_build': 'v8.5.0-rc10-unified2d-20261101',
    'mfg_build': '0.11+unified2e-20261101',
    'zip': {'path': ZIP.name, 'size': ZIP.stat().st_size, 'sha256': digest(ZIP)},
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
