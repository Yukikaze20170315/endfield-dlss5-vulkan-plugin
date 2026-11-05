"""CPU fixture for the Vulkan replacement serialization (release before create).

Extracts production recorded-use, retirement, lifecycle service, census and
ProcessNr reconciliation/gate code against faked Vulkan/ReShade objects.
Replacement creation waits for real release; slow waits only diagnose.
Pool eviction/failure and the diagnostic Keep policy retain their semantics.
"""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
DLSS5 = ROOT / 'src/unified-20261011/generic/src/addons/dlss5'
OUT = ROOT / 'builds/unified-20261011/vulkan-replacement-tests'


def function(source, signature):
    begin = source.index(signature)
    body = source.index('{', begin)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


def main():
    source = (DLSS5 / 'vulkan_path.hpp').read_text(encoding='utf-8')
    OUT.mkdir(parents=True, exist_ok=True)

    shared = (DLSS5 / 'dlssnr.hpp').read_text(encoding='utf-8')
    stage_passes = function(
        source, 'inline void RetireStagePasses(StageFeature* feature,')
    src_checks = [
        ('channel-offset retirement is marked',
         'RetireStagePasses(&feature, 0, true);' in source),
        ('suffix retirement is marked',
         'RetireStagePasses(&feature, stack, true);' in source),
        ('timeout escape removed', 'ClearStageReplacementWait' not in source),
        ('retired census is separate and includes holds',
         all(token in shared for token in ('vk::SnapshotRetirementStats()',
             '"] vk_retired[ws="', 'vk_retired.history', 'vk_retired.bytes >> 20',
             '<< vk::retirement_hold_events'))),
        ('lifecycle invokes Vulkan service under runtime lock after quiesce update',
         function(shared, 'inline void RunLifecycleTick(').index('RuntimeTryLock')
         < function(shared, 'inline void RunLifecycleTick(').index('UpdateShutdownQuiesce(')
         < function(shared, 'inline void RunLifecycleTick(').index('vk::ServiceNrRetirementLocked(')),
        ('recorded-use proof unchanged',
         function(source, 'inline bool RecordedUseComplete(uint64_t id)') ==
         'inline bool RecordedUseComplete(uint64_t id) {\n'
         '  if (id == 0) return true;\n'
         '  std::lock_guard lock(recorded_uses_mutex);\n'
         '  return live_recorded_uses.find(id) == live_recorded_uses.end();\n}'),
        ('RetireStagePasses forwards the mark to passes',
         'RetireStagePass(feature, pass, replacement);' in stage_passes),
        ('RetireStagePasses marks moved worksets',
         'workset.replacement = replacement;' in stage_passes),
        ('RetireStagePasses marks the commit workset',
         'retired.replacement = replacement;' in stage_passes),
        ('FailStagePass stays unmarked',
         'RetireStagePass(feature, pass);'
         in function(source, 'inline void FailStagePass(')),
        ('NR releases run in and report their residency token',
         all(token in function(source, sig) for sig, token in (
             ('inline void DrainRetiredFeature(StageFeature* feature,',
              'const residency::FeatureScope releasing(it->residency_token, true);'),
             ('inline void DrainRetiredFeature(StageFeature* feature,',
              'residency::NoteFeatureReleased(it->residency_token);'),
             ('inline void FreeStageNr(StageFeature* feature,',
              'const residency::FeatureScope releasing(retired.residency_token, true);'),
             ('inline void FreeStageNr(StageFeature* feature,',
              'residency::NoteFeatureReleased(retired.residency_token);'),
             ('inline void FreeStageNr(StageFeature* feature,',
              'residency::ForgetFeature(retired.residency_token);'),
         ))),
        ('NR create and evaluate are attributed to the slot token',
         source.count('const residency::FeatureScope attributed(slot.residency_token, false);') == 2
         and 'slot.residency_token = residency::NewFeatureToken();' in source
         and 'residency::ForgetFeature(slot.residency_token);' in source),
        ('the residency tick gets the present count',
         'presents_observed.load(std::memory_order_relaxed))'
         in shared[shared.index('vk::residency::Tick('):
                   shared.index(';', shared.index('vk::residency::Tick('))]),
    ]
    for name, ok in src_checks:
        if not ok:
            print(f'source contract failed: {name}')
            raise SystemExit(1)

    # The production gate block inside ProcessNr: contract retirement, stale
    # workset retirement, the drain and the decline gate.
    gate_begin = source.index(
        '  if (feature.nr_width != nr_width || feature.nr_height != nr_height')
    gate_end = source.index('  const codec::SourceUnits current_units', gate_begin)
    gate_block = source[gate_begin:gate_end]

    fixture = r'''
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

typedef void* VkImage;
struct NVSDK_NGX_Handle {};
struct NVSDK_NGX_Parameter {};
struct NVSDK_NGX_Parameter_Impl {};

namespace reshade::log { enum class level { info, warning, error }; }
namespace reshade::api {
struct command_list {};
struct resource { uint64_t handle = 0; };
struct resource_view { uint64_t handle = 0; };
enum class format : uint32_t { unknown = 0, r16g16b16a16_float = 10 };
enum class resource_usage { undefined };
struct resource_desc { struct { reshade::api::format format; uint32_t width, height; } texture; };
inline uint32_t format_row_pitch(format, uint32_t width) { return width * 8; }
inline uint32_t format_slice_pitch(format, uint32_t row, uint32_t height) { return row * height; }
struct device {
  resource_desc get_resource_desc(resource) { return {{format::r16g16b16a16_float, 256, 512}}; }
  void destroy_resource(resource) { ++destroyed_resources; }
  void destroy_resource_view(resource_view) { ++destroyed_views; }
  uint32_t destroyed_resources = 0;
  uint32_t destroyed_views = 0;
};
}  // namespace reshade::api

#include "declines.hpp"

namespace renodx::addons::dlss5::internal {

inline std::vector<std::string> logs;
inline void Log(reshade::log::level, const std::string& message) { logs.push_back(message); }
inline std::atomic_bool enabled{true};
namespace codec { struct SourceUnits {}; }
namespace stages {
enum class Point { Render, Upscaled, Present };
struct Plan {
  uint32_t render = 1, upscaled = 1, present = 0;
  uint32_t Count(Point point) const {
    return point == Point::Render ? render : point == Point::Upscaled ? upscaled : present;
  }
};
inline Plan plan;
inline Plan Snapshot() { return plan; }
}
struct ID3D12Device {};
inline ID3D12Device* direct_device = nullptr;
inline std::string VramSummary(ID3D12Device*, bool) { return {}; }
inline std::atomic_uint32_t workset_free_policy{0};
inline std::atomic_uint32_t live_nr_features{0};
inline uint64_t next_workset_use = 1;
inline uint64_t replacement_wait_starts = 0, replacement_wait_evaluates = 0,
                replacement_bound_hits = 0;
inline uint32_t replacement_wait_logged = 0, replacement_bound_logged = 0,
                nr_feature_released_logged = 0;
constexpr uint32_t kLifecycleLogCap = 256;
constexpr uint32_t kReplacementWaitEvaluateBound = 32;
struct DirectCallScope {};
namespace bridge { inline void DestroyOwnedParameters(NVSDK_NGX_Parameter*) {} }

namespace vk {

constexpr uint32_t kMaxNrPasses = 4;
'''
    for sig in ('enum Surface : uint32_t', 'struct Workset', 'struct LookHistoryImage',
                'struct RetiredLookHistory', 'struct RetiredNrHandle'):
        fixture += function(source, sig) + ';\n'
    fixture += r'''
struct LookHistoryVk { LookHistoryImage buffers[2]; };
struct NrFeatureSlot {
  NVSDK_NGX_Handle* handle = nullptr;
  NVSDK_NGX_Parameter* parameters = nullptr;
  uint64_t residency_token = 0;
};
struct NormStub {};

struct StageFeature {
  uint32_t channel_offset = 0;
  reshade::api::device* device = nullptr;
  std::vector<Workset> worksets;
  std::vector<Workset> retired_worksets;
  NrFeatureSlot slots[kMaxNrPasses];
  uint32_t nr_width = 0, nr_height = 0;
  uint8_t nr_hdr_mode = 0;
  uint64_t configuration = 0;
  std::vector<RetiredNrHandle> retired_nr;
  uint64_t nr_use[kMaxNrPasses] = {};
  LookHistoryVk look_history[kMaxNrPasses];
  std::vector<RetiredLookHistory> retired_history;
  uint64_t look_history_use[kMaxNrPasses] = {};
  struct { NormStub norm; } ngx;
  reshade::api::resource commit = {};
  reshade::api::resource_view commit_view = {};
  bool commit_cleared = false;
  uint64_t commit_use = 0;
  uint32_t replacement_wait_evaluates = 0;
};

inline std::mutex recorded_uses_mutex;
inline std::unordered_map<reshade::api::command_list*, std::vector<uint64_t>>
    recorded_uses;
inline std::unordered_map<uint64_t, uint32_t> live_recorded_uses;
inline uint64_t next_use_id = 1;
inline uint32_t next_workset_id = 0;
inline uint64_t lifecycle_events = 0;
inline uint32_t workset_released_logged = 0;
inline uint32_t stage_off_retire_logged = 0;
// vulkan_residency.hpp item 7, recorded: the FeatureScope each runtime
// release runs in, and the tokens reported released.
namespace residency {
inline uint64_t scope_token = 0;
inline bool scope_releasing = false;
struct FeatureScope {
  FeatureScope(uint64_t token, bool releasing) : saved_token(scope_token), saved_releasing(scope_releasing) {
    scope_token = token;
    scope_releasing = releasing;
  }
  ~FeatureScope() {
    scope_token = saved_token;
    scope_releasing = saved_releasing;
  }
  uint64_t saved_token;
  bool saved_releasing;
};
inline std::vector<uint64_t> released_tokens;
inline void NoteFeatureReleased(uint64_t token) { released_tokens.push_back(token); }
}  // namespace residency
inline uint32_t runtime_releases = 0;
inline std::vector<std::pair<uint64_t, bool>> release_scopes;
inline void OnRuntimeRelease(NVSDK_NGX_Handle*) {
  ++runtime_releases;
  release_scopes.emplace_back(residency::scope_token, residency::scope_releasing);
}
struct RuntimeStub {
  void (*release)(NVSDK_NGX_Handle*) = OnRuntimeRelease;
};
inline RuntimeStub runtime;

'''
    fixture += 'inline constexpr uint32_t kStageCount = 3;\n'
    fixture += 'inline uint32_t StageIndex(stages::Point stage) { return static_cast<uint32_t>(stage); }\n'
    fixture += 'struct Feature { StageFeature stages[kStageCount]; };\n'
    fixture += 'inline std::unordered_map<const NVSDK_NGX_Handle*, Feature> features;\n'
    begin = source.index('inline std::atomic_bool retirement_pending{false};')
    end = source.index('// A Vulkan command buffer may be begun again', begin)
    fixture += source[begin:end]
    # v9: the pre-FG stream's image lifecycle is declared in that slice; the
    # fixture has no such images, so it never holds a retirement.
    fixture += 'inline bool ServicePreFgRetirementLocked(bool) { return false; }\n'
    # v10e: the plan the evaluates run under is the user's here (no pre-FG
    # fallback in the fixture).
    fixture += 'inline stages::Plan EffectiveVulkanPlan(const stages::Plan& plan) { return plan; }\n'
    for sig in (
        'inline void TrackRecordedUse(reshade::api::command_list* list, uint64_t id)',
        'inline void ForgetRecordedUses(reshade::api::command_list* list)',
        'inline bool RecordedUseComplete(uint64_t id)',
        'inline void DestroyWorkset(reshade::api::device* device, const Workset& workset)',
        'inline void LogWorksetReleased(const Workset& workset)',
        'inline void DestroyLookHistoryImage(reshade::api::device* device,',
        'inline void NoteRetirementHold(',
        'inline void DrainRetiredFeature(StageFeature* feature,',
        'inline void RetireStagePass(StageFeature* feature, uint32_t pass, bool replacement = false)',
        'inline void RetireStagePasses(StageFeature* feature, uint32_t first, bool replacement = false)',
        'inline bool StageReplacementPending(const StageFeature& feature)',
        'inline void NoteStageReplacementSlowWait(',
        'inline bool RetireStageForDisable(StageFeature* feature,',
        'inline void ServiceNrRetirementLocked(bool nr_release_allowed)',
    ):
        fixture += function(source, sig) + '\n'

    reconcile_begin = source.index('    if (feature.channel_offset != offset)')
    reconcile_end = source.index('    if (stack > kMaxNrPasses)', reconcile_begin)
    fixture += ('inline NrDeclineReason ReconcileStage(StageFeature& feature, '
                'uint32_t stack, uint32_t offset) {\n'
                + source[reconcile_begin:reconcile_end]
                + 'return NrDeclineReason::kCount;\n}\n')
    fixture += r'''
inline uint64_t allocation_admissions = 0;
// The production gate from ProcessNr (contract retirement -> drain -> gate),
// driven with the caller's locals provided by this wrapper.
inline NrDeclineReason EvaluateStage(
    StageFeature& feature, const NVSDK_NGX_Handle* handle, VkImage image,
    uint32_t width, uint32_t height, reshade::api::format format,
    uint32_t resource_width, uint32_t resource_height, uint32_t nr_width,
    uint32_t nr_height, uint32_t hdr_mode, uint64_t configuration_generation_value) {
  const std::atomic_uint64_t configuration_generation{configuration_generation_value};
  struct Out { VkImage Image; } out{};
  out.Image = image;
  struct { struct { reshade::api::format format; } texture; } output_desc{};
  output_desc.texture.format = format;
'''
    fixture += gate_block
    fixture += r'''
  ++allocation_admissions;
  return NrDeclineReason::kCount;
}

}  // namespace vk
inline std::atomic_bool nr_retirement_pending{false};
inline bool fake_turn_available = true, fake_runtime_locked = false;
inline uint32_t fake_turn_attempts = 0;
struct BridgeDeviceLock {};
namespace ngx_serial {
struct PresentTurn {
  bool acquired;
  explicit PresentTurn(bool force) : acquired(fake_turn_available) {
    if (!force || fake_runtime_locked) std::abort();
    ++fake_turn_attempts;
  }
  bool owns() const { return acquired; }
};
}
inline int runtime_mutex = 0;
struct RuntimeTryLock {
  explicit RuntimeTryLock(int&) { fake_runtime_locked = true; }
  ~RuntimeTryLock() { fake_runtime_locked = false; }
  bool owns_lock() const { return true; }
  void lock() {}
};
inline std::atomic_int64_t lifecycle_tick_owed_ns{0};
inline std::atomic_uint64_t runtime_lock_present_deferred{0};
inline uint64_t present_generation = 0;
inline int64_t telemetry_previous_present_ns = 0;
inline std::vector<int> processed_outputs;
inline std::atomic_uint32_t streamline_tag_mask_this_present{0}, streamline_tag_max_batch{0};
inline void SettleRetiredQueueFences() {}
inline void UpdateShutdownQuiesce(bool, int64_t) {}
inline void ServiceNrRetirementLocked(bool) {}
'''
    tick_begin = shared.index('inline void RunLifecycleTick(')
    tick_end = shared.index('    gpu_timers::Poll(', tick_begin)
    fixture += shared[tick_begin:tick_end] + '  }\n}\n'
    fixture += r'''
}  // namespace renodx::addons::dlss5::internal

using namespace renodx::addons::dlss5;
using namespace renodx::addons::dlss5::internal;
using namespace renodx::addons::dlss5::internal::vk;

int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

reshade::api::device fake_device;
reshade::api::command_list cmd_a, cmd_b;
NVSDK_NGX_Handle game_handle;
NVSDK_NGX_Handle nr_handle;

StageFeature MakeStage() {
  StageFeature stage;
  stage.device = &fake_device;
  stage.nr_width = 1280;
  stage.nr_height = 720;
  stage.nr_hdr_mode = 0;
  stage.configuration = 7;
  Workset ws;
  ws.output = reinterpret_cast<VkImage>(0x11);
  ws.width = 1920;
  ws.height = 1080;
  ws.output_format = reshade::api::format::r16g16b16a16_float;
  ws.nr_width = 1280;
  ws.nr_height = 720;
  ws.source_width = 1920;
  ws.source_height = 1080;
  ws.images[0].handle = 0xAAAA;
  ws.use_id = next_use_id++;
  stage.worksets.push_back(ws);
  stage.slots[0].handle = &nr_handle;
  stage.slots[0].parameters = nullptr;
  stage.nr_use[0] = next_use_id++;
  live_nr_features.store(1);
  return stage;
}

// Evaluate with the same contract the stage was built with (no change).
NrDeclineReason EvalSame(StageFeature& stage, uint32_t config = 7) {
  return EvaluateStage(stage, &game_handle, stage.worksets.empty()
      ? reinterpret_cast<VkImage>(0x11) : stage.worksets.front().output,
      1920, 1080, reshade::api::format::r16g16b16a16_float,
      1920, 1080, 1280, 720, 0, config);
}

// Evaluate with a changed NR grid (contract change -> replacement retire).
NrDeclineReason EvalGrid(StageFeature& stage, uint32_t w, uint32_t h) {
  return EvaluateStage(stage, &game_handle,
      reinterpret_cast<VkImage>(0x22), 1920, 1080,
      reshade::api::format::r16g16b16a16_float,
      1920, 1080, w, h, 0, 7);
}

uint32_t ReplacementMarked(const StageFeature& stage) {
  uint32_t count = 0;
  for (const auto& w : stage.retired_worksets) if (w.replacement) ++count;
  for (const auto& n : stage.retired_nr) if (n.replacement) ++count;
  for (const auto& h : stage.retired_history) if (h.replacement) ++count;
  return count;
}

void ResetLifecycle() {
  features.clear();
  recorded_uses.clear();
  live_recorded_uses.clear();
  retirement_pending.store(false);
  nr_retirement_pending.store(false);
  fake_turn_available = true;
  fake_turn_attempts = 0;
  enabled.store(true);
  workset_free_policy.store(0);
  stages::plan = {};
  retirement_hold_events = 0;
  logs.clear();
}

StageFeature MakeRichStage() {
  StageFeature stage = MakeStage();
  stage.worksets.front().images[kSurfaceCount - 1].handle = 0xBBBB;
  stage.commit.handle = 0xCCCC;
  stage.commit_use = next_use_id++;
  stage.look_history[0].buffers[0].image.handle = 0xDDDD;
  stage.look_history[0].buffers[1].image.handle = 0xEEEE;
  stage.look_history_use[0] = next_use_id++;
  return stage;
}

void TrackStage(StageFeature& stage) {
  TrackRecordedUse(&cmd_a, stage.worksets.front().use_id);
  TrackRecordedUse(&cmd_a, stage.commit_use);
  TrackRecordedUse(&cmd_a, stage.look_history_use[0]);
  TrackRecordedUse(&cmd_b, stage.nr_use[0]);
}

int main() {
  // --- 1: a grid change gates until the recorded uses complete -----------
  {
    StageFeature stage = MakeStage();
    // The live workset and the NR slot were recorded into pending buffers.
    TrackRecordedUse(&cmd_a, stage.worksets.front().use_id);
    TrackRecordedUse(&cmd_b, stage.nr_use[0]);
    NrDeclineReason first = EvalGrid(stage, 960, 540);
    CHECK(first == NrDeclineReason::kReplacementPending);
    CHECK(stage.worksets.empty());                 // stale workset retired
    CHECK(stage.retired_worksets.size() == 1);
    CHECK(stage.retired_nr.size() == 1);
    CHECK(ReplacementMarked(stage) == 2);
    CHECK(stage.slots[0].handle == nullptr);
    CHECK(stage.nr_width == 960 && stage.nr_height == 540);
    // Still pending on a re-evaluate while the uses are live.
    NrDeclineReason second = EvalGrid(stage, 960, 540);
    CHECK(second == NrDeclineReason::kReplacementPending);
    CHECK(ReplacementMarked(stage) == 2);
    // One buffer finished; the other is still live -> still gated.
    ForgetRecordedUses(&cmd_a);
    NrDeclineReason third = EvalGrid(stage, 960, 540);
    CHECK(third == NrDeclineReason::kReplacementPending);
    // All recorded uses complete: the drain frees and creation proceeds.
    ForgetRecordedUses(&cmd_b);
    NrDeclineReason fourth = EvalGrid(stage, 960, 540);
    CHECK(fourth != NrDeclineReason::kReplacementPending);
    CHECK(stage.retired_worksets.empty() && stage.retired_nr.empty());
    CHECK(stage.replacement_wait_evaluates == 0);
  }

  // --- 2: pool eviction (replacement=false) never gates -------------------
  {
    StageFeature stage = MakeStage();
    // Retire the live workset the way the pool cap does: no mark.
    stage.retired_worksets.push_back(std::move(stage.worksets.front()));
    stage.worksets.clear();
    // Even an unprovable (use_id still live) unmarked entry cannot gate.
    TrackRecordedUse(&cmd_a, stage.retired_worksets.front().use_id);
    NrDeclineReason result = EvalSame(stage);
    CHECK(result != NrDeclineReason::kReplacementPending);
  }

  // --- 3: Keep policy never gates -----------------------------------------
  {
    StageFeature stage = MakeStage();
    workset_free_policy.store(1);
    TrackRecordedUse(&cmd_a, stage.worksets.front().use_id);
    TrackRecordedUse(&cmd_b, stage.nr_use[0]);
    // Contract change still retires marked entries, but the gate is open
    // (Keep semantics: retained resources are reusable, never blocking).
    NrDeclineReason result = EvalGrid(stage, 960, 540);
    CHECK(result != NrDeclineReason::kReplacementPending);
    workset_free_policy.store(0);
    ForgetRecordedUses(&cmd_a);
    ForgetRecordedUses(&cmd_b);
  }

  // --- 4: >1000 pending evaluates never reach the allocation tail ----------
  {
    StageFeature stage = MakeStage();
    TrackRecordedUse(&cmd_a, stage.worksets.front().use_id);
    TrackRecordedUse(&cmd_b, stage.nr_use[0]);
    const uint64_t hits_before = replacement_bound_hits;
    const uint64_t admissions = allocation_admissions;
    for (uint32_t i = 0; i < 1100; ++i) {
      CHECK(EvalGrid(stage, 960, 540) == NrDeclineReason::kReplacementPending);
      CHECK(allocation_admissions == admissions && stage.worksets.empty());
    }
    CHECK(replacement_bound_hits == hits_before + 1);
    CHECK(ReplacementMarked(stage) == 2);
    CHECK(!stage.retired_worksets.empty());
    stage.replacement_wait_evaluates = UINT32_MAX - 1;
    replacement_wait_evaluates = UINT64_MAX - 1;
    for (int i = 0; i < 4; ++i) CHECK(EvalGrid(stage, 960, 540) == NrDeclineReason::kReplacementPending);
    CHECK(stage.replacement_wait_evaluates == UINT32_MAX);
    CHECK(replacement_wait_evaluates == UINT64_MAX && replacement_bound_hits == hits_before + 1);
    StageFeature independent = MakeStage();
    CHECK(EvalSame(independent) == NrDeclineReason::kCount);
    ForgetRecordedUses(&cmd_a);
    ForgetRecordedUses(&cmd_b);
    CHECK(EvalGrid(stage, 960, 540) == NrDeclineReason::kCount);
    CHECK(stage.replacement_wait_evaluates == 0);
    CHECK(stage.retired_worksets.empty() && stage.retired_nr.empty());
  }

  // --- 5: an idle/failure retirement never gates ---------------------------
  {
    StageFeature stage = MakeStage();
    // Same path as FailStagePass: RetireStagePass without the mark.
    RetireStagePass(&stage, 0);
    TrackRecordedUse(&cmd_a, stage.retired_nr.front().use_id);
    NrDeclineReason result = EvalSame(stage);
    CHECK(result != NrDeclineReason::kReplacementPending);
    ForgetRecordedUses(&cmd_a);
  }

  // --- 6: a channel-offset reshuffle retires the whole stage marked -------
  {
    StageFeature stage = MakeStage();
    TrackRecordedUse(&cmd_a, stage.worksets.front().use_id);
    TrackRecordedUse(&cmd_b, stage.nr_use[0]);
    // The ProcessNr channel-offset path: every pass plus the worksets and
    // the committed divisor retire under one mark.
    RetireStagePasses(&stage, 0, true);
    CHECK(stage.worksets.empty());
    CHECK(stage.retired_worksets.size() == 1
          && stage.retired_worksets.front().replacement);
    CHECK(stage.retired_nr.size() == 1
          && stage.retired_nr.front().replacement);
    NrDeclineReason first = EvalSame(stage);
    CHECK(first == NrDeclineReason::kReplacementPending);
    // One buffer completes while the NR handle's use is still live.
    ForgetRecordedUses(&cmd_a);
    NrDeclineReason second = EvalSame(stage);
    CHECK(second == NrDeclineReason::kReplacementPending);
    // All recorded uses complete: the drain frees and the gate opens.
    ForgetRecordedUses(&cmd_b);
    NrDeclineReason third = EvalSame(stage);
    CHECK(third != NrDeclineReason::kReplacementPending);
    CHECK(stage.retired_worksets.empty() && stage.retired_nr.empty());
    CHECK(stage.replacement_wait_evaluates == 0);
  }

  // --- 7: same-offset 1->0->1 and 2->1->2 wait for the removed tail ---------
  for (uint32_t count : {1u, 2u}) {
    StageFeature stage = MakeStage();
    if (count == 2) {
      stage.slots[1].handle = reinterpret_cast<NVSDK_NGX_Handle*>(0x33);
      stage.nr_use[1] = next_use_id++;
    }
    TrackRecordedUse(&cmd_a, stage.worksets.front().use_id);
    TrackRecordedUse(&cmd_b, stage.nr_use[count - 1]);
    const auto removed = stage.slots[count - 1].handle;
    const uint32_t releases = runtime_releases;
    ReconcileStage(stage, count - 1, 0);
    CHECK(stage.channel_offset == 0 && stage.slots[count - 1].handle == nullptr);
    CHECK(stage.retired_nr.size() == 1 && stage.retired_nr.front().handle == removed);
    CHECK(stage.retired_nr.front().replacement);
    CHECK(stage.worksets.size() == (count == 2 ? 1u : 0u));
    const uint64_t admissions = allocation_admissions;
    ReconcileStage(stage, count, 0);
    CHECK(EvalSame(stage) == NrDeclineReason::kReplacementPending);
    CHECK(allocation_admissions == admissions && runtime_releases == releases);
    ForgetRecordedUses(&cmd_a);
    ReconcileStage(stage, count, 0);
    CHECK(EvalSame(stage) == NrDeclineReason::kReplacementPending);
    ForgetRecordedUses(&cmd_b);
    ReconcileStage(stage, count, 0);
    CHECK(runtime_releases == releases + 1);
    CHECK(EvalSame(stage) == NrDeclineReason::kCount);
    CHECK(allocation_admissions == admissions + 1);
    CHECK(stage.retired_worksets.empty() && stage.retired_nr.empty());
  }

  // --- 8: NR off retires the stage marked; re-enable gates until drained ---
  {
    StageFeature stage = MakeStage();
    TrackRecordedUse(&cmd_a, stage.worksets.front().use_id);
    TrackRecordedUse(&cmd_b, stage.nr_use[0]);
    const uint32_t releases_before = runtime_releases;
    CHECK(RetireStageForDisable(&stage));
    CHECK(stage.worksets.empty() && stage.slots[0].handle == nullptr);
    CHECK(stage.retired_worksets.size() == 1 && stage.retired_nr.size() == 1);
    CHECK(ReplacementMarked(stage) == 2);
    CHECK(runtime_releases == releases_before);  // uses still live: not freed
    // Later NR-off evaluates: nothing live to retire, the drain keeps running.
    CHECK(!RetireStageForDisable(&stage));
    CHECK(stage.retired_worksets.size() == 1 && stage.retired_nr.size() == 1);
    // Re-enable (an evaluate under the stage's contract) gates while the
    // recorded uses are live.
    NrDeclineReason first = EvalSame(stage);
    CHECK(first == NrDeclineReason::kReplacementPending);
    ForgetRecordedUses(&cmd_a);
    ForgetRecordedUses(&cmd_b);
    CHECK(!RetireStageForDisable(&stage));  // the drain frees now
    CHECK(stage.retired_worksets.empty() && stage.retired_nr.empty());
    CHECK(runtime_releases == releases_before + 1);
    NrDeclineReason second = EvalSame(stage);
    CHECK(second != NrDeclineReason::kReplacementPending);
    // Keep policy: NR off retires nothing.
    StageFeature kept = MakeStage();
    workset_free_policy.store(1);
    CHECK(!RetireStageForDisable(&kept));
    CHECK(kept.worksets.size() == 1 && kept.slots[0].handle != nullptr);
    workset_free_policy.store(0);
  }

  // No further evaluates: the production lifecycle services both Vulkan stages.
  {
    ResetLifecycle();
    auto& game = features[&game_handle];
    // Render and Upscaled; a game feature's Present stage (v9) stays empty.
    for (uint32_t i = 0; i < 2; ++i) { game.stages[i] = MakeRichStage(); TrackStage(game.stages[i]); }
    CHECK(SnapshotRetirementStats().bytes == 0);
    enabled.store(false);
    fake_turn_available = false;
    RunLifecycleTick(1, true);
    auto stats = SnapshotRetirementStats();
    CHECK(stats.worksets == 4 && stats.nr == 2 && stats.history == 2);
    CHECK(stats.bytes == (10ull << 20));
    CHECK(retirement_pending.load() && fake_turn_attempts == 0);
    CHECK(retirement_hold_events == 8);
    const uint64_t events = retirement_hold_events;
    for (int i = 0; i < 40; ++i) {
      enabled.store((i & 1) == 0);
      RunLifecycleTick(i + 2, true);
    }
    enabled.store(false);
    CHECK(retirement_hold_events == events && SnapshotRetirementStats().bytes == stats.bytes);
    CHECK(SnapshotRetirementStats().nr == 2 && fake_turn_attempts == 40);
    ForgetRecordedUses(&cmd_a);
    const uint32_t releases = runtime_releases;
    RunLifecycleTick(42, true);
    stats = SnapshotRetirementStats();
    CHECK(stats.worksets == 0 && stats.history == 0 && stats.bytes == 0 && stats.nr == 2);
    CHECK(runtime_releases == releases && retirement_pending.load());
    ForgetRecordedUses(&cmd_b);
    RunLifecycleTick(43, true);
    CHECK(retirement_hold_events == events + 2);  // proof now complete, only NGX turn missing
    for (int i = 0; i < 40; ++i) RunLifecycleTick(i + 44, true);
    CHECK(retirement_hold_events == events + 2 && runtime_releases == releases);
    fake_turn_available = true;
    RunLifecycleTick(84, true);
    CHECK(SnapshotRetirementStats().nr == 0 && !retirement_pending.load());
    CHECK(runtime_releases == releases + 2);
    RunLifecycleTick(85, true);
    CHECK(runtime_releases == releases + 2);
  }
  {
    ResetLifecycle();
    features[&game_handle].stages[0] = MakeRichStage();
    enabled.store(false);
    const uint32_t releases = runtime_releases;
    RunLifecycleTick(1, true);  // no prior pending -> no turn, but worksets/history may free
    CHECK(fake_turn_attempts == 0 && retirement_pending.load());
    CHECK(SnapshotRetirementStats().bytes == 0 && SnapshotRetirementStats().nr == 1);
    CHECK(runtime_releases == releases);
    RunLifecycleTick(2, true);
    CHECK(fake_turn_attempts == 1 && runtime_releases == releases + 1);
    CHECK(!retirement_pending.load());
  }
  // Zero-count Render retires without evaluate; another stream's proof is independent.
  {
    ResetLifecycle();
    auto& a = features[&game_handle];
    auto& b = features[&nr_handle];
    a.stages[0] = MakeRichStage();
    a.stages[1] = MakeStage();
    b.stages[0] = MakeRichStage();
    TrackStage(a.stages[0]);
    stages::plan.render = 0;
    const uint32_t releases = runtime_releases;
    RunLifecycleTick(1, true);
    CHECK(a.stages[0].worksets.empty() && b.stages[0].worksets.empty());
    CHECK(a.stages[1].worksets.size() == 1 && a.stages[1].slots[0].handle != nullptr);
    RunLifecycleTick(2, true);
    CHECK(runtime_releases == releases + 1 && b.stages[0].retired_nr.empty());
    CHECK(a.stages[0].retired_nr.size() == 1 && retirement_pending.load());
    CHECK(SnapshotRetirementStats().bytes == (5ull << 20));
    stages::plan.render = 1;
    CHECK(EvalSame(a.stages[0]) == NrDeclineReason::kReplacementPending);
    ForgetRecordedUses(&cmd_a);
    ForgetRecordedUses(&cmd_b);
    RunLifecycleTick(3, true);  // active again, yet no evaluate is needed to drain
    CHECK(SnapshotRetirementStats().bytes == 0 && SnapshotRetirementStats().nr == 0);
    CHECK(EvalSame(a.stages[0]) == NrDeclineReason::kCount);
    CHECK(!retirement_pending.load());
  }
  for (bool reopen : {false, true}) {
    ResetLifecycle();
    auto& stage = features[&game_handle].stages[0];
    stage = MakeRichStage();
    enabled.store(false);
    workset_free_policy.store(1);
    for (int i = 0; i < 40; ++i) RunLifecycleTick(i + 1, true);
    CHECK(stage.worksets.size() == 1 && stage.slots[0].handle != nullptr);
    CHECK(SnapshotRetirementStats().nr == 0 && !retirement_pending.load());
    enabled.store(reopen);
    workset_free_policy.store(0);
    RunLifecycleTick(41, true);
    CHECK((stage.worksets.size() == 1) == reopen);
    CHECK((stage.slots[0].handle != nullptr) == reopen);
    RunLifecycleTick(42, true);
    CHECK(!retirement_pending.load() && SnapshotRetirementStats().nr == 0);
  }
  // A history-only retirement still gates, but needs no NGX turn to release.
  {
    ResetLifecycle();
    auto& stage = features[&game_handle].stages[0];
    stage = MakeRichStage();
    TrackRecordedUse(&cmd_a, stage.look_history_use[0]);
    enabled.store(false);
    RunLifecycleTick(1, true);
    RunLifecycleTick(2, true);
    CHECK(!retirement_pending.load());
    CHECK(SnapshotRetirementStats().history == 1 && SnapshotRetirementStats().bytes == (2ull << 20));
    enabled.store(true);
    CHECK(EvalSame(stage) == NrDeclineReason::kReplacementPending);
    const uint32_t attempts = fake_turn_attempts;
    ForgetRecordedUses(&cmd_a);
    RunLifecycleTick(3, true);
    CHECK(fake_turn_attempts == attempts && SnapshotRetirementStats().history == 0);
    CHECK(EvalSame(stage) == NrDeclineReason::kCount);
  }
  // A pre-existing D3D12 retirement reservation also permits Vulkan release.
  {
    ResetLifecycle();
    features[&game_handle].stages[0] = MakeRichStage();
    enabled.store(false);
    nr_retirement_pending.store(true);
    RunLifecycleTick(1, true);
    CHECK(fake_turn_attempts == 1 && SnapshotRetirementStats().nr == 0);
    CHECK(!retirement_pending.load());
  }
  // Diagnostic Keep also holds an already-retired stage after proofs arrive.
  {
    ResetLifecycle();
    auto& stage = features[&game_handle].stages[0];
    stage = MakeRichStage();
    TrackStage(stage);
    enabled.store(false);
    RunLifecycleTick(1, true);
    ForgetRecordedUses(&cmd_a);
    ForgetRecordedUses(&cmd_b);
    workset_free_policy.store(1);
    RunLifecycleTick(2, true);
    CHECK(SnapshotRetirementStats().bytes == (5ull << 20));
    CHECK(SnapshotRetirementStats().nr == 1 && retirement_pending.load());
    workset_free_policy.store(0);
    RunLifecycleTick(3, true);
    CHECK(SnapshotRetirementStats().bytes == 0 && !retirement_pending.load());
  }
  // Hold logs are per-entry/state-change, capped at 64; counting continues.
  {
    ResetLifecycle();
    NVSDK_NGX_Handle handles[18];
    for (auto& handle : handles) {
      auto& stage = features[&handle].stages[0];
      stage = MakeRichStage();
      TrackStage(stage);
    }
    enabled.store(false);
    RunLifecycleTick(1, true);
    const auto held_logs = [] {
      return std::count_if(logs.begin(), logs.end(), [](const auto& line) {
        return line.starts_with("Vulkan NR retirement held:");
      });
    };
    CHECK(retirement_hold_events == 72 && held_logs() == 64);
    for (int i = 0; i < 40; ++i) RunLifecycleTick(i + 2, true);
    CHECK(retirement_hold_events == 72 && held_logs() == 64);
    ForgetRecordedUses(&cmd_a);
    ForgetRecordedUses(&cmd_b);
    fake_turn_available = false;
    RunLifecycleTick(42, true);
    CHECK(retirement_hold_events == 90 && held_logs() == 64);
    fake_turn_available = true;
    RunLifecycleTick(43, true);
    CHECK(SnapshotRetirementStats().nr == 0 && !retirement_pending.load());
  }

  // Residency item 7: the token rides the retirement, and the release runs
  // in its FeatureScope (releasing) and reports it once, after the proof.
  {
    ResetLifecycle();
    StageFeature stage = MakeStage();
    stage.slots[0].residency_token = 41;
    residency::released_tokens.clear();
    release_scopes.clear();
    RetireStagePass(&stage, 0, true);
    CHECK(stage.retired_nr.size() == 1 && stage.retired_nr.front().residency_token == 41);
    CHECK(stage.slots[0].residency_token == 0);
    TrackRecordedUse(&cmd_a, stage.retired_nr.front().use_id);
    DrainRetiredFeature(&stage);
    CHECK(release_scopes.empty() && residency::released_tokens.empty());  // use live: held
    ForgetRecordedUses(&cmd_a);
    DrainRetiredFeature(&stage);
    CHECK(stage.retired_nr.empty());
    CHECK(release_scopes.size() == 1 && release_scopes.front().first == 41
          && release_scopes.front().second);
    CHECK(residency::released_tokens.size() == 1 && residency::released_tokens.front() == 41);
    CHECK(residency::scope_token == 0 && !residency::scope_releasing);
  }

  std::printf("vulkan-replacement-serial: %d checks, %d failures\n",
              checks, failures);
  return failures ? 1 : 0;
}
'''

    import shutil
    shutil.copyfile(DLSS5 / 'declines.hpp', OUT / 'declines.hpp')
    (OUT / 'vulkan-replacement-fixture.cpp').write_text(fixture, encoding='utf-8')
    runner = OUT / 'run-vulkan-replacement-fixture.cmd'
    runner.write_text('@echo off\nsetlocal\nset "VSCMD_SKIP_SENDTELEMETRY=1"\n'
                      'call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64 >nul\n'
                      'if errorlevel 1 exit /b %errorlevel%\n'
                      'cl /nologo /W4 /O2 /MT /EHsc /std:c++20 /utf-8 /wd4505 vulkan-replacement-fixture.cpp /Fe:vulkan-replacement-fixture.exe\n'
                      'if errorlevel 1 exit /b %errorlevel%\n'
                      'vulkan-replacement-fixture.exe\nexit /b %errorlevel%\n', encoding='ascii')
    env = os.environ.copy()
    env.update(TEMP=str(OUT), TMP=str(OUT))
    result = subprocess.run(['cmd.exe', '/d', '/c', str(runner)], cwd=OUT,
                            env=env, capture_output=True)
    import sys
    sys.stdout.buffer.write(result.stdout)
    sys.stdout.buffer.write(result.stderr)
    if result.returncode:
        raise SystemExit(result.returncode)
    print(f'source contracts: {len(src_checks)} checks, 0 failures')
    print('Limit: extracted retirement/service/census, ProcessNr blocks and lifecycle prefix '
          'with fake devices/turns; no Vulkan, NGX or game rendering executed.')


if __name__ == '__main__':
    main()
