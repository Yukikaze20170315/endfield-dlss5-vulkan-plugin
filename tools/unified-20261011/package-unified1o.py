"""Package the unified candidate that selects the NR runtime by GPU architecture.

Base is the 1n package. The change is what the user's four goals ask for: one
package for RTX 20 through 50, with no per-generation file choice by the user.

* The single runtime directory `dlss5/nr/common-310.8/` is replaced by two
  entries, `dlss5/nr/rtx50/` (the signed sm_120-only build) and
  `dlss5/nr/rtx20-40/` (the multi-generation build carrying sm_75/86/89/120).
  A user who never opens this package's folders sees no difference: the same
  archive is installed the same way on every card.
* `dlss5-bridge.addon64` becomes nr-runtime-select1o, which resolves this
  machine's architecture, looks the runtime up in the payload table, verifies
  that exact file, and answers Generic's device query with its path.
* `payload-manifest.json` is regenerated from the same table, so the manifest
  and the compiled table cannot drift apart.
* The bilingual usage document and the default package path in deploy.ps1 are
  refreshed.

Everything else -- Generic, MFG Unlock, the recovery trio, the Streamline
subdirectory -- is carried over byte-for-byte from 1n. Previous packages are
preserved.
"""
from pathlib import Path
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'dist/unified-20261011n-candidate'
STAGING = ROOT / 'dist/unified-20261011o-payload-staging'
OUT = ROOT / 'dist/unified-20261011o-candidate'
BUILD = ROOT / 'builds/unified-20261011/nr-runtime-select1o'
EVIDENCE = ROOT / 'builds/unified-20261011/package-1o'

DOC = 'DLSS5-使用与恢复 Usage and Recovery.md'
BRIDGE = 'dlss5-bridge.addon64'
PAYLOAD = 'payload-manifest.json'
NR_OLD = 'dlss5/nr/common-310.8/nvngx_dlssnr.dll'
NR_BLACKWELL = 'dlss5/nr/rtx50/nvngx_dlssnr.dll'
NR_GENERIC = 'dlss5/nr/rtx20-40/nvngx_dlssnr.dll'

# Recorded as the tested combination; the package must match these.
expected_hashes = {
    BRIDGE: '8f7b0e5f2419be90a80cd0d9da05c24b0363c21cd083ff9ce1273b7fcec6cfdf',
}

overrides = {
    BRIDGE: BUILD / BRIDGE,
    DOC: ROOT / 'docs/统一候选使用与恢复-20261011.md',
    PAYLOAD: STAGING / PAYLOAD,
    NR_BLACKWELL: STAGING / NR_BLACKWELL,
    NR_GENERIC: STAGING / NR_GENERIC,
    # The external recovery entry point now needs to recognise both shipped
    # runtimes, not only the Blackwell one, so it is refreshed from tools/.
    'DLSS5-Clean-Recovery.ps1': ROOT / 'tools/unified-20261011/DLSS5-Clean-Recovery.ps1',
}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


for path in (BASE, STAGING, OUT, EVIDENCE, *BASE.parents, *STAGING.parents,
             *OUT.parents, *EVIDENCE.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
assert not OUT.exists(), 'Keep previous packages intact'
assert not EVIDENCE.exists(), 'Keep previous packaging evidence intact'

for name, expected in expected_hashes.items():
    actual = digest(overrides[name])
    assert actual == expected, f'{name}: expected {expected}, found {actual}'

# The regenerated payload manifest must describe exactly the two runtimes the
# compiled table names, at the same paths and with the same hashes.
staged = json.loads((STAGING / PAYLOAD).read_text(encoding='utf-8'))
table = (ROOT / 'src/unified-20261011/common/payload.hpp').read_text(encoding='utf-8')
assert {entry['path'] for entry in staged['nr_runtimes']} == {NR_BLACKWELL, NR_GENERIC}
for entry in staged['nr_runtimes']:
    assert digest(STAGING / entry['path']) == entry['sha256'], entry['path']
    assert f'L"{entry["path"]}"' in table, entry['path']
    assert entry['sha256'] in table, entry['sha256']
assert 'Turing' in staged['nr_mapping'] and 'Blackwell' in staged['nr_mapping']
assert staged['nr_mapping']['Blackwell']['path'] == NR_BLACKWELL
assert staged['nr_mapping']['Ampere']['path'] == NR_GENERIC
assert staged['nr_mapping']['Ada']['path'] == NR_GENERIC
assert staged['nr_mapping']['Turing']['path'] == NR_GENERIC

manifest = json.loads((BASE / 'release-manifest.json').read_text(encoding='utf-8'))
assert manifest['version'] == 'unified1n-20261012', manifest['version']

entries = []
carried = []


def add(name, source, expected=None):
    target = OUT / name
    assert target.resolve().is_relative_to(OUT.resolve()), name
    target.parent.mkdir(parents=True, exist_ok=True)
    expected = digest(source) if expected is None else expected
    shutil.copy2(source, target)
    assert digest(target) == expected, name
    entries.append({'path': name, 'sha256': expected, 'size': target.stat().st_size})


renamed = {NR_OLD: NR_BLACKWELL}
for item in manifest['files']:
    name = item['path']
    source = BASE / name
    assert source.resolve().is_relative_to(BASE.resolve()), name
    assert digest(source) == item['sha256'], name
    if name == NR_OLD:
        # The single-runtime directory is replaced by the two-runtime layout.
        # The file content is unchanged; only where it lives is.
        assert digest(source) == digest(STAGING / NR_BLACKWELL), 'the 1n runtime must be the Blackwell build'
        add(NR_BLACKWELL, source, item['sha256'])
        carried.append(f'{NR_OLD} -> {NR_BLACKWELL}')
        continue
    add(name, overrides.get(name, source))

add(NR_GENERIC, STAGING / NR_GENERIC)

paths = {entry['path'] for entry in entries}
assert len(paths) == len(entries), 'duplicate payload path'
assert NR_OLD not in paths, 'the single-runtime path must not survive'

EVIDENCE.mkdir(parents=True)
(OUT / 'release-manifest.json').write_text(json.dumps({
    'version': 'unified1o-20261012',
    'status': 'candidate; NR runtime selected by GPU architecture, two runtimes in one package; '
              'offline fixture and bridge build verified; no in-game or cross-GPU acceptance yet',
    'files': entries,
}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

# The release manifest is itself covered by the sums file, so 22 lines;
# deploy.ps1 verifies both manifests through the same hash list.
sums = ''.join(f"{e['sha256']}  {e['path']}\n" for e in entries)
sums += f"{digest(OUT / 'release-manifest.json')}  release-manifest.json\n"
(OUT / 'SHA256SUMS.txt').write_text(sums, encoding='utf-8')

# Known packaging requirements: the external recovery entry points use Windows
# PowerShell 5.1, whose parser needs the BOM, and the CMD wrapper needs CRLF.
assert (OUT / 'DLSS5-Clean-Recovery.ps1').read_bytes().startswith(b'\xef\xbb\xbf')
assert b'\r\n' in (OUT / 'DLSS5-Recovery 故障恢复.cmd').read_bytes()
assert len(entries) == 21, len(entries)
assert sum(p.is_file() for p in OUT.rglob('*')) == 23

changed = sorted(e['path'] for e in entries if not (BASE / e['path']).is_file()
                 or digest(BASE / e['path']) != e['sha256'])
assert set(changed) == {BRIDGE, DOC, PAYLOAD, NR_BLACKWELL, NR_GENERIC,
                       'DLSS5-Clean-Recovery.ps1'}, changed

result = {
    'version': 'unified1o',
    'payload_files': len(entries),
    'total_files': 23,
    'changed_payloads_from_1n': changed,
    'bridge_version': '1.4.13-pre8-unified1o-20261012 (nr-runtime-select1o)',
    'nr_layout': {
        'Blackwell': NR_BLACKWELL,
        'Turing/Ampere/Ada': NR_GENERIC,
    },
    'carried_over': carried,
    'generic_build': 'unified1i (unchanged from 1n)',
    'mfg_build': 'hook-reliability1n (unchanged from 1n)',
    'sha256': {e['path']: e['sha256'] for e in entries},
}
(EVIDENCE / 'package-verification.json').write_text(
    json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if k != 'sha256'}, ensure_ascii=False))
