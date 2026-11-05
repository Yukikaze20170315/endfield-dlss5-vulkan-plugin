"""Package the unified candidate that carries the wait-chain recovery fix.

Base is the 1k package (built-in Throttle=2, Generic 1i, production runtimes).
Two payloads are replaced:

* dlss5-bridge.addon64  -> pq-wait-fix2, which makes the recovery-chain waits
  (Synth12WaitFence, FgCompositeWait) establish completion from the fence
  instead of trusting a single event wake, within unchanged budgets.
* renodx-mfgunlock.addon64 -> hook-cause-audit1, the diagnostics-only MFG build
  that adds thread attribution for hook failures and changes no decision.

The bilingual usage document is refreshed from docs/. The result matches the
combination verified in game on the RTX 5090 development machine. Previous
packages are preserved.
"""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011k-candidate'
OUT = ROOT / 'dist/unified-20261011m-candidate'
EVIDENCE = ROOT / 'builds/unified-20261011/wait-recovery1m'

DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'
BRIDGE = 'dlss5-bridge.addon64'
MFG = 'renodx-mfgunlock.addon64'

overrides = {
    BRIDGE: ROOT / 'builds/unified-20261011/pq-wait-fix2/dlss5-bridge.addon64',
    MFG: ROOT / 'builds/unified-20261011/hook-cause-audit1/renodx-mfgunlock.addon64',
    DOC: ROOT / 'docs/统一候选使用与恢复-20261011.md',
}

# Recorded as the tested combination; the package must match these.
expected_hashes = {
    BRIDGE: 'f0433795b700199e5f08152ed8d158b62f5c4b8c687b05368d6e0d12de800b3a',
    MFG: '11faad1340d0e520d38e60a28db713fb7bbcad3eeb96256517be25c96b150660',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


for path in (BASE, OUT, EVIDENCE, *BASE.parents, *OUT.parents, *EVIDENCE.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
assert not OUT.exists(), 'Keep previous packages intact'
assert not EVIDENCE.exists(), 'Keep previous packaging evidence intact'

# The two replaced payloads are the exact builds that were verified in game.
for name, expected in expected_hashes.items():
    actual = digest(overrides[name])
    assert actual == expected, f'{name}: expected {expected}, found {actual}'

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified1k-20261011', manifest['version']
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
    'version': 'unified1m-20261011',
    'status': 'candidate; wait-chain recovery fix verified in game on RTX 5090; '
              'MFG hook diagnostics only, intermittent hook failure unresolved; '
              'cross-GPU acceptance pending',
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
assert changed == sorted(overrides), changed

result = {
    'version': 'unified1m',
    'payload_files': len(entries),
    'total_files': 22,
    'changed_payloads_from_1k': changed,
    'bridge_version': '1.4.13-pre8-pq-wait-fix2-20261011',
    'mfg_build': 'hook-cause-audit1 (diagnostics only)',
    'generic_build': 'unified1i (unchanged from 1k)',
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
