"""Patch unified-20261015q-candidate in place: recovery flow update.

Replaces exactly three payloads with the updated tools/docs sources and
rebuilds the zip, manifest and SHA256SUMS:

* DLSS5-Clean-Recovery.ps1: Apps= entries whose program no longer exists are
  reported as stale_apps instead of blocking removal as shared scope; the
  Restore entry point lists ignored stale paths and, when real shared
  programs remain and -AllowSharedReShade was not given, lists them and
  exits 3 before any mutation.
* DLSS5-Recovery 故障恢复.cmd: menu option 9 surfaces that exit-3 stop,
  asks Y/N, reruns with -AllowSharedReShade on Y, and reports success,
  cancellation and failure distinctly.
* DLSS5-使用与恢复 Usage and Recovery.md: matching wording for the new flow.

The original three payloads plus manifest, sums and zip are first backed up
to builds/unified-20261011/package-1q-original-20261019/. Version and status
strings are unchanged.
"""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]
PKG = ROOT / 'dist/unified-20261015q-candidate'
ZIP = ROOT / 'dist/unified-20261015q-candidate.zip'
BACKUP = ROOT / 'builds/unified-20261011/package-1q-original-20261019'
EVIDENCE = ROOT / 'builds/unified-20261011/package-1q-recovery-fix'

PS1 = 'DLSS5-Clean-Recovery.ps1'
CMD = 'DLSS5-Recovery 故障恢复.cmd'
DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'

sources = {
    PS1: ROOT / 'tools/unified-20261011/DLSS5-Clean-Recovery.ps1',
    CMD: ROOT / 'tools/unified-20261011/DLSS5-Recovery.cmd',
    DOC: ROOT / 'docs/统一候选使用与恢复-20261011.md',
}
expected_old = {
    PS1: '41eb488389af9c36deb6e29a7d2574330b0ec89c443398790f429472ef03ef72',
    CMD: '30484925c01037e82c6310887da38bdadb1c60cab263fa136263c56dd320abcf',
    DOC: '4b0fa3e386286facd25a2c50a70c62105b93c819ba2ba0e6f91051a170e543be',
}
backup_names = [PS1, CMD, DOC, 'release-manifest.json', 'SHA256SUMS.txt']


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


for path in (PKG, ZIP, BACKUP, EVIDENCE, *PKG.parents, *BACKUP.parents, *EVIDENCE.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
assert PKG.is_dir(), PKG
assert ZIP.is_file(), ZIP
assert not BACKUP.exists(), 'Keep the pre-patch backup destination free'
for name, source in sources.items():
    assert source.is_file(), f'missing source {source}'
old = {name: digest(PKG / name) for name in sources}
for name, expected in expected_old.items():
    assert old[name] == expected, f'{name}: unexpected pre-patch hash {old[name]}'

BACKUP.mkdir(parents=True)
for name in backup_names:
    target = BACKUP / name
    expected = digest(PKG / name)
    shutil.copy2(PKG / name, target)
    assert digest(target) == expected, f'backup mismatch: {name}'
expected_zip = digest(ZIP)
shutil.copy2(ZIP, BACKUP / ZIP.name)
assert digest(BACKUP / ZIP.name) == expected_zip, 'backup mismatch: zip'

manifest_path = PKG / 'release-manifest.json'
manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
entries = manifest['files']
by_path = {entry['path']: entry for entry in entries}
assert len(by_path) == len(entries) == 21, len(entries)
assert set(sources) <= set(by_path), 'payload missing from manifest'

new = {}
for name, source in sources.items():
    expected = digest(source)
    target = PKG / name
    shutil.copy2(source, target)
    assert digest(target) == expected, f'copy mismatch: {name}'
    new[name] = {'sha256': expected, 'size': target.stat().st_size}

changed = []
for entry in entries:
    name = entry['path']
    if name in new:
        entry['sha256'] = new[name]['sha256']
        entry['size'] = new[name]['size']
        changed.append(name)
    else:
        assert digest(PKG / name) == entry['sha256'], f'unrelated payload changed: {name}'
        assert (PKG / name).stat().st_size == entry['size'], f'unrelated payload size changed: {name}'
assert set(changed) == set(sources), changed

manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

sums = ''.join(f"{e['sha256']}  {e['path']}\r\n" for e in entries)
sums += f"{digest(manifest_path)}  release-manifest.json\r\n"
with (PKG / 'SHA256SUMS.txt').open('w', encoding='utf-8', newline='') as stream:
    stream.write(sums)

assert (PKG / PS1).read_bytes().startswith(b'\xef\xbb\xbf')
assert b'\r\n' in (PKG / CMD).read_bytes()
assert len(entries) == 21, len(entries)
assert sum(p.is_file() for p in PKG.rglob('*')) == 23

with zipfile.ZipFile(ZIP, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    for path in sorted(p for p in PKG.rglob('*') if p.is_file()):
        archive.write(path, path.relative_to(PKG).as_posix())
with zipfile.ZipFile(ZIP) as archive:
    names = archive.namelist()
    assert len(names) == 23, len(names)
    for entry in entries:
        assert hashlib.sha256(archive.read(entry['path'])).hexdigest() == entry['sha256'], entry['path']
    assert hashlib.sha256(archive.read('release-manifest.json')).hexdigest() == digest(manifest_path)
    assert hashlib.sha256(archive.read('SHA256SUMS.txt')).hexdigest() == digest(PKG / 'SHA256SUMS.txt')
    assert archive.testzip() is None

result = {
    'version': 'unified1q-20261015',
    'change': 'recovery flow: stale Apps= entries ignored, shared-scope exit 3 + menu Y/N confirmation',
    'changed_payloads': changed,
    'old_sha256': old,
    'new_sha256': {e['path']: e['sha256'] for e in entries if e['path'] in new},
    'zip': {'path': ZIP.name, 'size': ZIP.stat().st_size, 'sha256': digest(ZIP)},
}
EVIDENCE.mkdir(parents=True, exist_ok=True)
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, ensure_ascii=False))
