"""Package unified1u: the verified unified1t package with review corrections.

Changed against unified1t: the Bridge and Generic add-ons (rebuilt), the usage
document, the recovery menu (option A also works with a disabled or removed
Bridge), the clean-up recovery script and the standalone recovery helper (both
now also restore the Windows VRAM budget). MFG Unlock and all runtime files
are taken unchanged from unified1t. Existing candidates and archives are
preserved; copies, versions, file counts and ZIP contents are verified. This
script does not deploy to a game or change any driver/system configuration.
"""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261031t-candidate'
OUT = ROOT / 'dist/unified-20261031u-candidate'
ZIP = ROOT / 'dist/unified-20261031u-candidate.zip'
BUILD = ROOT / 'builds/unified-20261011/unified1u'
HELPER = ROOT / 'builds/unified-20261011/recovery/DLSS5-RestoreDriver.exe'
EVIDENCE = ROOT / 'builds/unified-20261011/package-1u'

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
    DOC: ROOT / 'docs/统一候选使用与恢复-20261031u.md',
    RECOVERY: ROOT / 'tools/unified-20261011/DLSS5-Recovery.cmd',
    CLEAN: ROOT / 'tools/unified-20261011/DLSS5-Clean-Recovery.ps1',
    RESTORE: HELPER,
}
expected_versions = {
    BRIDGE: b'1.4.13-pre8-unified1u-20261031',
    GENERIC: b'v8.5.0-rc10-unified1u-20261031',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


for path in (BASE, OUT, ZIP, EVIDENCE, BUILD, HELPER, *BASE.parents, *OUT.parents, *EVIDENCE.parents, *BUILD.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
assert not OUT.exists(), 'Keep previous packages intact'
assert not ZIP.exists(), 'Keep previous archives intact'
assert not EVIDENCE.exists(), 'Keep previous packaging evidence intact'
source_snapshot = json.loads((BUILD / 'source-manifest.json').read_text(encoding='utf-8'))
assert source_snapshot['version'] == 'unified1u-20261031'
for item in source_snapshot['files']:
    source = ROOT / item['path']
    assert source.resolve().is_relative_to((ROOT / 'src/unified-20261011').resolve())
    assert digest(source) == item['sha256'], f"source changed after build snapshot: {item['path']}"
for name, source in overrides.items():
    assert source.is_file(), f'missing build output {source}'
for name, marker in expected_versions.items():
    assert marker in overrides[name].read_bytes(), f'{name}: version marker {marker!r} not found'
assert HELPER.stat().st_mtime >= (BUILD / 'source-manifest.json').stat().st_mtime - 7200, 'recovery helper is stale'
assert b'Windows VRAM budget' in HELPER.read_bytes(), 'recovery helper lacks the VRAM budget step'

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified1t-20261031', manifest['version']
assert b'0.11+unified1t-20261031'.decode('ascii').encode('utf-16-le') in (BASE / MFG).read_bytes(), 'MFG from 1t'

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
    'version': 'unified1u-20261031',
    'status': 'candidate; unified1t plus review corrections: UI translations, crash-handler chain, VRAM budget '
              'restore in clean-up recovery and with a disabled Bridge, machine-wide VRAM budget backup, NR '
              'runtime query failure code; built and offline-tested; not deployed or accepted in game or across GPUs',
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
assert b'restore-vram-budget' in recovery_bytes and b'dlss5-bridge.addon64.off' in recovery_bytes
assert b'vram-budget-change' in (OUT / CLEAN).read_bytes()
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
    'version': 'unified1u',
    'payload_files': len(entries),
    'total_files': 23,
    'changed_payloads_from_1t': changed,
    'bridge_build': '1.4.13-pre8-unified1u-20261031',
    'generic_build': 'v8.5.0-rc10-unified1u-20261031',
    'mfg_build': '0.11+unified1t-20261031 (unchanged)',
    'zip': {'path': ZIP.name, 'size': ZIP.stat().st_size, 'sha256': digest(ZIP)},
    'source_manifest_sha256': digest(BUILD / 'source-manifest.json'),
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
