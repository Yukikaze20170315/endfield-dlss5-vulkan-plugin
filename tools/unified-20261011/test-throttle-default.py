"""Exercise the compiled bridge INI reader in separate startup processes."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'builds/unified-20261011/throttle-default1k'
CASES = [
    ('no-ini', None, 2),
    ('empty-ini', '', 2),
    ('missing-key', '[Adapter]\nPipeline=2\nImport=1\n', 2),
    ('unrelated-section', '[Other]\nThrottle=0\n', 2),
    *[(f'explicit-{n}', f'[Adapter]\nThrottle={n}\n', n) for n in range(4)],
    ('clamp-low', '[Adapter]\nThrottle=-1\n', 0),
    ('clamp-high', '[Adapter]\nThrottle=9\n', 3),
    ('import-off', '[Adapter]\nImport=0\n', 0),
    ('serial-path', '[Adapter]\nPipeline=0\n', 0),
    ('disabled', '[Adapter]\nEnabled=0\n', 0),
]
results = []
for name, content, expected in CASES:
    directory = BUILD / 'config-cases' / name
    directory.mkdir(parents=True, exist_ok=False)
    executable = directory / 'test-loader.exe'
    shutil.copy2(BUILD / 'test-loader.exe', executable)
    config = directory / 'vk-present-adapter.ini'
    if content is not None:
        config.write_bytes(content.replace('\n', '\r\n').encode('utf-8'))
    before = config.read_bytes() if config.exists() else None
    completed = subprocess.run([str(executable), '--throttle-config', str(expected)],
                               cwd=directory, capture_output=True, text=True, check=True)
    assert (config.read_bytes() if config.exists() else None) == before, name
    results.append({'case': name, 'expected': expected, 'exit_code': completed.returncode,
                    'config_unchanged': True, 'output': completed.stdout.strip()})
report = BUILD / 'throttle-config-tests.json'
report.write_text(json.dumps({'executable_sha256': hashlib.sha256(
    (BUILD / 'test-loader.exe').read_bytes()).hexdigest(), 'results': results}, indent=2)+'\n', encoding='utf-8')
print(f'PASS: {len(results)} real INI startup cases; configurations unchanged.')
