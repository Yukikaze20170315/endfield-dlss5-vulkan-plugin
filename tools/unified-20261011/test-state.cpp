#include <cstdio>
#include <thread>
#include <vector>
#include "../../src/unified-20261011/common/durable.hpp"
#include "../../src/unified-20261011/common/deployment-state.hpp"
#include "../../src/unified-20261011/common/panel-locale.hpp"
#include "../../src/unified-20261011/common/hook-mode.hpp"
#include "../../src/unified-20261011/generic/src/addons/dlss5/stage_plan.hpp"
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main() {
  using namespace dlss5;
  {
    namespace hm=dlss5::hook_mode;
    uint32_t stored=hm::kDefault,reads=0,writes=0;
    auto read=[&] { ++reads; return stored; };
    auto write=[&](uint32_t mode) { ++writes; stored=mode; };
    hm::Snapshot snap;
    CHECK(hm::Control(hm::Query,0,2,&snap,read,write));
    CHECK(snap.current==2 && snap.selected==2 && writes==0);
    for(uint32_t mode : {1u,0u,2u}) {
      CHECK(hm::Control(hm::Select,mode,2,&snap,read,write));
      CHECK(snap.current==2 && snap.selected==mode && stored==mode);
    }
    const auto saved_writes=writes;
    CHECK(!hm::Control(hm::Select,3,2,&snap,read,write));
    CHECK(!hm::Control(99,0,2,&snap,read,write));
    CHECK(!hm::Control(hm::Select,1,hm::kUnknown,&snap,read,write));
    CHECK(!hm::Control(hm::Select,1,2,nullptr,read,write));
    snap.version=99; CHECK(!hm::Control(hm::Select,1,2,&snap,read,write));
    snap.version=1; snap.size=0; CHECK(!hm::Control(hm::Select,1,2,&snap,read,write));
    snap.size=sizeof(snap); CHECK(writes==saved_writes);
    const auto saved_reads=reads;
    CHECK(hm::Control(hm::Current,0,1,&snap,read,write));
    CHECK(snap.current==1 && reads==saved_reads && writes==saved_writes);
    CHECK(!hm::Control(hm::Select,1,2,&snap,read,[](uint32_t){}));
    CHECK(hm::Effective(false,true)==0 && hm::Effective(true,false)==2 && hm::Effective(true,true)==1);
  }
  namespace st=renodx::addons::dlss5::stages;
  const auto root=durable::GameDirectory()/L"state-fixture";
  CHECK(durable::SamePath(L"C:\\Game\\dlss5\\sl-2.14.1\\sl.common.dll",L"c:/game/dlss5/sl-2.14.1/sl.common.dll"));
  CHECK(!durable::SamePath(L"C:\\Game\\sl.common.dll",L"C:/Game/dlss5/sl-2.14.1/sl.common.dll"));
  std::filesystem::create_directories(root);
  CHECK(dlss5_panel::SelectText("成功 / Success",true)=="成功");
  CHECK(dlss5_panel::SelectText("Success / 成功",false)=="Success");
  CHECK(dlss5_panel::SelectText("游戏无法启动时，双击 DLSS5-Recovery 故障恢复.cmd / Run DLSS5-Recovery 故障恢复.cmd if startup fails.",true)=="游戏无法启动时，双击 DLSS5-Recovery 故障恢复.cmd");
  CHECK(dlss5_panel::SelectText("foo / bar",true)=="foo / bar");
  CHECK(dlss5_panel::SelectText("游戏目录 / Game directory##deploy-sl0",true)=="游戏目录##deploy-sl0");
  CHECK(dlss5_panel::SelectText("游戏目录 / Game directory##deploy-sl0",false)=="Game directory##deploy-sl0");
  CHECK(dlss5_panel::SelectText("成功 / Success\nFailure / 失败",false)=="Success\nFailure");
  dlss5_panel::Preference preference; preference.Store(root,false);
  dlss5_panel::Preference reloaded; reloaded.Load(root); CHECK(!reloaded.chinese);
  deployment_policy::State s;
  s.selected=1;
  CHECK(!deployment_policy::Begin(s,42,false)); CHECK(s.trial==1 && s.active==1);
  CHECK(!deployment_policy::Begin(s,42,false)); CHECK(s.selected==1 && s.failures==1);
  CHECK(deployment_policy::Begin(s,42,false)); CHECK(s.selected==0 && !s.safe);
  s={}; s.selected=1; deployment_policy::Begin(s,42,false);
  CHECK(deployment_policy::Begin(s,42,true)); CHECK(s.selected==0 && !s.safe);
  s={}; deployment_policy::Begin(s,42,false); deployment_policy::Healthy(s,0);
  CHECK(!deployment_policy::Begin(s,42,true)); CHECK(s.failures==0);
  CHECK(!deployment_policy::Begin(s,42,true)); CHECK(s.failures==1);
  CHECK(deployment_policy::Begin(s,42,true)); CHECK(s.safe);
  deployment_policy::Healthy(s,0); deployment_policy::Begin(s,43,false); CHECK(s.safe);
  s={}; s.fingerprint=42; s.healthy=1; s.active=1;
  deployment_policy::Begin(s,43,false); CHECK(s.trial);
  CHECK(deployment_policy::Begin(s,43,true)); CHECK(s.safe);
  s={}; s.early=1; deployment_policy::Begin(s,1,false); CHECK(s.trial);
  CHECK(deployment_policy::Begin(s,1,true)); CHECK(s.early==0);
  {
    namespace dp=deployment_policy;
    const char none[1]={0}, crash[]="1", clean[]="C", crash_then_clean[]="1C", zeros[4]={0,0,0,0};
    CHECK(dp::ClassifyExitMark(none,0)==dp::kExitUnknown);
    CHECK(dp::ClassifyExitMark(zeros,4)==dp::kExitUnknown);
    CHECK(dp::ClassifyExitMark(crash,1)==dp::kExitCrashed);
    CHECK(dp::ClassifyExitMark(clean,1)==dp::kExitClean);
    CHECK(dp::ClassifyExitMark(crash_then_clean,2)==dp::kExitCrashed);
    // A trial that fails is rolled back with a fresh allowance.
    dp::State r; r.selected=1; dp::Begin(r,7,false);
    CHECK(dp::Begin(r,7,true)==dp::kBeginRolledBack); CHECK(r.selected==0 && r.failures==0 && !r.trial && !r.safe);
    CHECK(dp::Begin(r,7,false)==dp::kBeginNormal); CHECK(r.failures==1 && !r.safe);
    // An orderly exit before the health point is forgiven outside a trial...
    dp::State c; dp::Begin(c,7,false); dp::Healthy(c,0); dp::Begin(c,7,false);
    for(int i=0;i<5;++i) CHECK(dp::Begin(c,7,false,true)==dp::kBeginNormal);
    CHECK(c.failures==0 && !c.safe);
    // ...but counted in a trial, and never when the same launch crashed.
    dp::State t; t.selected=1; dp::Begin(t,7,false);
    CHECK(dp::Begin(t,7,false,true)==dp::kBeginNormal); CHECK(t.failures==1);
    CHECK(dp::Begin(t,7,false,true)==dp::kBeginRolledBack); CHECK(t.selected==0);
    dp::State k; dp::Begin(k,7,false); dp::Healthy(k,0); dp::Begin(k,7,false);
    CHECK(dp::Begin(k,7,true,true)==dp::kBeginNormal); CHECK(k.failures==1);
    CHECK(dp::Begin(k,7,true,false)==dp::kBeginSafeMode); CHECK(k.safe);
    // An environment change is a trial with nothing to roll back to: safe mode.
    dp::State e; dp::Begin(e,7,false); dp::Healthy(e,0); dp::Begin(e,8,false);
    CHECK(dp::Begin(e,8,true)==dp::kBeginSafeMode);
  }
  const auto file=root/L"atomic.state";
  CHECK(durable::Save(file,s)); deployment_policy::State loaded;
  CHECK(durable::Read(file,loaded,s.magic)==durable::ReadResult::Valid);
  const char corrupt[]="truncated";
  CHECK(durable::Write(file,corrupt,sizeof(corrupt)));
  CHECK(durable::Read(file,loaded,s.magic)==durable::ReadResult::Invalid);
  const auto bad_hash=durable::Sha256(file);
  CHECK(durable::ArchiveRecord(file)); CHECK(!std::filesystem::exists(file));
  bool archive_found=false;
  for(const auto& entry:std::filesystem::directory_iterator(root))
    if(entry.path().filename().wstring().find(L"atomic.state.recovered-")==0 && durable::Sha256(entry.path())==bad_hash) archive_found=true;
  CHECK(archive_found);
  CHECK(durable::Save(file,s));
  HANDLE held=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
  CHECK(held!=INVALID_HANDLE_VALUE);
  auto next=s; next.selected=1; CHECK(!durable::Save(file,next)); CloseHandle(held);
  CHECK(durable::Read(file,loaded,s.magic)==durable::ReadResult::Valid && loaded.selected==s.selected);
  // Exercise the exact production layer state and real disk writes.
  st::trial::file=root/L"generic-layers.state"; st::trial::writable=true; st::trial::record={}; st::trial::safe=false;
  st::configuration=8; st::trial::deadline=0;
  const st::Plan low{8}; const auto high=low.With(st::Point::Present,4);
  CHECK(st::trial::Request(high)); CHECK(st::Snapshot().Total()==5);
  st::trial::Record disk;
  CHECK(durable::Read(st::trial::file,disk,st::trial::kMagic)==durable::ReadResult::Valid && disk.pending && disk.confirmed==8);
  const auto deadline=st::trial::deadline.load();
  CHECK(!st::trial::Resolve(true,deadline-14999));
  CHECK(!st::trial::Resolve(true,deadline));
  CHECK(st::trial::Resolve(false)); CHECK(st::Snapshot().packed==low.packed);
  CHECK(st::trial::Request(high)); CHECK(st::trial::Resolve(true,st::trial::deadline-12999));
  CHECK(st::Snapshot().Total()==5 && st::trial::record.pending==0);
  CHECK(st::trial::Request(low)); CHECK(st::trial::record.confirmed==8);
  CHECK(st::trial::Request(high));
  CHECK(st::trial::Request(high.With(st::Point::Render,4)));
  CHECK(st::trial::record.confirmed==8);
  st::trial::deadline=GetTickCount64()-1;
  CHECK(st::Snapshot().packed==8); CHECK(st::trial::Poll()); CHECK(st::configuration==8);
  held=CreateFileW(st::trial::file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
  CHECK(held!=INVALID_HANDLE_VALUE); CHECK(!st::trial::Request(high)); CHECK(st::Snapshot().packed==8); CloseHandle(held);
  CHECK(st::trial::Request(high));
  held=CreateFileW(st::trial::file.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
  CHECK(!st::trial::Resolve(true,st::trial::deadline-12000));
  CHECK(st::trial::Resolve(false)); CHECK(st::Snapshot().packed==8); CloseHandle(held);
  CHECK(durable::Read(st::trial::file,disk,st::trial::kMagic)==durable::ReadResult::Valid && disk.pending);
  st::trial::Initialize(high,root);
  CHECK(st::Snapshot().packed==8 && !st::trial::record.pending);
  CHECK(st::trial::Request(high)); CHECK(st::trial::Resolve(true,st::trial::deadline-12000));
  st::trial::Initialize(high,root); CHECK(st::Snapshot().Total()==5 && !st::trial::deadline);
  CHECK(durable::Write(st::trial::file,corrupt,sizeof(corrupt)));
  st::trial::Initialize(high,root); CHECK(st::Snapshot().Total()==0 && st::trial::safe);
  st::trial::safe=true; CHECK(st::Snapshot().Total()==0); CHECK(!st::trial::Request(high));
  CHECK(st::configuration==high.packed); // Runtime suspension must not erase persisted parameters.
  st::trial::safe=false;
  st::trial::bridge_safe_query=+[]()->uint32_t { return 1; };
  CHECK(st::Snapshot().Total()==0); CHECK(!st::trial::Request(low)); CHECK(st::configuration==high.packed);
  st::trial::bridge_safe_query=nullptr;
  for(uint32_t r=0;r<5;++r) for(uint32_t u=0;u<5;++u) for(uint32_t p=0;p<5;++p) {
    st::Plan plan{r|(u<<3)|(p<<6)};
    CHECK(plan.Valid()); CHECK(plan.Total()==r+u+p); CHECK(plan.Offset(st::Point::Present)==r+u);
  }
  std::printf("PASS %u checks: crash/unknown failure thresholds, update/safe-mode stickiness, atomic save failures, layer trial/confirmation/timeout, 125 stage plans.\n",checks);
}
