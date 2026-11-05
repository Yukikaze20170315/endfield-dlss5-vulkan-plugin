// Reuse the production-policy fixture's fake SL functions and ReShade logger.
#define main native_policy_suite
#include "../../builds/unified-20261011/hook-cause-audit1/source/mfg/tests/native-policy.cpp"
#undef main

static DWORD WINAPI ExitWorker(void* event) {
  return WaitForSingleObject(static_cast<HANDLE>(event), 5000) == WAIT_OBJECT_0 ? 0 : 1;
}

int main() {
  Reset();
  fc::g_on_dlssg_function_resolved = nullptr;
  static int lookups = 0;
  fi::g_real_get_feature_function = [](sl::Feature, const char*, void*& function) {
    ++lookups;
    function = reinterpret_cast<void*>(&FakeSet);
    return sl::Result::eOk;
  };
  void* cached = nullptr;
  sl::DLSSGOptions options{};
  options.mode = sl::DLSSGMode::eOn;
  options.numFramesToGenerate = 3;
  const sl::ViewportHandle viewport(1);
  auto caller = [&] {
    // The same non-null cache rule used by SL_FEATURE_FUN_IMPORT_STATIC.
    if (!cached) {
      const auto result = fi::HookedGetFeatureFunction(sl::kFeatureDLSS_G,
                                                       "slDLSSGSetOptions", cached);
      if (result != sl::Result::eOk) return result;
    }
    return reinterpret_cast<fi::SetOptionsFn>(cached)(viewport, options);
  };
  CHECK(DetourTransactionBegin() == NO_ERROR);
  CHECK(caller() == sl::Result::eOk);
  CHECK(DetourTransactionAbort() == NO_ERROR);
  CHECK(lookups == 1 && !fi::g_set_options_entry.installed.load());
  CHECK(cached == reinterpret_cast<void*>(&FakeSet));
  for (const unsigned multiplier : {2u, 6u, 2u, 6u}) {
    fc::g_force_multiplier = multiplier;
    fc::ApplyLiveOptionsOnPresent();
    CHECK(caller() == sl::Result::eOk);
    CHECK(forwarded.back().numFramesToGenerate == 3);
  }
  CHECK(lookups == 1);
  std::printf("cached native entry: %d lookup; UI changed four times; all calls remain native 4x\n", lookups);
  CHECK(fi::HookedGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGSetOptions", cached) == sl::Result::eOk);
  CHECK(fi::g_set_options_entry.installed.load());
  CHECK(caller() == sl::Result::eOk && forwarded.back().numFramesToGenerate == 5);
  mfgunlock::hook::UninstallAddress(fi::g_set_options_entry);
  std::puts("explicit second lookup after the transient condition ends: 6x works");

  HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  DWORD id = 0;
  HANDLE worker = CreateThread(nullptr, 0, ExitWorker, event, 0, &id);
  CHECK(event && worker);
  std::vector<HANDLE> threads;
  std::vector<DWORD> ids;
  mfgunlock::hook::internal::ThreadFailure failure;
  CHECK(mfgunlock::hook::internal::OpenOtherThreads(threads, &failure, &ids));
  CHECK(SetEvent(event));
  CHECK(WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0);
  CHECK(DetourTransactionBegin() == NO_ERROR);
  CHECK(!mfgunlock::hook::internal::EnlistThreads(threads, &failure, &ids));
  CHECK(DetourTransactionAbort() == NO_ERROR);
  CHECK(failure.thread_id == id && failure.error == ERROR_ACCESS_DENIED);
  captured_logs.clear();
  mfgunlock::hook::internal::LogThreadCause(failure);
  CHECK(captured_logs.size() == 1);
  CHECK(captured_logs[0].find("wait=0 exit_ok=1 exit_code=0") != std::string::npos);
  std::puts(captured_logs[0].c_str());
  mfgunlock::hook::internal::CloseThreads(threads);
  CloseHandle(worker);
  CloseHandle(event);
  std::printf("PASS: %d checks; cached-lookup failure chain and post-abort thread identity diagnostics\n", checks);
}
