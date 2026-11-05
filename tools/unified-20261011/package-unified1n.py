"""Package the unified candidate that makes the frame-generation gateway hook reliable.

Base is the 1m package (wait-chain recovery Bridge, diagnostics-only MFG, Generic 1i).
One payload is replaced:

* renodx-mfgunlock.addon64 -> hook-reliability1n, which contains three changes and
  nothing else:
    1. EnlistThreads re-checks a failed registration against the thread's exit
       signal, so a thread that exited during enumeration is skipped instead of
       aborting the whole Detours transaction. A live thread which cannot be
       suspended still aborts, unchanged.
    2. HookedGetFeatureFunction installs the slDLSSGSetOptions entry proactively
       once any DLSS-G entry has been resolved from the plugin module, instead of
       waiting for a host lookup that only ever happens once.
    3. ApplyLiveOptionsOnPresent retries a failed SetOptions install with a
       bounded attempt count and geometric backoff.

The bilingual usage document is refreshed from docs/. Previous packages are preserved.
"""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011m-candidate'
OUT = ROOT / 'dist/unified-20261011n-candidate'
EVIDENCE = ROOT / 'builds/unified-20261011/hook-reliability1n'
BUILD = ROOT / 'builds/unified-20261011/mfg-hook-reliability1n'

DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'
MFG = 'renodx-mfgunlock.addon64'

# Recorded as the tested combination; the package must match these.
expected_hashes = {
    MFG: '4df107998bbf1fbda3cb5db7e2b3c16b1087cf8b5ec2e178d671a0a71b03c3e7',
}

overrides = {
    MFG: BUILD / MFG,
    DOC: ROOT / 'docs/统一候选使用与恢复-20261011.md',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


for path in (BASE, OUT, EVIDENCE, *BASE.parents, *OUT.parents, *EVIDENCE.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
assert not OUT.exists(), 'Keep previous packages intact'
assert not EVIDENCE.exists(), 'Keep previous packaging evidence intact'

for name, expected in expected_hashes.items():
    actual = digest(overrides[name])
    assert actual == expected, f'{name}: expected {expected}, found {actual}'

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified1m-20261011', manifest['version']
for item in manifest['files']:
    source = BASE / item['path']
    assert source.resolve().is_relative_to(BASE.resolve()), item['path']
    assert digest(source) == item['sha256'], item['path']
    assert overrides.get(item['path'], source).is_file(), item['path']
assert set(overrides) <= {item['path'] for item in manifest['files']}

entries = []
for item in manifest['files']:
    name = item['path']
    source = overrides.get(name, BASE / name)
    target = OUT / name
    assert target.resolve().is_relative_to(OUT.resolve()), name
    target.parent.mkdir(parents=True, exist_ok=True)
    expected = digest(source)
    shutil.copy2(source, target)
    assert digest(target) == expected, name
    entries.append({'path': name, 'sha256': expected, 'size': target.stat().st_size})

EVIDENCE.mkdir(parents=True)
(OUT / 'release-manifest.json').write_text(json.dumps({
    'version': 'unified1n-20261012',
    'status': 'candidate; MFG gateway-install reliability fix offline verified; '
              'no in-game or cross-GPU acceptance yet; five-layer histogram and '
              '30/40-series first-install acceptance still pending',
    'files': entries,
}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
(OUT / 'SHA256SUMS.txt').write_text(
    ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
    + f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n", encoding='utf-8')

# Known packaging requirements: the external recovery entry points use Windows
# PowerShell 5.1, whose parser needs the BOM, and the CMD wrapper needs CRLF.
assert (OUT / 'DLSS5-Clean-Recovery.ps1').read_bytes().startswith(b'\xef\xbb\xbf')
assert b'\r\n' in (OUT / 'DLSS5-Recovery 故障恢复.cmd').read_bytes()
assert len(entries) == 20, len(entries)
assert sum(p.is_file() for p in OUT.rglob('*')) == 22

changed = sorted(e['path'] for e in entries if digest(BASE / e['path']) != e['sha256'])
# The usage document is re-copied from docs/ and happens to be byte-identical to
# the 1m copy, so only payloads which really differ may appear here.
assert set(changed) <= set(overrides), changed
assert MFG in changed, changed

result = {
    'version': 'unified1n',
    'payload_files': len(entries),
    'total_files': 22,
    'changed_payloads_from_1m': changed,
    'bridge_version': '1.4.13-pre8-pq-wait-fix2-20261011 (unchanged from 1m)',
    'mfg_build': 'hook-reliability1n (exited-thread tolerance, proactive install, present retry)',
    'generic_build': 'unified1i (unchanged from 1m)',
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
