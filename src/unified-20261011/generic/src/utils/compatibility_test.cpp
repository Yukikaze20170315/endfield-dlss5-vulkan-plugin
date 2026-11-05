// Standalone shared-utility contract test; run with build-utils-compat.cmd.
#include <windows.h>
#define RESHADE_API_LIBRARY_EXPORT
#define BUILTIN_ADDON
#include "directx.hpp"
#include "vtable.hpp"

#ifdef UTILS_COMPAT_PEER
std::mutex* PeerTransactionMutex() {
  return &renodx::utils::vtable::TransactionMutex();
}
#else
#include <array>
#include <cstdio>
#include <cstdlib>
#include <thread>

extern std::mutex* PeerTransactionMutex();
extern "C" void ReShadeLogMessage(void*, int, const char* message) {
  std::puts(message);
}

static int checks = 0;
static void Check(bool result, const char* name) {
  if (!result) {
    std::fprintf(stderr, "FAIL: %s\n", name);
    std::exit(1);
  }
  ++checks;
}

static constexpr GUID kStreamlineBase = {
    0xADEC44E2, 0x61F0, 0x45C3, {0xAD, 0x9F, 0x1B, 0x37, 0x37, 0x92, 0x84, 0xFF}};

// A proxy owns its base. QueryInterface adds a reference to the returned face.
struct Object : IUnknown {
  ULONG refs = 1;
  IUnknown* reshade_base = nullptr;
  IUnknown* streamline_base = nullptr;
  bool fail_identity = false;
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** output) override {
    *output = nullptr;
    if (iid == __uuidof(IUnknown) && !fail_identity) *output = this;
    if (iid == __uuidof(renodx::utils::directx::ReShadeRetrieveBaseInterface)) {
      *output = reshade_base;
    }
    if (iid == kStreamlineBase) *output = streamline_base;
    if (*output == nullptr) return E_NOINTERFACE;
    static_cast<IUnknown*>(*output)->AddRef();
    return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
  ULONG STDMETHODCALLTYPE Release() override { return --refs; }
};

extern "C" __declspec(dllexport) __declspec(noinline) int CompatTarget(int value) {
  volatile int result = value;
  result = result + 7;
  return result;
}
using Function = int (*)(int);
static Function original = nullptr;
static int Replacement(int value) { return original(value) + 100; }

int main() {
  namespace dx = renodx::utils::directx;
  namespace vt = renodx::utils::vtable;
  Object native, other, proxy, streamline, wrapped_streamline, invalid;
  proxy.reshade_base = &native;
  streamline.streamline_base = &native;
  wrapped_streamline.reshade_base = &streamline;
  invalid.fail_identity = true;
  Check(dx::NativeIdentity(nullptr) == nullptr, "null identity");
  Check(dx::NativeIdentity(&native) == &native, "native identity");
  Check(dx::SameNativeObject(&proxy, &native), "ReShade/native equality");
  Check(!dx::SameNativeObject(&proxy, &other), "different objects");
  Check(!dx::SameNativeObject(nullptr, nullptr), "null is not an object");
  Check(!dx::SameNativeObject(&invalid, &invalid), "failed QI is not equality");
  Check(dx::NativeIdentity(&wrapped_streamline) == &streamline, "preserve Streamline face");
  Check(!dx::SameNativeObject(&wrapped_streamline, &native), "Streamline base stays distinct");
  Check(native.refs == 1 && proxy.refs == 1 && streamline.refs == 1
            && wrapped_streamline.refs == 1 && other.refs == 1 && invalid.refs == 1,
        "balanced COM references");
  Check(PeerTransactionMutex() == &vt::TransactionMutex(), "one lock across translation units");

  std::atomic<bool> holding{false}, release{false};
  std::thread external([&] {
    Check(DetourTransactionBegin() == NO_ERROR, "external transaction begins");
    holding.store(true);
    while (!release.load()) std::this_thread::yield();
    Check(DetourTransactionCommit() == NO_ERROR, "contention does not abort external owner");
  });
  while (!holding.load()) std::this_thread::yield();
  const auto before = vt::transaction_contended.load();
  {
    std::lock_guard<std::mutex> lock(vt::TransactionMutex());
    Check(!vt::BeginTransaction(), "external contention returns failure");
  }
  Check(vt::transaction_contended.load() == before + 1, "contention recorded");
  release.store(true);
  external.join();
  std::atomic<int> succeeded{0};
  auto install = [&] {
    for (int i = 0; i < 100; ++i) {
      std::lock_guard<std::mutex> lock(vt::TransactionMutex());
      if (vt::BeginTransaction() && DetourTransactionCommit() == NO_ERROR) ++succeeded;
    }
  };
  std::thread first(install), second(install);
  first.join();
  second.join();
  Check(succeeded == 200, "cooperating threads serialize real Detours transactions");
  Check(vt::transaction_contended.load() == before + 1, "cooperating threads do not contend");

  const std::array<vt::HookItem, 1> hooks = {{
      {"CompatTarget", reinterpret_cast<void**>(&original), reinterpret_cast<void*>(&Replacement)}}};
  auto entry = reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(nullptr), "CompatTarget"));
  Check(entry != nullptr && entry(5) == 12, "unhooked target");
  Check(vt::Hook(GetModuleHandleW(nullptr), hooks), "real export hook installs");
  Check(entry(5) == 112 && original(5) == 12, "replacement and trampoline execute");
  vt::Unhook(GetModuleHandleW(nullptr), hooks);
  Check(original == nullptr && entry(5) == 12, "detach restores target and clears slot");
  std::printf("PASS: %d checks\n", checks);
}
#endif
