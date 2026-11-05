#include <fstream>
#include <cstdio>
#include "../../src/unified-20261011/common/maintenance-command.hpp"
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) { std::printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main() {
  using namespace dlss5; namespace m=maintenance; namespace fs=std::filesystem;
  const auto key=m::UserKey(); CHECK(key!=0);
  CHECK(m::Parse("sync-drs",key)==m::DriverAction::Synchronize);
  CHECK(m::Parse("restore-drs@"+std::to_string(key),key)==m::DriverAction::Restore);
  CHECK(m::Parse("restore-drs@"+std::to_string(key+1),key)==m::DriverAction::None);
  CHECK(m::Parse("sync-drs & arbitrary-command",key)==m::DriverAction::None);
  CHECK(m::Parse("sync-drs",0)==m::DriverAction::None);
  CHECK(m::Parse("apply-vram-budget",key)==m::DriverAction::VramBudgetApply);
  CHECK(m::Parse("restore-vram-budget@"+std::to_string(key),key)==m::DriverAction::VramBudgetRestore);
  CHECK(m::Parse("apply-vram-budget@"+std::to_string(key+1),key)==m::DriverAction::None);
  CHECK(m::IsVramBudget(m::DriverAction::VramBudgetApply) && m::IsVramBudget(m::DriverAction::VramBudgetRestore));
  CHECK(!m::IsVramBudget(m::DriverAction::None) && !m::IsVramBudget(m::DriverAction::Synchronize) && !m::IsVramBudget(m::DriverAction::Restore));
  CHECK(m::Parameters(L"relative.addon64",m::DriverAction::Synchronize,key).empty());
  CHECK(m::Parameters(L"C:\\bad\"name.addon64",m::DriverAction::Synchronize,key).empty());
  CHECK(m::Parameters(L"C:\\Game\\bridge.addon64",m::DriverAction::None,key).empty());
  const auto root=durable::GameDirectory()/(L"维护 空格 & 括号(1)-"+std::to_wstring(GetTickCount64()));
  fs::create_directories(root);
  const auto dll=root/L"maintenance-probe.addon64";
  fs::copy_file(durable::GameDirectory()/L"maintenance-probe.addon64",dll);
  // The probe DLL only records the command it received, so no action here
  // writes driver settings or the registry, and none asks for elevation.
  for(const auto action:{m::DriverAction::Synchronize,m::DriverAction::Restore,
                         m::DriverAction::VramBudgetApply,m::DriverAction::VramBudgetRestore}) {
    HANDLE process=m::Start(dll,action); CHECK(process!=nullptr);
    const auto waited=WaitForSingleObject(process,15000); CloseHandle(process); CHECK(waited==WAIT_OBJECT_0);
    std::ifstream file(root/L"received.txt",std::ios::binary);
    const std::string actual((std::istreambuf_iterator<char>(file)),{});
    CHECK(actual==std::string(m::Name(action))+"@"+std::to_string(key));
  }
  std::printf("PASS %u maintenance checks: strict DRS and VRAM budget actions, account binding and real rundll32 launch in Chinese/space/ampersand path. No driver or registry writes and no UAC prompt.\n",checks);
  return 0;
}
