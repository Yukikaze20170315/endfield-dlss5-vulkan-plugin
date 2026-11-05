"""Reject translations damaged by a non-UTF-8 round trip.

Lang files store UTF-8 as \\xNN escapes. A tool that rewrote them through a
legacy code page turns every non-ASCII character into '?', e.g. "?ber" or
"??????". A '?' followed by a letter, digit or another '?' is never written
on purpose in these strings, so any such value fails the check.
"""
import pathlib, re, sys

LANG = pathlib.Path(__file__).resolve().parents[2] / 'src/unified-20261011/generic/src/addons/dlss5/ui/lang'
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
SUSPECT = re.compile(r'\?(?=[\w?])')


def decode(raw):
    out = bytearray(); i = 0
    while i < len(raw):
        if raw[i] == '\\':
            if raw[i + 1] == 'x':
                out += bytes([int(raw[i + 2:i + 4], 16)]); i += 4; continue
            out += {'n': b'\n', 't': b'\t'}.get(raw[i + 1], raw[i + 1].encode()); i += 2; continue
        out += raw[i].encode(); i += 1
    return out.decode('utf-8')


bad = 0; checked = 0
files = sorted(LANG.glob('*.hpp'))
for path in files:
    for number, line in enumerate(path.read_text('utf-8').splitlines(), 1):
        if line.lstrip().startswith('//') or '"' not in line:
            continue
        try:
            text = ''.join(decode(part) for part in LITERAL.findall(line))
        except (UnicodeDecodeError, ValueError) as error:
            print(f'{path.name}:{number}: undecodable literal ({error})'); bad += 1; continue
        checked += 1
        if SUSPECT.search(text) or '\ufffd' in text:
            print(f'{path.name}:{number}: suspected encoding damage: {text!r}'); bad += 1
if not files:
    print('no lang files found'); sys.exit(1)
print(f'lang strings: {len(files)} files, {checked} lines checked, {bad} bad')
sys.exit(1 if bad else 0)
