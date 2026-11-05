#include <thread>
#include <vector>
#include <dxgi1_6.h>
#pragma comment(lib,"dxgi.lib")
#include "../../src/unified-20261011/bridge/src/dlss5-bridge.cpp"
static unsigned device_calls=0,last_extensions=0;
static std::vector<std::string> panel_calls;
static std::string panel_click;
static uint32_t hook_current=2,hook_selected=2,hook_writes=0;
static bool hook_stored=false;
static bool hook_write_failure=false;
static const void* ngx_received_info=nullptr;
static bool ngx_arguments_ok=false;
static NVSDK_NGX_Result FakeNgxInit(unsigned long long app,const wchar_t* path,
    ID3D12Device* device,int version,const void* info) {
  ngx_received_info=info;
  ngx_arguments_ok=app==0x1000000ULL && wcscmp(path,L"original-data-path")==0 &&
      device==reinterpret_cast<ID3D12Device*>(0x1234) && version>=0x13 && version<=0x16;
  return NGX_SUCCESS;
}
static NVSDK_NGX_Result FakeNgxProject(const char* project,int engine,const char* engine_version,
    const wchar_t* path,ID3D12Device* device,int version,const void* info) {
  const auto result=FakeNgxInit(0x1000000ULL,path,device,version,info);
  ngx_arguments_ok=ngx_arguments_ok && strcmp(project,"fixture-project")==0 &&
      engine==0 && strcmp(engine_version,"1.0")==0;
  return result;
}
static bool CheckNgxInitPaths(bool redirected) {
  for(int version=0x13;version<=0x16;++version) for(int project=0;project<2;++project) {
    DWORD code=99;
    const auto result=project ? SafeInitProjectID(&FakeNgxProject,"fixture-project",L"original-data-path",
        reinterpret_cast<ID3D12Device*>(0x1234),version,&code) :
        SafeInitExt(&FakeNgxInit,0x1000000ULL,L"original-data-path",
        reinterpret_cast<ID3D12Device*>(0x1234),version,&code);
    if(result!=NGX_SUCCESS || code || !ngx_arguments_ok || bool(ngx_received_info)!=redirected) return false;
    if(redirected) {
      const auto* info=static_cast<const BridgeNgxRuntimeInfo*>(ngx_received_info);
      if(info->paths.length!=1 || !info->paths.path || !info->paths.path[0] ||
          wcscmp(info->paths.path[0],L"fixture-path") || info->internal ||
          info->logging.callback || info->logging.minimum_level || info->logging.disable_other_sinks) return false;
    }
  }
  return true;
}
static bool FakeHookMode(uint32_t operation,uint32_t value,dlss5::hook_mode::Snapshot* result) {
  return dlss5::hook_mode::Control(operation,value,hook_current,result,
    [result] { result->stored=hook_stored ? 1u : 0u; return hook_selected; },
    [](uint32_t next) { if(!hook_write_failure) {hook_selected=next; hook_stored=true; ++hook_writes;} });
}
static NVSDK_NGX_Result fake_evaluate_result=NGX_SUCCESS;
static NVSDK_NGX_Result FakeEvaluate(void*,const NVSDK_NGX_Handle*,const NVSDK_NGX_Parameter*,void*) {
  return fake_evaluate_result;
}
static uint32_t FakeCreateDevice(void*,const void* raw,const void*,void** output) {
  ++device_calls;
  last_extensions=static_cast<const FgrDeviceCreateInfo*>(raw)->enabledExtensionCount;
  if(output) *output=reinterpret_cast<void*>(0x1234);
  return kFgrErrorInitFailed;
}
int main(int argc,char** argv) {
  g_self=GetModuleHandleW(nullptr);
  // Separate processes exercise the real startup-only INI reader and its cache.
  if(argc==3 && strcmp(argv[1],"--throttle-config")==0) {
    const int expected=atoi(argv[2]);
    const int actual=PresentAdapterRelayThrottle();
    printf("Throttle startup: expected=%d actual=%d\n",expected,actual);
    return actual==expected ? 0 : 1;
  }
  InitializeCriticalSection(&g_log_cs); InitializeCriticalSection(&g_hook_cs);
  deployment::game=dlss5::durable::GameDirectory();
  wcscpy_s(g_sld_dir,L"fixture-path"); g_sld_paths[0]=g_sld_dir; g_sld_complete=true;
  SldInstallResolver();
  if(g_sld_hook_state!=1) { printf("resolver install failed %d\n",g_sld_hook_status); return 1; }
  std::atomic_uint32_t errors{0},calls{0};
  if(!CheckNgxInitPaths(false)) ++errors;
  if(std::filesystem::exists(deployment::game/L"vk-present-adapter.ini")) {
    printf("Fresh-install fixture must not contain adapter configuration.\n"); return 1;
  }
  if(!PresentationAdapterEnabled() || !PresentAdapterFgInput() || !PresentAdapterFollow() ||
     PresentAdapterPipeline()!=2 || !PresentAdapterRelayImport() || PresentAdapterRelayThrottle()!=2) ++errors;
  std::vector<std::thread> threads;
  for(unsigned t=0;t<8;++t) threads.emplace_back([&]{
    for(unsigned i=0;i<250;++i) {
      HMODULE mod=LoadLibraryW((deployment::game/L"sl.interposer.dll").c_str());
      if(!mod) { ++errors; continue; }
      auto init=reinterpret_cast<PFN_SldSlInit>(GetProcAddress(mod,"slInit"));
      SldPreferencesV1 pref{}; pref.flags=0x85; pref.renderAPI=2;
      auto before=pref;
      if(!init || init(&pref,123)!=0 || memcmp(&before,&pref,sizeof(pref))) ++errors;
      else ++calls;
      FreeLibrary(mod);
      HMODULE unrelated=LoadLibraryW(L"version.dll");
      if(!unrelated || !GetProcAddress(unrelated,"GetFileVersionInfoW")) ++errors;
      if(unrelated) FreeLibrary(unrelated);
    }
  });
  for(auto& thread:threads) thread.join();
  if(!CheckNgxInitPaths(true)) ++errors;
  g_sld_complete=false;
  auto mod=LoadLibraryW((deployment::game/L"sl.interposer.dll").c_str());
  auto original=reinterpret_cast<PFN_SldSlInit>(GetProcAddress(mod,"slInit"));
  SldPreferencesV1 pref{};pref.flags=0x85;pref.renderAPI=2;
  if(!original || original(&pref,123)!=9) ++errors;
  g_sld_complete=true;
  auto hooked=reinterpret_cast<PFN_SldSlInit>(GetProcAddress(mod,"slInit"));
  pref.structVersion=2;
  if(hooked(&pref,123)!=9) ++errors;
  if(!CheckNgxInitPaths(false)) ++errors;
  pref.structVersion=0; pref.numFeaturesToLoad=1; pref.featuresToLoad=nullptr;
  if(hooked(&pref,123)!=9) ++errors;
  pref.numFeaturesToLoad=0; pref.structType[0]=99;
  if(hooked(&pref,123)!=9) ++errors;
  if(!CheckNgxInitPaths(false)) ++errors;
  pref.structType[0]=0;
  if(hooked(&pref,999)!=-2 || !CheckNgxInitPaths(false)) ++errors;
  if(hooked(&pref,123)!=0 || !CheckNgxInitPaths(true)) ++errors;
  g_sld_complete=false;
  if(SldSlInit(&pref,123)!=9 || !CheckNgxInitPaths(false)) ++errors;
  g_sld_complete=true;
  if(hooked(&pref,123)!=0 || !CheckNgxInitPaths(true)) ++errors;
  g_fgr_create_device_hook.original=reinterpret_cast<void*>(&FakeCreateDevice);
  FgrDeviceCreateInfo create{};create.sType=kFgrStDeviceCreateInfo;
  void* device=nullptr;
  if(FgrCreateDeviceDetour(nullptr,&create,nullptr,&device)!=kFgrErrorInitFailed || device_calls!=1 || device || last_extensions!=1) ++errors;
  if(FgrCreateDeviceDetour(nullptr,&create,nullptr,&device)!=kFgrErrorInitFailed || device_calls!=2 || device || last_extensions!=0) ++errors;
  Hook evaluation{}; evaluation.original=reinterpret_cast<void*>(&FakeEvaluate);
  auto sr=reinterpret_cast<NVSDK_NGX_Handle*>(0x120000);
  auto fg=reinterpret_cast<NVSDK_NGX_Handle*>(0x120100);
  g_feature_registry.Created(FeatureApi::vulkan,sr,1,nullptr);
  g_feature_registry.Created(FeatureApi::vulkan,fg,11,nullptr);
  g_nest=1; // Exercise the real early-forward path without a Generic runtime.
  const auto evaluate=[&](NVSDK_NGX_Handle* handle) {
    return ForwardVkEvaluate(evaluation,"fixture",reinterpret_cast<void*>(0x110000),handle,
        reinterpret_cast<NVSDK_NGX_Parameter*>(0x130000),nullptr);
  };
  fake_evaluate_result=static_cast<NVSDK_NGX_Result>(0xbad00001u);
  if(evaluate(sr)!=fake_evaluate_result || g_deployment_sr_success) ++errors;
  fake_evaluate_result=NGX_SUCCESS;
  if(evaluate(fg)!=NGX_SUCCESS || g_deployment_sr_success) ++errors;
  if(evaluate(reinterpret_cast<NVSDK_NGX_Handle*>(0x140000))!=NGX_SUCCESS || g_deployment_sr_success) ++errors;
  if(evaluate(sr)!=NGX_SUCCESS || !g_deployment_sr_success || g_eval_count) ++errors;
  g_nest=0;
  PanelImGui panel{};
  panel.SameLine=+[](float,float){};
  panel.BeginChild=+[](const char*,const PanelVec2&,int,int) { return true; };
  panel.EndChild=+[](){};
  panel.GetTextLineHeightWithSpacing=+[]() { return 17.0f; };
  panel.TextUnformatted=+[](const char* value,const char*) { panel_calls.emplace_back(value); };
  panel.SeparatorText=+[](const char* value) { panel_calls.emplace_back(value); };
  panel.RadioButton2=+[](const char* label,int* value,int selection) {
    panel_calls.emplace_back(label);
    if(panel_click==label) { *value=selection; panel_click.clear(); return true; }
    return false;
  };
  panel.Checkbox=+[](const char* label,bool* value) {
    panel_calls.emplace_back(label);
    if(panel_click==label) { *value=!*value; panel_click.clear(); return true; }
    return false;
  };
  // The anomaly-log modal: OpenPopup marks it open, BeginPopupModal reports
  // that state, CloseCurrentPopup closes it, Button answers a scripted click.
  static bool popup_open=false;
  panel.PushTextWrapPos=+[](float){};
  panel.PopTextWrapPos=+[](){};
  panel.Button=+[](const char* label,const PanelVec2&) {
    panel_calls.emplace_back(label);
    if(panel_click==label) { panel_click.clear(); return true; }
    return false;
  };
  panel.OpenPopup=+[](const char* id,int) { panel_calls.emplace_back(std::string("open:")+id); popup_open=true; };
  panel.BeginPopupModal=+[](const char* title,bool*,int) { panel_calls.emplace_back(title); return popup_open; };
  panel.EndPopup=+[](){};
  panel.CloseCurrentPopup=+[](){ popup_open=false; };
  g_panel_gui=&panel;
  deployment::dir=deployment::game/L"panel-fixture";
  std::filesystem::create_directories(deployment::dir);
  deployment::ready=true; deployment::safe=false; deployment::status="状态正常 / Healthy";
  g_panel_language.loaded=true; g_panel_language.chinese=true; g_panel_advanced=false;
  const auto saw=[](const char* text) { for(const auto& line:panel_calls) if(line.find(text)!=std::string::npos) return true; return false; };
  const auto before=deployment::state;
  g_generic_hook_mode_control.store(&FakeHookMode);
  OnPanelDraw(nullptr);
  if(panel_calls.empty() || panel_calls[0]!="English##bridge_en" || !saw("状态正常") || saw(" / Healthy") || saw("CUDA") || saw("Driver settings")) ++errors;
  if(!saw("NR 钩子模式") || !saw("hook-mode1") || saw("确认选择并同步驱动") || hook_writes) ++errors;
  panel_calls.clear(); panel_click="2：仅 NGX（Generic 默认）##hook-mode2"; OnPanelDraw(nullptr);
  if(hook_writes!=1 || !hook_stored || hook_selected!=2 || hook_current!=2) ++errors;
  hook_writes=0;
  // Transient notes from the sections below the notice box are shown on the next frame.
  panel_calls.clear(); panel_click="1：NGX + Streamline（本套件呈现 NR）##hook-mode1"; OnPanelDraw(nullptr); OnPanelDraw(nullptr);
  if(hook_current!=2 || hook_selected!=1 || hook_writes!=1 || !saw("正常退出并重启") || GenericCurrentHookMode(L"unused.ini",0)!=2) ++errors;
  panel_calls.clear(); panel_click="0：停用 NR 钩子##hook-mode0"; OnPanelDraw(nullptr);
  if(hook_selected!=0 || hook_current!=2 || hook_writes!=2) ++errors;
  hook_write_failure=true;
  panel_calls.clear(); panel_click="1：NGX + Streamline（本套件呈现 NR）##hook-mode1"; OnPanelDraw(nullptr); OnPanelDraw(nullptr);
  if(hook_selected!=0 || hook_writes!=2 || !saw("未能确认配置写入")) ++errors;
  hook_write_failure=false;
  panel_calls.clear(); panel_click="2：仅 NGX（Generic 默认）##hook-mode2"; OnPanelDraw(nullptr);
  if(hook_selected!=2 || hook_writes!=3) ++errors;
  panel_calls.clear(); panel_click="English##bridge_en"; OnPanelDraw(nullptr);
  if(!saw("Healthy") || saw("状态正常") || saw("CUDA")) ++errors;
  if(!saw("NR hook mode") || !saw("2: NGX only") || saw("Active this session") || saw("Confirm selection and synchronize")) ++errors;
  // Removed descriptions stay removed; the VRAM budget is an on/off pair.
  if(saw("Current SL") || saw("Recovery") || saw("Purpose:") || !saw("##vram-budget-on") || !saw("##vram-budget-off") ||
     !saw("restarting the PC")) ++errors;
  panel_calls.clear(); panel_click="Advanced settings###bridge_advanced"; OnPanelDraw(nullptr);
  if(!saw("Early CUDA") || !saw("Driver settings") || std::memcmp(&before,&deployment::state,sizeof(before))) ++errors;
  // Anomaly log: last advanced item; turning full mode on needs the modal's
  // acknowledgement and confirm button, turning it off is immediate.
  {
    deployment::writable=true;
    const auto mode_file=deployment::dir/L"event-log-mode.txt";
    std::error_code ec; std::filesystem::remove(mode_file,ec);
    const auto read_mode=[&]{ std::string s; FILE* f=nullptr; if(_wfopen_s(&f,mode_file.c_str(),L"rb")==0 && f){ char b[16]{}; fread(b,1,15,f); fclose(f); s=b; } return s; };
    if(!saw("Anomaly log") || !saw("bounded (default)") || panel_calls.back().find("Turn on the full anomaly log?")==std::string::npos) ++errors;
    if(!saw("Full anomaly log (one full line per occurrence)###event-log-full")) ++errors;
    panel_calls.clear(); panel_click="Full anomaly log (one full line per occurrence)###event-log-full"; OnPanelDraw(nullptr);
    if(!popup_open || g_events.Full() || !saw("open:###dlss5-event-log-confirm")) ++errors;
    if(!saw("Warning: full mode writes every anomaly") || !saw("Turn on (tick the box above first)###event-log-yes")) ++errors;
    panel_calls.clear(); panel_click="Turn on (tick the box above first)###event-log-yes"; OnPanelDraw(nullptr);
    if(g_events.Full() || !popup_open || !read_mode().empty()) ++errors;
    panel_calls.clear(); panel_click="Cancel###event-log-no"; OnPanelDraw(nullptr);
    if(g_events.Full() || popup_open) ++errors;
    panel_calls.clear(); panel_click="Full anomaly log (one full line per occurrence)###event-log-full"; OnPanelDraw(nullptr);
    panel_calls.clear(); panel_click="I understand the disk-use risk above###event-log-ack"; OnPanelDraw(nullptr);
    if(g_events.Full() || !saw("Turn on###event-log-yes")) ++errors;
    panel_calls.clear(); panel_click="Turn on###event-log-yes"; OnPanelDraw(nullptr);
    if(!g_events.Full() || popup_open || read_mode()!="full=1\n") ++errors;
    panel_calls.clear(); OnPanelDraw(nullptr);
    if(!saw("full (every occurrence)") || !saw("Full anomaly log on")) ++errors;
    panel_calls.clear(); panel_click="Full anomaly log (one full line per occurrence)###event-log-full"; OnPanelDraw(nullptr);
    if(g_events.Full() || popup_open || read_mode()!="full=0\n") ++errors;
    deployment::writable=false;
    if(errors) printf("anomaly-log panel checks failed (errors=%u)\n",errors.load());
  }
  // Real overlay callback: failed package validation does not save; a valid
  // selection saves on the radio click. The pending event blocks maintenance
  // dispatch, so this test never calls a real driver-maintenance process.
  deployment::state.selected=0; deployment::current=0; deployment::sl_valid=false;
  deployment::writable=true;
  panel_calls.clear(); panel_click="2.14.1 + DLSS-G 310.9.1##deploy-sl1"; OnPanelDraw(nullptr);
  if(deployment::state.selected!=0 || hook_writes!=3) ++errors;
  deployment::sl_valid=true;
  g_driver_maintenance=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  if(!g_driver_maintenance) return 1;
  panel_calls.clear(); panel_click="2.14.1 + DLSS-G 310.9.1##deploy-sl1"; OnPanelDraw(nullptr); OnPanelDraw(nullptr);
  if(deployment::state.selected!=1 || !saw("restart the game to apply") || hook_writes!=3) ++errors;
  if(!CheckNgxInitPaths(true)) ++errors;
  const auto selected_generation=deployment::state.generation;
  panel_calls.clear(); panel_click="2.14.1 + DLSS-G 310.9.1##deploy-sl1"; OnPanelDraw(nullptr);
  if(deployment::state.generation!=selected_generation) ++errors;
  panel_calls.clear(); panel_click="Cancel scheduled change"; OnPanelDraw(nullptr);
  if(deployment::state.selected!=0 || hook_writes!=3) ++errors;
  if(!CheckNgxInitPaths(true)) ++errors;
  CloseHandle(g_driver_maintenance); g_driver_maintenance=nullptr;
  hook_current=1; hook_selected=0;
  if(GenericCurrentHookMode(L"unused.ini",0)!=1) ++errors;
  hook_current=0;
  if(GenericCurrentHookMode(L"unused.ini",1)!=0) ++errors;
  g_generic_hook_mode_control.store(nullptr);
  deployment::ready=false;
  // Read-only hardware probe: the NR selector must match DXGI adapter LUIDs,
  // not the order returned by NVAPI. No D3D device or driver setting is changed.
  IDXGIFactory1* factory=nullptr;
  unsigned matched_devices=0;
  if(NvAPI_Initialize()!=NVAPI_OK || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) ++errors;
  if(factory) {
    for(UINT index=0;;++index) {
      IDXGIAdapter1* adapter=nullptr;
      if(factory->EnumAdapters1(index,&adapter)==DXGI_ERROR_NOT_FOUND) break;
      if(!adapter) { ++errors; break; }
      DXGI_ADAPTER_DESC1 desc{};
      const auto hr=adapter->GetDesc1(&desc); adapter->Release();
      if(FAILED(hr)) { ++errors; continue; }
      const auto arch=deployment::ArchitectureForLuid(desc.AdapterLuid);
      if(desc.VendorId==0x10de && !(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) {
        // Indirect display adapters can share NVIDIA's vendor ID but have no
        // physical NVAPI node. They must remain unmapped rather than use GPU 0.
        if(arch && !deployment::SupportedArchitecture(arch)) ++errors;
        if(arch) ++matched_devices;
        printf("Read-only adapter LUID=%08x:%08x arch=0x%x\n",static_cast<unsigned>(desc.AdapterLuid.HighPart),desc.AdapterLuid.LowPart,arch);
      } else if(arch) ++errors;
    }
    factory->Release();
  }
  if(!matched_devices || deployment::ArchitectureForLuid(LUID{0xffffffffu,-1})) ++errors;
  printf("NGX init path: Ext/ProjectID SDK 0x13..0x16; default, accepted/rejected redirect, failed init, incomplete payload, pending selection/cancellation; data path and logging unchanged.\n");
  printf("Panel callbacks: basic EnableHooks 0/1/2 selection, deferred runtime changes, failed writes, current-mode queries; single-click SL save, invalid package rejection, repeat-click no-op and cancellation; language and advanced disclosure.\n");
  printf("%s: %u concurrent resolver/SL calls; %u errors; incomplete-package/unknown ABI pass-through, no reused Vulkan layer chain and successful SR health witness checked.\n",errors?"FAIL":"PASS",calls.load(),errors.load());
  return errors?1:0;
}
