"""Summarize the saved PID78372 rollback fault without modifying game files."""
from pathlib import Path
import collections
import hashlib
import json
import re

root = Path(__file__).resolve().parents[2]
base = root / 'evidence/unified-20261011'
out = base / 'root-cause-20261011/nr-rollback-audit.json'
if out.exists():
    raise RuntimeError('Preserve the previous audit')
snapshot = base / 'run-20261011-205945-653657'
bridge = (snapshot / 'dlss5-bridge.log').read_text(encoding='utf-8-sig').splitlines()
generic = (snapshot / 'ReShade.log').read_text(encoding='utf-8-sig').splitlines()
windows = []
for begin, end in [('20:48:39', '20:49:00'), ('20:49:50', '20:50:50'), ('20:51:00', '20:53:00')]:
    lines = [line for line in bridge if begin <= line[:8] < end]
    failed = [line for line in lines if '[fg-relay] frame=' in line and 'ok=0' in line]
    frames = [tuple(map(int, re.search(r'frame=(\d+) slot=(\d+)', line).groups())) for line in failed]
    windows.append({'begin': begin, 'end_exclusive': end,
                    'encode_failures': sum('output encoding failed' in line for line in lines),
                    'decode_failures': sum('the HDR10 decode did not run' in line for line in lines),
                    'failed_frames': len(frames),
                    'failure_frame_gaps': dict(collections.Counter(b[0] - a[0] for a, b in zip(frames, frames[1:]))),
                    'failure_slots': dict(collections.Counter(frame[1] for frame in frames)),
                    'first_last_failure': failed[:1] + failed[-1:]})

def evidence_lines(lines, needles):
    return [{'line': i, 'text': line} for i, line in enumerate(lines, 1) if any(n in line for n in needles)]

wait_sources = []
for relative in ['src/bridge-release/bridge.inc', 'src/bridge-vulkan-present/bridge.inc',
                 'src/bridge-mfg-coexist-codex-20261011/src/bridge.inc',
                 'src/unified-20261011/bridge/src/bridge.inc']:
    path = root / relative
    text = path.read_text(encoding='utf-8-sig')
    start = text.index('static bool PqPassWait(')
    end = text.index('\n}', start) + 2
    function = text[start:end]
    wait_sources.append({'path': relative, 'line': text[:start].count('\n') + 1,
                         'function_sha256': hashlib.sha256(function.encode()).hexdigest(),
                         'function': function})

result = {
    'process': {'pid': 78372, 'started_local': '2026-10-11T20:46:54+08:00'},
    'snapshot': str(snapshot),
    'snapshot_file_hashes': {name: hashlib.sha256((snapshot / name).read_bytes()).hexdigest()
                             for name in ['dlss5-bridge.log', 'ReShade.log', 'ReShade.ini', 'dlss5-bridge.cfg']},
    'scope': 'Observed encode/decode failure and reset chain. Does not prove which PqPassRun subcall failed in this session or that VRAM allocation failed.',
    'user_peak_layers': '7-8 total; exact full combination not confirmed',
    'window_counts': windows,
    'telemetry': evidence_lines(generic, ['telemetry t=']),
    'lifecycle': evidence_lines(generic, ['created inline NR resources', 'NR workset ws', 'NR quiesce',
                                          'NR-WARN enter stabilizer_restarting', 'NR toggled']),
    'relay_rates': evidence_lines(bridge, ['[fg-relay] rate2:', '[fg-relay] rate4:']),
    'api_allocation_error_lines': evidence_lines(bridge + generic, ['E_OUTOFMEMORY', '0x8007000E', 'VK_ERROR_OUT_OF_DEVICE_MEMORY']),
    'wait_source_ancestry': wait_sources,
    'older_patch': 'evidence/machines-20261010/raw/A机/AgentUse/relay-git/merge/patches/0002-Vulkan-serve-the-neural-add-on-s-Present-hook-point-.patch',
    'related_prior_throttle_fix': 'evidence/config-diagnosis-20261011/throttle-history-audit.json: fgrelay12, 2026-09-29',
    'isolated_tests': ['builds/unified-20261011/pq-stale-event-build.log',
                       'builds/unified-20261011/pq-cause-audit1-final-build.log'],
    'prior_memory_observations_transcribed_from_tool_output': [
        {'local_time': '2026-10-11 20:50:58', 'gpu_used_mib': 23566, 'gpu_total_mib': 32607,
         'process_dedicated_bytes': 19216683008, 'process_shared_bytes': 258199552, 'process_total_committed_bytes': 18809032704},
        {'local_time': '2026-10-11 20:53:04', 'gpu_used_mib': 24707, 'gpu_total_mib': 32607,
         'process_dedicated_bytes': 19827503104, 'process_shared_bytes': 260296704, 'process_total_committed_bytes': 19421949952}
    ],
}
out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(out)
print(json.dumps(windows, ensure_ascii=False, indent=2))
