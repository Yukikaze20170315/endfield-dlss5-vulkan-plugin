// Helper images for test-mfg-unload-race.cpp.
// MFG_RACE_GATE: mfg-race-gate.dll, whose DllMain calls the host's
// MfgRaceGateEnter while the loader lock is held. Otherwise mfg-race-victim.dll,
// an empty image whose only purpose is to need the loader lock to map.
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
#ifdef MFG_RACE_GATE
  if (reason == DLL_PROCESS_ATTACH) {
    using Enter = void (*)();
    if (auto enter = reinterpret_cast<Enter>(GetProcAddress(GetModuleHandleW(nullptr), "MfgRaceGateEnter")))
      enter();
  }
#else
  (void)reason;
#endif
  return TRUE;
}
