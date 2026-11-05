"""Read-only 50-second observation of the live layer record; keep only changes."""
from pathlib import Path
from datetime import datetime
import json
import struct
import time

ROOT = Path(__file__).resolve().parents[2]
GAME = Path('C:/Program Files/Hypergryph Launcher/games/Endfield Game')
OUT = ROOT / 'evidence/unified-20261011' / ('layers-' + datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
OUT.mkdir(parents=True, exist_ok=False)
deadline = time.monotonic() + 50
previous = None
with (OUT / 'changes.jsonl').open('w', encoding='utf-8') as output:
    print(OUT, flush=True)
    while time.monotonic() < deadline:
        try:
            data = (GAME / 'dlss5/state/generic-layers.state').read_bytes()
            if data != previous:
                record = dict(zip(['magic', 'version', 'checksum', 'generation', 'confirmed', 'requested', 'pending', 'reserved'], struct.unpack('<IIQQ4I', data)))
                for key in ['confirmed', 'requested']:
                    plan = record[key]
                    record[key + '_layers'] = [plan & 7, (plan >> 3) & 7, (plan >> 6) & 7]
                record['observed_at'] = datetime.now().isoformat(timespec='milliseconds')
                text = json.dumps(record, ensure_ascii=False)
                output.write(text + '\n')
                output.flush()
                print(text, flush=True)
                (OUT / ('state-' + str(record['generation']) + '.bin')).write_bytes(data)
                previous = data
        except (OSError, struct.error):
            pass
        time.sleep(0.25)
