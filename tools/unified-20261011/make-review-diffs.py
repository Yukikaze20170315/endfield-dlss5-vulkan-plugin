"""Generate unified diffs for review at evidence/opus-20261023/review/.

dlssnr.hpp and vulkan_path.hpp diff against the pristine unified1s reference
copies under tools/opus-20261023/ref-1s/.  declines.hpp and the bridge files
have no pristine snapshot in the tree, so their baseline is reconstructed by
reverting this task's own edits (each replacement verified to occur exactly
once); every such diff header is marked RECONSTRUCTED-BASELINE.
"""
import difflib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
DLSS5 = ROOT / 'src/unified-20261011/generic/src/addons/dlss5'
BRIDGE = ROOT / 'src/unified-20261011/bridge/src'
OUT = ROOT / (sys.argv[1] if len(sys.argv) > 1
              else 'evidence/opus-20261023/review')
OUT.mkdir(parents=True, exist_ok=True)


def diff(name, old_text, new_text, old_label, new_label):
    lines = list(difflib.unified_diff(
        old_text.splitlines(keepends=True),
        new_text.splitlines(keepends=True),
        fromfile=old_label, tofile=new_label, n=3))
    (OUT / name).write_text(''.join(lines), encoding='utf-8')
    print(f'{name}: {len(lines)} diff lines')


def revert_pairs(text, pairs, label):
    for new, old in pairs:
        count = text.count(new)
        if count != 1:
            raise SystemExit(
                f'{label}: expected 1 occurrence, found {count}: {new[:70]!r}')
        text = text.replace(new, old, 1)
    return text


def revert_blocks(text, blocks, label):
    for start, end in blocks:
        i = text.index(start)
        j = text.index(end, i)
        text = text[:i] + text[j:]
    return text


# --- against the unified1s reference copies --------------------------------
ref = ROOT / 'tools/opus-20261023/ref-1s'
diff('dlssnr.diff', (ref / 'dlssnr.hpp').read_text(encoding='utf-8'),
     (DLSS5 / 'dlssnr.hpp').read_text(encoding='utf-8'),
     'ref-1s/dlssnr.hpp', 'src/dlss5/dlssnr.hpp')
diff('vulkan_path.diff', (ref / 'vulkan_path.hpp').read_text(encoding='utf-8'),
     (DLSS5 / 'vulkan_path.hpp').read_text(encoding='utf-8'),
     'ref-1s/vulkan_path.hpp', 'src/dlss5/vulkan_path.hpp')

# --- declines.hpp -----------------------------------------------------------
declines = revert_pairs((DLSS5 / 'declines.hpp').read_text(encoding='utf-8'), [
    ('''  // v8.5.0 (release-before-create): this stream's contract changed and its
  // previous generation's retirement has not been released yet, so the
  // replacement is not allocated this evaluate - two generations of one
  // stream are never resident at once (8 GB budgets).  ELIGIBLE like
  // kSlotWarmupMaturation: a pass-through that explains itself, not a
  // failure - no latch, no backoff, the next evaluate retries.
  kReplacementPending,
''', ''),
    ('''    "the previous NR instance of this stream is being released before its"
    " replacement is allocated (one generation in memory; the image passes"
    " through)",
''', ''),
    ('''    "screenshot_wait", "vk_owner_mismatch",
    "replacement_wait",
''', '''    "screenshot_wait", "vk_owner_mismatch",
'''),
], 'declines')
diff('declines.diff', declines,
     (DLSS5 / 'declines.hpp').read_text(encoding='utf-8'),
     'baseline/declines.hpp (RECONSTRUCTED-BASELINE)', 'src/dlss5/declines.hpp')

# --- bridge: hook.c ----------------------------------------------------------
hook = (BRIDGE / 'minhook/src/hook.c').read_text(encoding='utf-8')
hook = revert_pairs(hook, [
    ('''    UINT8  extendedPrologue : 1;
    UINT8  inModuleRelay : 1;   // pDetour is a relay written into the target
                                // module's own section slack (the E9 patch
                                // jumps to it instead of jmpRelay).
''', '    UINT8  extendedPrologue : 1;\n'),
    ('''// Defined next to the in-module relay writer below.
static LPVOID HookJmpTarget(PHOOK_ENTRY pHook);

//-------------------------------------------------------------------------
// Returns INVALID_HOOK_POS if not found.
''', '''//-------------------------------------------------------------------------
// Returns INVALID_HOOK_POS if not found.
'''),
    ('    if (ip == (DWORD_PTR)HookJmpTarget(pHook))',
     '    if (ip == (DWORD_PTR)&pHook->pExecBuffer->jmpRelay)'),
    ('''            if ((PJMP_RELAY)HookJmpTarget(pHook) != pJmpRelay)
            {
                PEXEC_BUFFER pOtherExecBuffer;
                const UINT owner = FindHookByJmpTarget(pJmpRelay);
                if (owner != INVALID_HOOK_POS)
                    pOtherExecBuffer = g_hooks.pItems[owner].pExecBuffer;
                else
                    pOtherExecBuffer = (PEXEC_BUFFER)((LPBYTE)pJmpRelay - offsetof(EXEC_BUFFER, jmpRelay));
                return pOtherExecBuffer->pDisableHookChain(pOtherExecBuffer->hookIdent, pHook->pTarget, pos, EnableHookLL, pThreads);
            }
''', '''            if (&pHook->pExecBuffer->jmpRelay != pJmpRelay)
            {
                PEXEC_BUFFER pOtherExecBuffer = (PEXEC_BUFFER)((LPBYTE)pJmpRelay - offsetof(EXEC_BUFFER, jmpRelay));
                return pOtherExecBuffer->pDisableHookChain(pOtherExecBuffer->hookIdent, pHook->pTarget, pos, EnableHookLL, pThreads);
            }
'''),
    ('        const INT64 displacement = (INT64)(ULONG_PTR)HookJmpTarget(pHook)',
     '        const INT64 displacement = (INT64)(ULONG_PTR)&pHook->pExecBuffer->jmpRelay'),
    ('        pJmp->operand = (INT32)((LPBYTE)HookJmpTarget(pHook) - (pPatchTarget + sizeof(JMP_REL)));',
     '        pJmp->operand = (INT32)((LPBYTE)&pHook->pExecBuffer->jmpRelay - (pPatchTarget + sizeof(JMP_REL)));'),
    ('''            BOOL extended = FALSE;
            LPVOID inModuleRelay = NULL;
            PEXEC_BUFFER pBuffer = (PEXEC_BUFFER)AllocateBuffer(pTarget);
            if (pBuffer == NULL && allowExtended && MatchesExtendedPrologue(pTarget))
            {
                pBuffer = (PEXEC_BUFFER)AllocateBufferExtended(pTarget);
                extended = pBuffer != NULL;
            }
#if defined(_M_X64) || defined(__x86_64__)
            if (pBuffer == NULL && allowExtended && MatchesExtendedPrologue(pTarget))
            {
                // No executable memory within rel32 range at all.  Write the
                // 14-byte absolute relay into the slack of the target's own
                // executable section; the trampoline needs no range bound
                // because the extended prologue is copied verbatim and its
                // return jump is absolute.
                inModuleRelay = WriteInModuleRelay(pTarget, pDetour);
                if (inModuleRelay != NULL)
                {
                    pBuffer = (PEXEC_BUFFER)AllocateBufferAnywhere(pTarget);
                    if (pBuffer != NULL)
                        extended = TRUE;
                }
            }
#endif
''', '''            BOOL extended = FALSE;
            PEXEC_BUFFER pBuffer = (PEXEC_BUFFER)AllocateBuffer(pTarget);
            if (pBuffer == NULL && allowExtended && MatchesExtendedPrologue(pTarget))
            {
                pBuffer = (PEXEC_BUFFER)AllocateBufferExtended(pTarget);
                extended = pBuffer != NULL;
            }
'''),
    ('''                    // The in-module relay is what the E9 patch jumps to; the
                    // real detour address only lives in the relay's imm64.
                    pHook->pDetour = inModuleRelay != NULL ? inModuleRelay : pDetour;
                    pHook->pExecBuffer = pBuffer;
                    pHook->isEnabled = FALSE;
                    pHook->queueEnable = FALSE;
                    pHook->extendedPrologue = extended;
                    pHook->inModuleRelay = inModuleRelay != NULL;
''', '''                    pHook->pDetour = pDetour;
                    pHook->pExecBuffer = pBuffer;
                    pHook->isEnabled = FALSE;
                    pHook->queueEnable = FALSE;
                    pHook->extendedPrologue = extended;
'''),
], 'hook.c')
hook = revert_blocks(hook, [
    ('#if defined(_M_X64) || defined(__x86_64__)\n// Last resort for MH_CreateHookPrologueExtended',
     'static MH_STATUS CreateHookTrampoline(UINT pos)'),
    ('//-------------------------------------------------------------------------\nLPVOID WINAPI MH_GetInModuleRelay',
     'MH_STATUS WINAPI MH_CreateHook(LPVOID pTarget, LPVOID pDetour, LPVOID *ppOriginal)'),
], 'hook.c')
diff('bridge-hook.diff', hook, (BRIDGE / 'minhook/src/hook.c').read_text(encoding='utf-8'),
     'baseline/hook.c (RECONSTRUCTED-BASELINE)', 'src/minhook/src/hook.c')

# --- bridge: buffer.c / buffer.h / MinHook.h ----------------------------------
buf = revert_blocks((BRIDGE / 'minhook/src/buffer.c').read_text(encoding='utf-8'), [
    ('\nLPVOID AllocateBufferAnywhere(LPVOID pOrigin)', '\n//---'),
], 'buffer.c')
diff('bridge-buffer.diff', buf, (BRIDGE / 'minhook/src/buffer.c').read_text(encoding='utf-8'),
     'baseline/buffer.c (RECONSTRUCTED-BASELINE)', 'src/minhook/src/buffer.c')

bufh = revert_pairs((BRIDGE / 'minhook/src/buffer.h').read_text(encoding='utf-8'), [
    ('LPVOID AllocateBufferAnywhere(LPVOID pOrigin);\n', ''),
], 'buffer.h')
diff('bridge-buffer-h.diff', bufh, (BRIDGE / 'minhook/src/buffer.h').read_text(encoding='utf-8'),
     'baseline/buffer.h (RECONSTRUCTED-BASELINE)', 'src/minhook/src/buffer.h')

mhh = revert_pairs((BRIDGE / 'minhook/include/MinHook.h').read_text(encoding='utf-8'), [
    ('''    // The slack address of the in-module relay a hook created through
    // MH_CreateHookPrologueExtended's last-resort path uses, or NULL when the
    // hook allocated a normal relay (or does not exist).
    LPVOID WINAPI MH_GetInModuleRelay(LPVOID pTarget);
''', ''),
], 'MinHook.h')
diff('bridge-minhook-h.diff', mhh, (BRIDGE / 'minhook/include/MinHook.h').read_text(encoding='utf-8'),
     'baseline/MinHook.h (RECONSTRUCTED-BASELINE)', 'src/minhook/include/MinHook.h')

# --- bridge: present-adapter.inc ----------------------------------------------
pa = (BRIDGE / 'present-adapter.inc').read_text(encoding='utf-8')
pa = revert_blocks(pa, [
    ('// Final hook-creation failure diagnostics:',
     '// verbose lists every NGX evaluate module'),
], 'present-adapter')
pa = revert_pairs(pa, [
    ('        static std::atomic_bool diagnostics_logged{false};\n'
     '        if (!diagnostics_logged.exchange(true)) PresentAdapterLogHookAllocDiagnostics(entry);\n',
     ''),
    ('''    const void* in_module_relay = MH_GetInModuleRelay(entry);
    if (in_module_relay != nullptr)
        Log("[present-adapter] SR carrier used an in-module relay at %p (no executable memory within +/-2 GiB).",
            in_module_relay);
''', ''),
], 'present-adapter')
diff('bridge-present-adapter.diff', pa, (BRIDGE / 'present-adapter.inc').read_text(encoding='utf-8'),
     'baseline/present-adapter.inc (RECONSTRUCTED-BASELINE)', 'src/present-adapter.inc')

# --- bridge: version strings + provenance --------------------------------------
br = revert_pairs((BRIDGE / 'dlss5-bridge.cpp').read_text(encoding='utf-8'), [
    ('"1.4.13-pre8-unified1s-timing4-20261023"', '"1.4.13-pre8-unified1s-timing2-20261023"'),
], 'dlss5-bridge.cpp')
diff('bridge-version.diff', br, (BRIDGE / 'dlss5-bridge.cpp').read_text(encoding='utf-8'),
     'baseline/dlss5-bridge.cpp (RECONSTRUCTED-BASELINE)', 'src/dlss5-bridge.cpp')

vrc = revert_pairs((BRIDGE / 'version.rc').read_text(encoding='utf-8'), [
    ('VALUE "FileVersion",      "1.4.13-pre8-unified1s-timing4-20261023"',
     'VALUE "FileVersion",      "1.4.13-pre8-unified1s-20261022"'),
    ('VALUE "ProductVersion",   "1.4.13-pre8-unified1s-timing4-20261023"',
     'VALUE "ProductVersion",   "1.4.13-pre8-unified1s-20261022"'),
], 'version.rc')
diff('bridge-version-rc.diff', vrc, (BRIDGE / 'version.rc').read_text(encoding='utf-8'),
     'baseline/version.rc (RECONSTRUCTED-BASELINE)', 'src/version.rc')

prov = revert_blocks((BRIDGE / 'minhook/PROVENANCE.md').read_text(encoding='utf-8'), [
    ('\n- `hook.c` / `buffer.c` / `MinHook.h`: when both ranged allocations also fail,',
     '\n- `trampoline.c`'),
], 'PROVENANCE.md')
diff('bridge-provenance.diff', prov, (BRIDGE / 'minhook/PROVENANCE.md').read_text(encoding='utf-8'),
     'baseline/PROVENANCE.md (RECONSTRUCTED-BASELINE)', 'src/minhook/PROVENANCE.md')
