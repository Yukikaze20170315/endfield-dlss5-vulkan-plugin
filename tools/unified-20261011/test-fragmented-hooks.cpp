// Reproduce a fragmented low address range in this test process only.
// The fixture DLL has no entry point, imports, GPU calls or game dependencies.
#include "../../src/unified-20261011/mfg/addon/ngx_hook.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <string>
#include <vector>

extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(void*, uint32_t) { return true; }
extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(void*) {}
static std::vector<std::string> logs;
extern "C" __declspec(dllexport) void ReShadeLogMessage(void*, int, const char* text) {
  logs.emplace_back(text ? text : "");
}
using Fn = int(__cdecl*)(int);
static mfgunlock::hook::AddressHook hooks[64];
template<size_t I> int Replacement(int value) {
  return hooks[I].Original<Fn>()(value) + 1000;
}
template<size_t... I> constexpr auto Replacements(std::index_sequence<I...>) {
  return std::array<Fn,sizeof...(I)>{&Replacement<I>...};
}
static void Require(bool ok,const char* text) {
  if(!ok) { std::printf("FAIL: %s error=%lu\n",text,GetLastError()); std::exit(1); }
}
int main(int argc,char** argv) {
  Require(argc==3,"arguments: full|small|one|none success|partial|failure");
  const bool one_gap=strcmp(argv[1],"one")==0;
  const bool small_gaps=strcmp(argv[1],"small")==0 || one_gap;
  const bool empty=strcmp(argv[1],"none")==0;
  const bool expect_success=strcmp(argv[2],"success")==0;
  const bool expect_partial=strcmp(argv[2],"partial")==0;
  auto module=LoadLibraryW(L"fragment-target.dll");
  Require(module==reinterpret_cast<HMODULE>(0x6edd0000ull),"fixed fixture mapping");
  auto replacements=Replacements(std::make_index_sequence<64>{});
  Fn targets[64]{};
  for(unsigned i=0;i<64;++i) {
    char name[32];sprintf_s(name,"Target%u",i);
    targets[i]=reinterpret_cast<Fn>(GetProcAddress(module,name));
    Require(targets[i] && targets[i](7)==7+static_cast<int>(i),"original fixture");
  }
  // Leave three small aligned gaps. The rest of the +/-2GB range is reserved,
  // without committing physical memory. The OS releases all reservations on exit.
  const uintptr_t slots[]={0x078e0000,0x07cf0000,0x09000000};
  using Alloc2=void*(WINAPI*)(HANDLE,void*,SIZE_T,ULONG,ULONG,void*,ULONG);
  auto alloc2=reinterpret_cast<Alloc2>(GetProcAddress(GetModuleHandleW(L"kernelbase.dll"),"VirtualAlloc2"));
  Require(alloc2!=nullptr,"VirtualAlloc2 fixture support");
  for(auto slot:slots) {
    auto p=alloc2(GetCurrentProcess(),reinterpret_cast<void*>(slot),0x10000,
                 MEM_RESERVE|MEM_RESERVE_PLACEHOLDER,PAGE_NOACCESS,nullptr,0);
    Require(p==reinterpret_cast<void*>(slot),"reserve gap placeholder");
    if(small_gaps) Require(VirtualFree(p,0x1000,MEM_RELEASE|MEM_PRESERVE_PLACEHOLDER)!=0,"split placeholder");
  }
  constexpr uintptr_t lo=0x80000,hi=0xeee60000;
  size_t reservations=0;
  for(uintptr_t at=lo;at<hi;) {
    MEMORY_BASIC_INFORMATION m{};
    Require(VirtualQuery(reinterpret_cast<void*>(at),&m,sizeof(m))!=0,"query fixture range");
    auto end=std::min(hi,reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize);
    auto aligned=(at+0xffff)&~uintptr_t(0xffff);
    if(m.State==MEM_FREE && aligned<end) {
      auto p=VirtualAlloc(reinterpret_cast<void*>(aligned),end-aligned,MEM_RESERVE,PAGE_NOACCESS);
      Require(p==reinterpret_cast<void*>(aligned),"reserve free fixture interval");
      ++reservations;
    }
    Require(end>at,"address progress");at=end;
  }
  const unsigned gaps=empty?0:(one_gap?1:3);
  for(unsigned i=0;i<gaps;++i)
    Require(VirtualFree(reinterpret_cast<void*>(slots[i]),0,MEM_RELEASE)!=0,"open bounded gap");
  size_t free64=0,free4=0;
  for(uintptr_t at=lo;at<hi;) {
    MEMORY_BASIC_INFORMATION m{};Require(VirtualQuery(reinterpret_cast<void*>(at),&m,sizeof(m))!=0,"verify range");
    auto end=std::min(hi,reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize);
    auto aligned=(at+0xffff)&~uintptr_t(0xffff);
    if(m.State==MEM_FREE && aligned<end) {free64+=(end-aligned)>=0x10000;free4+=(end-aligned)>=0x1000;}
    at=end;
  }
  Require(free64==(small_gaps||empty?0:3) && free4==gaps,"fragmentation precondition");
  unsigned installed=0;
  for(unsigned i=0;i<64;++i) {
    bool ok=mfgunlock::hook::InstallAddress(hooks[i],reinterpret_cast<void*>(targets[i]),
       reinterpret_cast<void*>(replacements[i]),"fragment-fixture");
    if(!expect_success && (!expect_partial || !ok)) {
      Require(!ok && !hooks[i].installed.load() && hooks[i].trampoline.load()==nullptr,"failed attach clears state");
      Require(!logs.empty() && logs.back().find("stage=attach error=8")!=std::string::npos,"allocation error diagnostic");
      Require(targets[i](7)==7+static_cast<int>(i),"failed hook preserves target");
      if(expect_partial) Require(installed>0 && installed<64,"page capacity exhaustion");
      for(unsigned j=0;j<installed;++j)
        Require(targets[j](7)==1007+static_cast<int>(j),"failed attach preserves earlier hooks");
      break;
    }
    Require(ok,"attach fragmented hook");++installed;
    Require(targets[i](7)==1007+static_cast<int>(i),"replacement and original trampoline");
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(hooks[i].trampoline.load(),&memory,sizeof(memory))!=0,"query trampoline");
    Require(VirtualQuery(memory.AllocationBase,&memory,sizeof(memory))!=0,"query trampoline allocation");
    Require(memory.RegionSize==(small_gaps?0x1000:0x10000) &&
        memory.Protect==PAGE_EXECUTE_READ,"allocation size and executable-only protection");
  }
  if(expect_success) Require(installed==64,"all fixture hooks installed");
  if(expect_partial) Require(installed>0 && installed<64,"bounded partial result");
  for(unsigned i=0;i<installed;++i) {
    mfgunlock::hook::UninstallAddress(hooks[i]);
    Require(!hooks[i].installed.load() && targets[i](7)==7+static_cast<int>(i),"detach restores entry");
  }
  for(unsigned i=0;i<gaps;++i) {
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(reinterpret_cast<void*>(slots[i]),&memory,sizeof(memory))!=0,"query released region");
    Require(memory.State==MEM_FREE,"empty trampoline region released");
    if(small_gaps) {
      Require(VirtualQuery(reinterpret_cast<void*>(slots[i]+0x1000),&memory,sizeof(memory))!=0,"query neighbor");
      Require(memory.State==MEM_RESERVE,"adjacent reserved pages preserved");
    }
  }
  std::printf("PASS: mode=%s free64k=%zu free4k=%zu reservations=%zu hooked/detached=%u expected=%s\n",
      argv[1],free64,free4,reservations,installed,argv[2]);
  return 0;
}
