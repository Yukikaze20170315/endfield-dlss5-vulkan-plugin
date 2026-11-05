"""Prepare MFG Unlock unload-pin1p from the hook-reliability1n snapshot.

Defect (1o launch failure, PID 11388): ReShade on Vulkan frees every add-on at
each vkDestroyInstance, and Endfield creates and destroys several Vulkan
instances during startup. MFG Unlock was the only add-on that really unloaded:
its DllMain detach removed its kernel32 LoadLibrary/GetProcAddress detours and
the loader then unmapped the image. A thread that had already entered one of
those detours (Unity's Direct3D 9 probe, inside LoadLibraryExW and waiting for
the loader lock held by that FreeLibrary) returned into unmapped memory:
`DEP at <MFG base>+0x31183`, the instruction after `call g_real_load_library_ex_w`
in HookedLoadLibraryExW.

Change:
* After the safe-mode check and before any hook is installed, pin this module
  and the ReShade module it calls into for the process lifetime (the same rule
  the bridge already applies to its own kernel32 detour). If the add-on itself
  cannot be pinned, it falls back to the existing bypass (no hooks, no patches).
* Export AddonInit. ReShade calls it after the first LoadLibrary and, for an
  add-on that stayed registered, at every later vkCreateInstance. The first call
  adds nothing; later calls repeat DllMain's idempotent bootstrap scan, which a
  fresh DllMain previously did on each cycle.
* Version resource 0.11.2026.1014 / unload-pin1p. Nothing else changes.
"""
from pathlib import Path
import hashlib

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'builds/unified-20261011/mfg-hook-reliability1n/source'
OUT = ROOT / 'builds/unified-20261011/mfg-unload-pin1p/source'
ADDON = 'mfg/addon/addon.cpp'
RC = 'mfg/version.rc'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


for rel in (ADDON, RC):
    assert digest(OUT / rel) == digest(BASE / rel), f'{rel} is not the 1n source; refusing to patch twice'

# addon.cpp uses CRLF and version.rc LF; keep each file's own line endings.
def patch(rel, replacements):
    path = OUT / rel
    data = path.read_bytes().decode('utf-8')
    eol = '\r\n' if '\r\n' in data else '\n'
    for old, new in replacements:
        old = old.replace('\n', eol)
        new = new.replace('\n', eol)
        assert data.count(old) == 1, f'{rel}: anchor not unique: {old[:80]!r}'
        data = data.replace(old, new)
    path.write_bytes(data.encode('utf-8'))


patch(ADDON, [
    (
        ' * All binary changes are in-memory and restored on unload.\n',
        ' * All binary changes are in-memory. Once its hooks are installed the add-on\n'
        ' * stays resident until process exit, so they are never unmapped under a caller.\n',
    ),
    (
        '  return state.safe!=0;\n}\n\n',
        '  return state.safe!=0;\n}\n\n'
        '// Detours installed by this add-on can be entered from any thread independently\n'
        '// of ReShade\'s add-on references, and a caller can still be inside one when a\n'
        '// FreeLibrary removes it. Keep the image (and the ReShade module that its\n'
        '// callbacks call into) mapped until process exit.\n'
        'static bool PinModuleForProcessLifetime(HMODULE module) {\n'
        '  HMODULE pinned = nullptr;\n'
        '  return module != nullptr &&\n'
        '         GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,\n'
        '                            reinterpret_cast<LPCWSTR>(module), &pinned) != FALSE &&\n'
        '         pinned == module;\n'
        '}\n\n',
    ),
    (
        '        return TRUE;\n'
        '      }\n'
        '      mfgunlock::framecount::g_native_provider_ready = NativeBlackwellProviderReady;\n',
        '        return TRUE;\n'
        '      }\n'
        '      // ReShade on Vulkan frees every add-on at each vkDestroyInstance, and\n'
        '      // Endfield creates and destroys several instances during startup. A\n'
        '      // thread already inside the kernel32 detours below (Unity\'s Direct3D 9\n'
        '      // probe waits in LoadLibraryExW for the loader lock held by that\n'
        '      // FreeLibrary) would return into an unmapped image. Pin before the\n'
        '      // first hook; AddonInit repeats the bootstrap scan on later cycles.\n'
        '      if (!PinModuleForProcessLifetime(h_module)) {\n'
        '        g_deployment_bypass=true;\n'
        '        g_enabled=false;\n'
        '        reshade::log::message(reshade::log::level::error,"mfgunlock: the add-on could not be kept resident; hooks and provider patches skipped this session.");\n'
        '        return TRUE;\n'
        '      }\n'
        '      if (!PinModuleForProcessLifetime(reshade::internal::get_reshade_module_handle()))\n'
        '        mfgunlock::provider::Log("ReShade module could not be pinned; relying on its own lifetime", true);\n'
        '      mfgunlock::provider::Log("add-on kept resident for the process lifetime; later ReShade add-on cycles rescan instead of reloading");\n'
        '      mfgunlock::framecount::g_native_provider_ready = NativeBlackwellProviderReady;\n',
    ),
    (
        'BOOL APIENTRY DllMain(HMODULE h_module, DWORD fdw_reason, LPVOID reserved) {\n',
        '// ReShade calls AddonInit right after LoadLibrary and again, for an add-on that\n'
        '// stayed registered across a Vulkan instance teardown, at every later\n'
        '// vkCreateInstance. The first call directly follows DllMain\'s bootstrap scan.\n'
        '// Later calls repeat that scan, which a fresh DllMain did on each cycle before\n'
        '// the add-on was kept resident: modules mapped as dependencies outside the\n'
        '// LoadLibrary detours are found here. Every step is idempotent. It must return\n'
        '// true, or ReShade reports a failed initialization.\n'
        'extern "C" __declspec(dllexport) bool AddonInit(HMODULE, HMODULE) {\n'
        '  static std::atomic<unsigned int> calls{0};\n'
        '  const unsigned int call = calls.fetch_add(1, std::memory_order_acq_rel) + 1;\n'
        '  if (call == 1 || g_deployment_bypass) return true;\n'
        '  mfgunlock::provider::Log("add-on cycle " + std::to_string(call) + ": bootstrap rescan");\n'
        '  mfgunlock::loadhook::TryInstall();\n'
        '  mfgunlock::ngx::EnsureEntryHooks();\n'
        '  mfgunlock::streamline::internal::InstallLegacyInitHook();\n'
        '  mfgunlock::streamline::internal::InstallSupportHook();\n'
        '  RunProviderMaintenance();\n'
        '  if (LegacyRequested()) TryPatchFlipMetering();\n'
        '  mfgunlock::framecount::TryInstall();\n'
        '  return true;\n'
        '}\n\n'
        'BOOL APIENTRY DllMain(HMODULE h_module, DWORD fdw_reason, LPVOID reserved) {\n',
    ),
])

patch(RC, [
    (' FILEVERSION 0,11,2026,1013\n PRODUCTVERSION 0,11,2026,1013\n',
     ' FILEVERSION 0,11,2026,1014\n PRODUCTVERSION 0,11,2026,1014\n'),
    ('"MFG Unlock hook-reliability1n exited-thread tolerance"',
     '"MFG Unlock unload-pin1p resident after hook install"'),
    ('"FileVersion", "0.11+codex-hook-reliability1n-20261012"',
     '"FileVersion", "0.11+unload-pin1p-20261012"'),
    ('"ProductVersion", "0.11+codex-hook-reliability1n-20261012"',
     '"ProductVersion", "0.11+unload-pin1p-20261012"'),
])

for rel in (ADDON, RC):
    print(rel, digest(OUT / rel))
