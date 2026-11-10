"""Stage the unified-20261107m repository tree and release assets.

Writes only below dist/release-unified-20261107m/. Hand-written text (README.md,
BUILDING.md, .gitattributes) lives in that folder and is copied, not generated.

    python tools/release-unified-20261107m/stage.py [verify|repo|assets|all]

Inputs (read only): src/unified-20261011, tools/unified-20261011,
upstream/NVIDIA-nvapi-87dca62, the unified2l source manifest, the unified2m usage
document, dist/unified-20261107m-candidate.zip, the author's Chinese guide, and
the earlier READMEs already published at commit 5ee4367 of the local clone
worktrees/dlss5-vulkan-fg-relay.

unified2m has no source manifest of its own: its Generic was built from the
unified2l tree with the files in CHANGED_SINCE_2L replaced. verify() therefore
checks every other file against the unified2l manifest and these files against
the hashes recorded when the shipped Generic (build unified2m-fix2-20261107) was
built.
"""
import hashlib, json, re, shutil, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'dist' / 'release-unified-20261107m'
REPO = OUT / 'repo'
ASSETS = OUT / 'assets'
TAG = 'unified-20261107m'
CANDIDATE_ZIP = ROOT / 'dist' / 'unified-20261107m-candidate.zip'
CANDIDATE_SHA256 = '947eb1a5cce0a5158359f12b75e8dd614c453e99333a176720ffbd3492036248'
SOURCE_MANIFEST = ROOT / 'builds' / 'unified-20261011' / 'unified2l' / 'source-manifest.json'
GENERIC = 'src/unified-20261011/generic/src/addons/dlss5/'
CHANGED_SINCE_2L = {
    GENERIC + 'declines.hpp': 'd0dc63a0ed8a87ffdb875fb9f157da69fcaf90ba2256ad75bf9c831c34c45845',
    GENERIC + 'dlssnr.hpp': '9500dda939d8c63e420c7da4871b8ce4c3480a37f27d2484e9b0a749e18c1920',
    GENERIC + 'runtime_lock.hpp': '5f04f77f60b37c667149ff26e081f32e2765884cc1dd1cf5c0c171ffdff97845',
    GENERIC + 'screenshot.hpp': '85e961c57600d7193ded78bcc6d64045c52458230811a3588d97df21770473fa',
    GENERIC + 'vulkan_path.hpp': 'b2aa3792882899fc01e46ef06c59b368df04dcbea6cba8afff003fda0ab594c3',
}
PACKAGE_SCRIPT = 'tools/unified-20261011/package-unified2m.py'
PACKAGE_SCRIPT_SHA256 = '37032ba5bcd2f5e414c12d1fabf9997b56d024bd5fb9fbcf5c539b579aff5f20'
USAGE = ROOT / 'docs' / '统一候选使用与恢复-20261107-2m.md'
USAGE_SHA256 = 'c950691907e3b1ecd4dfa7bd9b4df1056949307acd85059fc52de9ddd26808b6'
PUBLISHED_GIT = ROOT / 'worktrees' / 'dlss5-vulkan-fg-relay'
PUBLISHED_COMMIT = '5ee4367'
# Taken byte for byte from the published commit: the READMEs of the two releases
# before unified-20261103l.
PUBLISHED_FILES = (
    'docs/history/README-unified-20261015q.md',
    'docs/history/README-vk-fgrelay-20261005.md',
)
# The author's own Chinese guide, published byte for byte (not translated).
ZH_GUIDE = Path(r'C:\Users\Administrator\Desktop\暂存文件\DLSS5相关\内测版\第二版\使用教程.txt')
ZH_GUIDE_SHA256 = '0fca44aa8e4cc0d6acc8972696c6182ecfbe6efb7382d3a8cde65fc46fa3e48d'
ZH_GUIDE_NAME = '中文使用教程.txt'

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
    listed = {f['path'] for f in manifest['files']}
    unknown = sorted(set(CHANGED_SINCE_2L) - listed)
    if unknown:
        raise SystemExit(f'changed files missing from the unified2l manifest: {unknown}')
    expected = {f['path']: CHANGED_SINCE_2L.get(f['path'], f['sha256']) for f in manifest['files']}
    wrong = [p for p, digest in expected.items() if not (ROOT / p).is_file() or sha256(ROOT / p) != digest]
    if wrong:
        raise SystemExit(f'source differs from the unified2m build tree: {wrong[:10]} ({len(wrong)})')
    same = [p for p in CHANGED_SINCE_2L if next(f['sha256'] for f in manifest['files'] if f['path'] == p) == CHANGED_SINCE_2L[p]]
    if same:
        raise SystemExit(f'listed as changed but identical to unified2l: {same}')
    shipped = {rel for _, rel in source_files()}
    missing = [p for p in listed if p not in shipped and not SOURCE_EXCLUDE.search(p)]
    if missing:
        raise SystemExit(f'manifest files not staged: {missing[:10]}')
    if PACKAGE_SCRIPT not in shipped or sha256(ROOT / PACKAGE_SCRIPT) != PACKAGE_SCRIPT_SHA256:
        raise SystemExit('package-unified2m.py is missing or changed')
    if sha256(CANDIDATE_ZIP) != CANDIDATE_SHA256:
        raise SystemExit('candidate zip hash changed')
    if sha256(USAGE) != USAGE_SHA256:
        raise SystemExit('usage document differs from the packaged copy')
    with zipfile.ZipFile(CANDIDATE_ZIP) as z:
        if hashlib.sha256(z.read('DLSS5-使用与恢复 Usage and Recovery.md')).hexdigest() != USAGE_SHA256:
            raise SystemExit('usage document in the candidate zip differs')
    if sha256(ZH_GUIDE) != ZH_GUIDE_SHA256:
        raise SystemExit('the Chinese guide changed; review it and update ZH_GUIDE_SHA256')
    print(f'verify: {len(expected)} manifest files match ({len(CHANGED_SINCE_2L)} changed since unified2l), '
          'candidate zip and usage document match')


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
    for rel in PUBLISHED_FILES:
        t = REPO / rel
        t.parent.mkdir(parents=True, exist_ok=True)
        t.write_bytes(git_show(rel)); count += 1
    shutil.copy2(ZH_GUIDE, REPO / ZH_GUIDE_NAME); count += 1
    shutil.copy2(USAGE, REPO / 'docs' / 'USAGE-AND-RECOVERY.md'); count += 1
    tools_out = REPO / 'tools' / f'release-{TAG}'
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
