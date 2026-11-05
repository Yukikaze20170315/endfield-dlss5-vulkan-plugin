// Fixture for the unified1q anomaly event log and the interposer allow-list.
// Exercises common/event-log.hpp, bridge/src/event-sink.h and
// common/payload.hpp in a private temporary folder; touches nothing else.
#include "../../src/unified-20261011/bridge/src/event-sink.h"
#include "../../src/unified-20261011/common/payload.hpp"
#include <cstdio>
#include <string>
#include <vector>

static int g_checks = 0, g_failures = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; std::printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static std::wstring g_root;
static uint64_t g_free_bytes = UINT64_MAX;
static uint64_t FakeFree(const wchar_t *) { return g_free_bytes; }

static std::wstring MakeDir(const wchar_t *name)
{
    std::wstring dir = g_root + L"\\" + name;
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}
static std::vector<std::wstring> List(const std::wstring &dir, const wchar_t *pattern)
{
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((dir + L"\\" + pattern).c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return out;
    do if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(data.cFileName); while (FindNextFileW(find, &data));
    FindClose(find);
    return out;
}
static std::string ReadAll(const std::wstring &path)
{
    std::string text;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return text;
    char buffer[65536]; DWORD got = 0;
    while (ReadFile(h, buffer, sizeof(buffer), &got, nullptr) && got != 0) text.append(buffer, got);
    CloseHandle(h);
    return text;
}
static std::string ReadFamily(const std::wstring &dir, const wchar_t *pattern)
{
    std::string text;
    for (const auto &name : List(dir, pattern)) text += ReadAll(dir + L"\\" + name);
    return text;
}
static size_t Count(const std::string &text, const char *needle)
{
    size_t n = 0, at = 0;
    while ((at = text.find(needle, at)) != std::string::npos) { ++n; at += strlen(needle); }
    return n;
}
static void Touch(const std::wstring &path, size_t bytes, int age_minutes)
{
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    std::string fill(bytes, 'x');
    DWORD done = 0;
    WriteFile(h, fill.data(), static_cast<DWORD>(fill.size()), &done, nullptr);
    FILETIME now; GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER t; t.LowPart = now.dwLowDateTime; t.HighPart = now.dwHighDateTime;
    t.QuadPart -= static_cast<ULONGLONG>(age_minutes) * 60ull * 10000000ull;
    FILETIME old{ t.LowPart, t.HighPart };
    SetFileTime(h, nullptr, nullptr, &old);
    CloseHandle(h);
}

static void TestClassifier()
{
    using namespace dlss5::eventlog;
    CHECK(LooksAnomalous("mfgunlock: slDLSSGSetOptions hook failed"));
    CHECK(LooksAnomalous("mfgunlock: Blackwell multiplier unchanged: provider not confirmed; original generated=3"));
    CHECK(LooksAnomalous("[pq-cause] wait timed out after 905 ms"));
    CHECK(LooksAnomalous("[focus-fg] unavailable: unique Streamline focus-gate contract was not found"));
    CHECK(LooksAnomalous("The previous run crashed. What it recorded at the time:"));
    CHECK(LooksAnomalous("[wait-cause] synth12 recovered stale_wakes=1"));
    CHECK(LooksAnomalous("nvngx_dlssnr.dll is missing"));
    CHECK(!LooksAnomalous("mfgunlock::hook: installed 3 hook(s) in kernel32.dll"));
    CHECK(!LooksAnomalous("Throttle depth=2 waits=0 timeouts=0 errors=0 failures=0"));
    CHECK(!LooksAnomalous("using the default value"));
    CHECK(!LooksAnomalous("loaded C:\\Game\\UnityCrashHandler64.exe"));
    CHECK(!LooksAnomalous("DLSS5 Generic: telemetry t=30.0s missing=heaps hook_exceptions=0"));
    CHECK(!LooksAnomalous("DLSS5 Generic: NR-VERDICT v3 state=ENGAGED missing=heaps"));
    CHECK(!LooksAnomalous("[fg-census] presents=120 failures=4"));
    CHECK(!LooksAnomalous(""));
    CHECK(!LooksAnomalous(nullptr));
    CHECK(LooksAnomalous("errors=3 seen"));
    CHECK(!LooksAnomalous("guard_lost=0 divisor_lost=0"));
    CHECK(LooksAnomalous("DLSS5 Generic: NR-WARN enter pumping tone=amber"));

    CHECK(KeyOf(kBridge, "waited 905 ms at 0x7ff612") == KeyOf(kBridge, "waited 1200 ms at 0x1234ab"));
    CHECK(KeyOf(kBridge, "waited 905 ms") != KeyOf(kGeneric, "waited 905 ms"));
    CHECK(KeyOf(kBridge, "hook failed") != KeyOf(kBridge, "hook installed"));
}

static void TestAllowList()
{
    using dlss5::payload::KnownInterposer;
    CHECK(KnownInterposer("39925bb5216ee1b3d164d1b9dac9e127869c2bcd7ea60f29c1c24b416de7a1f4"));
    CHECK(KnownInterposer("fabb19bb6495d743182c1bc51bcbbdc4481acc3dcaf445403f0b304a31a56ccc"));
    CHECK(!KnownInterposer("8c87c9499461da561edd529aa9bf7831d67d7b94ebb1c1a5ed54ef4934e1ea4c"));
    CHECK(!KnownInterposer("fabb19bb6495d743182c1bc51bcbbdc4481acc3dcaf445403f0b304a31a56cc"));
    CHECK(!KnownInterposer("fabb19bb6495d743182c1bc51bcbbdc4481acc3dcaf445403f0b304a31a56ccc0"));
    CHECK(!KnownInterposer(""));
    CHECK(!KnownInterposer(nullptr));
    static_assert(_countof(dlss5::payload::interposers) == 2, "two verified interposer builds");
}

static void TestLogDirectory()
{
    const std::wstring game = MakeDir(L"game");
    wchar_t out[MAX_PATH]{};
    CHECK(dlss5::eventlog::EnsureLogDirectory(game.c_str(), out, MAX_PATH));
    CHECK(std::wstring(out) == game + L"\\dlss5\\dlss5_log");
    CHECK(dlss5::eventlog::EnsureLogDirectory(game.c_str(), out, MAX_PATH));   // idempotent
    const std::wstring missing = g_root + L"\\no\\such\\parent";
    CHECK(!dlss5::eventlog::EnsureLogDirectory(missing.c_str(), out, MAX_PATH));
}

static void TestBounded()
{
    const std::wstring dir = MakeDir(L"bounded");
    EventSink sink;
    EventSink::Limits limits;
    limits.repeat_window_ms = 1000;
    sink.Configure(dir.c_str(), limits, &FakeFree);
    for (int i = 0; i < 100; ++i) {
        char text[64]; _snprintf_s(text, _TRUNCATE, "[pq-cause] wait timed out after %d ms", 900 + i);
        sink.Add(dlss5::eventlog::kBridge, dlss5::eventlog::kWarning, text);
    }
    sink.Add(dlss5::eventlog::kMfg, dlss5::eventlog::kWarning, "mfgunlock: multiplier not applied: decision=blocked-backend");
    sink.Flush(10000);
    std::string text = ReadFamily(dir, L"dlss5-events-2*.log");
    CHECK(Count(text, "wait timed out after 900 ms") == 1);
    CHECK(Count(text, "wait timed out after 9") == 1);
    CHECK(Count(text, "[mfg][warn] mfgunlock: multiplier not applied") == 1);
    CHECK(Count(text, "[repeat x") == 0);
    sink.Flush(11500);
    text = ReadFamily(dir, L"dlss5-events-2*.log");
    CHECK(Count(text, "[repeat x99 in 1.5s, total 100] [pq-cause] wait timed out after 999 ms") == 1);
    const EventSink::Stats s = sink.Snapshot();
    CHECK(!s.full);
    CHECK(s.suppressed_session == 99);
    CHECK(s.seen == 101);
    CHECK(List(dir, L"dlss5-events-full-*.log").empty());
}

static void TestFull()
{
    const std::wstring dir = MakeDir(L"full");
    EventSink sink;
    EventSink::Limits limits;
    limits.full_part = 4096;
    sink.Configure(dir.c_str(), limits, &FakeFree);
    sink.SetFull(true);
    for (int i = 0; i < 200; ++i) {
        char text[64]; _snprintf_s(text, _TRUNCATE, "[hitch] present interval %d ms", 100 + i);
        sink.Add(dlss5::eventlog::kSystem, dlss5::eventlog::kWarning, text);
    }
    sink.Flush(1000);
    const auto parts = List(dir, L"dlss5-events-full-*.log");
    CHECK(parts.size() >= 3);
    const std::string text = ReadFamily(dir, L"dlss5-events-full-*.log");
    CHECK(Count(text, "[system][warn] [hitch] present interval") == 200);
    CHECK(Count(text, "[repeat x") == 0);
    CHECK(List(dir, L"dlss5-events-2*.log").empty());
    // Switching back to bounded opens a bounded file and leaves the full parts.
    sink.SetFull(false);
    sink.Add(dlss5::eventlog::kBridge, dlss5::eventlog::kWarning, "after switch failed");
    sink.Flush(2000);
    CHECK(List(dir, L"dlss5-events-2*.log").size() == 1);
    CHECK(List(dir, L"dlss5-events-full-*.log").size() == parts.size());
}

static void TestBudgetAndRetention()
{
    const std::wstring dir = MakeDir(L"budget");
    // 40 old bounded files of 1 KB and 2 full-mode files: retention keeps 29 old
    // bounded files beside the current one and never touches full-mode files.
    for (int i = 0; i < 40; ++i) {
        wchar_t name[MAX_PATH]; _snwprintf_s(name, _TRUNCATE, L"%ls\\dlss5-events-20260101-0000%02d-%d.log", dir.c_str(), i, 100 + i);
        Touch(name, 1024, 1000 - i);
    }
    Touch(dir + L"\\dlss5-events-full-20260101-000000-1.log", 2048, 5000);
    Touch(dir + L"\\dlss5-events-full-20260101-000000-1-p2.log", 2048, 5000);
    Touch(dir + L"\\unrelated.log", 10, 9000);
    EventSink sink;
    EventSink::Limits limits;
    limits.bounded_part = 2048;
    limits.bounded_parts = 2;
    sink.Configure(dir.c_str(), limits, &FakeFree);
    for (int i = 0; i < 300; ++i) {
        char text[96]; _snprintf_s(text, _TRUNCATE, "distinct anomaly number word%c%c failed", 'a' + (i % 26), 'a' + (i / 26));
        sink.Add(dlss5::eventlog::kBridge, dlss5::eventlog::kWarning, text);
    }
    sink.Flush(1000);
    const auto bounded = List(dir, L"dlss5-events-2*.log");
    CHECK(bounded.size() <= 30);
    CHECK(List(dir, L"dlss5-events-full-*.log").size() == 2);
    CHECK(List(dir, L"unrelated.log").size() == 1);
    // The oldest synthetic files went first.
    CHECK(List(dir, L"dlss5-events-20260101-000000-100.log").empty());
    CHECK(List(dir, L"dlss5-events-20260101-000039-139.log").size() == 1);
    std::string session;
    size_t session_files = 0;
    for (const auto &name : bounded)
        if (name.find(L"20260101") == std::wstring::npos) { session += ReadAll(dir + L"\\" + name); ++session_files; }
    CHECK(session_files == 2);
    CHECK(Count(session, "bounded session budget of") == 1);
    CHECK(session.size() < 2048 * 2 + 1024);
    const EventSink::Stats s = sink.Snapshot();
    CHECK(s.budget_hit);
}

static void TestDiskFloorAndQueue()
{
    const std::wstring dir = MakeDir(L"floor");
    EventSink sink;
    EventSink::Limits limits;
    limits.space_check_ms = 0;
    limits.paused_check_ms = 0;
    limits.queue_cap_bytes = 4096;
    sink.Configure(dir.c_str(), limits, &FakeFree);
    sink.SetFull(true);
    g_free_bytes = 1ull << 30;   // 1 GB < 4 GB floor
    for (int i = 0; i < 5; ++i) sink.Add(dlss5::eventlog::kBridge, dlss5::eventlog::kWarning, "while paused failed");
    sink.Flush(100);
    std::string text = ReadFamily(dir, L"dlss5-events-full-*.log");
    CHECK(Count(text, "writing paused") == 1);
    CHECK(Count(text, "while paused failed") == 0);
    CHECK(sink.Snapshot().paused);
    g_free_bytes = 5ull << 30;   // between floor and resume: still paused
    sink.Flush(200);
    CHECK(sink.Snapshot().paused);
    g_free_bytes = 7ull << 30;
    sink.Add(dlss5::eventlog::kBridge, dlss5::eventlog::kWarning, "after resume failed");
    sink.Flush(300);
    text = ReadFamily(dir, L"dlss5-events-full-*.log");
    CHECK(Count(text, "writing resumed; 5 event(s) were counted while paused") == 1);
    CHECK(Count(text, "after resume failed") == 1);
    // Queue cap: lines beyond 4 KB between flushes are counted as discarded.
    for (int i = 0; i < 200; ++i) sink.Add(dlss5::eventlog::kBridge, dlss5::eventlog::kWarning, "queue pressure line that is long enough failed");
    sink.Flush(400);
    text = ReadFamily(dir, L"dlss5-events-full-*.log");
    CHECK(Count(text, "were discarded because the in-memory queue reached") == 1);
    g_free_bytes = UINT64_MAX;
}

static void TestCrashWrite()
{
    const std::wstring dir = MakeDir(L"crash");
    EventSink sink;
    sink.Configure(dir.c_str(), EventSink::Limits{}, &FakeFree);
    sink.Add(dlss5::eventlog::kGeneric, dlss5::eventlog::kError, "queued before the crash failed");
    sink.CrashWrite("### CRASH RECORDED ###");
    const std::string text = ReadFamily(dir, L"dlss5-events-*.log");
    CHECK(Count(text, "queued before the crash failed") == 1);
    CHECK(Count(text, "[bridge][error] ### CRASH RECORDED ###") == 1);
    CHECK(text.find("queued before") < text.find("CRASH RECORDED"));
}

int main()
{
    wchar_t temp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, temp);
    g_root = std::wstring(temp) + L"dlss5-event-log-fixture-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    CreateDirectoryW(g_root.c_str(), nullptr);
    TestClassifier();
    TestAllowList();
    TestLogDirectory();
    TestBounded();
    TestFull();
    TestBudgetAndRetention();
    TestDiskFloorAndQueue();
    TestCrashWrite();
    std::printf("%s: %d checks, %d failures (fixture folder %ls)\n", g_failures == 0 ? "PASS" : "FAIL", g_checks, g_failures, g_root.c_str());
    return g_failures == 0 ? 0 : 1;
}
