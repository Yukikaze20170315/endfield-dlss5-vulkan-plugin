#include <windows.h>
#include <cstdint>
#include <cstring>
extern "C" __declspec(dllexport) int slInit(const void* raw,uint64_t sdk) {
  if(!raw) return -1;
  const auto* bytes=static_cast<const unsigned char*>(raw);
  const wchar_t** paths=nullptr; uint32_t count=0; uint64_t flags=0;
  std::memcpy(&paths,bytes+40,sizeof(paths)); std::memcpy(&count,bytes+48,sizeof(count));
  std::memcpy(&flags,bytes+88,sizeof(flags));
  if(sdk!=123 || flags!=0x85) return -2;
  return count==1 && paths && paths[0] && wcscmp(paths[0],L"fixture-path")==0 ? 0 : 9;
}
