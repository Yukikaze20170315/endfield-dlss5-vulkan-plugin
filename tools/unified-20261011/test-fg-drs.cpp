// Reuse the isolated NVAPI implementation; no real driver APIs are linked.
#define main LegacyDrsMain
#include "test-drs.cpp"
#undef main
static drs::Result FgSync(bool restore=false) { return drs::Synchronize(dir,restore,game); }
static drs::FgJournal FgRecord() {
  drs::FgJournal j;
  if(drs::ReadFgJournal(dir,durable::RecoveryMirror(game),j)!=durable::ReadResult::Valid) std::abort();
  return j;
}
int main() {
  root=durable::GameDirectory()/(L"fg-drs-fixture-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
  fs::create_directories(root); SetEnvironmentVariableW(L"LOCALAPPDATA",root.c_str());
  Fixture(L"all-eight");
  for(unsigned i=0;i<drs::fg_count;++i) driver[drs::fg_ids[i]]={i+1};
  FgSync(); CHECK(saves==1 && FgRecord().pending);
  for(unsigned i=0;i<drs::fg_count;++i) { CHECK(driver.at(drs::fg_ids[i]).value==0); CHECK(FgRecord().old[i]==i+1); }
  CHECK(driver.at(FRL_FPS_ID).value==120);
  FgSync(true); CHECK(!FgRecord().pending);
  for(unsigned i=0;i<drs::fg_count;++i) CHECK(driver.at(drs::fg_ids[i]).value==i+1);
  Fixture(L"global-shadow"); driver.clear();
  for(unsigned i=0;i<drs::fg_count;++i) global_driver[drs::fg_ids[i]]={i+9};
  FgSync(); CHECK(saves==1);
  for(unsigned i=0;i<drs::fg_count;++i) { CHECK(driver.at(drs::fg_ids[i]).value==0); CHECK(global_driver.at(drs::fg_ids[i]).value==i+9); CHECK(!FgRecord().existed[i]); }
  FgSync(true); CHECK(driver.empty() && !FgRecord().pending);
  for(unsigned i=0;i<drs::fg_count;++i) CHECK(global_driver.at(drs::fg_ids[i]).value==i+9);
  Fixture(L"read-error"); read_error=NVAPI_ERROR; FgSync(); CHECK(!writes && !saves);
  Fixture(L"permission-retry"); save_error=NVAPI_INVALID_USER_PRIVILEGE;
  CHECK(FgSync().needs_elevation && FgRecord().pending && driver.at(drs::ids[0]).value==1);
  save_error=NVAPI_OK; FgSync(); CHECK(FgRecord().old[0]==1 && driver.at(drs::ids[0]).value==0);
  write_error=NVAPI_INVALID_USER_PRIVILEGE; CHECK(FgSync(true).needs_elevation && FgRecord().pending);
  write_error=NVAPI_OK; FgSync(true); CHECK(!FgRecord().pending && driver.at(drs::ids[1]).value==5);
  Fixture(L"external-change"); FgSync(); driver[drs::ids[0]]={7}; FgSync(true);
  CHECK(driver.at(drs::ids[0]).value==7 && !FgRecord().pending);
  Fixture(L"external-resync"); FgSync(); driver[drs::ids[0]]={7}; FgSync();
  CHECK(driver.at(drs::ids[0]).value==0 && FgRecord().old[0]==7);
  FgSync(true); CHECK(driver.at(drs::ids[0]).value==7 && driver.at(drs::ids[1]).value==5);
  Fixture(L"concurrent"); after_write=[] { driver[drs::ids[0]]={8}; };
  FgSync(); CHECK(!saves && driver.at(drs::ids[0]).value==8 && FgRecord().pending);
  Fixture(L"primary-missing"); FgSync(); CHECK(durable::ArchiveRecord(dir/L"drs-fg.state"));
  FgSync(true); CHECK(!FgRecord().pending && driver.at(drs::ids[0]).value==1);
  Fixture(L"contradictory"); FgSync(); auto bad=FgRecord(); bad.old[0]=99;
  CHECK(durable::Save(dir/L"drs-fg.state",bad)); FgSync(true); CHECK(saves==1 && driver.at(drs::ids[0]).value==0);
  Fixture(L"legacy-migration"); Sync(); CHECK(Journal().pending);
  FgSync(); CHECK(!Journal().pending && FgRecord().old[0]==1 && driver.at(drs::ids[0]).value==0);
  FgSync(true); CHECK(!FgRecord().pending && driver.at(drs::ids[1]).value==5);
  Fixture(L"no-overrides"); driver.clear(); FgSync(); CHECK(!saves && !writes);
  Fixture(L"busy-fg");
  { drs::TransactionLock held; CHECK(held.owned); std::thread peer([]{ FgSync(); }); peer.join(); CHECK(!writes && !saves); }
  std::printf("PASS %u FG DRS checks: 8 IDs, inherited overrides, permissions, v1 migration, external changes, journals and transaction lock.\n",checks);
  return 0;
}
