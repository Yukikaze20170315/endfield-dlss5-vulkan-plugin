"""Secret and personal-data scan over the staged repository and the source zip.

    python tools/release-unified-20261103l/secret-scan.py

Writes dist/release-unified-20261103l/secret-scan-results.txt. Binary files are
skipped. Every hit has to be reviewed by hand; a hit is not automatically a leak.
"""
import re, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'dist' / 'release-unified-20261103l'
REPO = OUT / 'repo'
ASSETS = OUT / 'assets'

TOKEN = re.compile(rb'ghp_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|gho_[A-Za-z0-9]{20,}'
                   rb'|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z0-9 ]*PRIVATE KEY-----|xox[bp]-[A-Za-z0-9-]{8,}')
KEYWORD = re.compile(r'(?i)password|passwd|pwd=|secret|api[_-]?key|\btoken\b')
EMAIL = re.compile(r'(?i)[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}')
PRIVATE = re.compile(r'(?i)10\.126\.126\.|\blenovo\b|LAPTOP-[A-Z0-9]+|\\Users\\(?!Administrator\\)[^\\\s"]+\\|sshd|easytier')

rows = []


def scan(relname, data):
    if b'\0' in data[:8000]:
        return
    for match in TOKEN.finditer(data):
        rows.append(('TOKEN', relname, 0, match.group(0)[:60].decode('ascii', 'replace')))
    text = data.decode('utf-8', 'replace')
    for i, line in enumerate(text.splitlines(), 1):
        for kind, rx in (('KEYWORD', KEYWORD), ('PRIVATE', PRIVATE)):
            if rx.search(line):
                rows.append((kind, relname, i, line.strip()[:160]))
        for m in EMAIL.finditer(line):
            rows.append(('EMAIL', relname, i, m.group(0)))


for p in sorted(REPO.rglob('*')):
    if p.is_file():
        scan('repo/' + p.relative_to(REPO).as_posix(), p.read_bytes())
for z in sorted(ASSETS.glob('*-source.zip')):
    with zipfile.ZipFile(z) as archive:
        for name in archive.namelist():
            scan(f'{z.name}:{name}', archive.read(name))

with (OUT / 'secret-scan-results.txt').open('w', encoding='utf-8') as f:
    f.write(f'total hits: {len(rows)}\n')
    for kind, rel, line, ctx in rows:
        f.write(f'{kind}\t{rel}\t{line}\t{ctx}\n')
counts = {}
for kind, *_ in rows:
    counts[kind] = counts.get(kind, 0) + 1
print('hits', counts)
for kind, rel, line, ctx in rows:
    if kind in ('TOKEN', 'PRIVATE', 'EMAIL'):
        print(f'{kind}\t{rel}\t{line}\t{ctx}')
