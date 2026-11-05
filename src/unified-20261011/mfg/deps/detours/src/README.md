# Local Detours trampoline allocation extension

This source replaces the core `detours.obj` when linking MFG Unlock. The existing
`../lib.X64/detours.lib` supplies the remaining Detours objects. Do not also replace
that library with an unrelated version. `provenance.json` records the local source
snapshot and the matching original library hash; the untouched source snapshot is
in the project evidence directory named there by the implementation report.

The default allocation remains 64KB. On x64, if no such block is available near
the target, allocation retries with one 4KB page. Bases remain 64KB-aligned, so
trampoline-to-region lookup retains its original invariant. Each region records
its actual allocation size for protection changes, instruction-cache flushing,
free-list capacity and empty-region checks. The public
`DetourAllocateRegionWithinJumpBounds` behavior is unchanged.

Transactions, thread enrollment, instruction relocation, commit/abort and release
proofs remain the Detours implementation. No executable-write policy is bypassed:
failure of both allocation sizes remains a failed transaction. This extension
does not guarantee a free page in every process layout.

`tools/unified-20261011/test-fragmented-hooks.cpp` exercises the production address
hook wrapper against a fixed low-address fixture DLL. Its isolated process reserves
the rest of the jump window, with full blocks, three small gaps, one small gap or
no gap. It checks original and replacement calls, executable-only protection after
commit, allocation exhaustion preserving earlier hooks, detach and region release.
The fixture's VirtualAlloc2 placeholder use is test-only; the plugin still uses
VirtualAlloc/VirtualQuery and does not acquire a new OS API dependency.

Microsoft's copyright and MIT license are retained in `detours.cpp` and `LICENSE.md`.
