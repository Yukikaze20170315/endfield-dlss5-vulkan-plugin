"""Create pq-wait-fix2 from the pq-wait-fix1 source snapshot.

fix1 changed only PqPassWait. Two more waits in the same recovery chain,
Synth12WaitFence (shared event of the private D3D12 session) and FgCompositeWait,
still return false on the first wake. fix2 routes both through the same
fence-is-the-answer wait (tools/unified-20261011/fence-deadline-wait.h) within the
caller's unchanged budget, and logs an unchecked identity-fallback failure. No
budget, guard threshold, refusal policy or release proof is changed.
"""
from pathlib import Path
import difflib
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[2]
base = root / 'builds/unified-20261011/pq-wait-fix1'
out = root / 'builds/unified-20261011/pq-wait-fix2'
for path in (base, out, *base.parents, *out.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
out.mkdir(exist_ok=False)
shutil.copytree(base / 'source', out / 'source')
src = out / 'source/bridge/src'
shutil.copyfile(Path(__file__).with_name('fence-deadline-wait.h'), src / 'fence-deadline-wait.h')


def replace_once(path, old, new):
    text = path.read_text(encoding='utf-8-sig')
    if text.count(old) != 1:
        raise RuntimeError(f'{path.name}: expected exactly one match, found {text.count(old)}')
    path.write_text(text.replace(old, new), encoding='utf-8')


replace_once(src / 'synth.inc',
'''static bool Synth12WaitFence(UINT64 value, DWORD ms)
{
    if (g_s12.fence == nullptr || g_s12.event == nullptr) return false;
    const UINT64 completed = g_s12.fence->GetCompletedValue();
    if (completed == UINT64_MAX) return false;
    if (completed >= value) return true;
    if (FAILED(g_s12.fence->SetEventOnCompletion(value, g_s12.event))) return false;
    if (WaitForSingleObject(g_s12.event, ms) != WAIT_OBJECT_0) return false;
    const UINT64 after = g_s12.fence->GetCompletedValue();
    return after != UINT64_MAX && after >= value;
}''',
'''#include "fence-deadline-wait.h"

static bool Synth12WaitFence(UINT64 value, DWORD ms)
{
    if (g_s12.fence == nullptr || g_s12.event == nullptr) return false;
    return FenceDeadlineWait(g_s12.fence, g_s12.event, value, ms, "synth12");
}''')

replace_once(src / 'fg-input.inc',
'''    if (c.fence == nullptr) return true;
    const UINT64 done = c.fence->GetCompletedValue();
    if (done == UINT64_MAX) return false;
    if (done >= c.fv) return true;
    if (FAILED(c.fence->SetEventOnCompletion(c.fv, c.ev))) return false;
    if (WaitForSingleObject(c.ev, ms) != WAIT_OBJECT_0) return false;
    const UINT64 after = c.fence->GetCompletedValue();
    return after != UINT64_MAX && after >= c.fv;
}''',
'''    if (c.fence == nullptr) return true;
    return FenceDeadlineWait(c.fence, c.ev, c.fv, ms, "fg-composite");
}''')

replace_once(src / 'fg-input.inc',
'''        // Keep the carriers equal to their inputs so the recorded copy back is a no-op.
        FgIdentityCopy(kVkmEvaluateMs);''',
'''        // Keep the carriers equal to their inputs so the recorded copy back is a no-op.
        if (!FgIdentityCopy(kVkmEvaluateMs))
            Log("[fg-input] identity fallback copy not confirmed within %lu ms.", kVkmEvaluateMs);''')

replace_once(src / 'fg-relay.inc',
'''        // recorded in the game's command buffer is a no-op.
        FgIdentityCopy(kVkmEvaluateMs);''',
'''        // recorded in the game's command buffer is a no-op.
        if (!FgIdentityCopy(kVkmEvaluateMs))
            Log("[fg-relay] identity fallback copy not confirmed within %lu ms.", kVkmEvaluateMs);''')

for name in ['dlss5-bridge.cpp', 'version.rc']:
    p = src / name
    old = p.read_text(encoding='utf-8-sig')
    new = old.replace('1.4.13-pre8-pq-wait-fix1-20261011', '1.4.13-pre8-pq-wait-fix2-20261011')
    if name == 'version.rc':
        new = new.replace('1,4,13,43', '1,4,13,44')
    if new == old:
        raise RuntimeError(f'{name}: version not updated')
    p.write_text(new, encoding='utf-8')

manifest = []
diff = []
for p in sorted((out / 'source').rglob('*')):
    if not p.is_file():
        continue
    relative = p.relative_to(out / 'source')
    before_path = base / 'source' / relative
    before = before_path.read_bytes() if before_path.exists() else b''
    after = p.read_bytes()
    manifest.append({'path': relative.as_posix(), 'base_sha256': hashlib.sha256(before).hexdigest(),
                     'sha256': hashlib.sha256(after).hexdigest(), 'changed': before != after})
    if before != after:
        diff.extend(difflib.unified_diff(before.decode('utf-8-sig').splitlines(),
                                         after.decode('utf-8-sig').splitlines(),
                                         fromfile='fix1/' + relative.as_posix(),
                                         tofile='fix2/' + relative.as_posix(), lineterm=''))
(out / 'source.diff').write_text('\n'.join(diff) + '\n', encoding='utf-8')
(out / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(out)
