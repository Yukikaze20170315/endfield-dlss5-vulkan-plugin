"""Snapshot sources into a new, non-overwriting development tree."""
from pathlib import Path
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[2]
out = root / 'src/unified-20261011'
if out.exists():
    raise SystemExit('Refusing to replace an existing source snapshot')
reno = Path('F:/Users/Administrator/Documents/dlss5-multistage-dev/fix-20261004/renodx')
out.mkdir()
shutil.copytree(root / 'src/bridge-mfg-coexist-codex-20261011', out / 'bridge', ignore=shutil.ignore_patterns('*.obj', '*.res', '*.lib', '*.exp', '*.addon64'))
shutil.copytree(root / 'src/mfg-blackwell-codex-20261011', out / 'mfg')
for part in ['addons/dlss5', 'utils', 'shaders', 'mods']:
    shutil.copytree(reno / 'src' / part, out / 'generic/src' / part)
shutil.copytree(reno.parent / 'build-dlss5/dlss5.include', out / 'generic/generated')
records = {}
for path in out.rglob('*'):
    if path.is_file():
        records[str(path.relative_to(out))] = hashlib.sha256(path.read_bytes()).hexdigest()
(out / 'baseline-sha256.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
print(f'Snapshotted {len(records)} files into {out}')
