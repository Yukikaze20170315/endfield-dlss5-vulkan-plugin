// Exercise the production MinHook in-module relay fallback in an isolated
// process: with every free region inside +/-2 GiB of the fixture module
// reserved, MH_CreateHookPrologueExtended can only succeed through the relay
// written into the fixture's own executable-section slack.
//
// Covered here:
//   1. the normal unfragmented path is unchanged (no in-module relay);
//   2. with +/-2 GiB fully reserved, create+enable succeeds, the detour runs,
//      and the original reached through the trampoline returns 8*x+3;
//   3. disable/remove restores the target's original bytes;
//   4. a section with no usable slack fails safely with status 9 and the
//      target bytes are unchanged.
//
// Only in-process fixture memory is touched; no game handles, GPU work or
// driver state are involved.
// The production MinHook sources are compiled as C by the build script and
// linked in as minhook.obj; this file only exercises the public API.
#include <windows.h>
#include "MinHook.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static int checks = 0;

static void Require(bool value, const char* label) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s error=%lu\n", label, GetLastError());
    std::exit(1);
  }
  ++checks;
}

// Reserve every MEM_FREE region in [lo, hi) so the pool allocator cannot find
// a single page inside the range.  Returns the reservations for cleanup.
static std::vector<void*> ReserveAllFree(uintptr_t lo, uintptr_t hi) {
  std::vector<void*> held;
  for (uintptr_t at = lo; at < hi;) {
    MEMORY_BASIC_INFORMATION memory = {};
    if (VirtualQuery(reinterpret_cast<void*>(at), &memory, sizeof(memory)) == 0) break;
    const uintptr_t next = reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    if (memory.State == MEM_FREE) {
      const uintptr_t start = at > reinterpret_cast<uintptr_t>(memory.BaseAddress)
          ? at : reinterpret_cast<uintptr_t>(memory.BaseAddress);
      const uintptr_t end = next < hi ? next : hi;
      // MEM_RESERVE must start on a 64 KiB boundary; a sub-granularity head or
      // tail of a free region is no use to the block allocator either.
      const uintptr_t begin = (start + 0xffffULL) & ~uintptr_t(0xffffULL);
      if (end > begin) {
        void* held_page = VirtualAlloc(reinterpret_cast<void*>(begin), end - begin,
                                       MEM_RESERVE, PAGE_NOACCESS);
        if (held_page != nullptr) held.push_back(held_page);
      }
    }
    if (next <= at) break;
    at = next;
  }
  return held;
}

// --- normal (unfragmented) case -------------------------------------------
__declspec(noinline) static int PlainTarget(int x) { return x * 2 + 1; }
static int (*g_plain_original)(int);
static int g_plain_calls = 0;
static int PlainDetour(int x) { ++g_plain_calls; return g_plain_original(x) + 100; }

static void NormalPathUnchanged() {
  Require(MH_CreateHook(reinterpret_cast<void*>(&PlainTarget),
                        reinterpret_cast<void*>(&PlainDetour),
                        reinterpret_cast<void**>(&g_plain_original)) == MH_OK,
          "normal hook created");
  Require(MH_EnableHook(reinterpret_cast<void*>(&PlainTarget)) == MH_OK, "normal hook enabled");
  Require(PlainTarget(3) == 107, "detour ran through normal relay");
  Require(g_plain_calls == 1, "detour counted");
  Require(g_plain_original(3) == 7, "trampoline answered");
  Require(MH_GetInModuleRelay(reinterpret_cast<void*>(&PlainTarget)) == nullptr,
          "normal hook did not use an in-module relay");
  Require(MH_DisableHook(reinterpret_cast<void*>(&PlainTarget)) == MH_OK, "normal hook disabled");
  Require(MH_RemoveHook(reinterpret_cast<void*>(&PlainTarget)) == MH_OK, "normal hook removed");
}

// --- fragmented case --------------------------------------------------------
typedef int (*CarrierFn)(int);
static CarrierFn g_carrier_original = nullptr;
static int g_carrier_calls = 0;
static int CarrierDetour(int x) { ++g_carrier_calls; return g_carrier_original(x) + 1; }

int main() {
  Require(MH_Initialize() == MH_OK, "MH_Initialize");

  NormalPathUnchanged();

  const HMODULE fixture = LoadLibraryW(L"sr-relay-fixture.dll");
  Require(fixture != nullptr, "fixture loaded");
  const auto carrier = reinterpret_cast<CarrierFn>(
      GetProcAddress(fixture, "CarrierEval"));
  const auto carrier2 = reinterpret_cast<CarrierFn>(
      GetProcAddress(fixture, "CarrierEval2"));
  Require(carrier != nullptr && carrier2 != nullptr, "fixture exports");

  const unsigned char prologue[5] = {0x48, 0x89, 0x5C, 0x24, 0x08};
  Require(memcmp(carrier, prologue, sizeof(prologue)) == 0, "fixture prologue matches");
  Require(carrier(5) == 43, "fixture carrier returns 8*x+3");

  // Reserve every free region within +/-2 GiB of the fixture module.
  const uintptr_t origin = reinterpret_cast<uintptr_t>(carrier);
  const uintptr_t lo = origin > 0x80000000ULL ? origin - 0x80000000ULL : 0x10000ULL;
  const uintptr_t hi = origin + 0x80000000ULL;
  std::vector<void*> held = ReserveAllFree(lo, hi);
  Require(!held.empty(), "reservations took memory");
  std::printf("fixture=%p origin=%p reserved=%zu lo=0x%llx hi=0x%llx\n",
              fixture, carrier, held.size(),
              static_cast<unsigned long long>(lo),
              static_cast<unsigned long long>(hi));

  // Create must now fall back to the in-module slack relay.
  Require(MH_CreateHookPrologueExtended(
              reinterpret_cast<void*>(carrier),
              reinterpret_cast<void*>(&CarrierDetour),
              reinterpret_cast<void**>(&g_carrier_original)) == MH_OK,
          "fragmented hook created via in-module relay");
  const void* relay = MH_GetInModuleRelay(reinterpret_cast<void*>(carrier));
  Require(relay != nullptr, "in-module relay recorded");
  Require(relay >= fixture && relay < reinterpret_cast<unsigned char*>(fixture) + 0x200000,
          "relay sits inside the fixture image");

  Require(MH_EnableHook(reinterpret_cast<void*>(carrier)) == MH_OK, "fragmented hook enabled");
  Require(carrier(5) == 44, "detour ran: 8*5+3 through trampoline, +1");
  Require(g_carrier_calls == 1, "carrier detour counted");
  Require(g_carrier_original != nullptr && g_carrier_original(5) == 43,
          "trampoline anywhere in memory returns 8*x+3");

  Require(MH_DisableHook(reinterpret_cast<void*>(carrier)) == MH_OK, "disable restores bytes");
  Require(memcmp(carrier, prologue, sizeof(prologue)) == 0,
          "target prologue restored");
  Require(MH_RemoveHook(reinterpret_cast<void*>(carrier)) == MH_OK, "hook removed");

  // The second export's section has zero slack: status 9, bytes unchanged.
  unsigned char before[8] = {};
  memcpy(before, carrier2, sizeof(before));
  const MH_STATUS noslack = MH_CreateHookPrologueExtended(
      reinterpret_cast<void*>(carrier2), reinterpret_cast<void*>(&CarrierDetour),
      nullptr);
  Require(noslack == MH_ERROR_MEMORY_ALLOC, "no slack fails with status 9");
  Require(memcmp(before, carrier2, sizeof(before)) == 0,
          "failed create leaves target bytes unchanged");

  for (void* page : held) VirtualFree(page, 0, MEM_RELEASE);
  Require(MH_Uninitialize() == MH_OK, "MH_Uninitialize");
  std::printf("PASS: %d checks; in-module relay hooks a fragmented target, "
              "restores it, and no-slack fails safely with status 9.\n", checks);
  return 0;
}
