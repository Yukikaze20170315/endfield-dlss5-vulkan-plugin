"""Integrate the complete FG policy and device-specific NR selection."""
from pathlib import Path
import json, hashlib
ROOT=Path(__file__).resolve().parents[2]
BACK=ROOT/'evidence/unified-20261011/source-before-1f'
records=json.loads((BACK/'sha256.json').read_text(encoding='utf-8'))
def edit(name,fn):
    p=ROOT/name; data=p.read_bytes(); target=BACK/name
    if not target.exists():
        target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(data)
        records[name]=hashlib.sha256(data).hexdigest()
    text=data.decode('utf-8-sig').replace('\r\n','\n')
    p.write_text(fn(text),encoding='utf-8',newline='\r\n')
def one(s,a,b):
    assert s.count(a)==1,(a[:90],s.count(a))
    return s.replace(a,b)
edit('src/unified-20261011/common/drs.hpp',lambda s:one(s,'inline Result Synchronize(', 'inline Result SynchronizeLegacy(')+'\n#include "drs-fg.hpp"\n')
def device(s):
    s=one(s,'static uint32_t architecture=0,current=0,current_early=0,current_nr=0;',
          'static uint32_t architecture=0,current=0,current_early=0,current_nr=0;\nstatic bool any_supported_gpu=false,early_cuda_needed=false;\nstatic std::atomic_bool driver_sync_requested{false};\nstatic void SynchronizeStartupDriver();')
    start=s.index('static void DetectGpu() {'); end=s.index('static void Initialize()',start)
    s=s[:start]+'''static bool SupportedArchitecture(uint32_t arch) {
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
''' + s[end:]
    s=one(s,'nr_valid=(architecture==0x160 || architecture==0x170 || architecture==0x190 || architecture==0x1b0) &&','nr_valid=any_supported_gpu &&')
    s=one(s,'(architecture==0x160 || architecture==0x170 || state.reserved)', '(early_cuda_needed || state.reserved)')
    s=one(s,'    ready=true;','    ready=true;\n    if(!safe.load()) driver_sync_requested=true;')
    s=one(s,'static void Tick() {\n', 'static void Tick() {\n    if(driver_sync_requested.exchange(false)) SynchronizeStartupDriver();\n')
    s+='''
extern "C" __declspec(dllexport) int __cdecl DLSS5GetNrRuntimePathV2(uint32_t low,int32_t high,wchar_t* out,uint32_t count) {
    if(!out || !count || !deployment::ready.load() || deployment::safe.load()) return -1;
    const LUID luid{low,high};
    const uint32_t arch=deployment::ArchitectureForLuid(luid);
    if(!deployment::SupportedArchitecture(arch) || !deployment::nr_valid) return -1;
    if(deployment::nr_path.size()+1>count) return -1;
    wcscpy_s(out,count,deployment::nr_path.c_str());
    Log("[deployment] NR selected for device LUID=%08x:%08x arch=0x%x path=%ls",static_cast<uint32_t>(high),low,arch,out);
    return 1;
}
'''
    return s
edit('src/unified-20261011/bridge/src/deployment.inc',device)
def panel(s):
    # Serialize the helper handle between the worker and the UI; process startup
    # and completion remain outside the graphics driver calls.
    s=one(s,'static HANDLE g_driver_maintenance=nullptr;', 'static HANDLE g_driver_maintenance=nullptr;\nstatic std::mutex g_driver_maintenance_mutex;')
    s=one(s,'    using namespace deployment;\n    if(g_driver_maintenance)', '    using namespace deployment;\n    std::lock_guard<std::mutex> maintenance_lock(g_driver_maintenance_mutex);\n    if(g_driver_maintenance)')
    s=one(s,'    if(g_driver_maintenance && WaitForSingleObject', '    std::lock_guard<std::mutex> maintenance_lock(g_driver_maintenance_mutex);\n    if(g_driver_maintenance && WaitForSingleObject')
    # Avoid reentrant locking when a UI click requests maintenance.
    s=s.replace('static std::mutex g_driver_maintenance_mutex;', 'static std::recursive_mutex g_driver_maintenance_mutex;').replace('std::lock_guard<std::mutex> maintenance_lock', 'std::lock_guard<std::recursive_mutex> maintenance_lock')
    marker='static void DeploymentPresent('
    where=s.index(marker)
    s=s[:where]+'''namespace deployment {
static void SynchronizeStartupDriver() {
    std::lock_guard<std::mutex> lock(mutex);
    if(safe.load()) return;
    const auto result=dlss5::drs::Synchronize(dir,false,game);
    status=result.message;
    dlss5::durable::Write(dir/L"drs-result.txt",status.data(),static_cast<DWORD>(status.size()));
    Log("[deployment] startup DRS: %s",status.c_str());
    if(result.needs_elevation) DeploymentRequestDriver(dlss5::maintenance::DriverAction::Synchronize);
    drs_snapshot=dlss5::drs::Snapshot();
}
}
''' + s[where:]
    s=s.replace('同步仅移除本游戏显式固定倍率覆盖 0x10308298、0x104D6667；保留全局及其他设置，记录原值。 / Remove only these local overrides; preserve other settings.',
                '同步 8 个公开帧生成项；本游戏的默认值屏蔽全局覆盖，其他游戏不变，原值可恢复。 / Synchronize 8 public FG settings in this game profile; shadow global overrides without changing other games and retain recovery values.')
    s=s.replace('选择 SL 并确认，插件同时同步本游戏的固定倍率覆盖；完成后重启。与 FG 开关无关。 / Confirm SL and synchronize local multiplier overrides, then restart. Independent of FG.',
                '启动时自动检测同步帧生成驱动配置。选择 SL 并确认后重启，与帧生成开关无关。 / FG driver settings are checked and synchronized at startup. Confirm SL and restart; independent of frame generation.')
    return s
edit('src/unified-20261011/bridge/src/deployment-panel.inc',panel)
def generic(s):
    s=one(s,'inline std::filesystem::path FindNrRuntimePath() {','inline std::filesystem::path FindNrRuntimePath(ID3D12Device* device) {')
    marker='  using RuntimePath = int (__cdecl*)(wchar_t*, uint32_t);'
    s=one(s,marker,'''  using RuntimePathV2 = int (__cdecl*)(uint32_t,int32_t,wchar_t*,uint32_t);
  auto selected_device = bridge ? reinterpret_cast<RuntimePathV2>(GetProcAddress(bridge,"DLSS5GetNrRuntimePathV2")):nullptr;
  if(selected_device) {
    wchar_t path[32768]={};
    if(device) {
      const LUID luid=device->GetAdapterLuid();
      if(selected_device(luid.LowPart,luid.HighPart,path,32768)>0) return std::filesystem::path(path);
    }
    return AddonDirectory()/L"dlss5-runtime-unavailable"/L"nvngx_dlssnr.dll";
  }
''' + marker)
    return one(s,'symbols.nr_path = FindNrRuntimePath();','symbols.nr_path = FindNrRuntimePath(device);')
edit('src/unified-20261011/generic/src/addons/dlss5/dlssnr.hpp',generic)
def recovery(s):
    s=one(s,'    std::puts("No pending driver changes."); return 0;', '    if(!dlss5::drs::FgPending(dir,mirror)) { std::puts("No pending driver changes."); return 0; }')
    s=one(s,'  if(result!=dlss5::durable::ReadResult::Valid) {','  if(result==dlss5::durable::ReadResult::Invalid) {')
    s=one(s,'  return result==dlss5::durable::ReadResult::Valid && !journal.pending ? 0:4;',
          '  const bool legacy_done=result==dlss5::durable::ReadResult::Missing || (result==dlss5::durable::ReadResult::Valid && !journal.pending);\n  return legacy_done && !dlss5::drs::FgPending(dir,mirror) ? 0:4;')
    return s
edit('tools/unified-20261011/restore-driver.cpp',recovery)
# Existing v1 transaction tests remain meaningful during the journal migration.
edit('tools/unified-20261011/test-drs.cpp',lambda s:s.replace('drs::Synchronize(', 'drs::SynchronizeLegacy('))
(BACK/'sha256.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print('Runtime integration complete')
