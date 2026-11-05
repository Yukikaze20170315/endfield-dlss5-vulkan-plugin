// Fixed-base fixture for the MinHook in-module relay fallback.  Two exported
// "carrier" functions are raw machine code placed in dedicated executable
// sections:
//
//   .rhook  - CarrierEval: 13 bytes, so the rest of its section's last page
//             is a wide zero slack the fallback can write the relay into.
//   .rhook2 - CarrierEval2: the same code plus a fill array that takes the
//             section's VirtualSize to exactly one page, leaving no slack.
//
// Both begin with the five-byte prologue 48 89 5C 24 08 (mov [rsp+8], rbx)
// that MatchesExtendedPrologue requires.  The body is lea eax,[rcx*8+3]; ret,
// so CarrierEval(x) == 8*x + 3.
//
// Built by build-sr-relay-fallback.cmd with /FIXED /BASE so the test can
// reserve every free region inside ±2 GiB of the image.
#include <windows.h>

#pragma section(".rhook", read, execute)
#pragma section(".rhook2$a", read, execute)
#pragma section(".rhook2$b", read, execute)

extern "C" __declspec(dllexport)
__declspec(allocate(".rhook"))
unsigned char CarrierEval[] = {
    0x48, 0x89, 0x5C, 0x24, 0x08,              // mov [rsp+8], rbx
    0x8D, 0x04, 0xCD, 0x03, 0x00, 0x00, 0x00,  // lea eax, [rcx*8+3]
    0xC3,                                      // ret
    0x90, 0x90, 0x90,                          // pad the export to 16 bytes
};

extern "C" __declspec(dllexport)
__declspec(allocate(".rhook2$a"))
unsigned char CarrierEval2[] = {
    0x48, 0x89, 0x5C, 0x24, 0x08,
    0x8D, 0x04, 0xCD, 0x03, 0x00, 0x00, 0x00,
    0xC3,
    0x90, 0x90, 0x90,
};

__declspec(allocate(".rhook2$b"))
unsigned char CarrierEval2Fill[0x1000 - sizeof(CarrierEval2)] = {0x90};

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) { return TRUE; }
