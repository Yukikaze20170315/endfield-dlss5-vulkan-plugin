// The production DRS transaction runs against isolated in-memory NVAPI sessions.
// No test calls the real driver. Files stay under the executable's fixture tree.
#define NOMINMAX
#include <windows.h>
#include <map>
#include <string>
#include <cstdio>
#include <thread>
#include <functional>
#include "../../upstream/NVIDIA-nvapi-87dca62/nvapi.h"
#include "../../upstream/NVIDIA-nvapi-87dca62/NvApiDriverSettings.h"
struct Value { NvU32 value; bool predefined=false; NVDRS_SETTING_LOCATION location=NVDRS_CURRENT_PROFILE_LOCATION; NVDRS_SETTING_TYPE type=NVDRS_DWORD_TYPE; };
using Settings=std::map<NvU32,Value>;
struct FakeSession { Settings settings; };
static Settings driver;
static Settings global_driver;
static int saves=0,writes=0,loads=0,reads=0;
static NvAPI_Status init_error=NVAPI_OK,read_error=NVAPI_OK,write_error=NVAPI_OK,save_error=NVAPI_OK;
static int fail_read_at=0;
static std::function<void()> after_write;
static const auto fake_profile=reinterpret_cast<NvDRSProfileHandle>(1);
static NvAPI_Status FakeInitialize() { return init_error; }
static NvAPI_Status FakeCreate(NvDRSSessionHandle* out) { *out=reinterpret_cast<NvDRSSessionHandle>(new FakeSession); return NVAPI_OK; }
static NvAPI_Status FakeDestroy(NvDRSSessionHandle h) { delete reinterpret_cast<FakeSession*>(h); return NVAPI_OK; }
static NvAPI_Status FakeLoad(NvDRSSessionHandle h) { ++loads; reinterpret_cast<FakeSession*>(h)->settings=driver; return NVAPI_OK; }
static NvAPI_Status FakeFind(NvDRSSessionHandle,const NvU16*,NvDRSProfileHandle* out,NVDRS_APPLICATION*) { *out=fake_profile; return NVAPI_OK; }
static NvAPI_Status FakeBase(NvDRSSessionHandle,NvDRSProfileHandle* out) { *out=reinterpret_cast<NvDRSProfileHandle>(2); return NVAPI_OK; }
static NvAPI_Status FakeInfo(NvDRSSessionHandle,NvDRSProfileHandle,NVDRS_PROFILE* out) {
  const wchar_t* name=L"Endfield fixture";
  for(unsigned i=0;name[i];++i) out->profileName[i]=static_cast<NvU16>(name[i]);
  return NVAPI_OK;
}
static NvAPI_Status FakeGet(NvDRSSessionHandle h,NvDRSProfileHandle,NvU32 id,NVDRS_SETTING* out) {
  ++reads;
  if(read_error!=NVAPI_OK || (fail_read_at && reads>=fail_read_at)) return read_error!=NVAPI_OK ? read_error:NVAPI_ERROR;
  const auto& settings=reinterpret_cast<FakeSession*>(h)->settings;
  const auto it=settings.find(id);
  if(it==settings.end()) {
    const auto inherited=global_driver.find(id);
    if(inherited==global_driver.end()) return NVAPI_SETTING_NOT_FOUND;
    out->settingId=id; out->settingType=NVDRS_DWORD_TYPE; out->settingLocation=NVDRS_GLOBAL_PROFILE_LOCATION;
    out->isCurrentPredefined=0; out->u32CurrentValue=inherited->second.value; return NVAPI_OK;
  }
  out->settingId=id; out->settingType=it->second.type; out->settingLocation=it->second.location;
  out->isCurrentPredefined=it->second.predefined; out->u32CurrentValue=it->second.value; return NVAPI_OK;
}
static NvAPI_Status FakeDelete(NvDRSSessionHandle h,NvDRSProfileHandle,NvU32 id) {
  ++writes; if(write_error!=NVAPI_OK) return write_error;
  reinterpret_cast<FakeSession*>(h)->settings.erase(id); if(after_write) after_write(); return NVAPI_OK;
}
static NvAPI_Status FakeSet(NvDRSSessionHandle h,NvDRSProfileHandle,NVDRS_SETTING* value) {
  ++writes; if(write_error!=NVAPI_OK) return write_error;
  reinterpret_cast<FakeSession*>(h)->settings[value->settingId]={value->u32CurrentValue}; if(after_write) after_write(); return NVAPI_OK;
}
static NvAPI_Status FakeSave(NvDRSSessionHandle h) { ++saves; if(save_error!=NVAPI_OK) return save_error; driver=reinterpret_cast<FakeSession*>(h)->settings; return NVAPI_OK; }
#define NvAPI_Initialize FakeInitialize
#define NvAPI_DRS_CreateSession FakeCreate
#define NvAPI_DRS_DestroySession FakeDestroy
#define NvAPI_DRS_LoadSettings FakeLoad
#define NvAPI_DRS_FindApplicationByName FakeFind
#define NvAPI_DRS_GetBaseProfile FakeBase
#define NvAPI_DRS_GetProfileInfo FakeInfo
#define NvAPI_DRS_GetSetting FakeGet
#define NvAPI_DRS_DeleteProfileSetting FakeDelete
#define NvAPI_DRS_SetSetting FakeSet
#define NvAPI_DRS_SaveSettings FakeSave
#include "../../src/unified-20261011/common/drs.hpp"
namespace fs=std::filesystem;
using namespace dlss5;
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static fs::path root,game,dir;
static void Fixture(const wchar_t* name) {
  game=root/name; dir=game/L"dlss5/state"; fs::create_directories(dir);
  driver={{drs::ids[0],{1}},{drs::ids[1],{5}},{FRL_FPS_ID,{120}}};
  global_driver.clear();
  saves=writes=loads=reads=fail_read_at=0;
  init_error=read_error=write_error=save_error=NVAPI_OK; after_write={};
}
static drs::Journal Journal() {
  drs::Journal j; if(drs::ReadJournal(dir/L"drs-journal.state",durable::RecoveryMirror(game)/L"drs-journal.state",j)!=durable::ReadResult::Valid) std::abort();
  return j;
}
static drs::Result Sync(bool restore=false) { return drs::SynchronizeLegacy(dir,restore,game); }
int main() {
  root=durable::GameDirectory()/(L"drs-fixture-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
  fs::create_directories(root);
  SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str());
  Fixture(L"basic");
  CHECK(!Sync().needs_elevation); CHECK(saves==1); CHECK(driver.count(drs::ids[0])==0 && driver.count(drs::ids[1])==0);
  CHECK(driver.at(FRL_FPS_ID).value==120); CHECK(Journal().pending==1);
  Sync(true); CHECK(saves==2 && driver.at(drs::ids[0]).value==1 && driver.at(drs::ids[1]).value==5);
  CHECK(!Journal().pending); CHECK(!Journal().valid[0] && !Journal().valid[1]);
  driver.erase(drs::ids[0]); Sync(true); CHECK(!driver.count(drs::ids[0]) && saves==2);

  Fixture(L"startup-read-failure"); init_error=NVAPI_ERROR;
  CHECK(drs::Snapshot().find("unavailable")!=std::string::npos); CHECK(!writes && !saves);
  init_error=NVAPI_OK; read_error=NVAPI_ERROR;
  CHECK(drs::Snapshot().find("status=")!=std::string::npos); CHECK(!writes && !saves);
  Sync(); CHECK(!writes && !saves && !fs::exists(dir/L"drs-journal.state"));

  Fixture(L"restore-read-failure"); Sync(); read_error=NVAPI_ERROR;
  const int old_writes=writes; Sync(true); CHECK(writes==old_writes && saves==1); CHECK(Journal().pending);
  read_error=NVAPI_OK; Sync(true); CHECK(!Journal().pending && saves==2);

  Fixture(L"wrong-type"); driver.at(drs::ids[0]).type=NVDRS_BINARY_TYPE;
  Sync(); CHECK(!writes && !saves);
  Fixture(L"inherited"); driver.at(drs::ids[0]).location=NVDRS_GLOBAL_PROFILE_LOCATION; driver.at(drs::ids[1]).predefined=true;
  Sync(); CHECK(!writes && !saves);

  Fixture(L"external-conflict"); Sync(); driver[drs::ids[1]]={9};
  const auto result=Sync(true); CHECK(result.message.find("External")!=std::string::npos);
  CHECK(driver.at(drs::ids[0]).value==1 && driver.at(drs::ids[1]).value==9 && !Journal().pending);
  driver.erase(drs::ids[0]); driver.erase(drs::ids[1]); Sync(true);
  CHECK(!driver.count(drs::ids[0]) && !driver.count(drs::ids[1]) && saves==2);

  Fixture(L"permission-retry"); save_error=NVAPI_INVALID_USER_PRIVILEGE;
  CHECK(Sync().needs_elevation); CHECK(driver.at(drs::ids[0]).value==1 && Journal().pending);
  save_error=NVAPI_OK; CHECK(!Sync().needs_elevation); CHECK(!driver.count(drs::ids[0]) && Journal().old[0]==1);
  write_error=NVAPI_INVALID_USER_PRIVILEGE; CHECK(Sync(true).needs_elevation && Journal().pending);
  write_error=NVAPI_OK; Sync(true); CHECK(!Journal().pending && driver.at(drs::ids[1]).value==5);

  Fixture(L"ordinary-write-error"); write_error=NVAPI_ERROR;
  CHECK(!Sync().needs_elevation && !saves && Journal().pending);
  Fixture(L"concurrent-driver-change"); after_write=[] { driver[drs::ids[0]]={7}; };
  Sync(); CHECK(!saves && driver.at(drs::ids[0]).value==7 && Journal().pending);

  Fixture(L"verification-read-failure"); fail_read_at=7;
  CHECK(Sync().message.find("verification read failed")!=std::string::npos); CHECK(saves==1 && Journal().pending);
  fail_read_at=0; Sync(true); CHECK(!Journal().pending);

  Fixture(L"newer-mirror"); Sync(); auto older=Journal(),newer=older;
  newer.pending=0; newer.valid[0]=newer.valid[1]=0; ++newer.generation;
  CHECK(durable::Save(durable::RecoveryMirror(game)/L"drs-journal.state",newer));
  Sync(true); CHECK(saves==1 && !driver.count(drs::ids[0]) && !Journal().pending);

  Fixture(L"missing-primary"); Sync(); CHECK(durable::ArchiveRecord(dir/L"drs-journal.state"));
  Sync(true); CHECK(!Journal().pending && driver.at(drs::ids[0]).value==1);
  Fixture(L"same-generation-conflict"); Sync(); auto bad=Journal(); bad.old[0]=99;
  CHECK(durable::Save(durable::RecoveryMirror(game)/L"drs-journal.state",bad));
  CHECK(Sync(true).message.find("Invalid DRS")!=std::string::npos && saves==1);
  Fixture(L"semantic-corruption"); Sync(); bad=Journal(); bad.valid[0]=3;
  CHECK(durable::Save(dir/L"drs-journal.state",bad));
  CHECK(Sync(true).message.find("Invalid DRS")!=std::string::npos && saves==1);

  Fixture(L"busy");
  { drs::TransactionLock held; CHECK(held.owned); bool refused=false;
    std::thread peer([&]{refused=Sync().message.find("transaction")!=std::string::npos;}); peer.join(); CHECK(refused && !writes && !saves); }
  std::printf("PASS %u DRS checks: isolated NVAPI, startup read failures, transactions, permission retries, external changes, dual journals and lock.\n",checks);
  return 0;
}
