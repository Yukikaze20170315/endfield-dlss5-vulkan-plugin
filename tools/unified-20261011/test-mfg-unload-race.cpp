// Deterministic reproduction of the 1o launch failure, run against a real
// renodx-mfgunlock.addon64 build in a host that impersonates ReShade's add-on
// API (and is named Endfield.exe, so MFG takes its Endfield startup path).
//
//   race  : thread C enters the loader (LdrLoadDll, not through MFG's hooks) and,
//           holding the loader lock inside mfg-race-gate.dll's DllMain, frees the
//           add-on the way ReShade does at vkDestroyInstance. Thread B has just
//           entered MFG's LoadLibraryExW detour and is queued on that lock, as
//           Unity's Direct3D 9 probe thread was. When B's real LoadLibraryExW
//           returns it executes the instruction after the call inside the detour.
//           If the image was unmapped this is a DEP fault inside the old range.
//   cycle : six ReShade add-on cycles (load/AddonInit/FreeLibrary). Counts the
//           kernel32 detour installs and AddonInit rescans while a hooked
//           LoadLibraryExW runs in every cycle.
//
// No GPU work, driver setting, game file or ReShade installation is involved.
#include <windows.h>
#include <winternl.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

static HMODULE g_mfg = nullptr;
static uintptr_t g_mfg_base = 0, g_mfg_end = 0;
static HANDLE g_entered = nullptr, g_release = nullptr;
static std::atomic<int> g_kernel32_installs{0}, g_rescans{0}, g_resident_logs{0};
static unsigned char g_imgui_table[1 << 16];

extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(void*, uint32_t) { return true; }
extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(void*) {}
extern "C" __declspec(dllexport) const void* ReShadeGetImGuiFunctionTable(uint32_t) { return g_imgui_table; }
extern "C" __declspec(dllexport) void ReShadeRegisterEvent(int, void*) {}
extern "C" __declspec(dllexport) void ReShadeUnregisterEvent(int, void*) {}
extern "C" __declspec(dllexport) void ReShadeRegisterOverlay(const char*, void*) {}
extern "C" __declspec(dllexport) void ReShadeUnregisterOverlay(const char*, void*) {}
extern "C" __declspec(dllexport) bool ReShadeGetConfigValue(void*, void*, const char*, const char*, char*, size_t*) { return false; }
extern "C" __declspec(dllexport) void ReShadeSetConfigValue(void*, void*, const char*, const char*, const char*) {}
extern "C" __declspec(dllexport) void ReShadeSetConfigArray(void*, void*, const char*, const char*, const char*, size_t) {}
extern "C" __declspec(dllexport) void ReShadeLogMessage(void*, int level, const char* message) {
  if (!message) return;
  if (std::strstr(message, "hook(s) in kernel32.dll")) ++g_kernel32_installs;
  if (std::strstr(message, "bootstrap rescan")) ++g_rescans;
  if (std::strstr(message, "kept resident")) ++g_resident_logs;
  std::printf("  [mfg %d] %s\n", level, message);
  std::fflush(stdout);
}

// Called by mfg-race-gate.dll's DllMain, i.e. with the loader lock held.
extern "C" __declspec(dllexport) void MfgRaceGateEnter() {
  SetEvent(g_entered);
  WaitForSingleObject(g_release, INFINITE);
  FreeLibrary(g_mfg);
}

static LONG CALLBACK OnException(EXCEPTION_POINTERS* info) {
  const auto* record = info->ExceptionRecord;
  if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2 ||
      record->ExceptionInformation[0] != 8) return EXCEPTION_CONTINUE_SEARCH;
  const auto address = reinterpret_cast<uintptr_t>(record->ExceptionAddress);
  const bool inside = address >= g_mfg_base && address < g_mfg_end;
  std::printf("RESULT crash: DEP at %p, %s the add-on's former image (offset 0x%llx), thread %lu\n",
              record->ExceptionAddress, inside ? "inside" : "outside",
              static_cast<unsigned long long>(inside ? address - g_mfg_base : 0), GetCurrentThreadId());
  std::fflush(stdout);
  TerminateProcess(GetCurrentProcess(), inside ? 0xC0000005u : 0xC000DEADu);
  return EXCEPTION_CONTINUE_SEARCH;
}

static std::wstring Beside(const wchar_t* leaf) {
  wchar_t path[MAX_PATH]{};
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  std::wstring result(path);
  return result.substr(0, result.find_last_of(L'\\') + 1) + leaf;
}

using AddonInitFn = bool (*)(HMODULE, HMODULE);
static bool Load(const wchar_t* path) {
  g_mfg = LoadLibraryW(path);
  if (!g_mfg) return false;
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_mfg);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const char*>(g_mfg) + dos->e_lfanew);
  g_mfg_base = reinterpret_cast<uintptr_t>(g_mfg);
  g_mfg_end = g_mfg_base + nt->OptionalHeader.SizeOfImage;
  return true;
}
static int CallAddonInit() {
  const auto init = reinterpret_cast<AddonInitFn>(GetProcAddress(g_mfg, "AddonInit"));
  if (!init) return -1;
  return init(g_mfg, GetModuleHandleW(nullptr)) ? 1 : 0;
}
static bool Resident() {
  HMODULE owner = nullptr;
  return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(g_mfg_base), &owner) && owner == g_mfg;
}

using LdrLoadDllFn = NTSTATUS(NTAPI*)(PWSTR, PULONG, PUNICODE_STRING, PVOID*);
static DWORD WINAPI GateThread(void*) {
  static const std::wstring gate = Beside(L"mfg-race-gate.dll");
  const auto ldr = reinterpret_cast<LdrLoadDllFn>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrLoadDll"));
  UNICODE_STRING name{};
  name.Buffer = const_cast<PWSTR>(gate.c_str());
  name.Length = static_cast<USHORT>(gate.size() * sizeof(wchar_t));
  name.MaximumLength = name.Length;
  PVOID base = nullptr;
  return ldr && ldr(nullptr, nullptr, &name, &base) >= 0 ? 0 : 1;
}
// Created before the gate takes the loader lock, as Unity's probe thread already
// existed: a thread started while the lock is held would stop in its own
// DLL_THREAD_ATTACH and never reach the detour.
static HANDLE g_go = nullptr;
static DWORD WINAPI VictimThread(void*) {
  static const std::wstring victim = Beside(L"mfg-race-victim.dll");
  WaitForSingleObject(g_go, INFINITE);
  return LoadLibraryExW(victim.c_str(), nullptr, 0) ? 0 : 1;
}

static int Race(const wchar_t* path) {
  if (!Load(path)) { std::printf("RESULT fixture: add-on did not load (%lu)\n", GetLastError()); return 2; }
  const int init = CallAddonInit();
  if (g_kernel32_installs.load() != 1) { std::printf("RESULT fixture: kernel32 detours not installed\n"); return 3; }
  g_entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  g_release = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  g_go = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  HANDLE victim = CreateThread(nullptr, 0, VictimThread, nullptr, 0, nullptr);
  Sleep(100);
  HANDLE gate = CreateThread(nullptr, 0, GateThread, nullptr, 0, nullptr);
  if (WaitForSingleObject(g_entered, 10000) != WAIT_OBJECT_0) { std::printf("RESULT fixture: gate not entered\n"); return 4; }
  SetEvent(g_go);
  Sleep(300);
  if (WaitForSingleObject(victim, 0) != WAIT_TIMEOUT) { std::printf("RESULT fixture: victim was not queued on the loader lock\n"); return 5; }
  SetEvent(g_release);
  HANDLE both[] = {gate, victim};
  if (WaitForMultipleObjects(2, both, TRUE, 10000) != WAIT_OBJECT_0) { std::printf("RESULT fixture: threads did not finish\n"); return 6; }
  DWORD gate_code = 1, victim_code = 1;
  GetExitCodeThread(gate, &gate_code); GetExitCodeThread(victim, &victim_code);
  const bool resident = Resident();
  std::printf("RESULT survived: addon_init=%d gate=%lu victim=%lu resident_after_free=%d\n",
              init, gate_code, victim_code, resident ? 1 : 0);
  return gate_code == 0 && victim_code == 0 && resident ? 0 : 7;
}

static int Cycle(const wchar_t* path) {
  const std::wstring victim = Beside(L"mfg-race-victim.dll");
  int fresh_loads = 0, external_inits = 0, failed_inits = 0, hooked_calls = 0;
  for (int cycle = 1; cycle <= 6; ++cycle) {
    if (cycle == 1 || !Resident()) {
      if (!Load(path)) { std::printf("RESULT fixture: load failed in cycle %d\n", cycle); return 2; }
      ++fresh_loads;
    } else {
      ++external_inits;
    }
    if (CallAddonInit() == 0) ++failed_inits;
    if (HMODULE m = LoadLibraryExW(victim.c_str(), nullptr, 0)) { ++hooked_calls; FreeLibrary(m); }
    FreeLibrary(g_mfg);  // ReShade's unload at vkDestroyInstance
  }
  std::printf("RESULT cycle: fresh_loads=%d external_inits=%d failed_inits=%d kernel32_installs=%d rescans=%d resident_logs=%d hooked_calls=%d resident_at_end=%d\n",
              fresh_loads, external_inits, failed_inits, g_kernel32_installs.load(), g_rescans.load(),
              g_resident_logs.load(), hooked_calls, Resident() ? 1 : 0);
  return failed_inits == 0 && hooked_calls == 6 ? 0 : 8;
}

int wmain(int argc, wchar_t** argv) {
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
  AddVectoredExceptionHandler(1, OnException);
  if (argc != 3) { std::printf("usage: Endfield.exe race|cycle <renodx-mfgunlock.addon64>\n"); return 64; }
  if (std::wcscmp(argv[1], L"race") == 0) return Race(argv[2]);
  if (std::wcscmp(argv[1], L"cycle") == 0) return Cycle(argv[2]);
  return 64;
}
