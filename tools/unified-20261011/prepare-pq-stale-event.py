"""Extract the unmodified production wait function for an isolated real-fence audit."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[2]
source = root / 'src/unified-20261011/bridge/src/bridge.inc'
text = source.read_text(encoding='utf-8-sig')
start = text.index('static bool PqPassWait(')
end = text.index('\nstatic bool PqPassRelease(', start)
function = text[start:end].rstrip() + '\n'
out = root / 'builds/unified-20261011/pq-stale-event-audit'
out.mkdir(exist_ok=False)
(out / 'production-pq-wait.inc').write_text(function, encoding='utf-8')
(out / 'provenance.json').write_text(json.dumps({
    'source': str(source),
    'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'function_sha256': hashlib.sha256(function.encode()).hexdigest(),
    'line': text[:start].count('\n') + 1,
    'scope': 'Unchanged production function, real WARP D3D12 fence and real auto-reset Win32 event; no game access.'
}, indent=2) + '\n', encoding='utf-8')
print(out)
