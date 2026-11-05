// Tests execute the production entry points against a fake Streamline runtime.
// No DLL injection, GPU workload, driver settings or game files are involved.
#include "../addon/addon.cpp"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(void*, uint32_t) { return true; }
extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(void*) {}
static std::vector<std::string> captured_logs;
extern "C" __declspec(dllexport) void ReShadeLogMessage(void*, int, const char* message) {
  captured_logs.emplace_back(message ? message : "");
}
static int config_reads = 0;
static int config_writes = 0;
// Optional in-memory config, enabled only by the legacy-pacing migration
// block at the end; every other check keeps the original "nothing saved" API.
static bool fake_config_store = false;
static std::map<std::string, std::string> fake_config;
static std::string FakeConfigKey(const char* section, const char* key) {
  return std::string(section ? section : "") + "|" + (key ? key : "");
}
extern "C" __declspec(dllexport) bool ReShadeGetConfigValue(void*, reshade::api::effect_runtime*, const char* section, const char* key, char* value, size_t* size) {
  ++config_reads;
  if (!fake_config_store || size == nullptr) return false;
  const auto found = fake_config.find(FakeConfigKey(section, key));
  if (found == fake_config.end()) return false;
  if (value != nullptr && *size != 0) {
    const size_t copied = (std::min)(*size - 1, found->second.size());
    std::memcpy(value, found->second.data(), copied);
    value[copied] = '\0';
  }
  *size = found->second.size() + 1;
  return true;
}
extern "C" __declspec(dllexport) void ReShadeSetConfigValue(void*, reshade::api::effect_runtime*, const char* section, const char* key, const char* value) {
  ++config_writes;
  if (fake_config_store) fake_config[FakeConfigKey(section, key)] = value ? value : "";
}
extern "C" __declspec(dllexport) void ReShadeSetConfigArray(void*, reshade::api::effect_runtime*, const char*, const char*, const char*, size_t) { ++config_writes; }

namespace fc = mfgunlock::framecount;
namespace fi = mfgunlock::framecount::internal;
namespace arch = mfgunlock::architecture;
static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while(0)
static std::vector<sl::DLSSGOptions> forwarded;
static std::vector<sl::Result> replies;
static int forbidden_calls = 0;
static bool ready = true;
__declspec(noinline) static sl::Result FakeSet(const sl::ViewportHandle&, const sl::DLSSGOptions& value) {
  forwarded.push_back(value);
  const auto index = forwarded.size() - 1;
  return index < replies.size() ? replies[index] : sl::Result::eOk;
}
static sl::Result FakeGet(const sl::ViewportHandle&, sl::DLSSGState& state, const sl::DLSSGOptions*) {
  state.numFramesToGenerateMax = 3;
  state.numFramesActuallyPresented = 7;
  return sl::Result::eOk;
}
static void Reset() {
  forwarded.clear(); replies.clear(); ready = true;
  arch::Configure(mfgunlock::Architecture::kBlackwell);
  mfgunlock::g_enabled = true;
  fc::g_force_multiplier = 6;
  fc::g_streamline_plugin_seen = true;
  fc::g_streamline_max_generated = 5;
  fi::g_real_set_options = FakeSet;
  fi::g_real_get_state = FakeGet;
  fc::g_native_provider_ready = [] { return ready; };
  fc::g_ensure_pacing = [] { ++forbidden_calls; };
  fc::g_pacing_ready = [] { ++forbidden_calls; return false; };
  fc::g_multi_frame_ready = [] { ++forbidden_calls; return false; };
  fc::g_ui_composition_enabled = true;
  fc::g_ui_candidate_injection_enabled = true;
  fc::g_hdr_state_seen = true;
  fc::g_hdr_active = false;
}
int main(int argc, char** argv) {
  using A = mfgunlock::Architecture;
  if (argc == 3 && std::strcmp(argv[1], "--provider-lifetime") == 0) {
    // Map the exact provider without executing its DllMain or resolving GPU
    // dependencies. Exercise real loader unload/reload and production pinning.
    arch::Configure(A::kAda); g_enabled=true;
    auto module=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    CHECK(module!=nullptr);
    CHECK(mfgunlock::provider::QualifiedNativeInventory(module));
    auto* gate=reinterpret_cast<unsigned char*>(module)+0x35e50;
    CHECK(*gate==0xb0);
    DWORD protect=0,ignored=0;
    CHECK(VirtualProtect(gate,1,PAGE_EXECUTE_READWRITE,&protect));
    *gate=0x90;
    CHECK(VirtualProtect(gate,1,protect,&ignored));
    CHECK(FreeLibrary(module));
    module=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    CHECK(module!=nullptr);
    gate=reinterpret_cast<unsigned char*>(module)+0x35e50;
    CHECK(*gate==0xb0); // Unload discards the previous in-memory patch.
    CHECK(RetainNativeProvider(module));
    CHECK(RetainNativeProvider(module));
    CHECK(g_retained_native_providers.size()==1);
    PatchArchGatesInModule(module);
    CHECK(*gate==0x90);
    CHECK(FreeLibrary(module));
    const auto again=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    CHECK(again==module);
    CHECK(*gate==0x90); // Published cache and mapped bytes now share a lifetime.
    PatchArchGatesInModule(again);
    CHECK(g_gate_modules.size()==1 && g_gate_sites.size()==2);
    CHECK(FreeLibrary(again));
    std::printf("PASS: %d assertions; actual provider unload resets patches, retained mapping preserves both gates. No GPU execution.\n",checks);
    return 0;
  }
  if (argc == 2 && std::strcmp(argv[1], "--startup-policy") == 0) {
    // Run this host under the actual executable basename. Any accidental
    // re-enabling of DllMain loading reaches the fake config API and fails.
    for (const auto value : {A::kAuto, A::kBlackwell, A::kAda, A::kAmpere, A::kTuring}) {
      arch::Configure(value);
      EnsureEarlyLoadConfig();
      CHECK(config_reads == 0 && config_writes == 0);
      CHECK(!g_early_load_restart_required.load());
    }
    std::printf("PASS: %d assertions; Endfield never auto-enables DllMain loading across GPU profiles.\n", checks);
    return 0;
  }
  CHECK(arch::Detect(0x10de, 0x1b0, 2) == A::kBlackwell);
  for(const auto value : {A::kBlackwell,A::kAda,A::kAmpere,A::kTuring,A::kUnknown,A::kAuto}) {
    arch::Configure(value); g_legacy_configured=false; g_force_flip_meter_off=false;
    CHECK(LegacyRequested());  // software pacing is the unconfigured default on every GPU
    g_legacy_configured=true;
    CHECK(!LegacyRequested());
    g_force_flip_meter_off=true;
    CHECK(LegacyRequested());
  }
  g_legacy_configured=false; g_force_flip_meter_off=false;
  CHECK(arch::Detect(0x10de, 0x190, 1) == A::kAda);
  CHECK(arch::Detect(0x10de, 0x170, 1) == A::kAmpere);
  CHECK(arch::Detect(0x10de, 0x160, 1) == A::kTuring);
  CHECK(arch::Detect(0x10de, 0x170, 0) == A::kUnknown);
  CHECK(arch::Detect(0x1002, 0x1b0, 2) == A::kUnknown);
  CHECK(arch::Detect(0x10de, 0x999, 1) == A::kUnknown);
  CHECK(arch::Parse(" Blackwell ") == A::kBlackwell);
  for(const auto value:{A::kAuto,A::kUnknown,A::kBlackwell,A::kAda,A::kAmpere,A::kTuring}) {
    arch::Configure(value); g_legacy_configured=false; g_force_flip_meter_off=false;
    CHECK(LegacyRequested());
    g_legacy_configured=true; CHECK(!LegacyRequested());
    g_force_flip_meter_off=true; CHECK(LegacyRequested());
  }
  // The no-patch boundaries below are checked with Legacy explicitly off (the
  // unconfigured default is on, which would let the flip-metering scan run).
  g_legacy_configured=true; g_force_flip_meter_off=false;
  Reset();
  CHECK(arch::SelectedProfile() == &arch::kBlackwell);
  CHECK(arch::ActiveProfile() == nullptr && !arch::NeedsBridge());
  CHECK(!fc::internal::CandidateInjectionRequested());
  auto invalid_module = reinterpret_cast<HMODULE>(1);
  PatchArchGatesInModule(invalid_module);
  PatchMidpointInModule(invalid_module);
  PatchQualityProviderInModule(invalid_module);
  PatchValidatedWarpInModule(invalid_module);
  PatchFrameCountCeiling(invalid_module);
  CHECK(!TryPatchFlipMeteringInModule(invalid_module));
  CHECK(!mfgunlock::provider::PrepareProvider(invalid_module));
  CHECK(g_gate_modules.empty() && g_midpoint_modules.empty() && g_blackwell_temporal_modules.empty());
  sl::ViewportHandle viewport(0);
  sl::DLSSGOptions options{};
  options.mode = sl::DLSSGMode::eOn;
  options.numFramesToGenerate = 1;
  options.colorWidth = 5120;
  options.flags = static_cast<sl::DLSSGFlags>(0x40);
  CHECK(!fi::ShouldRequestUiComposition(options));
  for (unsigned version = 1; version <= 5; ++version) {
    for (unsigned multiplier = 2; multiplier <= 6; ++multiplier) {
      Reset(); options.structVersion = version; fc::g_force_multiplier = multiplier;
      auto original = options;
      CHECK(fi::HookedSetOptions(viewport, options) == sl::Result::eOk);
      CHECK(forwarded.size() == 1 && forwarded[0].numFramesToGenerate == multiplier - 1);
      CHECK(forwarded[0].structVersion == version && forwarded[0].flags == options.flags);
      CHECK(forwarded[0].colorWidth == 5120);
      CHECK(forwarded[0].enableUserInterfaceRecomposition == options.enableUserInterfaceRecomposition);
      CHECK(std::memcmp(&original, &options, sizeof(options)) == 0);
    }
  }
  options.structVersion = 5;
  for (const auto mode : {sl::DLSSGMode::eOff, sl::DLSSGMode::eDynamic}) {
    Reset(); options.mode = mode;
    CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eOk);
    CHECK(forwarded.size() == 1 && forwarded[0].mode == mode && forwarded[0].numFramesToGenerate == 1);
  }
  options.mode = sl::DLSSGMode::eOn;
  for (int scenario = 0; scenario < 6; ++scenario) {
    Reset(); options.structVersion = 5;
    if (scenario == 0) options.structVersion = 99;
    if (scenario == 1) fc::g_streamline_max_generated = 0;
    if (scenario == 2) fc::g_streamline_max_generated = 3;
    if (scenario == 3) ready = false;
    if (scenario == 4) mfgunlock::g_enabled = false;
    if (scenario == 5) fc::g_force_multiplier = 0;
    CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eOk);
    CHECK(forwarded.size() == 1 && forwarded[0].numFramesToGenerate == 1);
  }
  Reset(); options.structVersion = 5;
  replies = {sl::Result::eErrorNotInitialized, sl::Result::eErrorNotInitialized, sl::Result::eOk};
  CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eOk);
  CHECK(forwarded.size() == 3 && forwarded[0].numFramesToGenerate == 5 && forwarded[1].numFramesToGenerate == 5 && forwarded[2].numFramesToGenerate == 1);
  CHECK(fc::g_fixed_override_status == fc::FixedOverrideStatus::kRejectedNativeFallback);
  Reset();
  replies = {sl::Result::eErrorNotInitialized, sl::Result::eOk};
  CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eOk);
  CHECK(forwarded.size() == 2 && fc::g_fixed_override_status == fc::FixedOverrideStatus::kAppliedAfterRetry);
  sl::DLSSGState state{};
  CHECK(fi::HookedGetState(viewport, state, nullptr) == sl::Result::eOk);
  CHECK(state.numFramesToGenerateMax == 3 && state.numFramesActuallyPresented == 7);
  Reset();
  replies = {sl::Result::eErrorNotInitialized, sl::Result::eErrorNotInitialized, sl::Result::eErrorNotInitialized};
  CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eErrorNotInitialized);
  CHECK(fc::g_fixed_override_status == fc::FixedOverrideStatus::kFallbackRejected);
  // The 1r acceptance policy must forward the warning without retry/fallback.
  Reset();
  replies = {sl::Result::eWarnOutOfVRAM};
  CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eWarnOutOfVRAM);
  CHECK(forwarded.size() == 1 && forwarded[0].numFramesToGenerate == 5);
  CHECK(fc::g_fixed_override_status == fc::FixedOverrideStatus::kApplied);
  Reset();
  replies = {sl::Result::eErrorNotInitialized, sl::Result::eWarnOutOfVRAM};
  CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eWarnOutOfVRAM);
  CHECK(forwarded.size() == 2 && fc::g_fixed_override_status == fc::FixedOverrideStatus::kAppliedAfterRetry);
  // Exercise the UI composition path used by Ada/Ampere separately from
  // native Blackwell. A warning must keep the UI request intact; real errors
  // must still fall back, and neither path may mutate caller-owned options.
  for (const auto gpu : {A::kAda, A::kAmpere, A::kTuring}) {
    Reset(); arch::Configure(gpu);
    options.structVersion = 5;
    options.mode = sl::DLSSGMode::eOn;
    auto original = options;
    CHECK(fi::ShouldRequestUiComposition(options));
    replies = {sl::Result::eWarnOutOfVRAM};
    CHECK(fi::ForwardSetOptions(viewport, options) == sl::Result::eWarnOutOfVRAM);
    CHECK(forwarded.size() == 1);
    CHECK(forwarded[0].enableUserInterfaceRecomposition == sl::Boolean::eTrue);
    CHECK(fc::g_ui_composition_applied && !fc::g_ui_composition_fell_back);
    CHECK(fc::g_last_accepted_generated == options.numFramesToGenerate);
    CHECK(std::memcmp(&original, &options, sizeof(options)) == 0);

    forwarded.clear(); replies = {sl::Result::eErrorNotInitialized, sl::Result::eWarnOutOfVRAM};
    CHECK(fi::ForwardSetOptions(viewport, options) == sl::Result::eWarnOutOfVRAM);
    CHECK(forwarded.size() == 2);
    CHECK(forwarded[1].enableUserInterfaceRecomposition == options.enableUserInterfaceRecomposition);
    CHECK(!fc::g_ui_composition_applied && fc::g_ui_composition_fell_back);
    CHECK(std::memcmp(&original, &options, sizeof(options)) == 0);

    forwarded.clear(); replies = {sl::Result::eErrorNotInitialized, sl::Result::eErrorNotInitialized};
    CHECK(fi::ForwardSetOptions(viewport, options) == sl::Result::eErrorNotInitialized);
    CHECK(forwarded.size() == 2 && fc::g_last_accepted_generated == mfgunlock::diagnostic::kUnknown32);
    CHECK(!fc::g_ui_composition_applied);
  }
  Reset();
  const auto old_epoch = fc::g_options_cache_epoch.load();
  OnDlssgPluginUnloaded(invalid_module);
  CHECK(fc::g_options_cache_epoch > old_epoch && fc::g_streamline_max_generated == 0);
  CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eOk);
  CHECK(forwarded.size() == 1 && forwarded[0].numFramesToGenerate == 1);
  Reset();
  CHECK(fi::SetOptionsImpl(viewport, options, 0) == sl::Result::eOk);
  CHECK(forwarded[0].numFramesToGenerate == 5);
  sl::ResourceTag tags[1]{};
  CHECK(fi::FilterUiTags(viewport, tags, 1, nullptr, [&](const sl::ResourceTag* ptr, uint32_t count) {
    CHECK(ptr == tags && count == 1); return sl::Result::eOk;
  }) == sl::Result::eOk);
  // A genuine v1-sized input adjacent to an inaccessible page catches reads
  // of v2-v5 fields that a full-sized object with version=1 would not expose.
  Reset();
  auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  CHECK(memory != nullptr);
  DWORD protection = 0;
  CHECK(VirtualProtect(memory + 4096, 4096, PAGE_NOACCESS, &protection));
  constexpr size_t v1_bytes = offsetof(sl::DLSSGOptions, bReserved15);
  options.structVersion = 1;
  auto* old_options = reinterpret_cast<sl::DLSSGOptions*>(memory + 4096 - v1_bytes);
  std::memcpy(old_options, &options, v1_bytes);
  CHECK(fi::HookedSetOptions(viewport, *old_options) == sl::Result::eOk);
  CHECK(forwarded.size() == 1 && forwarded[0].numFramesToGenerate == 5);
  CHECK(std::memcmp(old_options, &options, v1_bytes) == 0);
  CHECK(VirtualFree(memory, 0, MEM_RELEASE));
  CHECK(!NativeBlackwellProviderReady());
  CHECK(forbidden_calls == 0);
  for (const auto value : {A::kAda, A::kAmpere, A::kTuring}) {
    arch::Configure(value);
    CHECK(arch::ActiveProfile() == arch::GetProfile(value));
  }
  // No prior gateway lookup or bridge observer. A public feature lookup must
  // discover the ceiling before the first multiplier request is processed.
  Reset(); options.structVersion = 5;
  fc::g_streamline_plugin_seen = false;
  fc::g_streamline_max_generated = 0;
  static int discoveries = 0;
  fc::g_on_dlssg_function_resolved = [](void* entry) {
    CHECK(entry == reinterpret_cast<void*>(&FakeSet));
    ++discoveries;
    fc::g_streamline_plugin_seen = true;
    fc::g_streamline_max_generated = 5;
  };
  fi::g_real_get_feature_function = [](sl::Feature, const char*, void*& entry) {
    entry = reinterpret_cast<void*>(&FakeSet); return sl::Result::eOk;
  };
  void* entry = nullptr;
  CHECK(fi::HookedGetFeatureFunction(sl::kFeatureDLSS, "slDLSSGSetOptions", entry) == sl::Result::eOk);
  CHECK(discoveries == 0);
  CHECK(fi::HookedGetFeatureFunction(sl::kFeatureDLSS_G, "slDLSSGSetOptions", entry) == sl::Result::eOk);
  CHECK(discoveries == 1 && fi::g_set_options_entry.installed.load());
  CHECK(reinterpret_cast<fi::SetOptionsFn>(entry)(viewport, options) == sl::Result::eOk);
  CHECK(forwarded.size() == 1 && forwarded[0].numFramesToGenerate == 5);
  // Exercise the real address-hook implementation, including safe refusal and
  // exact diagnostics; a reported failure must not disturb an installed hook.
  auto has_log = [](const char* text) {
    for (const auto& line : captured_logs) if (line.find(text) != std::string::npos) return true;
    return false;
  };
  mfgunlock::hook::AddressHook diagnostic;
  captured_logs.clear();
  CHECK(!mfgunlock::hook::InstallAddress(diagnostic, nullptr,
        reinterpret_cast<void*>(&fi::HookedSetOptions), "test-null"));
  CHECK(has_log("label=test-null stage=arguments error=87"));
  auto* private_code = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
  CHECK(private_code != nullptr);
  captured_logs.clear();
  CHECK(!mfgunlock::hook::InstallAddress(diagnostic, private_code,
        reinterpret_cast<void*>(&fi::HookedSetOptions), "test-private"));
  CHECK(has_log("label=test-private stage=target-not-image"));
  CHECK(!diagnostic.installed.load() && diagnostic.trampoline.load() == nullptr);
  CHECK(VirtualFree(private_code, 0, MEM_RELEASE));
  captured_logs.clear();
  CHECK(DetourTransactionBegin() == NO_ERROR);
  CHECK(!mfgunlock::hook::InstallAddress(diagnostic, reinterpret_cast<void*>(&FakeGet),
        reinterpret_cast<void*>(&fi::HookedGetState), "test-busy"));
  CHECK(DetourTransactionAbort() == NO_ERROR);
  CHECK(has_log("label=test-busy stage=transaction-begin error=4317"));
  CHECK(!diagnostic.installed.load() && diagnostic.trampoline.load() == nullptr);
  captured_logs.clear();
  CHECK(mfgunlock::hook::InstallAddress(fi::g_set_options_entry, reinterpret_cast<void*>(&FakeSet),
        reinterpret_cast<void*>(&fi::HookedSetOptions), "test-same"));
  CHECK(captured_logs.empty());
  CHECK(!mfgunlock::hook::InstallAddress(fi::g_set_options_entry, reinterpret_cast<void*>(&FakeGet),
        reinterpret_cast<void*>(&fi::HookedSetOptions), "test-other"));
  CHECK(has_log("label=test-other stage=existing-target"));
  CHECK(fi::g_set_options_entry.Targets(reinterpret_cast<void*>(&FakeSet)));
  mfgunlock::hook::UninstallAddress(fi::g_set_options_entry);
  CHECK(!fi::g_set_options_entry.installed.load());
  forwarded.clear();
  CHECK(reinterpret_cast<fi::SetOptionsFn>(entry)(viewport, options) == sl::Result::eOk);
  CHECK(forwarded.size() == 1 && forwarded[0].numFramesToGenerate == options.numFramesToGenerate);

  // Legacy software flip pacing default (2026-10-31): LoadConfig resets every
  // older configuration to on exactly once, then honours the saved choice.
  {
    const auto legacy_key = FakeConfigKey(kConfigSection, "ForceFlipMeteringOff");
    const auto revision_key = FakeConfigKey(kConfigSection, kLegacyPacingRevisionKey);
    fake_config_store = true;
    fake_config.clear(); g_legacy_configured=false; g_force_flip_meter_off=false;
    LoadConfig();  // fresh install, nothing saved
    CHECK(g_legacy_configured.load() && g_force_flip_meter_off.load() && LegacyRequested());
    CHECK(fake_config[legacy_key] == "1" && fake_config[revision_key] == "1");
    fake_config.clear(); fake_config[legacy_key] = "0";
    g_legacy_configured=false; g_force_flip_meter_off=false;
    LoadConfig();  // saved 0 under the old per-architecture default
    CHECK(LegacyRequested() && fake_config[legacy_key] == "1" && fake_config[revision_key] == "1");
    fake_config[legacy_key] = "0";  // the user turns it off after the migration
    g_legacy_configured=false; g_force_flip_meter_off=true;
    LoadConfig();
    CHECK(!LegacyRequested() && fake_config[legacy_key] == "0");
    fake_config.clear(); fake_config[legacy_key] = "1";
    g_legacy_configured=false; g_force_flip_meter_off=false;
    LoadConfig();  // already on: only the revision marker is added
    CHECK(LegacyRequested() && fake_config[legacy_key] == "1" && fake_config[revision_key] == "1");
    fake_config.clear(); fake_config_store = false;
    g_legacy_configured=false; g_force_flip_meter_off=true;
  }
  std::printf("PASS: %d assertions; Blackwell multiplier ABI, fallback, no-patch boundaries, legacy profiles and legacy pacing default.\n", checks);
}
