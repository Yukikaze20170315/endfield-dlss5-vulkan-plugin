#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cassert>
#include <vector>
#include <fstream>
#include <iterator>
#include <string>
#include <cstring>
#include <intrin.h>

namespace reshade { namespace api {
struct effect_runtime { void *window; void *get_hwnd() const { return window; } };
} }
struct Hook { void *target = nullptr, *original = nullptr; bool active = false; };
static void TriggerHookScan() {}
static bool ForegroundIsThisProcess() { return false; }
static void FgtLog(const char *, ...) {}
static void Log(const char *, ...) {}
static HANDLE StartModuleWorker(LPTHREAD_START_ROUTINE) { return nullptr; }
static bool test_keep = false, test_protocol = false, test_plan_valid = true;
static uint32_t test_plan = 0;
static int test_hook_mode = 1;
static bool PresentAdapterFocusKeepFG() { return test_keep; }
static void PresentAdapterPath(wchar_t *path, size_t count, const wchar_t *leaf) {
    assert(GetModuleFileNameW(nullptr, path, static_cast<DWORD>(count)) != 0);
    wchar_t *slash = wcsrchr(path, L'\\');
    assert(slash != nullptr);
    slash[1] = 0;
    assert(wcscat_s(path, count, leaf) == 0);
}
static bool PresentAdapterStageProtocol() { return test_protocol; }
static bool PresentAdapterStagePlan(uint32_t *plan) { *plan = test_plan; return test_plan_valid; }
static int GenericCurrentHookMode(const wchar_t *, int) { return test_hook_mode; }
static bool PinHookModule(const void *) { return true; }
static bool HookInstall(Hook &, void *, void *) { return false; }
#include "focus-fg.inc"

static unsigned free_calls, original_calls;
static const void *expected_viewport;
static bool original_result;
static bool Original(void *) { ++original_calls; return original_result; }
static bool Capture(void *) { g_focus_fg.caller = _ReturnAddress(); return false; }
static PFN_FocusFgHasFocus volatile invoke_target;
__declspec(noinline) static bool CallerA()
{
    volatile bool result = invoke_target(nullptr);
    _ReadWriteBarrier();
    return result;
}
__declspec(noinline) static bool CallerB()
{
    volatile bool result = invoke_target(reinterpret_cast<void *>(1));
    _ReadWriteBarrier();
    return result;
}
static uint32_t Free(uint32_t feature, const void *viewport)
{
    assert(feature == 1000 && viewport == expected_viewport);
    ++free_calls;
    return 456u;
}

static void TestStages()
{
    unsigned checked = 0;
    for (uint32_t plan = 0; plan < 512; ++plan)
        for (uint32_t mode = 0; mode < 4; ++mode)
            for (unsigned neural = 0; neural < 2; ++neural) {
                const bool valid = (plan & 7u) <= 4 && ((plan >> 3) & 7u) <= 4 && ((plan >> 6) & 7u) <= 4;
                const bool expected = neural && (mode == 1 || mode == 2) && valid && plan != 0;
                assert(focus_fg::NrActive(neural != 0, mode, true, plan, 2) == expected);
                ++checked;
            }
    assert(!focus_fg::NrActive(true, 1, true, 0x200u, 2));
    for (int point = -1; point <= 3; ++point)
        for (uint32_t mode = 0; mode < 4; ++mode)
            for (unsigned neural = 0; neural < 2; ++neural) {
                const bool expected = neural && (mode == 1 || mode == 2) && point >= 0 && point <= 2;
                assert(focus_fg::NrActive(neural != 0, mode, false, 0, point) == expected);
                ++checked;
            }

    wchar_t ini[1024] = {};
    PresentAdapterPath(ini, _countof(ini), L"ReShade.ini");
    assert(WritePrivateProfileStringW(L"RenoDX.DLSS5", L"NeuralUplift", L"1", ini));
    assert(WritePrivateProfileStringW(L"RenoDX.DLSS5", L"NRHookPoint", L"2", ini));
    test_protocol = true;
    for (uint32_t render = 0; render <= 4; ++render)
        for (uint32_t upscaled = 0; upscaled <= 4; ++upscaled)
            for (uint32_t present = 0; present <= 4; ++present)
                for (unsigned keep = 0; keep < 2; ++keep) {
                    test_plan = render | (upscaled << 3) | (present << 6);
                    test_keep = keep != 0;
                    g_focus_fg.next_poll = 0;
                    FocusFgMaintenance();
                    assert((g_focus_fg_enabled != 0) == (test_plan != 0 && test_keep));
                    assert((g_composition_guard_wanted != 0) == (test_plan != 0));
                }
    // Transition out of an active plan must disable both policies, even if a
    // legacy point remains in the INI. Corrupt protocol data is not legacy.
    test_plan = 1;
    test_plan_valid = false;
    g_focus_fg.next_poll = 0;
    FocusFgMaintenance();
    assert(!g_focus_fg_enabled && !g_composition_guard_wanted);
    test_plan_valid = true;
    test_hook_mode = 0;
    g_focus_fg.next_poll = 0;
    FocusFgMaintenance();
    assert(!g_focus_fg_enabled && !g_composition_guard_wanted);
    test_hook_mode = 2;
    g_focus_fg.next_poll = 0;
    FocusFgMaintenance();
    assert(g_focus_fg_enabled && g_composition_guard_wanted);
    assert(WritePrivateProfileStringW(L"RenoDX.DLSS5", L"NeuralUplift", L"0", ini));
    g_focus_fg.next_poll = 0;
    FocusFgMaintenance();
    assert(!g_focus_fg_enabled && !g_composition_guard_wanted);
    printf("stages: %u policy combinations; 250 maintenance transitions; invalid/disabled/NGX-only PASS\n", checked);
}

static void TestPolicy()
{
    // Exhaust every guard independently and in combination. An original true
    // is always forwarded; false changes only when all eight guards are met.
    for (unsigned bits = 0; bits < 512; ++bits) {
        bool v[9];
        for (unsigned i = 0; i < 9; ++i) v[i] = (bits & (1u << i)) != 0;
        const bool expected = v[0] || (v[1] && v[2] && v[3] && v[4] && v[5] && !v[6] && v[7] && v[8]);
        assert(focus_fg::Override(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8]) == expected);
    }
    g_focus_fg.focus.original = reinterpret_cast<void *>(&Original);
    invoke_target = Capture;
    assert(!CallerA());
    assert(g_focus_fg.caller != nullptr);
    invoke_target = FocusFgHasFocusDetour;
    original_result = false;
    assert(!CallerA());
    assert(g_focus_fg_calls == 1 && g_focus_fg_original_false == 1);
    assert(!CallerB());
    assert(g_focus_fg_calls == 1 && g_focus_fg_original_false == 1);
    original_result = true;
    assert(CallerA());
    assert(CallerB());
    assert(g_focus_fg_calls == 2 && original_calls == 4);

    const HWND foreground = GetForegroundWindow();
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"", WS_POPUP,
        10, 10, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    assert(window != nullptr);
    reshade::api::effect_runtime visible{window};
    FocusFgSetRuntime(&visible);
    InterlockedExchange(&g_focus_fg_enabled, 1);
    original_result = false;
    assert(!CallerA()); // hidden
    ShowWindow(window, SW_SHOWNOACTIVATE);
    assert(CallerA()); // unfocused but drawable, exactly the known FG caller
    assert(!CallerB());
    assert(GetForegroundWindow() == foreground);
    SetWindowPos(window, nullptr, 10, 10, 65, 64, SWP_NOACTIVATE | SWP_NOZORDER);
    assert(!CallerA()); // stale swapchain size
    FocusFgSetRuntime(&visible);
    assert(CallerA());
    ShowWindow(window, SW_SHOWMINNOACTIVE);
    assert(!CallerA());
    ShowWindow(window, SW_SHOWNOACTIVATE);
    FocusFgSetRuntime(&visible);
    assert(CallerA());
    InterlockedExchange(&g_focus_fg_enabled, 0);
    assert(!CallerA());
    InterlockedExchange(&g_focus_fg_enabled, 1);
    FgtOnDestroyRuntime(&visible);
    assert(!CallerA());
    DestroyWindow(window);
    InterlockedExchange(&g_focus_fg_enabled, 0);

    reshade::api::effect_runtime first{ nullptr }, other{ nullptr };
    FocusFgSetRuntime(&first);
    assert(FocusFgWindowSnapshot().runtime == &first);
    FgtOnDestroyRuntime(&other);
    assert(FocusFgWindowSnapshot().runtime == &first);
    FgtOnDestroyRuntime(&first);
    assert(FocusFgWindowSnapshot().runtime == nullptr && FocusFgWindowSnapshot().window == nullptr);

    expected_viewport = &first;
    g_focus_fg.free_resources.original = reinterpret_cast<void *>(&Free);
    assert(FocusFgFreeDetour(1000, expected_viewport) == 456);
    assert(free_calls == 1);
    FocusFgOnPresent();
    FocusFgMaintenance();
    puts("policy: 512 guards; exact return-address scope; runtime retirement; free passthrough PASS");
}

static std::vector<uint8_t> MapDisk(const wchar_t *path)
{
    FILE *file = nullptr;
    _wfopen_s(&file, path, L"rb");
    assert(file != nullptr);
    fseek(file, 0, SEEK_END);
    const long length = ftell(file);
    assert(length > 4096);
    fseek(file, 0, SEEK_SET);
    std::vector<uint8_t> disk(static_cast<size_t>(length));
    assert(fread(disk.data(), 1, disk.size(), file) == disk.size());
    fclose(file);
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(disk.data());
    assert(dos->e_magic == IMAGE_DOS_SIGNATURE);
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(disk.data() + dos->e_lfanew);
    assert(nt->Signature == IMAGE_NT_SIGNATURE);
    std::vector<uint8_t> image(nt->OptionalHeader.SizeOfImage);
    memcpy(image.data(), disk.data(), nt->OptionalHeader.SizeOfHeaders);
    const auto *sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        assert(focus_fg::Within(sections[i].VirtualAddress, sections[i].SizeOfRawData, image.size()));
        assert(focus_fg::Within(sections[i].PointerToRawData, sections[i].SizeOfRawData, disk.size()));
        memcpy(image.data() + sections[i].VirtualAddress, disk.data() + sections[i].PointerToRawData, sections[i].SizeOfRawData);
    }
    return image;
}

static void TestRuntimeContract(const wchar_t *path, bool report_only)
{
    auto image = MapDisk(path);
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(image.data());
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(image.data() + dos->e_lfanew);
    const auto *sections = IMAGE_FIRST_SECTION(nt);
    focus_fg::Site site;
    unsigned found = 0;
    size_t start = 0, size = 0;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (!(sections[i].Characteristics & IMAGE_SCN_CNT_CODE)) continue;
        if (focus_fg::Find(image.data(), image.size(), sections[i].VirtualAddress, sections[i].Misc.VirtualSize, &site)) {
            ++found;
            start = sections[i].VirtualAddress;
            size = sections[i].Misc.VirtualSize;
        }
    }
    printf("disk contract search: matches=%u return RVA=0x%zx object slot RVA=0x%zx\n", found,
           found ? site.return_address : static_cast<size_t>(0), found ? site.object_slot : static_cast<size_t>(0));
    if (report_only) return;
    assert(found == 1 && site.return_address == 0x4c0a7 && site.object_slot == 0x94450);
    printf("disk contract: unique return RVA=0x%zx object slot RVA=0x%zx PASS (no DLL load)\n", site.return_address, site.object_slot);
    const size_t call = site.return_address - 13;
    // Unknown call implementation, malformed jump and truncated region refuse.
    image[call + 12] = 0x20;
    assert(!focus_fg::Find(image.data(), image.size(), start, size, &site));
    image[call + 12] = 0x18;
    image[call + 30] ^= 1;
    assert(!focus_fg::Find(image.data(), image.size(), start, size, &site));
    image[call + 30] ^= 1;
    assert(!focus_fg::Find(image.data(), image.size(), call, 73, &site));
    assert(!focus_fg::Find(image.data(), image.size(), image.size() - 1, 4, &site));
    // A second match is ambiguous; leave the runtime unchanged.
    std::vector<uint8_t> duplicate(image);
    memcpy(duplicate.data() + call - 0x200, image.data() + call, 74);
    const size_t copy = call - 0x200;
    for (size_t displacement : { 3u, 17u, 30u, 70u }) {
        int32_t value = 0;
        memcpy(&value, duplicate.data() + copy + displacement, sizeof(value));
        value += 0x200;
        memcpy(duplicate.data() + copy + displacement, &value, sizeof(value));
    }
    assert(!focus_fg::Find(duplicate.data(), duplicate.size(), start, size, &site));
    puts("contract rejection: changed ABI, different branches, truncation, out of bounds, ambiguous sites PASS");
}

int wmain(int argc, wchar_t **argv)
{
    // --report-only prints the contract search for another sl.dlss_g.dll without
    // asserting the RVAs of the Streamline 2.14.1 build this test was written for.
    const bool report_only = argc == 3 && wcscmp(argv[2], L"--report-only") == 0;
    assert(argc == 2 || report_only);
    TestPolicy();
    TestStages();
    TestRuntimeContract(argv[1], report_only);
    if (report_only) { puts("report only"); return 0; }
    puts("focus FG tests PASS");
}
