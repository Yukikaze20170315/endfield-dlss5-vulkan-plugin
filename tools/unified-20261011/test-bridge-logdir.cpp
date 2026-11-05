// Integration fixture for the unified1s log folder, driven through the real
// dlss5-bridge.addon64 in a private temporary "game" folder.
//
//   test-bridge-logdir.exe <path to dlss5-bridge.addon64>
//
// Compiled twice: with FAKE_RESHADE as a stand-in ReShade module exporting the
// six entry points the Bridge resolves, and without it as the host. The host
// copies itself as Endfield.exe into the temporary folder, seeds files an older
// version left beside the executable (including a crash block), and creates
// dlss5\safe-mode.flag so the deployment layer stays in safe mode: no driver
// profile is read for writing, no NR runtime is loaded. Then it loads the fake
// ReShade and the Bridge, calls AddonInit, forwards events the way Generic and
// MFG Unlock do, and checks what lands in dlss5\dlss5_log.
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#ifdef FAKE_RESHADE
#include <map>
static std::map<std::string, std::string> g_config;
static int g_config_sets = 0;
extern "C" __declspec(dllexport) bool ReShadeRegisterAddon(HMODULE, uint32_t) { return true; }
extern "C" __declspec(dllexport) void ReShadeUnregisterAddon(HMODULE) {}
extern "C" __declspec(dllexport) void ReShadeRegisterEvent(int, void *) {}
extern "C" __declspec(dllexport) void ReShadeRegisterOverlay(const char *, void *) {}
extern "C" __declspec(dllexport) void ReShadeLogMessage(HMODULE, int, const char *) {}
extern "C" __declspec(dllexport) const void *ReShadeGetImGuiFunctionTable(uint32_t) { return nullptr; }
// Global ReShade.ini only (runtime == nullptr), with ReShade's size contract.
extern "C" __declspec(dllexport) bool ReShadeGetConfigValue(void *, void *runtime, const char *section, const char *key, char *value, size_t *size)
{
    if (runtime || !size) return false;
    const auto it = g_config.find(std::string(section) + "/" + key);
    if (it == g_config.end()) return false;
    if (value && *size) {
        const size_t n = it->second.size() < *size - 1 ? it->second.size() : *size - 1;
        memcpy(value, it->second.data(), n);
        value[n] = 0;
    }
    *size = it->second.size() + 1;
    return true;
}
extern "C" __declspec(dllexport) void ReShadeSetConfigValue(void *, void *runtime, const char *section, const char *key, const char *value)
{
    if (runtime) return;
    ++g_config_sets;
    g_config[std::string(section) + "/" + key] = value ? value : "";
}
extern "C" __declspec(dllexport) int FakeConfigSets() { return g_config_sets; }
extern "C" __declspec(dllexport) const char *FakeConfigGet(const char *path)
{
    const auto it = g_config.find(path);
    return it == g_config.end() ? nullptr : it->second.c_str();
}
extern "C" __declspec(dllexport) void FakeConfigPut(const char *path, const char *value) { g_config[path] = value; }
BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
#else
static int g_checks = 0, g_failures = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; std::printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static std::wstring Join(const std::wstring &a, const wchar_t *b) { return a + L"\\" + b; }
static bool Exists(const std::wstring &p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
static void WriteText(const std::wstring &p, const char *text)
{
    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    DWORD done = 0;
    WriteFile(h, text, static_cast<DWORD>(strlen(text)), &done, nullptr);
    CloseHandle(h);
}
static std::string ReadText(const std::wstring &p)
{
    std::string text;
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return text;
    char buffer[65536]; DWORD got = 0;
    while (ReadFile(h, buffer, sizeof(buffer), &got, nullptr) && got) text.append(buffer, got);
    CloseHandle(h);
    return text;
}
static std::vector<std::wstring> List(const std::wstring &dir, const wchar_t *pattern)
{
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW d{};
    HANDLE f = FindFirstFileW(Join(dir, pattern).c_str(), &d);
    if (f == INVALID_HANDLE_VALUE) return out;
    do if (!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(d.cFileName); while (FindNextFileW(f, &d));
    FindClose(f);
    return out;
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t self[MAX_PATH]{};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    const std::wstring self_path = self;
    const bool child = argc >= 3 && wcscmp(argv[1], L"--child") == 0;
    if (!child) {
        if (argc < 2) { std::printf("usage: test-bridge-logdir <dlss5-bridge.addon64>\n"); return 2; }
        wchar_t temp[MAX_PATH]{};
        GetTempPathW(MAX_PATH, temp);
        const std::wstring game = std::wstring(temp) + L"dlss5-logdir-fixture-" + std::to_wstring(GetCurrentProcessId());
        CreateDirectoryW(game.c_str(), nullptr);
        CreateDirectoryW(Join(game, L"dlss5").c_str(), nullptr);
        WriteText(Join(game, L"dlss5\\safe-mode.flag"), "fixture\n");
        WriteText(Join(game, L"dlss5-bridge.log"),
                  "old line\n### CRASH RECORDED ###\n  exception 0xC0000005 at 0000000000001234\n#####\n");
        WriteText(Join(game, L"dlss5-observe.log"), "old observe\n");
        WriteText(Join(game, L"RenoDX-DLSS5-crash.log"), "[session] old pid=1\n");
        WriteText(Join(game, L"RenoDX-DLSS5-crash-1-1.dmp"), "MDMP");
        const std::wstring dir = self_path.substr(0, self_path.find_last_of(L'\\'));
        CopyFileW(self, Join(game, L"Endfield.exe").c_str(), FALSE);
        CopyFileW(Join(dir, L"fake-reshade.dll").c_str(), Join(game, L"fake-reshade.dll").c_str(), FALSE);
        CopyFileW(argv[1], Join(game, L"dlss5-bridge.addon64").c_str(), FALSE);
        std::wstring command = L"\"" + Join(game, L"Endfield.exe") + L"\" --child \"" + game + L"\"";
        STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, game.c_str(), &si, &pi)) {
            std::printf("cannot start child %lu\n", GetLastError()); return 2;
        }
        WaitForSingleObject(pi.hProcess, 60000);
        DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        std::printf("fixture folder: %ls\n", game.c_str());
        return static_cast<int>(code);
    }

    const std::wstring game = argv[2];
    const std::wstring logs = Join(game, L"dlss5\\dlss5_log");
    if (!LoadLibraryW(Join(game, L"fake-reshade.dll").c_str())) { std::printf("fake reshade %lu\n", GetLastError()); return 2; }
    HMODULE bridge = LoadLibraryW(Join(game, L"dlss5-bridge.addon64").c_str());
    CHECK(bridge != nullptr);
    if (!bridge) { std::printf("bridge load %lu\n", GetLastError()); return 1; }
    auto addon_init = reinterpret_cast<bool (*)(HMODULE, HMODULE)>(GetProcAddress(bridge, "AddonInit"));
    auto log_event = reinterpret_cast<void(__cdecl *)(uint32_t, uint32_t, const char *)>(GetProcAddress(bridge, "DLSS5LogEventV1"));
    auto full_mode = reinterpret_cast<uint32_t(__cdecl *)()>(GetProcAddress(bridge, "DLSS5LogFullModeV1"));
    auto exported_name = reinterpret_cast<const char *const *>(GetProcAddress(bridge, "NAME"));
    CHECK(exported_name && *exported_name);
    const std::string addon_name = exported_name && *exported_name ? *exported_name : "";
    const std::string name_prefix = "DLSS 5 Bridge ";
    const bool valid_name = addon_name.compare(0, name_prefix.size(), name_prefix) == 0;
    CHECK(valid_name);
    const std::string version = valid_name ? addon_name.substr(name_prefix.size()) : "";
    CHECK(!version.empty());
    CHECK(addon_init && log_event && full_mode);
    if (!addon_init || !log_event || !full_mode) return 1;
    CHECK(addon_init(bridge, GetModuleHandleW(L"fake-reshade.dll")));
    CHECK(full_mode() == 0);

    // SkipLoadingDisabledEffects: set once per ReShade.ini, then left to the player.
    {
        const HMODULE fake = GetModuleHandleW(L"fake-reshade.dll");
        const auto sets = reinterpret_cast<int (*)()>(GetProcAddress(fake, "FakeConfigSets"));
        const auto get = reinterpret_cast<const char *(*)(const char *)>(GetProcAddress(fake, "FakeConfigGet"));
        const auto put = reinterpret_cast<void (*)(const char *, const char *)>(GetProcAddress(fake, "FakeConfigPut"));
        CHECK(sets && get && put);
        if (sets && get && put) {
            const char *skip = get("GENERAL/SkipLoadingDisabledEffects");
            CHECK(skip && strcmp(skip, "1") == 0);
            const char *marker = get("DLSS5Bridge/SkipLoadingDisabledEffectsApplied");
            CHECK(marker && strcmp(marker, "1") == 0);
            CHECK(sets() == 2);
            put("GENERAL/SkipLoadingDisabledEffects", "0");
            CHECK(addon_init(bridge, fake));
            skip = get("GENERAL/SkipLoadingDisabledEffects");
            CHECK(skip && strcmp(skip, "0") == 0);
            CHECK(sets() == 2);
            const std::string log_now = ReadText(Join(logs, L"dlss5-bridge.log"));
            CHECK(log_now.find("SkipLoadingDisabledEffects: before=<absent> now=1 (set;") != std::string::npos);
            size_t lines = 0, pos = 0;
            while ((pos = log_now.find("SkipLoadingDisabledEffects: before=", pos)) != std::string::npos) { ++lines; ++pos; }
            CHECK(lines == 1);
        }
    }

    // Generic: a warning, an anomalous info line and an ordinary info line.
    for (int i = 0; i < 5; ++i) log_event(2, 2, "DLSS5 Generic: NR feature creation failed after 4 s");
    log_event(2, 3, "DLSS5 Generic: inline evaluation timed out");
    log_event(2, 3, "DLSS5 Generic: telemetry t=30.0s missing=heaps");
    log_event(2, 3, "DLSS5 Generic: inline feature 18 evaluation succeeded");
    // MFG Unlock: a per-call decision record.
    log_event(3, 2, "mfgunlock: multiplier not applied: decision=blocked-backend panel=6x");
    log_event(3, 3, "[action] phase=config_request key=ForceMultiplier before=2 requested=6");
    log_event(3, 3, "[action] phase=config_request key=ForceMultiplier before=6 requested=2");
    log_event(3, 3, "[action] phase=config_request key=ForceMultiplier before=2 requested=6");
    Sleep(1500);
    SetLastError(1234);
    log_event(3, 3, "mfgunlock: installed 3 hook(s)");
    CHECK(GetLastError() == 1234);
    const auto journals=List(logs,L"dlss5-actions-*.log");
    CHECK(journals.size()==1);
    const auto journal=journals.empty()?std::string():ReadText(Join(logs,journals[0].c_str()));
    CHECK(journal.find("journal-start")!=std::string::npos);
    CHECK(journal.find("key=ForceMultiplier before=6 requested=2")!=std::string::npos);
    size_t edits=0,offset=0;
    while((offset=journal.find("key=ForceMultiplier",offset))!=std::string::npos){++edits;++offset;}
    CHECK(edits==3);
    CHECK(journal.find("source=mfg")!=std::string::npos);

    // Old root files moved, nothing left beside the executable.
    CHECK(!Exists(Join(game, L"dlss5-bridge.log")));
    CHECK(!Exists(Join(game, L"dlss5-observe.log")));
    CHECK(!Exists(Join(game, L"RenoDX-DLSS5-crash.log")));
    CHECK(!Exists(Join(game, L"RenoDX-DLSS5-crash-1-1.dmp")));
    CHECK(Exists(Join(logs, L"RenoDX-DLSS5-crash.log")));
    CHECK(Exists(Join(logs, L"RenoDX-DLSS5-crash-1-1.dmp")));
    CHECK(Exists(Join(logs, L"dlss5-observe.prev.log")));
    CHECK(ReadText(Join(logs, L"dlss5-observe.prev.log")) == "old observe\n");
    CHECK(Exists(Join(logs, L"dlss5-observe.log")));
    // The old main log became the previous session and its crash was carried.
    CHECK(ReadText(Join(logs, L"dlss5-bridge.prev.log")).find("### CRASH RECORDED ###") != std::string::npos);
    const std::string main_log = ReadText(Join(logs, L"dlss5-bridge.log"));
    CHECK(!version.empty() && main_log.find("dlss5-bridge " + version + " (built ") != std::string::npos);
    CHECK(main_log.find("The previous run crashed") != std::string::npos);
    CHECK(main_log.find("exception 0xC0000005") != std::string::npos);
    CHECK(main_log.find("[deployment] SL check: interposer=") != std::string::npos);
    CHECK(main_log.find("[event-log] mode at start: bounded") != std::string::npos);
    CHECK(List(game, L"*.log").empty());

    const auto events = List(logs, L"dlss5-events-2*.log");
    CHECK(events.size() == 1);
    const std::string text = events.empty() ? std::string() : ReadText(Join(logs, events[0].c_str()));
    CHECK(text.find("[generic][warn] DLSS5 Generic: NR feature creation failed after 4 s") != std::string::npos);
    CHECK(text.find("[generic][info] DLSS5 Generic: inline evaluation timed out") != std::string::npos);
    CHECK(text.find("telemetry t=") == std::string::npos);
    CHECK(text.find("evaluation succeeded") == std::string::npos);
    CHECK(text.find("[mfg][warn] mfgunlock: multiplier not applied") != std::string::npos);
    CHECK(text.find("installed 3 hook") == std::string::npos);
    CHECK(text.find("The previous run crashed") != std::string::npos);
    // Five identical Generic warnings: one full line now; the repeat summary
    // follows within the 10 s window.
    size_t count = 0, at = 0;
    while ((at = text.find("NR feature creation failed", at)) != std::string::npos) { ++count; at += 10; }
    CHECK(count == 1);
    Sleep(11000);
    const std::string later = ReadText(Join(logs, events.empty() ? L"none" : events[0].c_str()));
    CHECK(later.find("[repeat x4 in ") != std::string::npos);
    if (GetProcAddress(bridge, "Dlss5TimingBuffer") != nullptr) {
        const std::string recovery = ReadText(Join(logs, L"dlss5-bridge.log"));
        CHECK(recovery.find("[recovery-memory]") != std::string::npos);
        CHECK(recovery.find("ram_valid=1 error=0") != std::string::npos);
        CHECK(recovery.find("process_valid=1 error=0") != std::string::npos);
        CHECK(recovery.find("page_faults_soft_plus_hard=") != std::string::npos);
        CHECK(recovery.find("[recovery-waits] site=reflex_original") != std::string::npos);
        CHECK(recovery.find("[recovery-waits] site=present_original") != std::string::npos);
    }
    std::printf("%s: %d checks, %d failures\n", g_failures ? "FAIL" : "PASS", g_checks, g_failures);
    std::fflush(stdout);
    std::fflush(stderr);
    ExitProcess(g_failures ? 1 : 0);
}
#endif
