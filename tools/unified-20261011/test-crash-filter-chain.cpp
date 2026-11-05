// Unit test for common/crash-filter-chain.hpp: every filter that was on top
// before or after the bridge's crash filter runs exactly once per crash, the
// original filter runs last, and the report is written once. A filter whose
// module was unloaded is skipped (test-crash-filter-dll.dll); a filter whose
// module hides or erases its PE headers is still called, without a fault.
#include "../../src/unified-20261011/common/crash-filter-chain.hpp"
#include <cstdio>
#include <string>

namespace cc = dlss5::crash_chain;
static int checks = 0;
#define CHECK(x) do { ++checks; if(!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)

static cc::Chain chain;
static std::string order;
static int reports = 0;
static LONG busy_verdict = -2;

static LONG WINAPI Bridge(EXCEPTION_POINTERS* ep) {
  switch (cc::Enter(chain)) {
  case cc::Entry::Reentry: return cc::CallReentry(chain, ep);
  case cc::Entry::Busy: return EXCEPTION_CONTINUE_SEARCH;
  case cc::Entry::First: cc::MarkCalled(chain, cc::Top(&Bridge)); break;
  }
  ++reports; order += 'B';
  return cc::CallFirst(chain, ep);
}
// G was on top before the bridge; it chains to nothing.
static LONG WINAPI G(EXCEPTION_POINTERS*) { order += 'G'; return EXCEPTION_EXECUTE_HANDLER; }
// X1 and X3 installed themselves above the bridge and chain back to it.
static LONG WINAPI X1(EXCEPTION_POINTERS* ep) { order += '1'; return Bridge(ep); }
static LONG WINAPI X3(EXCEPTION_POINTERS* ep) { order += '3'; return Bridge(ep); }
// X2 installed itself above X1 and chains to X1.
static LONG WINAPI X2(EXCEPTION_POINTERS* ep) { order += '2'; return X1(ep); }
// Calls its predecessor twice; the chain must still run each link once.
static LONG WINAPI Twice(EXCEPTION_POINTERS* ep) { order += 'T'; Bridge(ep); return Bridge(ep); }
// Crashes on another thread while this one is inside the chain.
static DWORD WINAPI Other(void*) { busy_verdict = Bridge(nullptr); return 0; }
static LONG WINAPI Blocking(EXCEPTION_POINTERS* ep) {
  order += 'K';
  HANDLE thread = CreateThread(nullptr, 0, &Other, nullptr, 0, nullptr);
  if (thread) { WaitForSingleObject(thread, INFINITE); CloseHandle(thread); }
  return Bridge(ep);
}

static int Run() {
  // No original filter: only the report.
  cc::Install(chain, nullptr);
  CHECK(Bridge(nullptr) == EXCEPTION_CONTINUE_SEARCH && order == "B" && reports == 1);
  CHECK(cc::Successor(chain) == nullptr);

  // Original filter only.
  order.clear(); reports = 0;
  cc::Install(chain, &G);
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "BG" && reports == 1);
  CHECK(cc::Successor(chain) == &G);

  // Retake 1 found X2 on top (X1 below it), retake 2 found X3. G is kept.
  order.clear(); reports = 0;
  CHECK(cc::Append(chain, &X2));
  CHECK(cc::Append(chain, &X3));
  CHECK(!cc::Append(chain, &X3));  // re-installed itself: one place only
  CHECK(!cc::Append(chain, &G));
  CHECK(chain.count == 2 && cc::Successor(chain) == &X3);
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER);
  CHECK(order == "B321G" && reports == 1);

  // The chain is released after a crash; a second one behaves the same.
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "B321G" && reports == 1);

  // A link that re-installed itself after its retake is on top and is called
  // by the system first; it chains to the bridge and must not run again.
  const LPTOP_LEVEL_EXCEPTION_FILTER saved_top = SetUnhandledExceptionFilter(&X3);
  order.clear(); reports = 0;
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, &X3));
  CHECK(X3(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "3BG" && reports == 1 && chain.owner == 0);
  CHECK(cc::Top(&Bridge) == &X3);  // reading the top leaves it in place
  // The same for a lower link: the newer ones still run, in order.
  SetUnhandledExceptionFilter(&X1);
  order.clear(); reports = 0;
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, &X1));
  CHECK(cc::Append(chain, &X3));
  CHECK(X1(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "1B3G" && reports == 1 && chain.owner == 0);
  // Next crash entered at the bridge again: every link runs once.
  SetUnhandledExceptionFilter(&Bridge);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "B31G" && reports == 1);
  SetUnhandledExceptionFilter(saved_top);

  // A filter that calls back twice does not run anything twice.
  order.clear(); reports = 0;
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, &Twice));
  CHECK(Bridge(nullptr) == EXCEPTION_CONTINUE_SEARCH);
  CHECK(order == "BTG" && reports == 1);  // second call-back finds the chain exhausted

  // Another thread crashing meanwhile is not chained and writes no report.
  order.clear(); reports = 0;
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, &Blocking));
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER);
  CHECK(busy_verdict == EXCEPTION_CONTINUE_SEARCH && order == "BKG" && reports == 1);
  CHECK(chain.owner == 0);

  // The chain holds at most kMaxRetakes replacements.
  cc::Install(chain, &G);
  static LPTOP_LEVEL_EXCEPTION_FILTER fakes[cc::kMaxRetakes + 1];
  for (LONG i = 0; i <= cc::kMaxRetakes; ++i)
    fakes[i] = reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(static_cast<uintptr_t>(0x1000 + i * 16));
  for (LONG i = 0; i < cc::kMaxRetakes; ++i) CHECK(cc::Append(chain, fakes[i]));
  CHECK(!cc::Append(chain, fakes[cc::kMaxRetakes]));
  CHECK(chain.count == cc::kMaxRetakes && chain.links[chain.count] == fakes[cc::kMaxRetakes - 1]);
  // None of the fake addresses is mapped: the chain falls back to G.
  CHECK(cc::Successor(chain) == &G);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "BG" && reports == 1);

  // A link inside a loaded DLL is called; once the DLL is unloaded it is skipped.
  HMODULE dll = LoadLibraryW(L"test-crash-filter-dll.dll");
  CHECK(dll != nullptr);
  auto* dll_filter = reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(GetProcAddress(dll, "DllFilter"));
  CHECK(dll_filter != nullptr);
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, dll_filter));
  CHECK(chain.images[1].base == dll && chain.images[1].identity && chain.images[1].size != 0);
  CHECK(cc::Successor(chain) == dll_filter);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == 7 && order == "B" && reports == 1);
  CHECK(FreeLibrary(dll) && GetModuleHandleW(L"test-crash-filter-dll.dll") == nullptr);
  CHECK(cc::Successor(chain) == &G);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "BG" && reports == 1 && chain.owner == 0);

  // An unloaded link between live ones: the others still run once each, in order.
  dll = LoadLibraryW(L"test-crash-filter-dll.dll");
  CHECK(dll != nullptr);
  dll_filter = reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(GetProcAddress(dll, "DllFilter"));
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, &X2));
  CHECK(cc::Append(chain, dll_filter));
  CHECK(cc::Append(chain, &X3));
  CHECK(FreeLibrary(dll) && GetModuleHandleW(L"test-crash-filter-dll.dll") == nullptr);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "B321G" && reports == 1);
  CHECK(cc::Successor(chain) == &X3);

  // Header page made inaccessible: recording and calling do not fault, and
  // the link is kept by its base address, also as the original filter.
  dll = LoadLibraryW(L"test-crash-filter-dll.dll");
  CHECK(dll != nullptr);
  dll_filter = reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(GetProcAddress(dll, "DllFilter"));
  CHECK(dll_filter != nullptr);
  DWORD protect = 0, ignored = 0;
  CHECK(VirtualProtect(dll, 1, PAGE_NOACCESS, &protect));
  cc::Install(chain, dll_filter);
  CHECK(chain.images[0].base == dll && !chain.images[0].identity);
  CHECK(cc::Successor(chain) == dll_filter);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == 7 && order == "B" && reports == 1);
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, dll_filter));
  CHECK(chain.images[1].base == dll && !chain.images[1].identity);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == 7 && order == "B" && reports == 1);
  CHECK(VirtualProtect(dll, 1, protect, &ignored));

  // Headers erased after the link was recorded: still called.
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, dll_filter));
  CHECK(chain.images[1].identity);
  CHECK(VirtualProtect(dll, 1, PAGE_READWRITE, &protect));
  auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(dll);
  const WORD magic = dos->e_magic;
  dos->e_magic = 0;
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == 7 && order == "B" && reports == 1);

  // Headers already erased when recorded, as the original filter: still called.
  cc::Install(chain, dll_filter);
  CHECK(chain.images[0].base == dll && !chain.images[0].identity);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == 7 && order == "B" && reports == 1);

  // Recorded without identity, then the DLL is unloaded: skipped.
  cc::Install(chain, &G);
  CHECK(cc::Append(chain, dll_filter));
  CHECK(!chain.images[1].identity);
  dos->e_magic = magic;
  CHECK(VirtualProtect(dll, 1, protect, &ignored));
  CHECK(FreeLibrary(dll) && GetModuleHandleW(L"test-crash-filter-dll.dll") == nullptr);
  CHECK(cc::Successor(chain) == &G);
  order.clear(); reports = 0;
  CHECK(Bridge(nullptr) == EXCEPTION_EXECUTE_HANDLER && order == "BG" && reports == 1 && chain.owner == 0);
  return 0;
}

int main() {
  const int result = Run();
  if (result == 0) std::printf("crash filter chain: %d checks passed\n", checks);
  return result;
}
