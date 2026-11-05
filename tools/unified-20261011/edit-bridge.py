from pathlib import Path
root=Path(__file__).resolve().parents[2]/'src/unified-20261011/bridge/src'
p=root/'deploy-observer.inc'; s=p.read_text(encoding='utf-8')
s=s.replace('static void SldOnModuleMapped(const wchar_t *leaf, void *base, ULONG size);\n','')
s=s.replace('    if (reason == 1) SldOnModuleMapped(ObsLeaf(r.path), r.base, r.size);\n','')
s=s.replace('''    if (r.interesting && reason == 1)
        r.frames = RtlCaptureStackBackTrace(0, _countof(r.stack), r.stack, nullptr);''','''    // The loader notification only publishes bounded records. Hooking, stack
    // walking, hashing and file access belong to callers outside the loader lock.''')
s=s.replace('''    L"sl.common.dll", L"sl.dlss.dll", L"sl.dlss_g.dll", L"sl.reflex.dll", L"sl.pcl.dll", L"nvngx_dlssg.dll" };''','''    L"sl.common.dll", L"sl.deepdvc.dll", L"sl.dlss_d.dll", L"sl.dlss.dll", L"sl.dlss_g.dll",
    L"sl.reflex.dll", L"sl.pcl.dll", L"nvngx_dlssg.dll", L"nvngx_dlss.dll", L"nvngx_dlssd.dll", L"nvngx_deepdvc.dll" };''')
s=s.replace('static void SldDescribePreferences(', '#include "deployment.inc"\n\nstatic void SldDescribePreferences(',1)
s=s.replace('''    static SldPreferencesV1 copy;''','''    SldPreferencesV1 copy{};''')
s=s.replace('''    ObsLog("[sl-dir] slInit returned %d", result);
    return result;''','''    ObsLog("[sl-dir] slInit returned %d", result);
    deployment::SlResult(result, use != pref);
    return result;''')
start=s.index('// The export\'s address from the mapped image\'s export directory.')
end=s.index('static void SldReport()',start)
s=s[:start]+'''// Intercept symbol resolution from AddonInit, outside the loader lock. This
// catches the loading thread synchronously before it can call slInit, without
// patching the newly mapped image or suspending threads in a DLL notification.
using SldGetProc = FARPROC (WINAPI*)(HMODULE,LPCSTR);
static SldGetProc g_sld_get_proc;
static FARPROC WINAPI SldResolve(HMODULE module,LPCSTR name) {
    const FARPROC original=g_sld_get_proc(module,name);
    if(!original || !g_sld_complete || reinterpret_cast<uintptr_t>(name)<=65535 || strcmp(name,"slInit")!=0) return original;
    wchar_t path[32768]{};
    if(!GetModuleFileNameW(module,path,32768) || _wcsicmp(path,(deployment::game/L"sl.interposer.dll").c_str())!=0) return original;
    const auto prior=InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&g_sld_original),reinterpret_cast<PVOID>(original),nullptr);
    if(prior && prior!=reinterpret_cast<PVOID>(original)) return original;
    return reinterpret_cast<FARPROC>(&SldSlInit);
}
static void SldInstallResolver() {
    if(!g_sld_complete || g_sld_hook_state) return;
    void* target=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetProcAddress"));
    MH_STATUS result=MH_Initialize();
    if(result==MH_OK || result==MH_ERROR_ALREADY_INITIALIZED)
        result=MH_CreateHook(target,reinterpret_cast<void*>(&SldResolve),reinterpret_cast<void**>(&g_sld_get_proc));
    if(result==MH_OK && PinHookModule(reinterpret_cast<void*>(&SldResolve))) result=MH_EnableHook(target);
    else if(result==MH_OK) result=MH_ERROR_MEMORY_ALLOC;
    g_sld_target=target; g_sld_hook_status=result;
    InterlockedExchange(&g_sld_hook_state,result==MH_OK ? 1:-1);
}

''' +s[end:]
s=s.replace('slInit hooked at %p as sl.interposer.dll was mapped (before its DllMain)','slInit resolver armed at %p from AddonInit, outside DLL notification')
s=s.replace('''    const LONG n = InterlockedIncrement(&g_obs_addon_init_calls);''','''    deployment::Initialize();
    SldInstallResolver();
    const LONG n = InterlockedIncrement(&g_obs_addon_init_calls);''')
p.write_text(s,encoding='utf-8')
p=root/'dlss5-bridge.cpp'; s=p.read_text(encoding='utf-8')
s=s.replace('static LONG WINAPI CrashFilter(EXCEPTION_POINTERS *ep)\n{','namespace deployment { static void RecordCrash(); }\nstatic LONG WINAPI CrashFilter(EXCEPTION_POINTERS *ep)\n{\n    deployment::RecordCrash();')
s=s.replace('                ObsAfterAttach();','                deployment::Tick();\n                ObsAfterAttach();')
s=s.replace('''        g_self = module;
        g_shutting_down = false;''','''        g_self = module;
        wchar_t host[MAX_PATH]{};
        GetModuleFileNameW(nullptr,host,MAX_PATH);
        const wchar_t* leaf=wcsrchr(host,L'\\\\');
        if(leaf && _wcsicmp(leaf+1,L"rundll32.exe")==0) return TRUE;
        g_shutting_down = false;''')
# Maintenance hosts skip the normal attach/detach lifecycle entirely.
s=s.replace('''    else if (reason == DLL_PROCESS_DETACH)
    {''','''    else if (reason == DLL_PROCESS_DETACH)
    {
        wchar_t host[MAX_PATH]{}; GetModuleFileNameW(nullptr,host,MAX_PATH);
        const wchar_t* leaf=wcsrchr(host,L'\\\\');
        if(leaf && _wcsicmp(leaf+1,L"rundll32.exe")==0) return TRUE;''')
s=s.replace('#include "vkmirror.inc"','#include "vkmirror.inc"\n#include "deployment-panel.inc"')
s=s.replace('        PanelRegister();','        PanelRegister();\n        DeploymentRegisterEvents();')
s=s.replace('1.4.13-pre8-vk-fgrelay-20261011-coexist1','1.4.13-pre8-unified1-20261011')
p.write_text(s,encoding='utf-8')
p=root/'present-adapter-config.h';s=p.read_text(encoding='utf-8')
for key in ['Enabled','AutoRun','Follow']:
    s=s.replace('L"'+key+'", 0, path','L"'+key+'", 1, path')
s=s.replace('L"Source", L"present"','L"Source", L"fg-input"')
p.write_text(s,encoding='utf-8')
p=root/'synth.inc';s=p.read_text(encoding='utf-8')
s=s.replace('static void OnPanelDraw(reshade::api::effect_runtime *rt)','static void DeploymentDraw();\nstatic void OnPanelDraw(reshade::api::effect_runtime *rt)')
s=s.replace('''    const LONG src     = InterlockedCompareExchange(&g_source, 0, 0);''','''    DeploymentDraw();
    const LONG src     = InterlockedCompareExchange(&g_source, 0, 0);''')
p.write_text(s,encoding='utf-8')
print('Bridge integration applied')
