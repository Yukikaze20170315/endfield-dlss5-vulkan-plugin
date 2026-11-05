// Endfield deployment state. Initialization is called by AddonInit, never by
// a DLL notification. GPU work and game files are not changed by this module.
#include "../../common/drs.hpp"
#include "../../common/payload.hpp"
#include "../../common/deployment-state.hpp"
#include "../../common/vram-budget.hpp"
#include "gpu-residency.h"
#include <mutex>

namespace deployment {
namespace fs = std::filesystem;
using namespace dlss5;
using State=dlss5::deployment_policy::State;
static State state;
static fs::path game,dir;
static std::mutex mutex;
static std::atomic_bool ready{false},safe{true};
// Set when Initialize gave up for good (no game path); ready stays false.
static std::atomic_bool init_failed{false};
static bool writable=false,sl_valid=false,nr_valid=false,redirected=false,load_failed=false;
static uint32_t architecture=0,current=0,current_early=0,current_nr=0;
static bool any_supported_gpu=false,early_cuda_needed=false;
static std::atomic_bool driver_sync_requested{false};
static void SynchronizeStartupDriver();
static void LoadEventLogMode();
static std::string status,drs_snapshot,sl_reject,interposer_hash;
// The routine "this launch is healthy" status; the panel does not show it.
static constexpr char kStatusHealthy[]="本次启动已通过健康点 / Startup health confirmed.";
static std::wstring nr_path;
static std::string nr_label;
static std::atomic_uint64_t last_present{0},first_present{0};
static HANDLE crash_file=INVALID_HANDLE_VALUE;
static bool Save(State next) {
    Log("[action] phase=state_request origin=deployment selected=%u->%u confirmed=%u->%u generation=%llu",
        state.selected,next.selected,state.confirmed,next.confirmed,static_cast<unsigned long long>(state.generation));
    next.generation=state.generation+1;
    if(!writable || !durable::Save(dir/L"bridge.state",next)) {
        status="状态不可写，本次暂停部署干预 / Cannot save deployment state.";
        safe=true; return false;
    }
    state=next; return true;
}
static uint64_t Fingerprint() {
    std::string s=durable::Sha256(game/L"sl.interposer.dll");
    for(const wchar_t* name : {L"Endfield.exe",L"GameAssembly.dll"}) {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if(GetFileAttributesExW((game/name).c_str(),GetFileExInfoStandard,&data))
        {
            s.append(reinterpret_cast<const char*>(&data.ftLastWriteTime),sizeof(data.ftLastWriteTime));
            s+=std::to_string(data.nFileSizeHigh)+":"+std::to_string(data.nFileSizeLow);
        }
    }
    NvU32 driver=0; NvAPI_ShortString branch{};
    if(NvAPI_SYS_GetDriverAndBranchVersion(&driver,branch)==NVAPI_OK) s+=std::to_string(driver);
    s+=std::to_string(architecture);
    wchar_t build[64]{}; DWORD bytes=sizeof(build),ubr=0,ubr_bytes=sizeof(ubr);
    RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"CurrentBuildNumber",RRF_RT_REG_SZ,nullptr,build,&bytes);
    RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"UBR",RRF_RT_REG_DWORD,nullptr,&ubr,&ubr_bytes);
    s.append(reinterpret_cast<const char*>(build),wcslen(build)*sizeof(wchar_t)); s+=std::to_string(ubr);
    return durable::Checksum(s.data(),s.size());
}
static bool SupportedArchitecture(uint32_t arch) {
    return arch==0x160 || arch==0x170 || arch==0x190 || arch==0x1b0;
}
static void DetectGpu() {
    if(NvAPI_Initialize()!=NVAPI_OK) return;
    NvPhysicalGpuHandle gpu[NVAPI_MAX_PHYSICAL_GPUS]{}; NvU32 count=0;
    if(NvAPI_EnumPhysicalGPUs(gpu,&count)!=NVAPI_OK) return;
    bool mixed=false;
    for(NvU32 i=0;i<count;++i) {
        NV_GPU_ARCH_INFO info{}; info.version=NV_GPU_ARCH_INFO_VER;
        if(NvAPI_GPU_GetArchInfo(gpu[i],&info)!=NVAPI_OK) { mixed=true; continue; }
        if(i && architecture!=info.architecture) mixed=true;
        architecture=info.architecture;
        any_supported_gpu=any_supported_gpu || SupportedArchitecture(info.architecture);
        early_cuda_needed=early_cuda_needed || info.architecture==0x160 || info.architecture==0x170;
    }
    if(mixed) architecture=0;
}
static uint32_t ArchitectureForLuid(const LUID& wanted) {
    NvPhysicalGpuHandle gpus[NVAPI_MAX_PHYSICAL_GPUS]{}; NvU32 count=0;
    if(NvAPI_EnumPhysicalGPUs(gpus,&count)!=NVAPI_OK) return 0;
    for(NvU32 i=0;i<count;++i) {
        NvLogicalGpuHandle logical=nullptr;
        if(NvAPI_GetLogicalGPUFromPhysicalGPU(gpus[i],&logical)!=NVAPI_OK) continue;
        LUID actual{}; NV_LOGICAL_GPU_DATA data{}; data.version=NV_LOGICAL_GPU_DATA_VER; data.pOSAdapterId=&actual;
        if(NvAPI_GPU_GetLogicalGpuInfo(logical,&data)!=NVAPI_OK ||
           actual.HighPart!=wanted.HighPart || actual.LowPart!=wanted.LowPart) continue;
        // Linked nodes must share a supported mapping. Never pick GPU zero.
        uint32_t matched=0;
        if(!data.physicalGpuCount || data.physicalGpuCount>NVAPI_MAX_PHYSICAL_GPUS) return 0;
        for(NvU32 n=0;n<data.physicalGpuCount;++n) {
            NV_GPU_ARCH_INFO info{}; info.version=NV_GPU_ARCH_INFO_VER;
            if(NvAPI_GPU_GetArchInfo(data.physicalGpuHandles[n],&info)!=NVAPI_OK || !SupportedArchitecture(info.architecture)) return 0;
            if(matched && matched!=info.architecture) return 0;
            matched=info.architecture;
        }
        return matched;
    }
    return 0;
}
// Verify one table entry's file, once per process. The hash of a 166 MB file
// is not repeated per device or per query: Initialize already verifies this
// machine's selection, and the first device query that names a different
// architecture pays for that file once and then reads a cached answer.
struct RuntimeSlot {
    const payload::NrRuntime* runtime=nullptr;
    bool verified=false;
    bool usable=false;
    std::wstring path;
};
static RuntimeSlot runtime_slots[2];
static std::wstring& VerifiedRuntimePath(const payload::NrRuntime& runtime) {
    static std::wstring empty;
    RuntimeSlot* slot=nullptr;
    for(RuntimeSlot& candidate : runtime_slots) {
        if(candidate.runtime==&runtime) { slot=&candidate; break; }
        if(!candidate.runtime) { candidate.runtime=&runtime; slot=&candidate; break; }
    }
    if(!slot) return empty;
    if(!slot->verified) {
        slot->verified=true;
        slot->usable=durable::Sha256(game/runtime.path)==runtime.hash;
        if(slot->usable) slot->path=(game/runtime.path).wstring();
        Log("[deployment] NR runtime %s for arch table entry: %s",runtime.label,
            slot->usable ? "verified":"hash mismatch or missing");
    }
    return slot->usable ? slot->path:empty;
}
// One line per launch with the Windows VRAM budget policy (vram-budget.hpp), so
// a report shows whether 64/64 was set and whether it still waits for a restart.
static void LogVramBudget() {
    const dlss5::vram_budget::Policy vram_policy=dlss5::vram_budget::Query();
    static constexpr const char* kKinds[]={"unknown","default","candidate","custom"};
    const auto value_text=[](const dlss5::vram_budget::Value& value) {
        return !value.exists ? std::string("absent") : !value.dword ? std::string("non_dword") : std::to_string(value.data);
    };
    Log("[deployment] Windows VRAM budget: kind=%s frag=%s cap=%s percent_custom=%u restart_pending=%u",
        kKinds[static_cast<int>(dlss5::vram_budget::Classify(vram_policy))],
        value_text(vram_policy.values[0]).c_str(),value_text(vram_policy.values[1]).c_str(),
        vram_policy.percent_custom ? 1u:0u,
        dlss5::vram_budget::RestartPending(vram_policy,dlss5::vram_budget::LastChange(dir)) ? 1u:0u);
}
static void Initialize() {
    std::lock_guard<std::mutex> lock(mutex);
    if(ready.load()) return;
    game=durable::GameDirectory();
    if(game.empty()) {
        // A failed path allocation/query must not turn deployment paths into
        // paths relative to the host's current working directory.
        safe=true; writable=false; g_sld_complete=false; init_failed=true;
        g_cfg.stage=0; g_vk_mirror=0;
        status="无法读取游戏路径，本次暂停部署干预 / Cannot read game path; deployment suspended.";
        Log("[deployment] executable path unavailable; deployment stays inactive.");
        return;
    }
    dir=durable::StateDirectory(game); writable=!dir.empty();
    DetectGpu();
    LogVramBudget();
    const auto result=writable ? durable::Read(dir/L"bridge.state",state,state.magic) : durable::ReadResult::Invalid;
    if(result==durable::ReadResult::Invalid || !deployment_policy::Valid(state)) {
        if(writable && fs::exists(dir/L"bridge.state")) {
            std::error_code ec;
            fs::copy_file(dir/L"bridge.state",dir/(L"bridge.state.bad-"+std::to_wstring(GetTickCount64())),fs::copy_options::none,ec);
            if(ec) writable=false;
        }
        state={}; state.safe=1;
        status="状态损坏或目录不可写，本次安全模式 / Invalid state: safe mode.";
    }
    if(result==durable::ReadResult::Missing) {
        // First install. The 2.14.1 stack is required on Turing and Ampere
        // because MFG Unlock's provider path needs DLSS-G 310.9.1 there, and it
        // is the recommended combination on Ada for multi-frame generation.
        // Preselecting it per architecture removes the one setting a fresh
        // 20/30/40-series installation would otherwise have to get right before
        // the game can be played; Blackwell keeps the game's own files by
        // default. An explicit cfg choice or an existing subdirectory still
        // wins, and a selection whose files fail verification is dropped below.
        state.selected=g_sld_dir[0] ? 1u : (architecture==0x190 || architecture==0x170 || architecture==0x160 ? 1u : 0u);
    }
    const uint64_t fingerprint=Fingerprint();
    deployment_policy::ExitMark exit_mark=deployment_policy::kExitUnknown;
    if(writable) {
        HANDLE marker=CreateFileW((dir/L"startup-crash.marker").c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        if(marker!=INVALID_HANDLE_VALUE) {
            char bytes[16]{}; DWORD got=0;
            if(ReadFile(marker,bytes,sizeof(bytes),&got,nullptr)) exit_mark=deployment_policy::ClassifyExitMark(bytes,got);
            CloseHandle(marker);
        }
    }
    const bool crashed=exit_mark==deployment_policy::kExitCrashed;
    const bool was_unconfirmed=state.active && !state.healthy;
    const uint32_t previous_failures=state.failures;
    const uint32_t begin=deployment_policy::Begin(state,fingerprint,crashed,exit_mark==deployment_policy::kExitClean);
    if(was_unconfirmed)
        Log("[deployment] previous launch ended before the health point: exit=%s trial=%u failures=%u->%u result=%s",
            crashed ? "crash" : exit_mark==deployment_policy::kExitClean ? "orderly" : "unknown",
            state.trial,previous_failures,state.failures,
            begin==deployment_policy::kBeginRolledBack ? "rolled_back" : begin==deployment_policy::kBeginSafeMode ? "safe_mode" : "counted");
    if(begin==deployment_policy::kBeginRolledBack)
        status=state.selected ? "启动未通过健康点，已回退到上次确认可用的部署 / Startup did not reach health confirmation; the last confirmed deployment is restored."
                              : "启动未通过健康点，已回退到“游戏目录” / Startup did not reach health confirmation; restored to the game directory.";
    else if(begin==deployment_policy::kBeginSafeMode)
        status="连续启动未通过健康点，已进入安全模式（插件暂停干预）；在 DLSS5Bridge 页取消勾选“下次启动安全模式”并重启游戏即可解除 / Repeated launches did not reach health confirmation; safe mode is on (add-ons stand down). Clear \"Safe mode next launch\" on the DLSS5Bridge page and restart to leave it.";
    if(durable::Plain(game/L"dlss5/safe-mode.flag") && GetFileAttributesW((game/L"dlss5/safe-mode.flag").c_str())!=INVALID_FILE_ATTRIBUTES) state.safe=1;
    safe=state.safe!=0;
    Save(state);
    if(writable) crash_file=CreateFileW((dir/L"startup-crash.marker").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_FLAG_WRITE_THROUGH,nullptr);
    current=safe.load() ? 0u:state.selected;
    current_early=state.early; current_nr=state.nr;
    if(safe.load()) { g_cfg.stage=0; g_vk_mirror=0; }
    // Two independent conditions, reported separately so a game update and a
    // damaged package never read as the same message.
    interposer_hash=durable::Sha256(game/L"sl.interposer.dll");
    const bool interposer_known=payload::KnownInterposer(interposer_hash.c_str());
    std::string package_bad;
    for(const auto& entry:payload::sl) {
        if(durable::Sha256(game/L"dlss5/sl-2.14.1"/entry.leaf)!=entry.hash) {
            if(package_bad.empty()) for(const wchar_t* c=entry.leaf; *c; ++c) package_bad.push_back(static_cast<char>(*c & 0x7f));
        }
    }
    sl_valid=interposer_known && package_bad.empty();
    if(!interposer_known) {
        sl_reject="游戏的 sl.interposer.dll 不在已验证名单（SHA256 "+interposer_hash.substr(0,12)+
            "…），2.14.1 组合暂不可用；请使用适配新版游戏的插件包 / The game's sl.interposer.dll is not a verified build (SHA256 "+
            interposer_hash.substr(0,12)+"...); the 2.14.1 stack stays unavailable until a package for this game version.";
    } else if(!package_bad.empty()) {
        sl_reject="套件文件 dlss5/sl-2.14.1/"+package_bad+" 缺失或已改变，请重新解压覆盖完整套件 / Package file dlss5/sl-2.14.1/"+
            package_bad+" is missing or changed; extract the complete package again.";
    }
    Log("[deployment] SL check: interposer=%s known=%u package=%s",interposer_hash.empty()?"(unreadable)":interposer_hash.c_str(),
        interposer_known?1u:0u,package_bad.empty()?"verified":package_bad.c_str());
    if(current && !sl_valid) {
        current=0; State next=state; next.selected=next.confirmed; Save(next);
        status=sl_reject.empty() ? "SL 组合或文件校验失败，使用游戏目录 / SL package rejected; game directory retained." : sl_reject;
    }
    g_sld_dir[0]=0; g_sld_complete=false;
    if(current) {
        const auto path=(game/L"dlss5/sl-2.14.1").wstring();
        wcscpy_s(g_sld_dir,path.c_str()); g_sld_paths[0]=g_sld_dir; g_sld_complete=true;
    }
    // Pick this machine's runtime from the payload table and verify that exact
    // file. A runtime the table names for a different architecture is never
    // validated here, so a missing or altered file for another generation
    // cannot disable the one this machine needs.
    {
        const payload::NrRuntime* selected_runtime=payload::SelectionForArchitecture(architecture);
        if(selected_runtime) {
            nr_label=selected_runtime->label;
            nr_valid=any_supported_gpu &&
                durable::Sha256(game/selected_runtime->path)==selected_runtime->hash;
            if(nr_valid) nr_path=(game/selected_runtime->path).wstring();
        }
    }
    if(!nr_valid && !safe.load()) status="NR 运行库或 GPU 未通过校验，本次暂停 NR / NR runtime/GPU validation failed.";
    // 0=auto, 1=on, 2=off. Only auto on Turing/Ampere executes by default.
    if(!safe.load() && (state.early==1 || (state.early==0 && (early_cuda_needed || state.reserved)))) {
        const HMODULE cuda=LoadLibraryExW(L"nvcuda.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Init=int(__stdcall*)(unsigned);
        auto init=cuda ? reinterpret_cast<Init>(GetProcAddress(cuda,"cuInit")):nullptr;
        const int result_cuda=init ? init(0):-1;
        Log("[deployment] early cuInit=%d arch=0x%x",result_cuda,architecture);
    }
    drs_snapshot=drs::Snapshot();
    LoadEventLogMode();
    if(!status.empty() && writable) {
        const std::string note=status+"\n游戏异常时运行 DLSS5-Recovery 故障恢复.cmd。\nRun DLSS5-Recovery 故障恢复.cmd if the game cannot start.\n";
        durable::Write(game/L"DLSS5-上次启动异常-说明.txt",note.data(),static_cast<DWORD>(note.size()));
    }
    ready=true;
    if(!safe.load()) driver_sync_requested=true;
    Log("[deployment] ready arch=0x%x safe=%u selected=%u current=%u sl_valid=%u nr_valid=%u state=%ls",
        architecture,safe.load()?1u:0u,state.selected,current,sl_valid?1u:0u,nr_valid?1u:0u,dir.c_str());
}
static void RecordCrash() {
    if(crash_file!=INVALID_HANDLE_VALUE) { DWORD written=0; const char mark='1'; WriteFile(crash_file,&mark,1,&written,nullptr); FlushFileBuffers(crash_file); }
}
// DllMain detach: the bridge is being unloaded in order (process exit or
// FreeLibrary). Appended after any crash byte, which still wins on reading.
static void RecordOrderlyExit() {
    if(crash_file!=INVALID_HANDLE_VALUE) { DWORD written=0; const char mark='C'; WriteFile(crash_file,&mark,1,&written,nullptr); }
}
// Lock-free read for the worker loop: the crash marker only decides anything
// while the current launch is still waiting for its health point.
static std::atomic<bool> health_reached{false};
static bool BeforeHealthPoint() { return crash_file!=INVALID_HANDLE_VALUE && !health_reached.load(std::memory_order_relaxed); }
static void DeviceCreateFailed(uint32_t result) {
    if(result!=0xfffffffdu || !ready.load()) return;
    std::lock_guard<std::mutex> lock(mutex);
    State next=state; next.reserved=1; Save(next);
    safe=true;
    InterlockedExchange(&g_vk_mirror,0);
    status="设备初始化 -3；本次暂停 NR，下一次自动提前初始化 CUDA / Device init failed; NR paused, early CUDA enabled next launch.";
    Log("[deployment] vkCreateDevice=-3: no internal layer-chain retry; current-session NR suspended.");
}
static void SlResult(int result,bool used) {
    std::lock_guard<std::mutex> lock(mutex);
    redirected=used && result==0;
    if(current && (!used || result!=0)) {
        load_failed=true; State next=state; next.selected=0; next.confirmed=0; Save(next);
        status="SL 试用失败，已计划下次启动使用游戏目录 / SL failed; game-directory stack on next launch.";
    }
}
// --- anomaly event log ------------------------------------------------------
// Present intervals of 100 ms or more are hitches; no present for 2 s while the
// game window is in the foreground and not minimised is a stall.
static constexpr ULONGLONG kHitchMs=100, kStallMs=2000;
static bool ForegroundIsGame() {
    const HWND window=GetForegroundWindow();
    DWORD pid=0;
    return window!=nullptr && GetWindowThreadProcessId(window,&pid) && pid==GetCurrentProcessId() && !IsIconic(window);
}
// dlss5/state/event-log-mode.txt holds "full=1" while the full anomaly log is
// on. Absent or anything else is the bounded default.
static void LoadEventLogMode() {
    char text[16]{};
    if(writable) {
        HANDLE h=CreateFileW((dir/L"event-log-mode.txt").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        if(h!=INVALID_HANDLE_VALUE) {
            DWORD got=0;
            ReadFile(h,text,sizeof(text)-1,&got,nullptr);
            CloseHandle(h);
        }
    }
    const bool full=strncmp(text,"full=1",6)==0;
    g_events.SetFull(full);
    Log("[event-log] mode at start: %s",full?"full (every occurrence)":"bounded");
}
static bool StoreEventLogMode(bool full) {
    if(!writable) return false;
    const char* text=full ? "full=1\n":"full=0\n";
    if(!durable::Write(dir/L"event-log-mode.txt",text,7)) return false;
    g_events.SetFull(full);
    Log("[event-log] mode changed from the panel: %s",full?"full (every occurrence)":"bounded");
    return true;
}
static void EventsTick() {
    const ULONGLONG now=GetTickCount64();
    static bool focus_known=false, previous_focus=false;
    static ULONGLONG heartbeat=0;
    const bool focused=ForegroundIsGame();
    if(!focus_known || focused!=previous_focus || now-heartbeat>=5000) {
        char text[192];
        _snprintf_s(text,_TRUNCATE,"[action] phase=observed event=%s foreground=%u last_present_ms=%llu sample_ms=%llu",
            !focus_known || focused!=previous_focus ? "focus_state" : "heartbeat",
            focused?1u:0u,static_cast<unsigned long long>(last_present.load()),static_cast<unsigned long long>(now));
        g_actions.Add(dlss5::eventlog::kSystem,text);
        focus_known=true; previous_focus=focused; heartbeat=now;
    }
    static bool stalled=false;
    static ULONGLONG stall_from=0, rescan_at=0;
    const ULONGLONG last=last_present.load();
    if(last && now-last>=kStallMs) {
        if(!stalled && ForegroundIsGame()) {
            stalled=true; stall_from=last;
            char text[160];
            _snprintf_s(text,_TRUNCATE,"[stall] no frame presented for %llu ms while the game window is in the foreground",
                static_cast<unsigned long long>(now-last));
            g_events.Add(dlss5::eventlog::kSystem,dlss5::eventlog::kWarning,text);
        }
    } else if(stalled && last>stall_from) {
        stalled=false;
        char text[160];
        _snprintf_s(text,_TRUNCATE,"[stall] presenting resumed after %llu ms",static_cast<unsigned long long>(last-stall_from));
        g_events.Add(dlss5::eventlog::kSystem,dlss5::eventlog::kWarning,text);
    }
    g_events.Flush(now);
    g_actions.Flush();
    if(now-rescan_at>=5000) { rescan_at=now; g_events.Rescan(); }
}
static void Present() {
    const auto now=GetTickCount64();
    const auto previous=last_present.exchange(now);
    if(!previous || now-previous>5000) first_present=now;
    if(previous && now-previous>=kHitchMs) {
        char text[160];
        _snprintf_s(text,_TRUNCATE,"[hitch] present interval %llu ms (foreground=%u)",
            static_cast<unsigned long long>(now-previous),ForegroundIsGame()?1u:0u);
        g_events.Add(dlss5::eventlog::kSystem,dlss5::eventlog::kWarning,text);
    }
}
static void Tick() {
    if(driver_sync_requested.exchange(false)) SynchronizeStartupDriver();
    if(!ready.load() || safe.load()) return;
    std::lock_guard<std::mutex> lock(mutex);
    if(state.healthy || load_failed) { health_reached.store(true,std::memory_order_relaxed); return; }
    const auto now=GetTickCount64(), first=first_present.load(), last=last_present.load();
    if(!first || now-last>5000 || now-first<60000) return;
    if(current) {
        if(!redirected) return;
        // Loaded plugins may be optional. Every loaded member must come from
        // the selected directory; common, DLSS and Reflex must be present.
        for(const auto& entry:payload::sl) {
            if(wcsncmp(entry.leaf,L"sl.",3)!=0) continue;
            HMODULE mod=GetModuleHandleW(entry.leaf);
            const bool required=wcscmp(entry.leaf,L"sl.common.dll")==0 || wcscmp(entry.leaf,L"sl.dlss.dll")==0 || wcscmp(entry.leaf,L"sl.reflex.dll")==0;
            if(!mod) { if(required) return; else continue; }
            dlss5::WidePathBuffer path;
            if(!path.Module(mod)) return;
            if(!durable::SamePath(path.data(),game/L"dlss5/sl-2.14.1"/entry.leaf)) {
                load_failed=true; State next=state; next.selected=0; Save(next);
                status="SL 路径混合，下次启动回退 / Mixed SL paths; restore next launch."; return;
            }
        }
        // Successful game DLSS evaluation is required: directory load alone
        // cannot establish that the game's super-resolution feature exists.
        if(InterlockedCompareExchange(&g_deployment_sr_success,0,0)==0) return;
    }
    State next=state; deployment_policy::Healthy(next,current);
    next.confirmed_early=current_early; next.confirmed_nr=current_nr;
    if(Save(next)) {
        health_reached.store(true,std::memory_order_relaxed);
        status=kStatusHealthy;
        Log("[deployment] startup health confirmed: sl=%u successful_game_sr=%ld",current,
            InterlockedCompareExchange(&g_deployment_sr_success,0,0));
    }
}
static void RecoveryObserveTick() {
#if defined(DLSS5_TIMING_DIAGNOSTIC)
    if (!dlss5timing::Ready()) return;
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (last == 0) { last = now; return; }
    const ULONGLONG elapsed = now - last;
    if (elapsed < 5000) return;
    last = now;
    MEMORYSTATUSEX system = {};
    system.dwLength = sizeof(system);
    const BOOL system_ok = GlobalMemoryStatusEx(&system);
    const DWORD system_error = system_ok ? ERROR_SUCCESS : GetLastError();
    PROCESS_MEMORY_COUNTERS_EX process = {};
    process.cb = sizeof(process);
    const BOOL process_ok = K32GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process), sizeof(process));
    const DWORD process_error = process_ok ? ERROR_SUCCESS : GetLastError();
    const ULONGLONG sampled = GetTickCount64();
    static bool previous_valid = false;
    static DWORD previous_faults = 0;
    static ULONGLONG previous_sample = 0;
    const bool delta_valid = process_ok && previous_valid;
    const DWORD faults = delta_valid ? process.PageFaultCount - previous_faults : 0;
    const ULONGLONG fault_span = delta_valid ? sampled - previous_sample : 0;
    Log("[recovery-memory] sample_ms=%llu query_ms=%llu nr_at_sample=%u ram_valid=%u error=%lu available=%llu/%llu MiB process_valid=%u error=%lu private=%llu working_set=%llu MiB page_faults_soft_plus_hard=%lu delta_valid=%u delta=%lu delta_ms=%llu; endpoint samples, not window peaks.",
        sampled, sampled - now, static_cast<unsigned>(PresentAdapterNrDemand()),
        system_ok ? 1u : 0u, system_error, system.ullAvailPhys >> 20, system.ullTotalPhys >> 20,
        process_ok ? 1u : 0u, process_error, static_cast<unsigned long long>(process.PrivateUsage >> 20),
        static_cast<unsigned long long>(process.WorkingSetSize >> 20), process.PageFaultCount,
        delta_valid ? 1u : 0u, faults, fault_span);
    previous_valid = process_ok != FALSE;
    previous_faults = process.PageFaultCount;
    previous_sample = sampled;
    static constexpr uint32_t sites[] = {
        bridge_timing::ReflexSleep, bridge_timing::RelayThrottle,
        bridge_timing::GamePresent, bridge_timing::GamePresentNative,
        bridge_timing::FgEvaluate, bridge_timing::FgBridgeInput, bridge_timing::FgNativeEvaluate,
        bridge_timing::RelayWorker, bridge_timing::NrSubmit, bridge_timing::CompositeSubmit,
        bridge_timing::FenceWait
    };
    static const char* const names[] = {
        "reflex_original", "relay_throttle", "present_total", "present_original",
        "fg_total", "fg_bridge_input", "fg_original", "relay_worker", "nr_submit", "composite_submit", "fence_wait"
    };
    static ULONGLONG previous_report[_countof(sites)] = {};
    const double to_ms = 1000.0 / static_cast<double>(
        dlss5timing::active_buffer.load(std::memory_order_relaxed)->qpc_frequency);
    for (size_t i = 0; i < _countof(sites); ++i) {
        dlss5timing::SummarySnapshot snapshot;
        if (!dlss5timing::TakeSummary(sites[i], &snapshot)) {
            Log("[recovery-waits] site=%s summary_busy=1; samples retained for the next report.", names[i]);
            continue;
        }
        const ULONGLONG report_time = GetTickCount64();
        const ULONGLONG interval = previous_report[i] ? report_time - previous_report[i] : report_time - (now - elapsed);
        previous_report[i] = report_time;
        const auto& sum = snapshot.completed;
        Log("[recovery-waits] site=%s sample_ms=%llu interval_ms=%llu completions=%llu avg_ms=%.3f max_ms=%.3f ge50ms=%llu ge100ms=%llu dropped=%llu in_flight_at_snapshot=%llu first_begin_qpc=%lld last_end_qpc=%lld; CPU spans by return time, not GPU completion or display latency.",
            names[i], report_time, interval, sum.calls, sum.calls ? static_cast<double>(sum.ticks) * to_ms / static_cast<double>(sum.calls) : 0.0,
            static_cast<double>(sum.max_ticks) * to_ms, sum.ge50ms, sum.ge100ms,
            snapshot.dropped, snapshot.in_flight, sum.first_begin_qpc, sum.last_end_qpc);
    }
#endif
}
// Adapter of the private D3D12 session, published by the render thread when the
// session opens. 0 = not known yet. HighPart in the upper half.
static std::atomic_uint64_t residency_luid{0};
static void NoteAdapterLuid(LUID luid) {
    residency_luid.store((static_cast<uint64_t>(static_cast<uint32_t>(luid.HighPart)) << 32) | luid.LowPart);
}
// Event-driven, bounded: logs the first reading, every change of the demoted
// amount by 64 MiB or more, every crossing of zero, and every NR switch change;
// after 256 lines one line per minute at most (v9; the 5 s cadence of a
// diagnostic build stopped at 21 minutes before). Diagnostic builds log every
// 5 s sample until the cap. Kept in all builds because a player's log otherwise
// cannot show that the game's own VRAM was pushed into system memory
// (2026-10-26). Without a private D3D12 session (v9: the consumer serves
// Vulkan Present itself) the adapter is found among this process's own GPU
// Process Memory counter instances (gpu_residency::Reader::Discover), retried
// every 5 s until one exists.
static void ResidencyTick() {
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (now - last < 5000) return;
    last = now;
    static gpu_residency::Reader reader;
    static bool discovered = false;
    uint64_t packed = residency_luid.load();
    LUID luid;
    if (packed == 0) {
        if (!reader.Discover(&luid)) return;
        residency_luid.store((static_cast<uint64_t>(static_cast<uint32_t>(luid.HighPart)) << 32) | luid.LowPart);
        if (!discovered) {
            discovered = true;
            Log("[vram-residency] adapter 0x%08X_0x%08X taken from this process's GPU Process Memory counters "
                "(no private D3D12 session publishes one).",
                static_cast<unsigned>(luid.HighPart), static_cast<unsigned>(luid.LowPart));
        }
    } else {
        luid.LowPart = static_cast<DWORD>(packed & 0xFFFFFFFFu);
        luid.HighPart = static_cast<LONG>(packed >> 32);
    }
    gpu_residency::Sample sample;
    if (!reader.Read(luid, &sample)) return;
    static bool logged = false;
    static long long logged_demoted = 0;
    static uint32_t logged_nr = UINT32_MAX;
    static unsigned lines = 0, suppressed = 0;
    static ULONGLONG last_line = 0;
    const long long demoted = sample.DemotedMiB();
    const uint32_t nr = static_cast<uint32_t>(PresentAdapterNrDemand());
    bool due = !logged || nr != logged_nr || (demoted > 0) != (logged_demoted > 0) ||
               demoted - logged_demoted >= 64 || logged_demoted - demoted >= 64;
#if defined(DLSS5_TIMING_DIAGNOSTIC)
    due = true;
#endif
    if (!due) return;
    if (lines >= 256) {
        if (now - last_line < 60000) { ++suppressed; return; }
    }
    ++lines;
    last_line = now;
    logged = true; logged_demoted = demoted; logged_nr = nr;
    Log("[vram-residency] local=%llu nonlocal=%llu committed=%llu demoted=%lld MiB nr=%u owner=%ld line=%u%s; "
        "demoted = committed - local: VRAM-preferred allocations of this process held in system memory "
        "(all devices of the process).",
        sample.local >> 20, sample.nonlocal >> 20, sample.committed >> 20, demoted, nr,
        InterlockedCompareExchange(&g_adapter_owner, 0, 0), lines,
        lines > 256 ? " (throttled to one line per minute)" :
        lines == 256 ? " (limit reached; from here one line per minute at most)" : "");
    if (lines > 256 && suppressed != 0) {
        Log("[vram-residency] %u reading(s) since the previous line were not logged (throttled).", suppressed);
        suppressed = 0;
    }
}
static DWORD WINAPI MaintenanceWorker(LPVOID owner) {
    while(!g_shutting_down) { Tick(); EventsTick(); RecoveryObserveTick(); ResidencyTick(); Sleep(250); }
    g_events.Flush(GetTickCount64());
    g_actions.Flush();
    FreeLibraryAndExitThread(static_cast<HMODULE>(owner),0);
}
static void StartMaintenance() {
    static std::atomic_bool started{false};
    if(started.exchange(true)) return;
    if(HANDLE thread=StartModuleWorker(MaintenanceWorker)) CloseHandle(thread);
}
} // namespace deployment
static bool DeploymentSuspended() { return deployment::ready.load() && deployment::safe.load(); }

extern "C" __declspec(dllexport) uint32_t __cdecl DLSS5DeploymentSafeMode() {
    return !deployment::ready.load() || deployment::safe.load() ? 1u:0u;
}

// Generic and MFG Unlock forward their log messages here (common/event-log.hpp).
// Warnings and errors are anomalies; info and debug lines are classified.
extern "C" __declspec(dllexport) void __cdecl DLSS5LogEventV1(uint32_t source,uint32_t level,const char* text) {
    if(text==nullptr || *text==0) return;
    const DWORD last_error=GetLastError();
    if(dlss5::eventlog::IsOperationEvidence(text)) g_actions.Add(source,text);
    if(level<=dlss5::eventlog::kWarning || dlss5::eventlog::LooksAnomalous(text))
        g_events.Add(source,level==0 ? static_cast<uint32_t>(dlss5::eventlog::kError):level,text);
    SetLastError(last_error);
}
extern "C" __declspec(dllexport) uint32_t __cdecl DLSS5LogFullModeV1() {
    return g_events.Full() ? 1u:0u;
}
extern "C" __declspec(dllexport) int __cdecl DLSS5GetNrRuntimePathV1(wchar_t* out,uint32_t count) {
    // 0 = not decided yet (the deployment state is still being read); Generic
    // asks again instead of switching NR off. Negative values are final.
    // safe starts true and is settled only by Initialize, so ready is checked first.
    if(!out || !count) return -1;
    if(!deployment::ready.load()) return deployment::init_failed.load() ? -1 : 0;
    if(deployment::safe.load()) return -1;
    // No device: use the machine-wide architecture. A machine whose physical
    // GPUs disagree has architecture 0 and gets no runtime, rather than the
    // runtime of whichever card happened to be enumerated first.
    const dlss5::payload::NrRuntime* runtime=dlss5::payload::SelectionForArchitecture(deployment::architecture);
    const std::wstring path=runtime ? deployment::VerifiedRuntimePath(*runtime):std::wstring();
    if(path.empty()) {
        static LONG once=0;
        if(InterlockedExchange(&once,1)==0)
            Log("[deployment] no NR runtime for the machine-wide GPU architecture 0x%x (%s); a caller that names its GPU is asked separately.",
                deployment::architecture,runtime ? "runtime failed verification" : "no single supported architecture");
        return -1;
    }
    if(path.size()+1>count) return -1;
    wcscpy_s(out,count,path.c_str()); return 1;
}

extern "C" __declspec(dllexport) int __cdecl DLSS5GetNrRuntimePathV2(uint32_t low,int32_t high,wchar_t* out,uint32_t count) {
    // safe starts true and is settled only by Initialize, so ready is checked first.
    if(!out || !count) return -1;
    if(!deployment::ready.load()) return deployment::init_failed.load() ? -1 : 0;
    if(deployment::safe.load()) return -1;
    const LUID luid{low,high};
    const uint32_t arch=deployment::ArchitectureForLuid(luid);
    const dlss5::payload::NrRuntime* runtime=deployment::SupportedArchitecture(arch) ? dlss5::payload::SelectionForArchitecture(arch) : nullptr;
    const std::wstring path=runtime ? deployment::VerifiedRuntimePath(*runtime):std::wstring();
    if(path.empty() || path.size()+1>count) {
        static LONG logged=0;
        if(InterlockedIncrement(&logged)<=4)
            Log("[deployment] no NR runtime for device LUID=%08x:%08x arch=0x%x: %s",
                static_cast<uint32_t>(high),low,arch,
                !deployment::SupportedArchitecture(arch) ? "architecture not supported by NR"
                : !runtime ? "no runtime mapped to this architecture"
                : path.empty() ? "runtime file missing or failed verification" : "path too long");
        return -1;
    }
    wcscpy_s(out,count,path.c_str());
    Log("[deployment] NR selected for device LUID=%08x:%08x arch=0x%x runtime=%s path=%ls",
        static_cast<uint32_t>(high),low,arch,runtime->label,out);
    return 1;
}

// The panel shows which runtime this launch resolved, so a report can name it
// without reading the log.
extern "C" __declspec(dllexport) const char* __cdecl DLSS5GetNrRuntimeLabel() {
    return deployment::nr_label.empty() ? "" : deployment::nr_label.c_str();
}
