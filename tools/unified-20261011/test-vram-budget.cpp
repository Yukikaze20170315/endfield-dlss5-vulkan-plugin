// Windows VRAM budget policy (src/unified-20261011/common/vram-budget.hpp)
// against a private HKCU key: no HKLM write, no dxgmms2.sys check, no UAC.
#include <cstdio>
#include <thread>
#include "../../src/unified-20261011/common/vram-budget.hpp"
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
namespace vb=dlss5::vram_budget;

static bool SetDword(const vb::Target& target,const wchar_t* name,DWORD data) {
  HKEY key=nullptr;
  if(RegCreateKeyExW(target.root,target.path.c_str(),0,nullptr,REG_OPTION_NON_VOLATILE,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS) return false;
  const bool okay=RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&data),sizeof(data))==ERROR_SUCCESS;
  RegCloseKey(key);
  return okay;
}
static bool SetText(const vb::Target& target,const wchar_t* name,const wchar_t* text) {
  HKEY key=nullptr;
  if(RegCreateKeyExW(target.root,target.path.c_str(),0,nullptr,REG_OPTION_NON_VOLATILE,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS) return false;
  const DWORD size=static_cast<DWORD>((wcslen(text)+1)*sizeof(wchar_t));
  const bool okay=RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(text),size)==ERROR_SUCCESS;
  RegCloseKey(key);
  return okay;
}
static bool Is(const vb::Policy& policy,DWORD fragmentation,DWORD cap) {
  return policy.readable && policy.values[0].exists && policy.values[0].dword && policy.values[0].data==fragmentation &&
         policy.values[1].exists && policy.values[1].dword && policy.values[1].data==cap;
}
static bool StartsWith(const std::string& text,const char* prefix) { return text.rfind(prefix,0)==0; }
// ArchiveRecord names a moved-aside backup by process id and GetTickCount64,
// whose resolution is about 16 ms. Real Apply calls each run in a new process.
static void NextTick() { Sleep(40); }

static int Run(const vb::Target& target,const std::filesystem::path& root) {
  namespace fs=std::filesystem;
  const auto state=root/L"state",mirror=root/L"mirror",bare=root/L"no-backup";
  for(const auto& folder:{state,mirror,bare}) { std::error_code made; fs::create_directories(folder,made); CHECK(!made); }

  vb::Policy unreadable;
  CHECK(vb::Classify(unreadable)==vb::Kind::Unknown);
  CHECK(!vb::RestartPending(unreadable,0));

  // A missing key means the Windows defaults.
  vb::Policy policy=vb::Query(target);
  CHECK(policy.readable && !policy.key_exists && vb::Classify(policy)==vb::Kind::Default);
  CHECK(vb::LastChange(state)==0);

  // Apply saves the backup in both folders and records the change for the restart notice.
  CHECK(StartsWith(vb::Apply(state,mirror,target),"已设为 64/64"));
  policy=vb::Query(target);
  CHECK(vb::Classify(policy)==vb::Kind::Candidate);
  CHECK(fs::exists(state/vb::kBackupLeaf) && fs::exists(mirror/vb::kBackupLeaf));
  CHECK(vb::LastChange(state)!=0 && vb::RestartPending(policy,vb::LastChange(state)));
  CHECK(StartsWith(vb::Apply(state,mirror,target),"已经是 64/64"));

  // Restore deletes the key again because it did not exist before Apply.
  CHECK(StartsWith(vb::Restore(state,mirror,target),"已恢复应用前的原值"));
  policy=vb::Query(target);
  CHECK(policy.readable && !policy.key_exists && vb::Classify(policy)==vb::Kind::Default);
  CHECK(vb::RestartPending(policy,vb::LastChange(state)));
  CHECK(StartsWith(vb::Restore(state,mirror,target),"已经是应用前的原值"));

  // A custom policy of 64 or more comes back exactly.
  CHECK(SetDword(target,vb::kNames[0],128) && SetDword(target,vb::kNames[1],96));
  CHECK(vb::Classify(vb::Query(target))==vb::Kind::Custom);
  NextTick();
  CHECK(StartsWith(vb::Apply(state,mirror,target),"已设为 64/64"));
  CHECK(vb::Classify(vb::Query(target))==vb::Kind::Candidate);
  CHECK(StartsWith(vb::Restore(state,mirror,target),"已恢复应用前的原值"));
  CHECK(Is(vb::Query(target),128,96));

  // A value changed by someone else after Apply is never overwritten, while a
  // mix of 64 and the original value, as an interrupted Apply leaves it, is restored.
  NextTick();
  CHECK(StartsWith(vb::Apply(state,mirror,target),"已设为 64/64"));
  CHECK(SetDword(target,vb::kNames[1],100));
  CHECK(StartsWith(vb::Restore(state,mirror,target),"设置已被其他程序改动"));
  CHECK(Is(vb::Query(target),64,100));
  CHECK(SetDword(target,vb::kNames[1],96));
  CHECK(StartsWith(vb::Restore(state,mirror,target),"已恢复应用前的原值"));
  CHECK(Is(vb::Query(target),128,96));

  // Only one value written (a part-way Apply): the panel offers Off, and Apply
  // reuses the backup, so Restore still returns the real original afterwards.
  CHECK(!vb::PartlyApplied(state,mirror,vb::Query(target),target));
  CHECK(SetDword(target,vb::kNames[0],64));
  CHECK(vb::PartlyApplied(state,mirror,vb::Query(target),target));
  NextTick();
  CHECK(StartsWith(vb::Apply(state,mirror,target),"已设为 64/64"));
  CHECK(Is(vb::Query(target),64,64) && !vb::PartlyApplied(state,mirror,vb::Query(target),target));
  CHECK(StartsWith(vb::Restore(state,mirror,target),"已恢复应用前的原值"));
  CHECK(Is(vb::Query(target),128,96));
  CHECK(SetDword(target,vb::kNames[1],64));
  CHECK(StartsWith(vb::Restore(state,mirror,target),"已恢复应用前的原值"));
  CHECK(Is(vb::Query(target),128,96));
  CHECK(SetDword(target,vb::kNames[1],100));
  CHECK(!vb::PartlyApplied(state,mirror,vb::Query(target),target));
  CHECK(SetDword(target,vb::kNames[1],96));

  // The copy in the second folder is used when the state folder lost its backup.
  NextTick();
  CHECK(StartsWith(vb::Apply(state,mirror,target),"已设为 64/64"));
  std::error_code moved;
  fs::rename(state/vb::kBackupLeaf,state/L"moved-aside.state",moved);
  CHECK(!moved);
  CHECK(StartsWith(vb::Restore(state,mirror,target),"已恢复应用前的原值"));
  CHECK(Is(vb::Query(target),128,96));

  // Values below 64, non-DWORD values and percentage settings are left alone.
  CHECK(SetDword(target,vb::kNames[0],32));
  CHECK(StartsWith(vb::Apply(state,mirror,target),"现有设置不是 64 或更大的 DWORD"));
  CHECK(Is(vb::Query(target),32,96));
  CHECK(SetText(target,vb::kNames[0],L"64"));
  policy=vb::Query(target);
  CHECK(policy.values[0].exists && !policy.values[0].dword && vb::Classify(policy)==vb::Kind::Custom);
  CHECK(StartsWith(vb::Apply(state,mirror,target),"现有设置不是 64 或更大的 DWORD"));
  CHECK(SetDword(target,vb::kNames[0],128) && SetDword(target,vb::kPercentNames[0],8));
  CHECK(vb::Query(target).percent_custom);
  CHECK(StartsWith(vb::Apply(state,mirror,target),"已有自定义的百分比设置"));
  CHECK(SetDword(target,vb::kPercentNames[0],vb::kPercentDefaults[0]));
  CHECK(!vb::Query(target).percent_custom);

  // A backup made on another PC is ignored. Without a backup, Restore removes
  // only an exact 64/64 and keeps the key while other values remain in it.
  vb::Backup other;
  other.machine=vb::Machine()+1; other.key_existed=1;
  other.present[0]=other.present[1]=1; other.data[0]=other.data[1]=200;
  other.created=vb::Now();
  CHECK(dlss5::durable::Save(bare/vb::kBackupLeaf,other));
  CHECK(StartsWith(vb::Restore(bare,fs::path(),target),"没有本机的备份"));
  CHECK(Is(vb::Query(target),128,96));
  CHECK(SetDword(target,vb::kNames[0],64) && SetDword(target,vb::kNames[1],64));
  CHECK(StartsWith(vb::Restore(bare,fs::path(),target),"没有找到备份"));
  policy=vb::Query(target);
  CHECK(policy.key_exists && vb::Classify(policy)==vb::Kind::Default && !policy.percent_custom);

  // The machine-wide third copy survives a moved or cleaned game folder, and
  // the newest backup on this PC wins over an older one left in a game folder.
  vb::Target wide=target;
  wide.machine_backup=root/L"machine";
  const auto moved_game=root/L"moved-game",other_game=root/L"other-game";
  for(const auto& folder:{moved_game,other_game}) { std::error_code made; fs::create_directories(folder,made); CHECK(!made); }
  CHECK(vb::MachineBackupDirectory(target).empty());
  CHECK(!vb::HasBackup(moved_game,fs::path(),target));
  NextTick();
  CHECK(StartsWith(vb::Apply(state,mirror,wide),"已设为 64/64"));
  CHECK(fs::exists(wide.machine_backup/vb::kBackupLeaf));
  CHECK(vb::HasBackup(moved_game,fs::path(),wide) && !vb::HasBackup(moved_game,fs::path(),target));
  CHECK(StartsWith(vb::Restore(moved_game,fs::path(),wide),"已恢复应用前的原值"));
  policy=vb::Query(target);
  CHECK(policy.key_exists && vb::Classify(policy)==vb::Kind::Default);
  CHECK(SetDword(target,vb::kNames[0],128) && SetDword(target,vb::kNames[1],96));
  NextTick();
  CHECK(StartsWith(vb::Apply(other_game,fs::path(),wide),"已设为 64/64"));
  // state and mirror still hold the older backup (key without values).
  CHECK(StartsWith(vb::Restore(state,mirror,wide),"已恢复应用前的原值"));
  CHECK(Is(vb::Query(target),128,96));
  // A newer backup made on another PC in the machine folder is still ignored.
  NextTick();
  CHECK(StartsWith(vb::Apply(other_game,fs::path(),wide),"已设为 64/64"));
  other.created=vb::Now()+1000000000ull;
  NextTick();
  CHECK(dlss5::durable::ArchiveRecord(wide.machine_backup/vb::kBackupLeaf) &&
        dlss5::durable::Save(wide.machine_backup/vb::kBackupLeaf,other));
  CHECK(StartsWith(vb::Restore(moved_game,fs::path(),wide),"没有找到备份"));
  CHECK(vb::Classify(vb::Query(target))==vb::Kind::Default);

  // One operation at a time: the lock is a named mutex shared with the PowerShell tool.
  HANDLE held=CreateEventW(nullptr,TRUE,FALSE,nullptr),release=CreateEventW(nullptr,TRUE,FALSE,nullptr);
  CHECK(held && release);
  std::thread holder([held,release] { vb::PolicyLock lock; SetEvent(held); WaitForSingleObject(release,INFINITE); });
  WaitForSingleObject(held,INFINITE);
  const std::string busy=vb::Apply(state,mirror,target);
  SetEvent(release); holder.join(); CloseHandle(held); CloseHandle(release);
  CHECK(StartsWith(busy,"另一个显存预算操作正在进行"));
  return 0;
}

int main() {
  const auto tick=std::to_wstring(GetTickCount64());
  vb::Target target;
  target.root=HKEY_CURRENT_USER;
  target.path=L"Software\\DLSS5-vram-budget-test-"+tick;
  target.check_reader=false;
  const auto root=dlss5::durable::GameDirectory()/(L"显存 预算 & 测试(1)-"+tick);
  const int failed=Run(target,root);
  RegDeleteTreeW(HKEY_CURRENT_USER,target.path.c_str());
  if(!failed) std::printf("PASS %u VRAM budget checks: classification, apply/restore round trips, backup in two folders plus a machine-wide copy (newest same-PC backup wins), refusal of foreign, percentage and externally changed values, cross-thread lock. Private HKCU key only (deleted afterwards); no HKLM write and no UAC prompt. Fixture folder kept beside the executable.\n",checks);
  return failed;
}
