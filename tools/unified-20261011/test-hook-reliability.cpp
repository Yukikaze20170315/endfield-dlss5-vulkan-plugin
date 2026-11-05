// Exercise the production thread-enlistment and gateway-install recovery code in
// an isolated process. Only controlled worker threads and in-process function
// addresses are touched; no game handles, GPU work or driver state are involved.
//
// Covered here:
//   1. a thread which exited after enumeration is skipped, and the install
//      proceeds instead of aborting;
//   2. a live thread the caller cannot suspend still aborts the transaction;
//   3. an unrunnable thread whose exit code equals STILL_ACTIVE is treated as
//      unknown, never as exited;
//   4. the SetOptions gateway retry disarms on success and stops after a bounded
//      number of attempts on a target that cannot be hooked.
#include "../../builds/unified-20261011/mfg-hook-reliability1n/source/mfg/addon/framecount.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// The production sources log through ReShade's exported function pointers. This
// fixture is a plain executable, so it provides the same exports the plugin
// defines; without them every log call would jump through a null pointer.
extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(void*, uint32_t) { return true; }
extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(void*) {}
extern "C" __declspec(dllexport) void ReShadeLogMessage(void*, int, const char* message) {
  static std::vector<std::string> lines;
  lines.emplace_back(message ? message : "");
}
extern "C" __declspec(dllexport) bool ReShadeGetConfigValue(void*, reshade::api::effect_runtime*,
                                                           const char*, const char*, char*, size_t*) {
  return false;
}
extern "C" __declspec(dllexport) void ReShadeSetConfigValue(void*, reshade::api::effect_runtime*,
                                                            const char*, const char*, const char*) {}
extern "C" __declspec(dllexport) void ReShadeSetConfigArray(void*, reshade::api::effect_runtime*,
                                                            const char*, const char*, const char*, size_t) {}

namespace fci = mfgunlock::framecount::internal;

static int checks = 0;

static void Require(bool value, const char* label) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s error=%lu\n", label, GetLastError());
    std::exit(1);
  }
  ++checks;
}

static DWORD WINAPI Worker(void* stop) {
  return WaitForSingleObject(static_cast<HANDLE>(stop), 5000) == WAIT_OBJECT_0 ? 0 : 1;
}

static DWORD WINAPI ShortLived(void*) { return 259; }

// 1 + 3 (part): a thread which has certainly finished is skipped, and its exit
// is proven from the exit signal rather than guessed from an error code.
static void ExitedThreadIsSkipped() {
  HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  DWORD id = 0;
  HANDLE worker = CreateThread(nullptr, 0, Worker, stop, 0, &id);
  Require(stop != nullptr && worker != nullptr, "create worker");
  HANDLE probe = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT,
                            FALSE, id);
  Require(probe != nullptr, "production rights");
  Require(mfgunlock::hook::internal::VerifyThreadExited(id) ==
              mfgunlock::hook::internal::ThreadLifetime::kAlive,
          "live worker reports alive");

  std::vector<HANDLE> threads{probe};
  std::vector<DWORD> ids{id};
  mfgunlock::hook::internal::ThreadFailure failure;
  Require(DetourTransactionBegin() == NO_ERROR, "begin transaction");
  Require(mfgunlock::hook::internal::EnlistThreads(threads, &failure, &ids), "alive worker enlists");
  Require(DetourTransactionAbort() == NO_ERROR, "abort");
  Require(failure.exited_skipped == 0, "no skip while alive");

  Require(SetEvent(stop) != FALSE, "release worker");
  Require(WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0, "worker exited");
  Require(mfgunlock::hook::internal::VerifyThreadExited(id) ==
              mfgunlock::hook::internal::ThreadLifetime::kExited,
          "exited worker reports exited");

  // The handle stays valid after termination; this is exactly the state which
  // produced enlist-thread/ERROR_ACCESS_DENIED in the game.
  std::vector<HANDLE> ended{probe};
  std::vector<DWORD> ended_ids{id};
  mfgunlock::hook::internal::ThreadFailure ended_failure;
  Require(DetourTransactionBegin() == NO_ERROR, "begin second transaction");
  const bool enlisted = mfgunlock::hook::internal::EnlistThreads(ended, &ended_failure, &ended_ids);
  Require(DetourTransactionAbort() == NO_ERROR, "abort second transaction");
  Require(enlisted, "a thread that exited is skipped instead of failing the transaction");
  Require(ended_failure.exited_skipped == 1, "the skip is counted for the log");

  // GetThreadId on the enumeration handle would answer zero here, which is why
  // the snapshot id has to be carried along.
  Require(GetThreadId(probe) == 0, "the enumeration handle cannot be queried");

  CloseHandle(probe);
  CloseHandle(worker);
  CloseHandle(stop);
}

// 3 (part): an exit code equal to STILL_ACTIVE is not proof that the thread has
// left, so the decision must abstain instead of treating it as exited.
static void StillActiveExitCodeIsUnknown() {
  DWORD id = 0;
  HANDLE thread = CreateThread(nullptr, 0, ShortLived, nullptr, 0, &id);
  Require(thread != nullptr, "create short-lived worker");
  Require(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0, "short-lived worker exited");
  DWORD exit_code = 0;
  Require(GetExitCodeThread(thread, &exit_code) != FALSE, "read exit code");
  Require(exit_code == STILL_ACTIVE, "the fixture really exits with STILL_ACTIVE");
  Require(mfgunlock::hook::internal::VerifyThreadExited(id) !=
              mfgunlock::hook::internal::ThreadLifetime::kExited,
          "an exit code of STILL_ACTIVE is never treated as exited");
  CloseHandle(thread);
}

// 2: the original protection is unchanged. A live thread whose handle lacks
// suspend rights cannot be parked, so the transaction must still abort.
static void LiveUnsuspendableThreadStillAborts() {
  HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  DWORD id = 0;
  HANDLE worker = CreateThread(nullptr, 0, Worker, stop, 0, &id);
  Require(stop != nullptr && worker != nullptr, "create worker");
  HANDLE limited = OpenThread(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, id);
  Require(limited != nullptr, "limited rights handle");

  std::vector<HANDLE> threads{limited};
  std::vector<DWORD> ids{id};
  mfgunlock::hook::internal::ThreadFailure failure;
  Require(DetourTransactionBegin() == NO_ERROR, "begin transaction");
  const bool enlisted = mfgunlock::hook::internal::EnlistThreads(threads, &failure, &ids);
  Require(DetourTransactionAbort() == NO_ERROR, "abort");
  Require(mfgunlock::hook::internal::VerifyThreadExited(id) ==
              mfgunlock::hook::internal::ThreadLifetime::kAlive,
          "the live thread is confirmed alive");
  Require(!enlisted, "a live thread that cannot be suspended still fails the transaction");
  Require(failure.exited_skipped == 0, "no skip for a live thread");
  Require(std::strcmp(failure.stage, "enlist-thread") == 0, "failure stage is reported");
  Require(failure.thread_id == id, "failure names the snapshot id");

  Require(SetEvent(stop) != FALSE, "release worker");
  Require(WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0, "worker completion");
  CloseHandle(limited);
  CloseHandle(worker);
  CloseHandle(stop);
}

__declspec(noinline) static sl::Result FakeGateway(const sl::ViewportHandle&, const sl::DLSSGOptions&) {
  return sl::Result::eOk;
}

// 4: the retry path is bounded, disarms on success, and never runs once the
// gateway is installed.
static void RetryIsBoundedAndDisarms() {
  Require(!fci::g_set_options_entry.installed.load(std::memory_order_acquire), "no entry installed yet");
  Require(!fci::g_set_options_retry_armed.load(std::memory_order_acquire), "retry starts disarmed");
  fci::RetrySetOptionsInstallOnPresent();
  Require(!fci::g_set_options_retry_armed.load(std::memory_order_acquire), "disarmed retry is a no-op");

  // A target outside any module image cannot be hooked, so the retry must give
  // up after a bounded number of attempts instead of retrying forever.
  char* not_an_image = new char[64];
  fci::ArmSetOptionsRetry(not_an_image);
  Require(fci::g_set_options_retry_armed.load(std::memory_order_acquire), "bogus target arms the retry");
  std::puts("  marker: bogus loop"); std::fflush(stdout);
  for (uint32_t attempt = 0; attempt < 4096; ++attempt) fci::RetrySetOptionsInstallOnPresent();
  std::puts("  marker: bogus loop done"); std::fflush(stdout);
  Require(!fci::g_set_options_retry_armed.load(std::memory_order_acquire),
          "the retry disarms after its attempt limit");
  Require(fci::g_set_options_retry_attempts.load(std::memory_order_acquire) <= fci::kSetOptionsRetryLimit,
          "attempts never exceed the limit");

  // A real entry installs, after which the retry reports success and disarms.
  fci::ArmSetOptionsRetry(reinterpret_cast<void*>(&FakeGateway));
  std::puts("  marker: real loop"); std::fflush(stdout);
  for (uint32_t attempt = 0; attempt < 4096; ++attempt) fci::RetrySetOptionsInstallOnPresent();
  std::puts("  marker: real loop done"); std::fflush(stdout);
  Require(fci::g_set_options_entry.installed.load(std::memory_order_acquire),
          "present retry installs the gateway hook");
  Require(!fci::g_set_options_retry_armed.load(std::memory_order_acquire), "installed hook disarms the retry");
  Require(fci::RealSetOptions() != nullptr, "the trampoline is published");
  std::puts("  marker: uninstall"); std::fflush(stdout);
  mfgunlock::hook::UninstallAddress(fci::g_set_options_entry);
  std::puts("  marker: uninstall done"); std::fflush(stdout);
  Require(!fci::g_set_options_entry.installed.load(std::memory_order_acquire), "hook removed");
}

int main() {
  std::puts("stage: exited");
  std::fflush(stdout);
  ExitedThreadIsSkipped();
  std::puts("stage: still-active");
  std::fflush(stdout);
  StillActiveExitCodeIsUnknown();
  std::puts("stage: live-unsuspendable");
  std::fflush(stdout);
  LiveUnsuspendableThreadStillAborts();
  std::puts("stage: retry");
  std::fflush(stdout);
  RetryIsBoundedAndDisarms();
  std::printf("PASS: %d checks; exited threads are skipped, live threads still abort, "
              "and the gateway retry is bounded.\n", checks);
  return 0;
}
