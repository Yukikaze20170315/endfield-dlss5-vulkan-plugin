"""Stage the unified-20261103l repository tree and release assets.

Writes only below dist/release-unified-20261103l/. Hand-written text (README.md,
BUILDING.md, .gitattributes) lives in that folder and is copied, not generated.

    python tools/release-unified-20261103l/stage.py [verify|repo|assets|all]

Inputs (read only): src/unified-20261011, tools/unified-20261011,
upstream/NVIDIA-nvapi-87dca62, the unified2l source manifest, the unified2l usage
document, dist/unified-20261103l-candidate.zip and the README files already
published at commit 5532bb6 of the local clone worktrees/dlss5-vulkan-fg-relay.
"""
import hashlib, json, re, shutil, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'dist' / 'release-unified-20261103l'
REPO = OUT / 'repo'
ASSETS = OUT / 'assets'
TAG = 'unified-20261103l'
CANDIDATE_ZIP = ROOT / 'dist' / 'unified-20261103l-candidate.zip'
CANDIDATE_SHA256 = 'a4bc27f82fb83433fd5bf7243e2b598449b98e79d8f8a5acb3516f012d322985'
SOURCE_MANIFEST = ROOT / 'builds' / 'unified-20261011' / 'unified2l' / 'source-manifest.json'
USAGE = ROOT / 'docs' / '统一候选使用与恢复-20261103-2l.md'
USAGE_SHA256 = '8cdc14f408a799d8304ca2731a63f31745d9fdc479aaf3d5b497fffcd3d30d31'
PUBLISHED_GIT = ROOT / 'worktrees' / 'dlss5-vulkan-fg-relay'
PUBLISHED_COMMIT = '5532bb6'

SOURCE_TREES = (
    (ROOT / 'src' / 'unified-20261011', 'src/unified-20261011'),
    (ROOT / 'tools' / 'unified-20261011', 'tools/unified-20261011'),
    (ROOT / 'upstream' / 'NVIDIA-nvapi-87dca62', 'upstream/NVIDIA-nvapi-87dca62'),
)
# Build outputs left in bridge/src by its own build.cmd, git metadata, Python
# caches and the contrib/ copy of earlier release material are left out. The NGX
# SDK headers under mfg/deps/ngx carry NVIDIA's proprietary licence notice and
# are not redistributed; BUILDING.md says where to get them.
SOURCE_EXCLUDE = re.compile(
    r'(^|/)(contrib/dlss5-bridge-vulkan-fg-relay/|\.git/|__pycache__/)'
    r'|^src/unified-20261011/mfg/deps/ngx/'
    r'|^src/unified-20261011/bridge/src/[^/]+\.(obj|lib|exp|res|addon64)$'
    r'|\.(pdb|ilk|iobj|ipdb|pyc)$', re.I)
HAND_WRITTEN = ('README.md', 'BUILDING.md', '.gitattributes')


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def source_files():
    for base, prefix in SOURCE_TREES:
        for p in sorted(base.rglob('*')):
            if p.is_file():
                rel = prefix + '/' + p.relative_to(base).as_posix()
                if not SOURCE_EXCLUDE.search(rel):
                    yield p, rel


def verify():
    manifest = json.loads(SOURCE_MANIFEST.read_text(encoding='utf-8'))
    wrong = [f['path'] for f in manifest['files'] if not (ROOT / f['path']).is_file() or sha256(ROOT / f['path']) != f['sha256']]
    if wrong:
        raise SystemExit(f'source differs from the unified2l build snapshot: {wrong[:10]} ({len(wrong)})')
    shipped = {rel for _, rel in source_files()}
    missing = [f['path'] for f in manifest['files'] if f['path'] not in shipped and not SOURCE_EXCLUDE.search(f['path'])]
    if missing:
        raise SystemExit(f'manifest files not staged: {missing[:10]}')
    if sha256(CANDIDATE_ZIP) != CANDIDATE_SHA256:
        raise SystemExit('candidate zip hash changed')
    if sha256(USAGE) != USAGE_SHA256:
        raise SystemExit('usage document differs from the packaged copy')
    print(f'verify: {len(manifest["files"])} manifest files match, candidate zip and usage document match')


def git_show(spec):
    return subprocess.run(['git', '-C', str(PUBLISHED_GIT), 'show', f'{PUBLISHED_COMMIT}:{spec}'], check=True, capture_output=True).stdout


def stage_repo():
    if REPO.exists():
        shutil.rmtree(REPO)
    REPO.mkdir(parents=True)
    count = 0
    for p, rel in source_files():
        t = REPO / rel
        t.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(p, t); count += 1
    for name in HAND_WRITTEN:
        shutil.copy2(OUT / name, REPO / name); count += 1
    docs = REPO / 'docs'
    (docs / 'history').mkdir(parents=True)
    shutil.copy2(USAGE, docs / 'USAGE-AND-RECOVERY.md'); count += 1
    (docs / 'history' / 'README-unified-20261015q.md').write_bytes(git_show('README.md')); count += 1
    (docs / 'history' / 'README-vk-fgrelay-20261005.md').write_bytes(git_show('docs/history/README-vk-fgrelay-20261005.md')); count += 1
    tools_out = REPO / 'tools' / 'release-unified-20261103l'
    tools_out.mkdir(parents=True)
    for name in ('stage.py', 'secret-scan.py'):
        shutil.copy2(Path(__file__).parent / name, tools_out / name); count += 1
    print('repo files', count)


def stage_assets():
    ASSETS.mkdir(parents=True, exist_ok=True)
    target = ASSETS / f'{TAG}-source.zip'
    count = 0
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for p, rel in source_files():
            z.write(p, rel); count += 1
        z.write(OUT / 'BUILDING.md', 'BUILDING.md'); count += 1
    print('source zip entries', count)
    shutil.copy2(CANDIDATE_ZIP, ASSETS / f'{TAG}-candidate.zip')
    rows = [f'{sha256(p)}  {p.name}\n' for p in sorted(ASSETS.iterdir()) if p.is_file() and p.name != 'SHA256SUMS.txt']
    (ASSETS / 'SHA256SUMS.txt').write_text(''.join(rows), encoding='utf-8', newline='\n')
    print(''.join(rows), end='')


def write_repo_manifest():
    rows = [f'{sha256(p)}  {p.relative_to(REPO).as_posix()}\n' for p in sorted(REPO.rglob('*')) if p.is_file()]
    (OUT / 'REPO-MANIFEST.txt').write_text(''.join(rows), encoding='utf-8', newline='\n')
    print('repo manifest rows', len(rows))


if __name__ == '__main__':
    step = sys.argv[1] if len(sys.argv) > 1 else 'all'
    verify()
    if step in ('repo', 'all'):
        stage_repo(); write_repo_manifest()
    if step in ('assets', 'all'):
        stage_assets()
