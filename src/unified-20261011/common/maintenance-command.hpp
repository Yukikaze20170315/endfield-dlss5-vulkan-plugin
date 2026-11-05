#pragma once
#include "durable.hpp"

namespace dlss5::maintenance {
// Every account-bound command that may need elevation, not only DRS work: the
// Windows VRAM budget actions reuse the same rundll32/runas channel.
enum class DriverAction { None, Synchronize, Restore, VramBudgetApply, VramBudgetRestore };
inline const char* Name(DriverAction action) {
  switch(action) {
    case DriverAction::Synchronize: return "sync-drs";
    case DriverAction::Restore: return "restore-drs";
    case DriverAction::VramBudgetApply: return "apply-vram-budget";
    case DriverAction::VramBudgetRestore: return "restore-vram-budget";
    default: return "";
  }
}
inline bool IsVramBudget(DriverAction action) {
  return action==DriverAction::VramBudgetApply || action==DriverAction::VramBudgetRestore;
}
inline uint64_t UserKey() {
  HANDLE token=nullptr;
  if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return 0;
  DWORD size=0; GetTokenInformation(token,TokenUser,nullptr,0,&size);
  std::vector<unsigned char> buffer(size);
  const bool okay=size && GetTokenInformation(token,TokenUser,buffer.data(),size,&size);
  CloseHandle(token);
  if(!okay) return 0;
  auto sid=reinterpret_cast<const TOKEN_USER*>(buffer.data())->User.Sid;
  return IsValidSid(sid) ? durable::Checksum(sid,GetLengthSid(sid)):0;
}
inline bool Elevated() {
  HANDLE token=nullptr; TOKEN_ELEVATION elevation{}; DWORD size=0;
  if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) return false;
  const bool okay=GetTokenInformation(token,TokenElevation,&elevation,sizeof(elevation),&size)!=FALSE;
  CloseHandle(token); return okay && elevation.TokenIsElevated;
}
inline DriverAction Parse(const std::string& command,uint64_t user) {
  if(!user) return DriverAction::None;
  for(const auto action:{DriverAction::Synchronize,DriverAction::Restore,DriverAction::VramBudgetApply,DriverAction::VramBudgetRestore}) {
    if(command==Name(action) || command==std::string(Name(action))+"@"+std::to_string(user)) return action;
  }
  return DriverAction::None;
}
inline std::wstring Parameters(const std::filesystem::path& module,DriverAction action,uint64_t user) {
  if(action==DriverAction::None || !user || !module.is_absolute() || module.wstring().find(L'"')!=std::wstring::npos) return {};
  const std::string name=Name(action);
  return L"\""+module.wstring()+L"\",Maintenance "+std::wstring(name.begin(),name.end())+L"@"+std::to_wstring(user);
}
inline std::filesystem::path Runner() {
  WidePathBuffer system;
  const auto n=system ? GetSystemDirectoryW(system.data(),system.capacity) : 0;
  return n && n<system.capacity ? std::filesystem::path(system.data())/L"rundll32.exe":std::filesystem::path{};
}
inline HANDLE Start(const std::filesystem::path& module,DriverAction action) {
  const auto runner=Runner(); const auto params=Parameters(module,action,UserKey());
  if(runner.empty() || params.empty() || !durable::Plain(module)) return nullptr;
  std::wstring command=L"\""+runner.wstring()+L"\" "+params;
  STARTUPINFOW startup{}; startup.cb=sizeof(startup);
  startup.dwFlags=STARTF_USESHOWWINDOW; startup.wShowWindow=SW_HIDE;
  PROCESS_INFORMATION process{};
  // Shell/UAC work stays in the maintenance process, away from game rendering.
  if(!CreateProcessW(runner.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,
                     module.parent_path().c_str(),&startup,&process)) return nullptr;
  CloseHandle(process.hThread); return process.hProcess;
}
} // namespace dlss5::maintenance
