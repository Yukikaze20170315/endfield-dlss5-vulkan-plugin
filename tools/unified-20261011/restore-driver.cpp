// Game-external recovery for the clean-up option of the recovery menu
// (DLSS5-Clean-Recovery.ps1). Never loads ReShade or any addon DLL, so it
// still works after the add-ons were disabled (.off) or are damaged.
// 1. Restores the plugin's driver profile (DRS) changes.
// 2. Restores the Windows VRAM budget values when this PC carries the
//    add-on's 64/64 or a backup of an earlier Apply; untouched PCs are skipped.
// Exit codes: 0 done, 2 bad arguments, 3 game running or the process check
// failed, 4 driver journal not restored, 5 driver restore needs elevation,
// 6 VRAM budget not restored, 8 VRAM budget not restored and driver changes
// not restored (4 or 5; the message printed above tells which).
#include "../../src/unified-20261011/common/drs.hpp"
#include "../../src/unified-20261011/common/vram-budget.hpp"
#include <cstdio>
#include <tlhelp32.h>

static int RestoreDriverProfile(const std::filesystem::path& game,const std::filesystem::path& dir,const std::filesystem::path& mirror) {
  if(dir.empty() || mirror.empty()) return 4;
  dlss5::drs::Journal journal;
  auto result=dlss5::drs::ReadJournal(dir/L"drs-journal.state",mirror/L"drs-journal.state",journal);
  if(result==dlss5::durable::ReadResult::Missing || (result==dlss5::durable::ReadResult::Valid && !journal.pending)) {
    if(!dlss5::drs::FgPending(dir,mirror)) { std::puts("No pending driver changes."); return 0; }
  }
  if(result==dlss5::durable::ReadResult::Invalid) {
    std::puts("Invalid driver journal; originals retained."); return 4;
  }
  const auto restored=dlss5::drs::Synchronize(dir,true,game);
  std::puts(restored.message.c_str());
  if(restored.needs_elevation) return 5;
  result=dlss5::drs::ReadJournal(dir/L"drs-journal.state",mirror/L"drs-journal.state",journal);
  const bool legacy_done=result==dlss5::durable::ReadResult::Missing || (result==dlss5::durable::ReadResult::Valid && !journal.pending);
  return legacy_done && !dlss5::drs::FgPending(dir,mirror) ? 0:4;
}

// Results are "中文 / English"; the PowerShell host may not decode UTF-8, so
// only the English half is printed.
static void PrintEnglish(const std::string& message) {
  const auto split=message.find(" / ");
  std::puts(split==std::string::npos ? message.c_str():message.c_str()+split+3);
}

static int RestoreVramBudget(const std::filesystem::path& dir,const std::filesystem::path& mirror) {
  namespace vb=dlss5::vram_budget;
  const vb::Policy before=vb::Query();
  if(!before.readable) { std::puts("Cannot read the Windows VRAM budget settings; not restored."); return 6; }
  if(vb::Classify(before)!=vb::Kind::Candidate && !vb::HasBackup(dir,mirror)) {
    std::puts("Windows VRAM budget: not changed by this add-on."); return 0;
  }
  PrintEnglish("Windows VRAM budget: "+vb::Restore(dir,mirror));
  // Restore refuses values someone else changed; only a 64/64 left behind is a failure.
  return vb::Classify(vb::Query())==vb::Kind::Candidate ? 6:0;
}

int wmain(int argc, wchar_t** argv) {
  if (argc != 2) return 2;
  const auto game=std::filesystem::absolute(argv[1]).lexically_normal();
  if (!dlss5::durable::Plain(game) || !std::filesystem::is_regular_file(game/L"Endfield.exe")) return 2;
  HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
  if(snapshot==INVALID_HANDLE_VALUE) return 3;
  PROCESSENTRY32W process{}; process.dwSize=sizeof(process);
  bool running=false,checked=Process32FirstW(snapshot,&process)!=FALSE;
  if(checked) do { if(_wcsicmp(process.szExeFile,L"Endfield.exe")==0) running=true; } while(Process32NextW(snapshot,&process));
  CloseHandle(snapshot);
  if(!checked || running) return 3;
  const auto dir=dlss5::durable::StateDirectory(game), mirror=dlss5::durable::RecoveryMirror(game);
  const int driver=RestoreDriverProfile(game,dir,mirror);
  // Runs even when the driver step failed: the two are independent.
  const int vram=RestoreVramBudget(dir,mirror);
  // Neither failure may hide the other.
  if(driver!=0 && vram!=0) return 8;
  return driver!=0 ? driver:vram;
}
