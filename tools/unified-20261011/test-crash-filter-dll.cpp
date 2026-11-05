// Test-only DLL for test-crash-filter-chain.cpp: a filter that lives in a
// module which the test unloads while the chain still records it.
#include <windows.h>

extern "C" __declspec(dllexport) LONG WINAPI DllFilter(EXCEPTION_POINTERS*) { return 7; }
