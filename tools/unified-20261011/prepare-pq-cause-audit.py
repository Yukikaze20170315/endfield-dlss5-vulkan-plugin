"""Prepare a logging-only bridge snapshot; preserve production sources and packages."""
from pathlib import Path
import difflib
import hashlib
import json
import shutil

root = Path(__file__).resolve().parents[2]
source = root / 'src/unified-20261011'
out = root / 'builds/unified-20261011/pq-cause-audit1'
for path in (source, out, *source.parents, *out.parents):
    if path.exists() and (path.is_symlink() or path.is_junction()):
        raise RuntimeError(f'Linked path refused: {path}')
out.mkdir(exist_ok=False)
snapshot = out / 'source'
shutil.copytree(source / 'bridge/src', snapshot / 'bridge/src')
shutil.copytree(source / 'common', snapshot / 'common')
path = snapshot / 'bridge/src/bridge.inc'
original = path.read_text(encoding='utf-8-sig')
text = original


def replace(before, after):
    global text
    if text.count(before) != 1:
        raise RuntimeError(f'Expected exactly one anchor: {before[:100]}')
    text = text.replace(before, after)


start = text.index('static bool PqPassWait(')
end = text.index('\nstatic bool PqPassRelease(', start)
replacement = r'''static bool PqPassWait(PqPass *p, UINT64 value, DWORD wait_ms = 4000)
{
    if (p->fence == nullptr) return true;
    const ULONGLONG begin = GetTickCount64();
    const UINT64 completed = p->fence->GetCompletedValue();
    // Only failed calls are logged; completion tests and wait count are unchanged.
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
    const DWORD wr = WaitForSingleObject(p->ev, wait_ms);
    if (wr != WAIT_OBJECT_0) return fail("wait-result", S_OK, wr, completed);
    const UINT64 after = p->fence->GetCompletedValue();
    if (after == UINT64_MAX || after < value) return fail("wake-before-target", S_OK, wr, after);
    return true;
}
'''
text = text[:start] + replacement + text[end:]
(out / 'production-pq-wait.inc').write_text(replacement, encoding='utf-8')

replace('    if (!PqPassWait(p, p->done[mode], wait_ms)) return false;\n'
        '    if (FAILED(p->alloc[mode]->Reset()) || FAILED(list->Reset(p->alloc[mode], nullptr))) return false;',
r'''    const auto fail = [&](const char *stage, HRESULT hr) {
        const DWORD saved_error = GetLastError();
        Log("[pq-cause] run stage=%s tid=%lu pass=%p mode=%d target=%llu fv=%llu hr=0x%08lX.",
            stage, GetCurrentThreadId(), p, mode, p->done[mode], p->fv, static_cast<unsigned long>(hr));
        SetLastError(saved_error);
        return false;
    };
    if (!PqPassWait(p, p->done[mode], wait_ms)) return fail("prior-completion", S_OK);
    const HRESULT alloc_hr = p->alloc[mode]->Reset();
    if (FAILED(alloc_hr)) return fail("allocator-reset", alloc_hr);
    const HRESULT list_hr = list->Reset(p->alloc[mode], nullptr);
    if (FAILED(list_hr)) return fail("list-reset", list_hr);''')
replace('    if (FAILED(list->Close())) return false;\n'
        '    ID3D12CommandList *lists[] = { list };\n'
        '    queue->ExecuteCommandLists(1, lists);\n'
        '    const UINT64 value = ++p->fv;\n'
        '    if (FAILED(queue->Signal(p->fence, value))) return false;',
r'''    const HRESULT close_hr = list->Close();
    if (FAILED(close_hr)) return fail("list-close", close_hr);
    ID3D12CommandList *lists[] = { list };
    queue->ExecuteCommandLists(1, lists);
    const UINT64 value = ++p->fv;
    const HRESULT signal_hr = queue->Signal(p->fence, value);
    if (FAILED(signal_hr)) return fail("pq-signal", signal_hr);''')
replace('    if (FAILED(queue->Signal(session_fence, v))) return fallback;',
r'''    const HRESULT signal_hr = queue->Signal(session_fence, v);
    if (FAILED(signal_hr)) {
        const DWORD saved_error = GetLastError();
        Log("[pq-cause] after stage=session-signal tid=%lu pass=%p target=%llu fallback=%llu hr=0x%08lX.",
            GetCurrentThreadId(), p, v, fallback, static_cast<unsigned long>(signal_hr));
        SetLastError(saved_error);
        return fallback;
    }''')
path.write_text(text, encoding='utf-8')

for name in ['dlss5-bridge.cpp', 'version.rc']:
    version_path = snapshot / 'bridge/src' / name
    old = version_path.read_text(encoding='utf-8-sig')
    updated = old.replace('1.4.13-pre8-unified1k-20261011', '1.4.13-pre8-pq-cause-audit1-20261011')
    if name == 'version.rc':
        updated = updated.replace('1,4,13,40', '1,4,13,42')
    if updated == old:
        raise RuntimeError(f'Missing version anchor: {name}')
    version_path.write_text(updated, encoding='utf-8')

inventory = []
diffs = []
for copied in sorted(snapshot.rglob('*')):
    if not copied.is_file():
        continue
    relative = copied.relative_to(snapshot)
    before = (source / relative).read_bytes()
    after = copied.read_bytes()
    inventory.append({'path': relative.as_posix(), 'source_sha256': hashlib.sha256(before).hexdigest(),
                      'snapshot_sha256': hashlib.sha256(after).hexdigest(), 'changed': before != after})
    if before != after:
        diffs.extend(difflib.unified_diff(before.decode('utf-8-sig').splitlines(True),
                                         after.decode('utf-8-sig').splitlines(True),
                                         fromfile='source/' + relative.as_posix(), tofile='audit/' + relative.as_posix()))
(out / 'source.diff').write_text(''.join(diffs), encoding='utf-8')
(out / 'source-manifest.json').write_text(json.dumps(inventory, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(out)
print('Changed snapshot files:', [i['path'] for i in inventory if i['changed']])
