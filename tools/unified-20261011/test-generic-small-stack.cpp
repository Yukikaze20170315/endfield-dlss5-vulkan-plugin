// Executes exact production runtime helpers extracted at build time, plus the
// real debug crash reporter. All files belong to this isolated build folder.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <new>
#include <string>
#define RENODX_DLSS5_DEBUG
#include "../../src/unified-20261011/generic/src/addons/dlss5/debug.hpp"

static bool fail_path=false;
static unsigned api=1,checks=0;
static int selection=1;
static std::wstring selected_path;
static std::filesystem::path folder,hash_file,report_file;
static void Require(bool value,const char* label) {
    if(!value) { std::fprintf(stderr,"FAIL: %s error=%lu\n",label,GetLastError()); ExitProcess(1); }
    ++checks;
}
static LPVOID WINAPI FixtureHeapAlloc(HANDLE heap,DWORD flags,SIZE_T bytes) {
    if(fail_path && bytes==32768*sizeof(wchar_t)) return nullptr;
    return HeapAlloc(heap,flags,bytes);
}
#define HeapAlloc FixtureHeapAlloc
#include "../../src/unified-20261011/common/wide-path-buffer.hpp"
#undef HeapAlloc
struct ID3D12Device { LUID GetAdapterLuid() { return {4,5}; } };
static const std::filesystem::path& AddonDirectory() { return folder; }
static int __cdecl SelectedV1(wchar_t* out,uint32_t size) {
    if(selection>0) Require(wcscpy_s(out,size,selected_path.c_str())==0,"copy complete selected path");
    return selection;
}
static int __cdecl SelectedV2(uint32_t low,int32_t high,wchar_t* out,uint32_t size) {
    Require(low==4 && high==5,"device LUID forwarded"); return SelectedV1(out,size);
}
static HMODULE WINAPI FixtureGetModuleHandleW(LPCWSTR name) {
    return wcscmp(name,L"dlss5-bridge.addon64")==0 ? reinterpret_cast<HMODULE>(1):GetModuleHandleW(name);
}
static FARPROC WINAPI FixtureGetProcAddress(HMODULE module,LPCSTR name) {
    if(module!=reinterpret_cast<HMODULE>(1)) return GetProcAddress(module,name);
    if(api==2 && strcmp(name,"DLSS5GetNrRuntimePathV2")==0) return reinterpret_cast<FARPROC>(&SelectedV2);
    if(api==1 && strcmp(name,"DLSS5GetNrRuntimePathV1")==0) return reinterpret_cast<FARPROC>(&SelectedV1);
    return nullptr;
}
#define GetModuleHandleW FixtureGetModuleHandleW
#define GetProcAddress FixtureGetProcAddress
#include "generic-runtime-functions.inc"
#undef GetProcAddress
#undef GetModuleHandleW

static DWORD WINAPI Worker(void*) {
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(&memory,&memory,sizeof(memory))!=0,"query worker stack");
    Require(reinterpret_cast<uintptr_t>(reinterpret_cast<NT_TIB*>(NtCurrentTeb())->StackBase)-
            reinterpret_cast<uintptr_t>(memory.AllocationBase)==65536,"actual 64 KiB stack reservation");
    ID3D12Device device;
    const auto refused=folder/L"dlss5-runtime-unavailable"/L"nvngx_dlssnr.dll";
    const uint64_t game_luid=(uint64_t{5}<<32)|4;
    for(api=1;api<=2;++api) {
        auto found=FindNrRuntimePath(&device,0);
        Require(found.path==selected_path && !found.deferred,"complete >1024-char runtime path");
        fail_path=true;
        Require(FindNrRuntimePath(&device,0).path.empty(),"allocation failure cannot fall back to another runtime");
        fail_path=false;
        selection=-1;
        found=FindNrRuntimePath(&device,0);
        Require(found.path==refused && !found.deferred,"Bridge refusal for a named GPU stays authoritative");
        found=FindNrRuntimePath(nullptr,0);
        Require(found.path==refused && found.deferred,"refusal without a GPU identity is deferred, not final");
        selection=0;
        found=FindNrRuntimePath(&device,0);
        Require(found.path==refused && found.deferred,"bridge still starting defers even with a device");
        selection=1;
    }
    api=2;
    auto found=FindNrRuntimePath(nullptr,0);
    Require(found.path==refused && found.deferred,"device API without device or adapter LUID is deferred");
    found=FindNrRuntimePath(nullptr,game_luid);
    Require(found.path==selected_path && !found.deferred,"game adapter LUID selects the runtime without a device");
    selection=-1;
    found=FindNrRuntimePath(nullptr,game_luid);
    Require(found.path==refused && !found.deferred,"refusal for the game adapter LUID is final");
    selection=1;
    Require(ComputeRuntimeSha256(hash_file)=="BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
            "real SHA256 of abc on 64 KiB stack");
    Require(ComputeRuntimeSha256(hash_file)=="BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD",
            "cached SHA256 unchanged");
    renodx::addons::dlss5::debug::WriteCrashReport(nullptr,report_file.c_str());
    Require(std::filesystem::file_size(report_file)>0,"crash reporter runs on 64 KiB stack");
    return 0;
}
int main() {
    folder=std::filesystem::current_path();
    hash_file=folder/(L"hash-"+std::to_wstring(GetCurrentProcessId())+L".txt");
    report_file=folder/(L"crash-fixture-"+std::to_wstring(GetCurrentProcessId())+L".txt");
    { std::ofstream file(hash_file,std::ios::binary); file<<"abc"; }
    selected_path=L"C:\\";
    for(unsigned i=0;i<6;++i) selected_path+=std::wstring(190,L'x')+L"\\";
    selected_path+=L"nvngx_dlssnr.dll";
    Require(selected_path.size()>1024,"long-path precondition");
    auto worker=CreateThread(nullptr,65536,Worker,nullptr,STACK_SIZE_PARAM_IS_A_RESERVATION,nullptr);
    Require(worker!=nullptr,"create worker");
    Require(WaitForSingleObject(worker,30000)==WAIT_OBJECT_0,"worker completes");
    DWORD code=1; Require(GetExitCodeThread(worker,&code) && code==0,"worker exit");
    CloseHandle(worker);
    std::printf("PASS: %u checks; Generic runtime selection, SHA256 and debug report on 64 KiB stack\n",checks);
    return 0;
}
