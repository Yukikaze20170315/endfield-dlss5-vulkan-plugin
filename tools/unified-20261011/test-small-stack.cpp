// Runs the production Bridge resolver and DLL notification callback in this
// isolated executable. No game, GPU, driver profile or registry changes occur.
// The Win32 wrappers inject path-query/allocation failures only in this fixture.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static thread_local bool fixture_fail_path_allocation=false;
static thread_local unsigned fixture_path_mode=0;
static thread_local size_t fixture_path_allocations=0;
static HMODULE fixture_module=nullptr;
static std::wstring fixture_module_path;
static LPVOID WINAPI FixtureHeapAlloc(HANDLE heap,DWORD flags,SIZE_T bytes) {
    if(bytes==32768*sizeof(wchar_t)) {
        ++fixture_path_allocations;
        if(fixture_fail_path_allocation) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
    }
    return HeapAlloc(heap,flags,bytes);
}
static DWORD WINAPI FixtureGetModuleFileNameW(HMODULE module,LPWSTR out,DWORD capacity) {
    if(fixture_path_mode && module==fixture_module) {
        if(fixture_path_mode==1) { SetLastError(ERROR_INVALID_HANDLE); return 0; }
        if(fixture_path_mode==2) {
            for(DWORD i=0;i<capacity;++i) out[i]=L'x';
            SetLastError(ERROR_INSUFFICIENT_BUFFER); return capacity;
        }
        const DWORD size=static_cast<DWORD>(fixture_module_path.size());
        if(size>=capacity) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return capacity; }
        memcpy(out,fixture_module_path.c_str(),(size+1)*sizeof(wchar_t));
        SetLastError(761); return size;
    }
    return GetModuleFileNameW(module,out,capacity);
}

#define HeapAlloc FixtureHeapAlloc
#define GetModuleFileNameW FixtureGetModuleFileNameW
#include "../../src/unified-20261011/bridge/src/dlss5-bridge.cpp"
#undef GetModuleFileNameW
#undef HeapAlloc

static std::atomic_uint checks{0};
static void Require(bool value,const char* label) {
    if(!value) {
        std::fprintf(stderr,"FAIL: %s (error=%lu)\n",label,GetLastError());
        std::fflush(stderr); ExitProcess(1);
    }
    ++checks;
}

// Sum the stack allocation and pushed registers described by Windows' unwind
// metadata. RtlLookupFunctionEntry sees the optimized code actually executed.
static size_t FrameSize(const void* function) {
    DWORD64 image=0;
    auto entry=RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(function),&image,nullptr);
    if(!entry) return 0; // A leaf which does not allocate stack has no entry.
    const auto* info=reinterpret_cast<const unsigned char*>(image+entry->UnwindData);
    Require((info[0]&7)==1,"x64 unwind version");
    const unsigned count=info[2]; size_t bytes=0;
    for(unsigned i=0;i<count;) {
        const unsigned op=info[5+2*i]&15,operand=info[5+2*i]>>4;
        if(op==0) { bytes+=8; ++i; }
        else if(op==1 && operand==0) {
            bytes+=*reinterpret_cast<const unsigned short*>(info+4+2*(i+1))*8u; i+=2;
        } else if(op==1 && operand==1) {
            bytes+=*reinterpret_cast<const unsigned long*>(info+4+2*(i+1)); i+=3;
        } else if(op==2) { bytes+=operand*8+8; ++i; }
        else if(op==3) ++i;
        else if(op==4 || op==8) i+=2;
        else if(op==5 || op==9) i+=3;
        else if(op==10) { bytes+=operand?48:40; ++i; }
        else { Require(false,"recognized unwind operation"); break; }
    }
    return bytes;
}
static void CheckFrame(const char* name,const void* function,size_t limit) {
    const size_t frame=FrameSize(function);
    std::printf("frame %-24s %5zu bytes (limit %zu)\n",name,frame,limit);
    Require(frame<=limit,"callback stack frame limit");
}

static FARPROC Expected(HMODULE module,LPCSTR name,DWORD* error) {
    SetLastError(1234);
    FARPROC result=g_sld_get_proc(module,name);
    *error=GetLastError(); return result;
}
static void CheckPassThrough(HMODULE module,LPCSTR name) {
    DWORD error=0; const FARPROC expected=Expected(module,name,&error);
    const size_t before=fixture_path_allocations;
    SetLastError(1234);
    Require(GetProcAddress(module,name)==expected,"unrelated resolver result preserved");
    Require(GetLastError()==error,"unrelated resolver LastError preserved");
    Require(fixture_path_allocations==before,"unrelated symbol performs no path allocation");
}
static void CheckInit(bool wrapped) {
    DWORD error=0; const FARPROC original=Expected(fixture_module,"slInit",&error);
    SetLastError(1234);
    const FARPROC actual=GetProcAddress(fixture_module,"slInit");
    Require(actual==(wrapped?reinterpret_cast<FARPROC>(&SldSlInit):original),"slInit ownership/fallback");
    Require(GetLastError()==error,"slInit LastError preserved on all helper exits");
}

static DWORD WINAPI SmallStackWorker(void* opaque) {
    const bool scenarios=opaque!=nullptr;
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(&memory,&memory,sizeof(memory))!=0,"read stack allocation");
    const size_t reserved=reinterpret_cast<uintptr_t>(reinterpret_cast<NT_TIB*>(NtCurrentTeb())->StackBase)-
        reinterpret_cast<uintptr_t>(memory.AllocationBase);
    Require(reserved==65536,"worker really has 64 KiB total stack reservation");
    auto kernel=GetModuleHandleW(L"kernel32.dll");
    for(unsigned i=0;i<2000;++i) {
        CheckPassThrough(kernel,"GetCurrentProcessId");
        CheckPassThrough(fixture_module,MAKEINTRESOURCEA(1));
        CheckPassThrough(fixture_module,"missing_export");
    }
    CheckInit(true);
    SldPreferencesV1 pref{}; pref.flags=0x85; pref.renderAPI=2;
    auto init=reinterpret_cast<PFN_SldSlInit>(GetProcAddress(fixture_module,"slInit"));
    Require(init(&pref,123)==0,"redirected slInit executes on 64 KiB stack");
    Require(pref.numPathsToPlugins==0 && pref.pathsToPlugins==nullptr,"caller preferences preserved");
    if(scenarios) {
        fixture_fail_path_allocation=true; CheckInit(false);
        {
            dlss5::WidePathBuffer path;
            Require(!path && !path.Module(fixture_module),"nonthrowing failed path allocation");
            Require(dlss5::durable::GameDirectory().empty(),"failed game path is empty");
            Require(dlss5::maintenance::Runner().empty(),"failed maintenance path disables runner");
        }
        fixture_fail_path_allocation=false;
        fixture_path_mode=1; CheckInit(false);
        fixture_path_mode=2; CheckInit(false);
        fixture_path_mode=3; CheckInit(true);
        const auto saved=deployment::game;
        deployment::game=fixture_module_path.substr(0,fixture_module_path.find_last_of(L'\\'));
        CheckInit(true);
        deployment::game=saved;
        fixture_path_mode=0;
        const auto prior=g_sld_original;
        InterlockedExchangePointer(&g_sld_original,reinterpret_cast<PVOID>(&SldSlInit));
        CheckInit(false);
        InterlockedExchangePointer(&g_sld_original,prior);
        // Trigger the real notification callback under the Windows loader lock.
        // Discovery stays disabled: this fixture never launches the hook worker.
        HMODULE version=LoadLibraryExW(L"version.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        Require(version!=nullptr,"small-stack LoadLibrary with registered callback");
        FreeLibrary(version);
        wchar_t name[]=L"C:\\fixture\\sl.test.dll";
        ObsUnicodeString text{static_cast<USHORT>(wcslen(name)*sizeof(wchar_t)),sizeof(name),name};
        LdrDllNotificationData data{};
        data.FullDllName=&text;
        data.BaseDllName=&text;
        data.DllBase=reinterpret_cast<void*>(0x10000);
        OnDllLoaded(1,&data,nullptr); OnDllLoaded(2,&data,nullptr);
        Require(g_obs_write>=0,"loader callback recorded an event");
    }
    return 0;
}

static void RunThreads(unsigned count,bool scenarios) {
    std::vector<HANDLE> threads;
    for(unsigned i=0;i<count;++i) {
        auto thread=CreateThread(nullptr,65536,SmallStackWorker,scenarios?reinterpret_cast<void*>(1):nullptr,
                                 STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);
        Require(thread!=nullptr,"create 64 KiB thread"); threads.push_back(thread);
    }
    for(HANDLE thread:threads) {
        Require(WaitForSingleObject(thread,30000)==WAIT_OBJECT_0,"64 KiB worker completed");
        DWORD code=1; Require(GetExitCodeThread(thread,&code) && code==0,"64 KiB worker exit status");
        CloseHandle(thread);
    }
}

int main() {
    g_self=GetModuleHandleW(nullptr);
    InitializeCriticalSection(&g_log_cs); InitializeCriticalSection(&g_hook_cs);
    deployment::game=dlss5::durable::GameDirectory();
    Require(!deployment::game.empty(),"fixture executable path");
    fixture_module=LoadLibraryW((deployment::game/L"sl.interposer.dll").c_str());
    Require(fixture_module!=nullptr,"load isolated fake interposer");
    fixture_module_path=(deployment::game/L"sl.interposer.dll").wstring();
    wcscpy_s(g_sld_dir,L"fixture-path"); g_sld_paths[0]=g_sld_dir; g_sld_complete=true;
    SldInstallResolver(); Require(g_sld_hook_state==1,"install production GetProcAddress hook");
    CheckFrame("SldResolve",reinterpret_cast<const void*>(&SldResolve),512);
    CheckFrame("SldResolveInit",reinterpret_cast<const void*>(&SldResolveInit),2048);
    CheckFrame("OnDllLoaded",reinterpret_cast<const void*>(&OnDllLoaded),2048);
    CheckFrame("ObsOnLoaderEvent",reinterpret_cast<const void*>(&ObsOnLoaderEvent),2048);
    CheckFrame("TriggerHookScan",reinterpret_cast<const void*>(&TriggerHookScan),2048);
    CheckFrame("StartModuleWorker",reinterpret_cast<const void*>(&StartModuleWorker),2048);
    g_obs_on=true; QueryPerformanceCounter(&g_obs_t0); QueryPerformanceFrequency(&g_obs_freq);
    auto reg=reinterpret_cast<PFN_LdrRegisterDllNotification>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"LdrRegisterDllNotification"));
    auto unreg=reinterpret_cast<PFN_LdrUnregisterDllNotification>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"LdrUnregisterDllNotification"));
    void* cookie=nullptr; Require(reg && unreg && reg(0,&OnDllLoaded,nullptr,&cookie)>=0,"register production loader callback");
    RunThreads(1,true);
    // Load an actual fixture DLL from a >1024-character absolute path. Every
    // component respects the filesystem limit; only this build folder is used.
    // A path must not be silently truncated to avoid stack pressure.
    const auto game=deployment::game;
    const HMODULE module=fixture_module;
    const auto original=g_sld_original;
    std::filesystem::path long_game=L"\\\\?\\"+game.wstring()+L"\\long-path-"+std::to_wstring(GetCurrentProcessId());
    for(unsigned i=0;i<6;++i) long_game/=std::wstring(190,L'x')+std::to_wstring(i);
    std::filesystem::create_directories(long_game);
    Require(CopyFileW((game/L"sl.interposer.dll").c_str(),(long_game/L"sl.interposer.dll").c_str(),TRUE)!=FALSE,
            "copy fixture into long path without replacing existing data");
    deployment::game=long_game;
    fixture_module=LoadLibraryW((long_game/L"sl.interposer.dll").c_str());
    Require(fixture_module && fixture_module!=module,"load separate DLL from long path");
    {
        dlss5::WidePathBuffer path;
        Require(path.Module(fixture_module) && wcslen(path.data())>1024,"real module path exceeds 1024 characters");
        fixture_module_path=path.data();
        std::printf("real module path: %zu UTF-16 characters\n",fixture_module_path.size());
    }
    InterlockedExchangePointer(&g_sld_original,nullptr);
    RunThreads(1,true);
    deployment::game=game; fixture_module=module; fixture_module_path=(game/L"sl.interposer.dll").wstring();
    InterlockedExchangePointer(&g_sld_original,original);
    RunThreads(8,false);
    Require(unreg(cookie)>=0,"unregister loader callback");
    g_obs_on=false;
    fixture_fail_path_allocation=true;
    deployment::ready=false; deployment::Initialize();
    Require(!deployment::ready.load() && deployment::safe.load() && !g_sld_complete,
            "missing executable path stops deployment before relative-path I/O");
    Require(dlss5::durable::RecoveryMirror({}).empty(),"empty game cannot create recovery directory");
    std::printf("PASS: %u checks; 10 real 64 KiB threads; resolver, loader, concurrency and failure paths\n",checks.load());
    return 0;
}
