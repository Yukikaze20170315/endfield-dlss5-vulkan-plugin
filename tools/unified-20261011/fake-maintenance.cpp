#include "../../src/unified-20261011/common/maintenance-command.hpp"
static HMODULE self=nullptr;
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) { if(reason==DLL_PROCESS_ATTACH) self=module; return TRUE; }
extern "C" __declspec(dllexport) void CALLBACK Maintenance(HWND,HINSTANCE,LPSTR command,int) {
  const std::string action=command?command:"";
  if(dlss5::maintenance::Parse(action,dlss5::maintenance::UserKey())==dlss5::maintenance::DriverAction::None) return;
  wchar_t module[32768]{}; GetModuleFileNameW(self,module,32768);
  const auto out=std::filesystem::path(module).parent_path()/L"received.txt";
  dlss5::durable::Write(out,action.data(),static_cast<DWORD>(action.size()));
}
