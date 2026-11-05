"""Create an isolated MFG diagnostic build; production source and game stay untouched."""
from pathlib import Path
import difflib
import hashlib
import json
import shutil

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "builds/unified-20261011/hook-cause-audit1"
if OUT.exists():
    raise RuntimeError("Preserve existing diagnostic build")
SOURCE = ROOT / "src/unified-20261011"
for parent in (OUT, SOURCE, *OUT.parents, *SOURCE.parents):
    if parent.exists() and (parent.is_symlink() or parent.is_junction()):
        raise RuntimeError(f"Linked path refused: {parent}")
OUT.mkdir()
snapshot = OUT / "source"
shutil.copytree(SOURCE / "mfg/addon", snapshot / "mfg/addon")
shutil.copytree(SOURCE / "common", snapshot / "common")
shutil.copytree(SOURCE / "mfg/tests", snapshot / "mfg/tests")
header = snapshot / "mfg/addon/ngx_hook.hpp"
original = header.read_text(encoding="utf-8-sig")
text = original


def replace(before, after):
    global text
    if text.count(before) != 1:
        raise RuntimeError(f"Expected one source anchor: {before[:80]}")
    text = text.replace(before, after)


replace("  DWORD thread_id = 0;\n};", """  DWORD thread_id = 0;
  uint64_t enlist_begin = 0, enlist_end = 0;
};

inline uint64_t AuditFileTime() {
  FILETIME value{};
  GetSystemTimePreciseAsFileTime(&value);
  return (static_cast<uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}""")
replace("inline bool OpenOtherThreads(std::vector<HANDLE>& threads, ThreadFailure* failure = nullptr) {",
        "inline bool OpenOtherThreads(std::vector<HANDLE>& threads, ThreadFailure* failure = nullptr,\n"
        "                             std::vector<DWORD>* known_ids = nullptr) {")
replace("if (thread != nullptr) threads.push_back(thread);", """if (thread != nullptr) {
        threads.push_back(thread);
        if (known_ids) known_ids->push_back(entry.th32ThreadID);
      }""")
replace("inline bool EnlistThreads(const std::vector<HANDLE>& threads, ThreadFailure* failure = nullptr) {",
        "inline bool EnlistThreads(const std::vector<HANDLE>& threads, ThreadFailure* failure = nullptr,\n"
        "                          const std::vector<DWORD>* known_ids = nullptr) {")
replace("  for (HANDLE thread : threads) {\n    const LONG result = DetourUpdateThread(thread);",
        "  size_t thread_index = 0;\n  for (HANDLE thread : threads) {\n"
        "    const auto enlist_begin = known_ids ? AuditFileTime() : 0;\n"
        "    const LONG result = DetourUpdateThread(thread);")
replace("      if (ok) SetThreadFailure(failure, \"enlist-thread\", result, GetThreadId(thread));\n      ok = false;\n    }\n  }",
        """      if (ok) {
        SetThreadFailure(failure, "enlist-thread", result,
          known_ids && thread_index < known_ids->size() ? (*known_ids)[thread_index] : GetThreadId(thread));
        if (failure && known_ids) {
          failure->enlist_begin = enlist_begin;
          failure->enlist_end = AuditFileTime();
        }
      }
      ok = false;
    }
    ++thread_index;
  }""")
helper = r'''
// Diagnostic only: called after transaction abort has resumed all threads and
// before the original handles are closed. It never changes the install result.
inline void LogThreadCause(const ThreadFailure& failure) {
  const DWORD id = failure.thread_id;
  const DWORD saved_error = GetLastError();
  HANDLE query = OpenThread(THREAD_QUERY_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION |
                              SYNCHRONIZE, FALSE, id);
  DWORD open_error = query ? NO_ERROR : GetLastError();
  if (!query) query = OpenThread(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, id);
  const DWORD limited_error = query ? NO_ERROR : GetLastError();
  DWORD exit_code = 0;
  BOOL exit_ok = FALSE;
  DWORD wait = WAIT_FAILED, owner = 0;
  FILETIME created{}, exited{}, kernel{}, user{};
  BOOL times_ok = FALSE;
  void* start = nullptr;
  LONG start_status = -1;
  WCHAR module_path[1024]{};
  if (query) {
    wait = WaitForSingleObject(query, 0);
    exit_ok = GetExitCodeThread(query, &exit_code);
    owner = GetProcessIdOfThread(query);
    times_ok = GetThreadTimes(query, &created, &exited, &kernel, &user);
    using QueryThread = LONG (WINAPI*)(HANDLE, ULONG, void*, ULONG, ULONG*);
    auto nt_query = reinterpret_cast<QueryThread>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),
                                                                 "NtQueryInformationThread"));
    if (nt_query) start_status = nt_query(query, 9, &start, sizeof(start), nullptr);
    if (start_status >= 0 && start) {
      MEMORY_BASIC_INFORMATION memory{};
      if (VirtualQuery(start, &memory, sizeof(memory)) && memory.Type == MEM_IMAGE)
        GetModuleFileNameW(static_cast<HMODULE>(memory.AllocationBase), module_path, 1024);
    }
    CloseHandle(query);
  }
  char utf8[4096]{};
  WideCharToMultiByte(CP_UTF8, 0, module_path, -1, utf8, sizeof(utf8), nullptr, nullptr);
  auto ticks = [](FILETIME t) { return (static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
  std::stringstream message;
  message << "mfgunlock::hook: cause-audit thread=" << id << " owner=" << owner
          << " enlist_begin_ft=" << failure.enlist_begin << " enlist_end_ft=" << failure.enlist_end
          << " full_query_error=" << open_error << " limited_query_error=" << limited_error
          << " wait=" << wait << " exit_ok=" << exit_ok << " exit_code=" << exit_code
          << " times_ok=" << times_ok << " created_ft=" << ticks(created) << " exited_ft=" << ticks(exited)
          << " start_status=" << start_status << " start=" << start << " start_module=" << utf8;
  reshade::log::message(reshade::log::level::warning, message.str().c_str());
  SetLastError(saved_error);
}
'''
replace("\n}  // namespace internal\n\n// Installs `hooks`", helper + "\n}  // namespace internal\n\n// Installs `hooks`")
replace("  internal::ThreadFailure failure;\n  if (!internal::OpenOtherThreads(threads, &failure))",
        "  internal::ThreadFailure failure;\n  std::vector<DWORD> known_ids;\n  if (!internal::OpenOtherThreads(threads, &failure, &known_ids))")
replace("  if (!internal::EnlistThreads(threads, &failure)) {\n    DetourTransactionAbort();\n    internal::CloseThreads(threads);",
        "  if (!internal::EnlistThreads(threads, &failure, &known_ids)) {\n    DetourTransactionAbort();\n"
        "    internal::LogThreadCause(failure);\n    internal::CloseThreads(threads);")
header.write_text(text, encoding="utf-8")
(OUT / "diagnostic-only.diff").write_text("".join(difflib.unified_diff(original.splitlines(True), text.splitlines(True),
    fromfile="baseline/ngx_hook.hpp", tofile="diagnostic/ngx_hook.hpp")), encoding="utf-8")
version = (ROOT / "evidence/unified-20261011/before-mfg-fragmentation1l/src/unified-20261011/mfg/version.rc").read_text(encoding="utf-8-sig")
version = version.replace("unified1j", "hook-cause-audit1").replace("0,11,2026,1011", "0,11,2026,1013")
(snapshot / "mfg/version.rc").write_text(version, encoding="utf-8")
inventory = [{"path": str(p.relative_to(snapshot)), "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
             for p in sorted(snapshot.rglob("*")) if p.is_file()]
(OUT / "source-manifest.json").write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8")
print(OUT)
