// Exercise the production thread-enlistment code in an isolated process.
// The controlled worker is the only target; no game handles or GPU APIs are used.
#include "../../builds/unified-20261011/mfg-hook-reliability1n/source/mfg/addon/ngx_hook.hpp"
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* label) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s error=%lu\n", label, GetLastError());
    std::exit(1);
  }
}

static DWORD WINAPI Worker(void* stop) {
  return WaitForSingleObject(static_cast<HANDLE>(stop), 5000) == WAIT_OBJECT_0 ? 0 : 1;
}

static void Run(bool exit_before_enlist) {
  HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  DWORD id = 0;
  HANDLE original = CreateThread(nullptr, 0, Worker, stop, 0, &id);
  Require(stop && original, "create worker");
  HANDLE same_rights = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                                  THREAD_SET_CONTEXT, FALSE, id);
  Require(same_rights != nullptr, "production access rights");
  SetLastError(NO_ERROR);
  const DWORD queried_id = GetThreadId(same_rights);
  const DWORD id_error = GetLastError();
  Require(GetThreadId(original) == id, "worker identity with query rights");
  Require(queried_id == 0 && id_error == ERROR_ACCESS_DENIED,
          "missing query rights return thread zero even while alive");

  std::vector<HANDLE> handles;
  std::vector<DWORD> known_ids;
  mfgunlock::hook::internal::ThreadFailure failure;
  Require(mfgunlock::hook::internal::OpenOtherThreads(handles, &failure, &known_ids),
          "production enumeration");
  Require(known_ids.size() == handles.size(), "snapshot ids travel with the handles");
  Require(!handles.empty(), "snapshot includes worker");
  if (exit_before_enlist) {
    Require(SetEvent(stop) != FALSE, "release worker");
    Require(WaitForSingleObject(original, 5000) == WAIT_OBJECT_0, "worker exited after enumeration");
  }
  DWORD exit_code = 0;
  Require(GetExitCodeThread(original, &exit_code) != FALSE, "query worker state");
  Require(exit_code == (exit_before_enlist ? 0u : STILL_ACTIVE), "controlled lifetime");

  Require(DetourTransactionBegin() == NO_ERROR, "begin transaction");
  const bool enlisted = mfgunlock::hook::internal::EnlistThreads(handles, &failure, &known_ids);
  Require(DetourTransactionAbort() == NO_ERROR, "abort and resume enlisted threads");
  mfgunlock::hook::internal::CloseThreads(handles);
  // A thread which exited between enumeration and registration is skipped, so
  // the transaction survives in both cases. The reason differs, and the
  // distinction is what keeps the original protection for live threads.
  Require(enlisted, "an exited thread no longer aborts the transaction");
  Require(failure.exited_skipped == (exit_before_enlist ? 1u : 0u),
          "only a confirmed exit is skipped");
  if (exit_before_enlist) {
    Require(failure.error == NO_ERROR && failure.thread_id == 0,
            "the benign case is no longer recorded as a failure");
  }
  std::printf("worker=%lu lifetime=%s enlisted=%d skipped=%u stage=%s error=%lu logged_tid=%lu "
              "GetThreadId_without_query=%lu query_error=%lu\n",
              id, exit_before_enlist ? "exited-after-open" : "alive", enlisted,
              failure.exited_skipped, failure.stage, failure.error, failure.thread_id,
              queried_id, id_error);
  Require(SetEvent(stop) != FALSE, "release alive worker");
  Require(WaitForSingleObject(original, 5000) == WAIT_OBJECT_0, "worker completion");
  CloseHandle(same_rights);
  CloseHandle(original);
  CloseHandle(stop);
}

int main() {
  Run(false);
  Run(true);
  std::puts("PASS: controlled thread exit reproduces enlist-thread/error5/thread0; "
            "this is a mechanism test, not proof of the game's failing thread identity.");
}
