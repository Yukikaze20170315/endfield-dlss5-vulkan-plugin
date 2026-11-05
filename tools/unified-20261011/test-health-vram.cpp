// The VRAM notice rows of the Generic health ledger: vram_over_budget enters
// only past the budget and leaves under 95%; vram_near_limit logs the
// 95-100% band; vram_demoted enters on any moved memory.
#include "../../src/unified-20261011/generic/src/addons/dlss5/health.hpp"

#include <cstdio>
#include <cstring>

namespace h = renodx::addons::dlss5::health;

static int g_checks = 0;
static int g_failures = 0;
#define CHECK(x)                                                     \
  do {                                                               \
    ++g_checks;                                                      \
    if (!(x)) {                                                      \
      ++g_failures;                                                  \
      std::printf("FAIL line %d: %s\n", __LINE__, #x);               \
    }                                                                \
  } while (0)

static std::size_t RowIndex(const char* slug) {
  for (std::size_t i = 0; i < h::kRowCount; ++i) {
    if (std::strcmp(h::kRows[i].slug, slug) == 0) return i;
  }
  return h::kRowCount;
}

struct Run {
  h::Ledger ledger;
  double t = 0.0;
  // One 2-second window per call after the first: eight 0.25 s frames, so a
  // window (>= 1 s and >= 8 frames) closes on each call's first tick and is
  // judged on that call's sample.
  void Window(double usage, double budget, double demoted = 0.0) {
    h::Sample s;
    s[h::Signal::kVramUsageMib] = usage;
    s[h::Signal::kVramBudgetMib] = budget;
    s[h::Signal::kVramDemotedMib] = demoted;
    h::Event events[h::kRowCount];
    for (int f = 0; f < 8; ++f) {
      t += 0.25;
      ledger.Tick(s, t, true, true, events, h::kRowCount);
    }
  }
  bool Active(std::size_t row) const { return (ledger.ActiveMask() >> row) & 1u; }
};

int main() {
  const std::size_t over = RowIndex("vram_over_budget");
  const std::size_t near = RowIndex("vram_near_limit");
  const std::size_t demoted = RowIndex("vram_demoted");
  CHECK(over < h::kRowCount);
  CHECK(near < h::kRowCount);
  CHECK(demoted < h::kRowCount);
  if (g_failures != 0) return 1;

  CHECK(h::kRows[over].tone == h::Tone::kWarn);
  CHECK(h::kRows[near].tone == h::Tone::kLog);
  CHECK(h::kRows[demoted].tone == h::Tone::kWarn);
  CHECK(std::strcmp(h::kRows[over].title, h::kRows[demoted].title) != 0);
  CHECK(h::kRows[over].fix2 != nullptr && h::kRows[demoted].fix2 != nullptr);
  // Slugs are append-only: the new row is last.
  CHECK(near == h::kRowCount - 1);

  {
    // 97% for a minute: logged near the limit, no amber notice.
    Run r;
    r.Window(0, 0);
    for (int i = 0; i < 30; ++i) r.Window(970, 1000);
    CHECK(!r.Active(over));
    CHECK(r.Active(near));
    CHECK(!r.Active(demoted));
  }
  {
    // Over the budget: enters after 10 s, not before.
    Run r;
    r.Window(0, 0);
    r.Window(1050, 1000);
    r.Window(1050, 1000);
    r.Window(1050, 1000);
    r.Window(1050, 1000);
    CHECK(!r.Active(over));
    r.Window(1050, 1000);
    CHECK(r.Active(over));
    CHECK(h::Counted(h::kRows[over]));
    // Hovering at 97%: inside the hysteresis band, stays for a minute.
    for (int i = 0; i < 30; ++i) r.Window(970, 1000);
    CHECK(r.Active(over));
    // Under 95%: leaves after 8 s (four 2-second windows).
    r.Window(930, 1000);
    r.Window(930, 1000);
    r.Window(930, 1000);
    CHECK(r.Active(over));
    r.Window(930, 1000);
    CHECK(!r.Active(over));
    // near_limit holds until under 90%.
    CHECK(r.Active(near));
    for (int i = 0; i < 4; ++i) r.Window(880, 1000);
    CHECK(!r.Active(near));
  }
  {
    // Alternating 1.02 / 0.96 used to flap at the 0.95 limit; now one entry.
    Run r;
    r.Window(0, 0);
    for (int i = 0; i < 6; ++i) r.Window(1020, 1000);
    CHECK(r.Active(over));
    const std::uint32_t entered = r.ledger.State(over).enters;
    for (int i = 0; i < 40; ++i) r.Window(i % 2 ? 1020 : 960, 1000);
    CHECK(r.Active(over));
    CHECK(r.ledger.State(over).enters == entered);
  }
  {
    // Demoted memory: enters after 2 s, leaves 8 s after it returns.
    Run r;
    r.Window(0, 0, 0);
    r.Window(0, 0, 64);
    r.Window(0, 0, 64);
    CHECK(r.Active(demoted));
    for (int i = 0; i < 3; ++i) r.Window(0, 0, 0);
    CHECK(r.Active(demoted));
    r.Window(0, 0, 0);
    CHECK(!r.Active(demoted));
  }
  {
    // A zero budget (the kernel account in use) is no evidence either way.
    Run r;
    r.Window(0, 0);
    for (int i = 0; i < 10; ++i) r.Window(1200, 0);
    CHECK(!r.Active(over));
    CHECK(!r.Active(near));
  }

  std::printf("%s: %d checks, %d failures (health VRAM rows)\n",
              g_failures == 0 ? "PASS" : "FAIL", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
