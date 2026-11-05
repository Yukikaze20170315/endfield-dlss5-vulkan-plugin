"""Package unified1y: the unified1x package with an uninstall-only recovery tool.

Changed against unified1x: the recovery launcher (an ASCII-only single-line
launcher that runs the full uninstall, replacing the numbered menu whose far
goto jumps could land on the wrong label), the clean-up script (Uninstall
action with confirmations, archives its own tool files into the backup after a
complete restore, temporary-folder elevation report), the Bridge and MFG Unlock
add-ons (in-game texts no longer mention menu options; versions) and the usage
document. The Generic add-on, the recovery helper and all runtime files are
taken unchanged from unified1x. Existing candidates and archives are preserved;
copies, versions, file counts and ZIP contents are verified. This script does
not deploy to a game or change any driver/system configuration.
"""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261031x-candidate'
OUT = ROOT / 'dist/unified-20261031y-candidate'
ZIP = ROOT / 'dist/unified-20261031y-candidate.zip'
BUILD = ROOT / 'builds/unified-20261011/unified1y'
EVIDENCE = ROOT / 'builds/unified-20261011/package-1y'

DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'
BRIDGE = 'dlss5-bridge.addon64'
GENERIC = 'renodx-dlss5.addon64'
MFG = 'renodx-mfgunlock.addon64'
RECOVERY = 'DLSS5-Recovery 故障恢复.cmd'
CLEAN = 'DLSS5-Clean-Recovery.ps1'
RESTORE = 'DLSS5-RestoreDriver.exe'

overrides = {
    BRIDGE: BUILD / 'bridge' / BRIDGE,
    MFG: BUILD / 'mfg' / MFG,
    RECOVERY: ROOT / 'tools/unified-20261011/DLSS5-Recovery.cmd',
    CLEAN: ROOT / 'tools/unified-20261011/DLSS5-Clean-Recovery.ps1',
    DOC: ROOT / 'docs/统一候选使用与恢复-20261101y.md',
}
expected_versions = {
    BRIDGE: b'1.4.13-pre8-unified1y-20261101',
    MFG: '0.11+unified1y-20261101'.encode('utf-16-le'),
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
assert source_snapshot['version'] == 'unified1y-20261101'
for item in source_snapshot['files']:
    source = ROOT / item['path']
    assert source.resolve().is_relative_to((ROOT / 'src/unified-20261011').resolve())
    assert digest(source) == item['sha256'], f"source changed after build snapshot: {item['path']}"
for name, source in overrides.items():
    assert source.is_file(), f'missing build output {source}'
for name, marker in expected_versions.items():
    assert marker in overrides[name].read_bytes(), f'{name}: version marker {marker!r} not found'

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified1x-20261031', manifest['version']
assert b'v8.5.0-rc10-unified1x-20261031' in (BASE / GENERIC).read_bytes(), 'Generic from 1x'

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
    'version': 'unified1y-20261101',
    'status': 'candidate; unified1x with an uninstall-only recovery launcher (fixes the menu jumping to the wrong '
              'option), uninstaller files archived after a complete restore and in-game texts updated; '
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
assert max(recovery_bytes) < 128, 'recovery launcher must stay ASCII-only'
assert b'-Action Uninstall' in recovery_bytes and b'(goto) 2>nul' in recovery_bytes
assert b'goto :' not in recovery_bytes.lower() and b'WindowStyle Hidden' not in recovery_bytes
assert b'Move-CleanTools' in (OUT / CLEAN).read_bytes()
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
    'version': 'unified1y',
    'payload_files': len(entries),
    'total_files': 23,
    'changed_payloads_from_1x': changed,
    'bridge_build': '1.4.13-pre8-unified1y-20261101',
    'generic_build': 'v8.5.0-rc10-unified1x-20261031 (unchanged)',
    'mfg_build': '0.11+unified1y-20261101',
    'zip': {'path': ZIP.name, 'size': ZIP.stat().st_size, 'sha256': digest(ZIP)},
    'source_manifest_sha256': digest(BUILD / 'source-manifest.json'),
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
