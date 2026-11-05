"""Bounded read-only evidence capture around one NR layer transition."""
from pathlib import Path
from datetime import datetime
import hashlib
import json
import re
import subprocess
import sys
import time

sys.stdout.reconfigure(encoding="utf-8")
ROOT = Path(__file__).resolve().parents[2]
GAME = Path("C:/Program Files/Hypergryph Launcher/games/Endfield Game")
OUT = ROOT / "evidence/unified-20261011" / ("nr-transition-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
for path in (GAME, OUT):
    for item in (path, *path.parents):
        if item.exists() and (item.is_symlink() or item.is_junction()):
            raise RuntimeError(f"Linked path refused: {item}")
OUT.mkdir()
start = time.monotonic()
duration = int(sys.argv[1]) if len(sys.argv) > 1 else 180
if not 10 <= duration <= 300:
    raise ValueError("duration must be 10..300 seconds")
files = ("ReShade.log", "dlss5-bridge.log", "dlss5-observe.log", "dlss5-sl.log",
         "ReShade.ini", "dlss5-bridge.cfg", "dlss5/state/generic-layers.state")
events = []


def snapshot(label):
    folder = OUT / (datetime.now().strftime("%H%M%S-%f") + "-" + label)
    folder.mkdir()
    manifest = []
    for name in files:
        source = GAME / name
        if not source.is_file():
            continue
        # A snapshot is not atomic across files. A single opened file is bounded
        # to its initial size, so subsequent appends cannot extend the capture.
        with source.open("rb") as stream:
            size = source.stat().st_size
            if size > 128 * 1024 * 1024:
                raise RuntimeError(f"Log bound exceeded: {name}")
            data = stream.read(size)
        destination = folder / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        manifest.append({"path": name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    (folder / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"snapshot {folder}", flush=True)


print(f"READY {OUT}", flush=True)
snapshot("baseline")
previous = None
next_memory = 0
with (OUT / "observations.jsonl").open("w", encoding="utf-8") as record:
    while time.monotonic() - start < duration:
        elapsed = time.monotonic() - start
        text = (GAME / "ReShade.ini").read_text(encoding="utf-8-sig", errors="replace")
        settings = dict(re.findall(r"^(NRRenderPasses|NRUpscaledPasses|NRPresentPasses|NRResolutionScale|ForceMultiplier)=(.*)$", text, re.M))
        layer_state = GAME / "dlss5/state/generic-layers.state"
        key = (tuple(sorted(settings.items())), layer_state.read_bytes() if layer_state.exists() else b"")
        if key != previous:
            item = {"at": datetime.now().astimezone().isoformat(), "elapsed": elapsed,
                    "settings": settings, "state_hex": key[1].hex()}
            record.write(json.dumps(item) + "\n")
            record.flush()
            if previous is not None:
                snapshot("settings-change")
            previous = key
        if elapsed >= next_memory:
            result = subprocess.run(["nvidia-smi", "--query-gpu=timestamp,memory.used,memory.free",
                                     "--format=csv,noheader,nounits"], capture_output=True, text=True, timeout=5)
            record.write(json.dumps({"at": datetime.now().astimezone().isoformat(), "elapsed": elapsed,
                                     "gpu_memory": result.stdout.strip(), "returncode": result.returncode}) + "\n")
            record.flush()
            next_memory = elapsed + 3
        time.sleep(0.5)
snapshot("final")
print("DONE " + str(OUT), flush=True)
