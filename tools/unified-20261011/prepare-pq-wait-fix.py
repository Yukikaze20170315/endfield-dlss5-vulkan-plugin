"""Create a bounded PQ wait candidate from the diagnosed logging snapshot."""
from pathlib import Path
import difflib
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[2]
base = root / 'builds/unified-20261011/pq-cause-audit1'
out = root / 'builds/unified-20261011/pq-wait-fix1'
for path in (base, out, *base.parents, *out.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
out.mkdir(exist_ok=False)
shutil.copytree(base / 'source', out / 'source')
path = out / 'source/bridge/src/bridge.inc'
original = path.read_text(encoding='utf-8-sig')
start = original.index('static bool PqPassWait(')
end = original.index('\nstatic bool PqPassRelease(', start)
function = r'''static bool PqPassWait(PqPass *p, UINT64 value, DWORD wait_ms = 4000)
{
    if (p->fence == nullptr) return true;
    const ULONGLONG begin = GetTickCount64();
    const ULONGLONG deadline = begin + wait_ms;
    const UINT64 completed = p->fence->GetCompletedValue();
    const auto fail = [&](const char *stage, HRESULT hr, DWORD wr, UINT64 after) {
        const DWORD saved_error = GetLastError();
        Log("[pq-cause] wait stage=%s tid=%lu pass=%p event=%p target=%llu before=%llu after=%llu "
            "budget_ms=%lu elapsed_ms=%llu hr=0x%08lX wait=0x%08lX last_error=%lu.",
            stage, GetCurrentThreadId(), p, p->ev, value, completed, after,
            wait_ms, GetTickCount64() - begin, static_cast<unsigned long>(hr), wr, saved_error);
        SetLastError(saved_error);
        return false;
    };
    if (completed == UINT64_MAX) return fail("removed-before", S_OK, WAIT_FAILED, completed);
    if (completed >= value) return true;
    const HRESULT event_hr = p->fence->SetEventOnCompletion(value, p->ev);
    if (FAILED(event_hr)) return fail("set-event", event_hr, WAIT_FAILED, completed);
    // A timed-out registration may signal this auto-reset event later. Register
    // the current target once and treat notifications only as wakes; completion
    // is established by the fence within one unchanged total deadline.
    unsigned stale_wakes = 0;
    DWORD wr = WAIT_OBJECT_0;
    for (;;) {
        const UINT64 after = p->fence->GetCompletedValue();
        if (after == UINT64_MAX) return fail("removed-after", S_OK, wr, after);
        if (after >= value) {
            if (stale_wakes != 0) {
                const DWORD saved_error = GetLastError();
                Log("[pq-cause] recovered tid=%lu pass=%p target=%llu after=%llu stale_wakes=%u "
                    "budget_ms=%lu elapsed_ms=%llu.", GetCurrentThreadId(), p, value, after,
                    stale_wakes, wait_ms, GetTickCount64() - begin);
                SetLastError(saved_error);
            }
            return true;
        }
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline) return fail("deadline", S_OK, WAIT_TIMEOUT, after);
        wr = WaitForSingleObject(p->ev, static_cast<DWORD>(deadline - now));
        if (wr != WAIT_OBJECT_0 && wr != WAIT_TIMEOUT) return fail("wait-result", S_OK, wr, after);
        if (wr == WAIT_OBJECT_0) {
            const UINT64 awake = p->fence->GetCompletedValue();
            if (awake != UINT64_MAX && awake < value) ++stale_wakes;
        }
    }
}
'''
path.write_text(original[:start] + function + original[end:], encoding='utf-8')
(out / 'production-pq-wait.inc').write_text(function, encoding='utf-8')
for name in ['dlss5-bridge.cpp', 'version.rc']:
    p = out / 'source/bridge/src' / name
    old = p.read_text(encoding='utf-8-sig')
    new = old.replace('1.4.13-pre8-pq-cause-audit1-20261011', '1.4.13-pre8-pq-wait-fix1-20261011')
    if name == 'version.rc':
        new = new.replace('1,4,13,42', '1,4,13,43')
    assert new != old
    p.write_text(new, encoding='utf-8')
manifest = []
diff = []
for p in sorted((out / 'source').rglob('*')):
    if not p.is_file(): continue
    relative = p.relative_to(out / 'source')
    before = (base / 'source' / relative).read_bytes()
    after = p.read_bytes()
    manifest.append({'path': relative.as_posix(), 'base_sha256': hashlib.sha256(before).hexdigest(),
                     'sha256': hashlib.sha256(after).hexdigest(), 'changed': before != after})
    if before != after:
        diff.extend(difflib.unified_diff(before.decode('utf-8-sig').splitlines(), after.decode('utf-8-sig').splitlines(),
                                         fromfile='diagnostic/' + relative.as_posix(), tofile='candidate/' + relative.as_posix(), lineterm=''))
(out / 'source.diff').write_text('\n'.join(diff) + '\n', encoding='utf-8')
(out / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
print(out)
