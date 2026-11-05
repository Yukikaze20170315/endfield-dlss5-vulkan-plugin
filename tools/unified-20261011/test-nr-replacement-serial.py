"""CPU fixture for the D3D12 replacement serialization (release before create).

Extracts the production retirement helpers, the ReplacementWait* gate and the
EnsureWorkset prefix (everything up to the pool budget, replaced by a fake
allocation tail) from dlssnr.hpp and compiles them with faked D3D12/NGX
objects. Asserts that a stream's replacement never allocates while its
replacement-marked retired generation is still provably resident, and that
idle eviction/failure/teardown retirements never gate. No GPU or game runs.
"""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
DLSS5 = ROOT / 'src/unified-20261011/generic/src/addons/dlss5'
OUT = ROOT / 'builds/unified-20261011/nr-replacement-tests'


def function(source, signature):
    begin = source.index(signature)
    body = source.index('{', begin)
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


def struct_(source, signature):
    return function(source, signature) + ';'


def prefix(source, signature, marker):
    begin = source.index(signature)
    end = source.index(marker, begin)
    return source[begin:end]


def main():
    shared = (DLSS5 / 'dlssnr.hpp').read_text(encoding='utf-8')
    OUT.mkdir(parents=True, exist_ok=True)

    tick = function(shared, 'inline void RunLifecycleTick(')
    src_checks = [
        ('turn precedes runtime lock and both services follow quiesce update',
         tick.index('retirement_turn.emplace(/*force=*/true)') < tick.index('RuntimeTryLock')
         < tick.index('UpdateShutdownQuiesce(') < tick.index('ServiceNrRetirementLocked(')
         < tick.index('vk::ServiceNrRetirementLocked(')),
        ('both retirement queues request a turn',
         '|| vk::retirement_pending.load(std::memory_order_relaxed)' in tick),
        ('timeout escape removed', 'ClearReplacementWait' not in shared),
        ('off service counter is observable', '<< nr_off_service_events' in shared),
        ('RecreateFeatures retires marked',
         'ReleaseAllNrSlots(feature, true, true);'
         in function(shared, 'inline void RecreateFeatures() {')),
        ('ForgetSourceHandleLocked retires unmarked',
         'ReleaseAllNrSlots(entry->second, true);'
         in function(
             shared,
             'inline bool ForgetSourceHandleLocked(const NVSDK_NGX_Handle*'
             ' handle) {')),
        ('idle stream retirement stays unmarked',
         'if (stream_idle) ReleaseAllNrSlots(state->second, true);' in shared),
        ('failed-pass retirement stays unmarked',
         shared.count('ReleaseNrSlot(feature, failed_pass, true);') == 2),
        ('runtime stats probe removed',
         all(token not in shared for token in
             ('DLSSNRGetStatsCallback', 'NrStatsBytes', 'ngx_nr_mib'))),
        ('replay hold counter stays in retired telemetry',
         '<< " replay_hold_events=" << replay_hold_events'
         in prefix(shared, '"] retired[ws="', '"] lifetime[ws +"')),
    ]
    for name, ok in src_checks:
        if not ok:
            print(f'source contract failed: {name}')
            raise SystemExit(1)

    fixture = r'''
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

typedef long HRESULT;
#define S_OK ((HRESULT)0)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define IID_PPV_ARGS(pp) 0, pp

struct ID3D12Device {
  virtual HRESULT GetDeviceRemovedReason() { return S_OK; }
  virtual void Release() {}
};
struct ID3D12Resource { virtual void Release() {} int token = 0; };
struct ID3D12GraphicsCommandList {};
struct ID3D12Fence {
  virtual HRESULT GetDevice(int, ID3D12Device** out) {
    *out = new ID3D12Device();
    return S_OK;
  }
};
struct NVSDK_NGX_Handle {};
struct NVSDK_NGX_Parameter {};
typedef int NVSDK_NGX_Feature;
constexpr NVSDK_NGX_Feature kFeatureDlss = 18;
typedef int DXGI_FORMAT;
constexpr DXGI_FORMAT DXGI_FORMAT_UNKNOWN = 0;

namespace reshade::log { enum class level { info, warning, error }; }

// The same source-units shape dlssnr.hpp reads from renodx::dlss5::codec.
namespace renodx::dlss5::codec {
enum class Encoding { Linear, Pq };
struct SourceUnits {
  Encoding encoding = Encoding::Linear;
  bool absolute = false;
  float unit_nits = 0.f;
  bool bt2020 = false;
};
}  // namespace renodx::dlss5::codec

#include "declines.hpp"

namespace renodx::addons::dlss5::internal {
namespace codec = ::renodx::dlss5::codec;

constexpr uint32_t kMaxNrPasses = 4;
struct NormStream {};

struct NrFeatureSlot {
  NVSDK_NGX_Handle* handle = nullptr;
  NVSDK_NGX_Parameter* parameters = nullptr;
  bool via_core = false;
  bool tracked_since_create = false;
  bool failed = false;
  int64_t failed_ns = 0;
  uint32_t fail_count = 0;
  struct { ID3D12Resource* buffers[2] = {}; } look_history;
};

struct FeatureState {
  const ID3D12Resource* last_workset_key = nullptr;
  NormStream norm;
  uint32_t replacement_wait_evaluates = 0;
  NrFeatureSlot slots[kMaxNrPasses];
};

struct FinalResources {
  uint32_t id = 0;
  ID3D12Resource* original = nullptr;
  ID3D12Resource* proxy = nullptr;
  ID3D12Resource* nr_output = nullptr;
  ID3D12Resource* decoded = nullptr;
  ID3D12Resource* decoded_alt = nullptr;
  ID3D12Resource* pre_sr_color = nullptr;
  ID3D12Resource* block_mean = nullptr;
  ID3D12Resource* work0 = nullptr;
  ID3D12Resource* work_a = nullptr;
  ID3D12Resource* work_b = nullptr;
  ID3D12Resource* norm_scale = nullptr;
  ID3D12Resource* norm_commit = nullptr;
  ID3D12Resource* descriptors = nullptr;
  ID3D12Resource* pre_sr_source = nullptr;
  ID3D12Resource* look_output = nullptr;
  ID3D12Resource* look_band_a = nullptr;
  ID3D12Resource* look_band_b = nullptr;
  ID3D12Resource* look_band_max_a = nullptr;
  ID3D12Resource* look_band_max_b = nullptr;
  ID3D12Resource* look_up_proxy = nullptr;
  ID3D12Resource* look_up_neural = nullptr;
  ID3D12Resource* look_trace_histogram = nullptr;
  ID3D12Resource* look_trace_blocks = nullptr;
  uint32_t width = 0, height = 0, input_width = 0, input_height = 0;
  uint32_t stack_passes = 0;
  DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
  bool with_pre_sr_color = false;
  uint8_t hdr_mode = 0;
  codec::SourceUnits units;
  uint32_t look_surfaces = 0;
  uint64_t allocated_bytes = 0;
  uint64_t last_used_generation = 0;
  int64_t last_used_ns = 0;
  const FeatureState* retirement_owner = nullptr;
  bool replacement = false;
};

enum class GpuLeaseState { kPending, kCompleted, kDeviceRemoved };
struct GpuLease {
  std::vector<std::pair<ID3D12Fence*, uint64_t>> waits;
  bool coverage_complete = false;
  GpuLeaseState state = GpuLeaseState::kPending;
  bool empty() const { return !coverage_complete || waits.empty(); }
};

'''
    fixture += struct_(shared, 'struct RetiredFinalResources {') + '\n'
    fixture += struct_(shared, 'struct RetiredNrFeature {') + '\n'
    fixture += r'''
// --- globals and fakes the extracted code references -----------------------
inline std::vector<std::string> logs;
inline void Log(reshade::log::level, const std::string& message) {
  logs.push_back(message);
}
inline int64_t fake_now_ns = 0;
inline int64_t SteadyNowNs() { return fake_now_ns; }
inline codec::SourceUnits InterpretSourceUnits(uint8_t) { return {}; }
inline std::string VramSummary(ID3D12Device*, bool) { return {}; }
struct DirectCallScope {};
inline void (*core_release_feature)(NVSDK_NGX_Handle*) = nullptr;
inline void (*core_destroy_parameters)(NVSDK_NGX_Parameter*) = nullptr;
struct DirectApi { void (*release)(NVSDK_NGX_Handle*) = nullptr; };
inline DirectApi direct_api;
template <class T> inline void ReleaseCom(T*& p) { p = nullptr; }

inline bool fake_releasable = false;
inline std::unordered_set<const void*> fake_replay_held_uses;
inline std::vector<const void*> fake_replay_queries;
namespace submission {
struct GenerationSubmission {};
inline void PruneCompletedGenerations(
    void (*)(const void*, const std::vector<GenerationSubmission>&)) {}
inline bool ResourceReleasable(const void* use) {
  return use == nullptr || fake_releasable;
}
inline bool UseHeldByOpenRecording(const void* use) {
  fake_replay_queries.push_back(use);
  return use != nullptr && fake_replay_held_uses.contains(use);
}
}  // namespace submission
namespace screenshot { inline bool WantsFinalPresent() { return false; } }
inline void PreservePresentCaptureProof(
    const void*, const std::vector<submission::GenerationSubmission>&) {}

inline GpuLeaseState QueryGpuLease(const GpuLease& lease) { return lease.state; }
inline GpuLease next_lease;
inline GpuLease AcquireGpuLease(uint64_t = 1) { return next_lease; }

inline uint64_t present_generation = 0;
inline std::atomic_bool enabled{true};
inline std::atomic_bool queue_tracking_active{false};
inline std::atomic_uint32_t workset_free_policy{0}, disable_release_mode{0};
inline std::atomic_bool shutdown_quiesce_latched{false};
constexpr uint32_t kDefaultMaxWorksets = 4;
inline std::atomic_uint32_t max_worksets = kDefaultMaxWorksets;
inline uint32_t next_workset_id = 1;
inline uint64_t worksets_created = 0, worksets_retired = 0, worksets_released = 0;
inline uint32_t worksets_released_logged = 0;
inline uint64_t nr_features_created = 0, nr_features_retired = 0,
                nr_features_released = 0;
inline uint64_t nr_features_retired_on_lease = 0;
inline uint64_t retired_late_leases = 0;
inline std::atomic_bool nr_retirement_pending{false};
inline std::atomic_uint32_t active_features{0};
inline std::atomic_bool nr_feature_ever_ready{false};
inline const NVSDK_NGX_Handle* workset_idle_exempt_handle = nullptr;
inline int64_t workset_idle_exempt_ns = 0;
inline constexpr int64_t kWorksetIdleRetireNs = 5'000'000'000;
inline uint32_t worksets_idle_retired_logged = 0;
inline uint64_t worksets_idle_retired = 0;
constexpr uint32_t kLifecycleLogCap = 256;
constexpr uint32_t kReplacementWaitEvaluateBound = 32;
inline uint64_t replacement_wait_starts = 0, replacement_wait_evaluates = 0,
                replacement_bound_hits = 0;
inline uint32_t replacement_wait_logged = 0, replacement_bound_logged = 0,
                nr_feature_released_logged = 0;
inline uint64_t replay_hold_events = 0;
inline ID3D12Device* direct_device = nullptr;
struct GameWindow { uint64_t alternations = 0; };
inline GameWindow game_window;
inline std::unordered_map<const NVSDK_NGX_Handle*, FeatureState> features;
inline std::vector<RetiredFinalResources> retired_final_resources;
inline std::vector<RetiredNrFeature> retired_nr_features;
inline std::vector<uint32_t> released_workset_ids;
inline std::vector<const void*> released_feature_handles;
inline void ReleaseFinalResources(FinalResources& resources) {
  released_workset_ids.push_back(resources.id);
}
inline void ReleaseRetiredNrFeature(const RetiredNrFeature& retired) {
  released_feature_handles.push_back(retired.handle);
}
'''
    # Extracted verbatim from production.
    fixture += struct_(shared, 'struct WorksetKey {') + '\n'
    fixture += struct_(shared, 'struct WorksetKeyHash {') + '\n'
    fixture += 'inline std::unordered_map<WorksetKey, FinalResources, WorksetKeyHash> worksets;\n'
    fixture += 'inline void ReleaseAllNrSlots(FeatureState& state, bool retire = false, bool replacement = false);\n'
    fixture += function(shared, 'inline const std::vector<const void*>& CollectWorksetGpuObjects(') + '\n'
    fixture += function(shared, 'inline bool WorksetRecordingsComplete(') + '\n'
    fixture += function(shared, 'inline void MarkWorksetUsed(FinalResources* resources)') + '\n'
    fixture += function(shared, 'inline void RetireWorksetResources(FinalResources&& resources)') + '\n'
    fixture += function(shared, 'inline void RetireWorkset(const WorksetKey& key)') + '\n'
    fixture += function(shared, 'inline void RetireSupersededWorksets(int64_t now_ns)') + '\n'
    fixture += function(shared, 'inline void RefreshActiveFeatureCount()') + '\n'
    fixture += function(shared, 'inline RetiredNrFeature NrRetirement(const NrFeatureSlot& slot)') + '\n'
    fixture += function(shared, 'inline void ReleaseNrSlot(') + '\n'
    fixture += function(shared, 'inline void ReleaseAllNrSlots(FeatureState& state, bool retire, bool replacement) {') + '\n'
    fixture += function(shared, 'inline void DrainRetiredResources(bool nr_release_allowed)') + '\n'
    fixture += function(shared, 'inline bool ReplacementWaitActive(const FeatureState* owner)') + '\n'
    fixture += function(shared, 'inline void NoteReplacementSlowWait(const FeatureState* owner)') + '\n'
    fixture += function(shared, 'inline bool ReplacementWaitGate(') + '\n'
    fixture += 'inline std::atomic_bool teardown_pending{false};\n'
    fixture += prefix(shared, 'inline constexpr int64_t kQuiesceUserOverrideWindowNs',
                      'inline void NoteUserNrChange()')
    fixture += function(shared, 'inline void NoteUserNrChange()') + '\n'
    fixture += function(shared, 'inline bool QuiesceHoldsRelease()') + '\n'
    fixture += prefix(shared, 'inline uint8_t nr_off_retirement_hold = 0;', '// Quiesce hold (')
    fixture += r'''
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
namespace vk {
inline std::atomic_bool retirement_pending{false};
inline void ServiceNrRetirementLocked(bool) {}
}
inline std::atomic_int64_t lifecycle_tick_owed_ns{0};
inline std::atomic_uint64_t runtime_lock_present_deferred{0};
inline int64_t telemetry_previous_present_ns = 0;
inline std::vector<int> processed_outputs;
inline std::atomic_uint32_t streamline_tag_mask_this_present{0}, streamline_tag_max_batch{0};
inline void SettleRetiredQueueFences() {}
inline void UpdateShutdownQuiesce(bool, int64_t) {}
'''
    fixture += prefix(shared, 'inline void RunLifecycleTick(', '    gpu_timers::Poll(') + '  }\n}\n'
    fixture += function(shared, 'inline bool QuiesceHoldsAllocation()') + '\n'
    fixture += function(shared, 'inline bool QuiesceHoldsFeatureCreate(') + '\n'
    fixture += prefix(
        shared,
        'inline FinalResources* EnsureWorkset(',
        '  // Enforce the pool budget by evicting the least recently used workset.')
    fixture += r'''
  // Fixture tail: the allocation the gate protects.  Mirrors the production
  // create's contract fields so a later evaluate with the same contract
  // matches.
  FinalResources created;
  created.original = reinterpret_cast<ID3D12Resource*>(1);
  created.block_mean = reinterpret_cast<ID3D12Resource*>(2);
  if (hdr_mode != 0) {
    created.work0 = reinterpret_cast<ID3D12Resource*>(3);
    created.norm_scale = reinterpret_cast<ID3D12Resource*>(4);
    created.norm_commit = reinterpret_cast<ID3D12Resource*>(5);
  }
  created.width = output_width;
  created.height = output_height;
  created.input_width = input_width;
  created.input_height = input_height;
  created.format = format;
  created.with_pre_sr_color = create_pre_sr_color;
  created.stack_passes = requested_stack;
  created.hdr_mode = hdr_mode;
  created.units = current_units;
  created.look_surfaces = look_alloc;
  created.retirement_owner = stream_owner;
  MarkWorksetUsed(&created);
  created.id = next_workset_id++;
  created.allocated_bytes = 1 << 20;
  ++worksets_created;
  const auto [it, emplaced] = worksets.emplace(workset_key, std::move(created));
  return &it->second;
}

}  // namespace renodx::addons::dlss5::internal

// ---------------------------------------------------------------------------
// Test driver: mirrors ProcessInline's call pattern (EnsureWorkset, then the
// shared replacement gate) and drives the drain like RunLifecycleTick.
// ---------------------------------------------------------------------------
using namespace renodx::addons::dlss5;
using namespace renodx::addons::dlss5::internal;

int checks = 0, failures = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; \
    std::printf("FAIL line %d: %s\n", __LINE__, #x); } } while (0)

ID3D12Device fake_device;
ID3D12Fence fake_fence;

// An evaluate like ProcessInline's: EnsureWorkset + the shared gate.  Returns
// true when the evaluate was declined by the replacement wait.
struct EvalOut { FinalResources* ws; bool declined; bool quiesce_hold; };
EvalOut Eval(FeatureState& feature, const NVSDK_NGX_Handle* handle,
             const ID3D12Resource* key, uint32_t in_w, uint32_t in_h,
             uint32_t out_w = 1920, uint32_t out_h = 1080,
             uint32_t stack = 1, uint32_t look = 0) {
  bool pending = false;
  bool hold = false;
  FinalResources* ws = EnsureWorkset(
      &fake_device, handle, key, in_w, in_h, out_w, out_h,
      /*format=*/10, /*hdr_mode=*/0, stack, look, false, nullptr, &pending, &hold);
  if (ReplacementWaitGate(feature, handle, pending, ws, in_w, in_h)) {
    CountNrDecline(NrDeclineReason::kReplacementPending);
    return {ws, true, false};
  }
  // The production quiesce-hold gate that follows ReplacementWaitGate.
  if (hold || QuiesceHoldsFeatureCreate(feature, stack)) {
    CountNrDecline(NrDeclineReason::kQuiesceHold);
    return {ws, true, true};
  }
  return {ws, false, false};
}

void CompleteRetiredLeases() {
  for (auto& entry : retired_final_resources) {
    if (!entry.lease.empty()) entry.lease.state = GpuLeaseState::kCompleted;
  }
  for (auto& entry : retired_nr_features) {
    if (!entry.lease.empty()) entry.lease.state = GpuLeaseState::kCompleted;
  }
  DrainRetiredResources(true);
}

GpuLease PendingLease() {
  GpuLease lease;
  lease.waits.push_back({&fake_fence, 7});
  lease.coverage_complete = true;
  lease.state = GpuLeaseState::kPending;
  return lease;
}

void Reset() {
  worksets.clear();
  features.clear();
  retired_final_resources.clear();
  retired_nr_features.clear();
  released_workset_ids.clear();
  released_feature_handles.clear();
  nr_retirement_pending.store(false);
  workset_free_policy.store(0);
  disable_release_mode.store(0);
  nr_off_retirement_hold = 0;
  nr_off_service_events = 0;
  fake_turn_available = true;
  fake_turn_attempts = 0;
  shutdown_quiesce_latched.store(false);
  teardown_pending.store(false);
  last_user_nr_change_ns.store(0);
  quiesce_override_ticks = 0;
  quiesce_override_logged = 0;
  quiesce_override_noted = false;
  queue_tracking_active.store(false);
  enabled.store(true);
  fake_releasable = false;
  fake_replay_held_uses.clear();
  fake_replay_queries.clear();
  logs.clear();
  replay_hold_events = 0;
  fake_now_ns = 0;
  present_generation = 0;
  next_lease = GpuLease{};
  replacement_wait_starts = replacement_wait_evaluates = replacement_bound_hits = 0;
  for (size_t i = 0; i < static_cast<size_t>(NrDeclineReason::kCount); ++i) {
    nr_decline_counts[i].store(0);
  }
}

uint32_t ReplacementMarkedWorksets() {
  uint32_t count = 0;
  for (const auto& entry : retired_final_resources) {
    if (entry.resources.replacement) ++count;
  }
  return count;
}

void RetireReplayPair(FeatureState& owner, NVSDK_NGX_Handle* handle,
                      ID3D12Resource* resource, uint32_t id) {
  FinalResources ws;
  ws.id = id;
  ws.original = resource;
  ws.retirement_owner = &owner;
  ws.replacement = true;
  RetireWorksetResources(std::move(ws));
  owner.slots[0].handle = handle;
  owner.slots[0].tracked_since_create = true;
  ReleaseAllNrSlots(owner, true, true);
}

size_t ReplayLogCount() {
  return std::count_if(logs.begin(), logs.end(), [](const std::string& message) {
    return message.starts_with("NR retirement held:");
  });
}

int main() {
  // Objects standing in for the game handle and its output resource.
  NVSDK_NGX_Handle handle_a, handle_b;
  ID3D12Resource key1, key2, key3;

  // --- 1: contract change -> marked retire -> pending -> release -> create ---
  {
    Reset();
    FeatureState& stream = features[&handle_a];
    next_lease = PendingLease();
    EvalOut first = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(!first.declined && first.ws != nullptr);
    const uint32_t first_id = first.ws->id;
    // Contract change: same key, different NR input dims.
    EvalOut second = Eval(stream, &handle_a, &key1, 960, 540);
    CHECK(second.declined && second.ws == nullptr);
    CHECK(ReplacementMarkedWorksets() == 1);
    CHECK(retired_final_resources.front().resources.id == first_id);
    CHECK(retired_final_resources.front().resources.retirement_owner == &stream);
    // A third contract while pending: still declined, still one generation.
    EvalOut third = Eval(stream, &handle_a, &key1, 853, 480);
    CHECK(third.declined && third.ws == nullptr);
    CHECK(ReplacementMarkedWorksets() == 1 && worksets.empty());
    CompleteRetiredLeases();
    CHECK(retired_final_resources.empty());
    CHECK(released_workset_ids.size() == 1 && released_workset_ids[0] == first_id);
    // The then-latest contract (853x480) is the one that allocates.
    EvalOut fourth = Eval(stream, &handle_a, &key1, 853, 480);
    CHECK(!fourth.declined && fourth.ws != nullptr);
    CHECK(fourth.ws->input_width == 853 && worksets.size() == 1);
    // Nothing retired a second time: one release only.
    CHECK(released_workset_ids.size() == 1);
  }

  // --- 2: an unmarked (pool-eviction style) retirement never gates ---------
  {
    Reset();
    FeatureState& stream = features[&handle_a];
    next_lease = PendingLease();
    EvalOut first = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(first.ws != nullptr);
    // Same shape RetireWorksetResources takes on the LRU path: retired, but
    // never marked replacement.
    FinalResources moved = std::move(worksets.begin()->second);
    worksets.erase(worksets.begin());
    RetireWorksetResources(std::move(moved));
    CHECK(retired_final_resources.size() == 1
          && !retired_final_resources.front().resources.replacement);
    EvalOut second = Eval(stream, &handle_a, &key2, 1280, 720);
    CHECK(!second.declined && second.ws != nullptr);
  }

  // --- 3: an unprovable replacement entry cannot gate ----------------------
  {
    Reset();
    FeatureState& stream = features[&handle_a];
    next_lease = GpuLease{};  // no fence anywhere: empty lease, no tracker
    EvalOut first = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(first.ws != nullptr);
    EvalOut second = Eval(stream, &handle_a, &key1, 960, 540);
    CHECK(!second.declined && second.ws != nullptr);
    CHECK(ReplacementMarkedWorksets() == 1);
  }

  // --- 4: quiesce holds allocation (not the replacement wait); Keep never
  // gates --------------------------------------------------------------------
  {
    Reset();
    FeatureState& stream = features[&handle_a];
    next_lease = PendingLease();
    EvalOut first = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(first.ws != nullptr);
    NVSDK_NGX_Handle nr_handle;
    stream.slots[0].handle = &nr_handle;
    shutdown_quiesce_latched.store(true);
    // Same contract, workset and slot live: the latch holds nothing.
    EvalOut same = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(!same.declined && same.ws != nullptr);
    // A live workset whose NR slot is missing: the slot create is held.
    stream.slots[0].handle = nullptr;
    EvalOut slot_held = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(slot_held.declined && slot_held.quiesce_hold && slot_held.ws != nullptr);
    CHECK(worksets.size() == 1);
    stream.slots[0].handle = &nr_handle;
    // Contract change under the latch: the old generation retires (held),
    // the replacement wait does not apply, and the new workset is NOT
    // allocated - quiesce_hold, a pass-through.
    EvalOut second = Eval(stream, &handle_a, &key1, 960, 540);
    CHECK(second.declined && second.quiesce_hold && second.ws == nullptr);
    CHECK(worksets.empty() && retired_final_resources.size() == 1);
    // Latch lifts: the held generation releases, the replacement allocates.
    shutdown_quiesce_latched.store(false);
    EvalOut gated = Eval(stream, &handle_a, &key1, 960, 540);
    CHECK(gated.declined && !gated.quiesce_hold);  // replacement wait now
    CompleteRetiredLeases();
    EvalOut after = Eval(stream, &handle_a, &key1, 960, 540);
    CHECK(!after.declined && after.ws != nullptr);
    workset_free_policy.store(1);
    Reset();
    FeatureState& stream2 = features[&handle_a];
    next_lease = PendingLease();
    workset_free_policy.store(1);
    Eval(stream2, &handle_a, &key1, 1280, 720);
    EvalOut third = Eval(stream2, &handle_a, &key1, 960, 540);
    CHECK(!third.declined && third.ws != nullptr);
    workset_free_policy.store(0);
  }

  // --- 5: another stream's replacement does not gate this stream -----------
  {
    Reset();
    FeatureState& stream_a = features[&handle_a];
    FeatureState& stream_b = features[&handle_b];
    next_lease = PendingLease();
    Eval(stream_a, &handle_a, &key1, 1280, 720);
    EvalOut declined = Eval(stream_a, &handle_a, &key1, 960, 540);
    CHECK(declined.declined);
    EvalOut other = Eval(stream_b, &handle_b, &key2, 640, 360);
    CHECK(!other.declined && other.ws != nullptr);
  }

  // --- 6: slow waits only diagnose; saturation cannot reopen the gate -----
  {
    Reset();
    FeatureState& stream = features[&handle_a];
    next_lease = PendingLease();
    Eval(stream, &handle_a, &key1, 1280, 720);
    const uint64_t creates = worksets_created;
    for (uint32_t i = 0; i < 1100; ++i) {
      EvalOut out = Eval(stream, &handle_a, &key1, 960, 540);
      CHECK(out.declined && out.ws == nullptr && worksets_created == creates);
    }
    CHECK(replacement_bound_hits == 1);
    CHECK(ReplacementMarkedWorksets() == 1);
    CHECK(retired_final_resources.size() == 1);
    stream.replacement_wait_evaluates = UINT32_MAX - 1;
    replacement_wait_evaluates = UINT64_MAX - 1;
    for (int i = 0; i < 4; ++i) CHECK(Eval(stream, &handle_a, &key1, 960, 540).declined);
    CHECK(stream.replacement_wait_evaluates == UINT32_MAX);
    CHECK(replacement_wait_evaluates == UINT64_MAX && replacement_bound_hits == 1);
    CompleteRetiredLeases();
    EvalOut after = Eval(stream, &handle_a, &key1, 960, 540);
    CHECK(!after.declined && after.ws != nullptr);
    CHECK(stream.replacement_wait_evaluates == 0);
  }

  // --- 7: contract-retired features gate; failure-retired ones do not ------
  {
    Reset();
    FeatureState& stream = features[&handle_a];
    next_lease = PendingLease();
    EvalOut first = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(first.ws != nullptr);
    NVSDK_NGX_Handle nr_handle;
    stream.slots[0].handle = &nr_handle;
    stream.slots[0].parameters = nullptr;
    // Failure retirement (replacement=false) leaves the gate open even while
    // the entry is still provably resident.
    ReleaseAllNrSlots(stream, true);
    CHECK(retired_nr_features.size() == 1
          && !retired_nr_features.front().replacement);
    EvalOut second = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(!second.declined && second.ws != nullptr);
    // Contract retirement of the features themselves: the workset survives,
    // and the gate still holds the stream back.
    stream.slots[1].handle = reinterpret_cast<NVSDK_NGX_Handle*>(&key3);
    ReleaseAllNrSlots(stream, true, true);
    CHECK(ReplacementWaitActive(&stream));
    EvalOut third = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(third.declined && third.ws != nullptr);  // workset found, still gated
    CompleteRetiredLeases();
    EvalOut fourth = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(!fourth.declined && fourth.ws != nullptr);
    // The contract-retired feature was the one released in the drain.
    bool released_marked = false;
    for (const void* h : released_feature_handles) {
      released_marked = released_marked || h == reinterpret_cast<void*>(&key3);
    }
    CHECK(released_marked);
  }

  // --- 8: idle retirement (RetireSupersededWorksets) never gates -----------
  {
    Reset();
    FeatureState& stream_a = features[&handle_a];
    FeatureState& stream_b = features[&handle_b];
    next_lease = PendingLease();
    fake_now_ns = 10'000'000'000;
    EvalOut a = Eval(stream_a, &handle_a, &key1, 1280, 720);
    CHECK(a.ws != nullptr);
    worksets.begin()->second.last_used_ns = fake_now_ns - 6'000'000'000;
    EvalOut b = Eval(stream_b, &handle_b, &key2, 640, 360);  // live session
    CHECK(b.ws != nullptr);
    fake_now_ns += 100'000'000;
    RetireSupersededWorksets(fake_now_ns);
    CHECK(retired_final_resources.size() == 1
          && !retired_final_resources.front().resources.replacement);
    EvalOut next = Eval(stream_a, &handle_a, &key3, 1280, 720);
    CHECK(!next.declined && next.ws != nullptr);
  }

  // --- 9: NR off retires every stream marked; re-enable waits for release --
  {
    Reset();
    FeatureState& stream = features[&handle_a];
    next_lease = PendingLease();
    EvalOut first = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(first.ws != nullptr);
    NVSDK_NGX_Handle nr_handle;
    stream.slots[0].handle = &nr_handle;
    stream.slots[0].parameters = nullptr;
    enabled.store(false);
    RetireNrForDisableLocked();
    CHECK(worksets.empty() && stream.slots[0].handle == nullptr);
    CHECK(retired_final_resources.size() == 1
          && retired_final_resources.front().resources.replacement
          && retired_final_resources.front().resources.retirement_owner == &stream);
    CHECK(retired_nr_features.size() == 1
          && retired_nr_features.front().replacement
          && retired_nr_features.front().owner == &stream);
    CHECK(ReplacementWaitActive(&stream));
    for (int i = 0; i < 20; ++i) {
      enabled.store((i & 1) == 0);
      RunLifecycleTick(i + 1, true);
    }
    CHECK(retired_final_resources.size() == 1 && retired_nr_features.size() == 1);
    CHECK(nr_off_service_events == 1);
    // Re-enable: the stream's next evaluate waits for the old generation.
    enabled.store(true);
    EvalOut second = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(second.declined && second.ws == nullptr);
    CompleteRetiredLeases();
    CHECK(retired_final_resources.empty() && retired_nr_features.empty());
    EvalOut third = Eval(stream, &handle_a, &key1, 1280, 720);
    CHECK(!third.declined && third.ws != nullptr);
    // A later NR-off with only the workset live retires just that.
    enabled.store(false);
    RetireNrForDisableLocked();
    CHECK(worksets.empty() && retired_final_resources.size() == 1
          && retired_nr_features.empty());
    // Nothing live at all: a no-op.
    CompleteRetiredLeases();
    RetireNrForDisableLocked();
    CHECK(worksets.empty() && retired_final_resources.empty());
    // Keep policy and the quiesce latch never retire.
    Reset();
    FeatureState& kept = features[&handle_a];
    next_lease = PendingLease();
    Eval(kept, &handle_a, &key1, 1280, 720);
    enabled.store(false);
    workset_free_policy.store(1);
    RetireNrForDisableLocked();
    CHECK(worksets.size() == 1 && retired_final_resources.empty());
    workset_free_policy.store(0);
    shutdown_quiesce_latched.store(true);
    RetireNrForDisableLocked();
    CHECK(worksets.size() == 1 && retired_final_resources.empty());
    shutdown_quiesce_latched.store(false);
  }

  // The production lifecycle prefix calls the extracted desired-state service.
  for (bool keep : {false, true}) {
    Reset();
    FeatureState& stream = features[&handle_a];
    queue_tracking_active.store(true);
    Eval(stream, &handle_a, &key1, 1280, 720);
    stream.slots[0].handle = &handle_b;
    stream.slots[0].tracked_since_create = true;
    enabled.store(false);
    workset_free_policy.store(keep ? 1 : 0);
    shutdown_quiesce_latched.store(!keep);
    RetireNrForDisableLocked();
    for (int i = 0; i < 40; ++i) RunLifecycleTick(i + 1, true);
    CHECK(worksets.size() == 1 && retired_final_resources.empty());
    CHECK(stream.slots[0].handle == &handle_b && retired_nr_features.empty());
    CHECK(nr_off_service_events == 1 && logs.back().find("deferred") != std::string::npos);
    CHECK(fake_turn_attempts == 0);
    workset_free_policy.store(0);
    shutdown_quiesce_latched.store(false);
    fake_releasable = true;
    RunLifecycleTick(41, true);
    CHECK(worksets.empty() && retired_final_resources.empty());
    CHECK(stream.slots[0].handle == nullptr && retired_nr_features.size() == 1);
    CHECK(released_workset_ids.size() == 1 && released_feature_handles.empty());
    CHECK(nr_off_service_events == 3 && nr_retirement_pending.load());
    CHECK(fake_turn_attempts == 0);  // new retirement requests a turn only next tick
    fake_turn_available = false;
    RunLifecycleTick(42, true);
    CHECK(fake_turn_attempts == 1 && retired_nr_features.size() == 1);
    fake_turn_available = true;
    RunLifecycleTick(43, true);
    CHECK(fake_turn_attempts == 2 && retired_nr_features.empty());
    CHECK(released_feature_handles.size() == 1 && !nr_retirement_pending.load());
    RunLifecycleTick(44, true);
    CHECK(nr_off_service_events == 3 && released_workset_ids.size() == 1);
    CHECK(fake_turn_attempts == 2 && released_feature_handles.size() == 1);
  }
  for (bool keep : {false, true}) {
    Reset();
    FeatureState& stream = features[&handle_a];
    Eval(stream, &handle_a, &key1, 1280, 720);
    stream.slots[0].handle = &handle_b;
    enabled.store(false);
    workset_free_policy.store(keep ? 1 : 0);
    shutdown_quiesce_latched.store(!keep);
    RunLifecycleTick(1, true);
    enabled.store(true);
    RunLifecycleTick(2, true);
    CHECK(nr_off_service_events == 2 && logs.back().find("cancelled") != std::string::npos);
    workset_free_policy.store(0);
    shutdown_quiesce_latched.store(false);
    RunLifecycleTick(3, true);
    CHECK(worksets.size() == 1 && stream.slots[0].handle == &handle_b);
    CHECK(retired_final_resources.empty() && retired_nr_features.empty());
    CHECK(nr_off_retirement_hold == 0 && nr_off_service_events == 2);
  }
  {
    Reset();
    shutdown_quiesce_latched.store(true);
    for (int i = 0; i < 40; ++i) {
      enabled.store(false);
      RunLifecycleTick(2 * i + 1, true);
      enabled.store(true);
      RunLifecycleTick(2 * i + 2, true);
    }
    CHECK(nr_off_service_events == 80 && logs.size() == 64);
  }

  // vramserial7: a user's NR change overrides the latch's release hold.  The
  // latch alone (no user change recorded) still holds; a pending teardown
  // always holds; a stale change (past the window) holds unless NR is off.
  {
    Reset();
    FeatureState& stream = features[&handle_a];
    queue_tracking_active.store(true);
    Eval(stream, &handle_a, &key1, 1280, 720);
    stream.slots[0].handle = &handle_b;
    stream.slots[0].tracked_since_create = true;
    fake_releasable = true;
    fake_now_ns = 100'000'000'000;
    shutdown_quiesce_latched.store(true);
    // No user change on record: held, as before.
    enabled.store(false);
    RunLifecycleTick(1, true);
    CHECK(worksets.size() == 1 && retired_final_resources.empty());
    CHECK(nr_off_retirement_hold == 2 && quiesce_override_ticks == 0);
    // The user's off (NoteUserNrChange on the off edge): retire and release
    // despite the latch, one override line for the episode.
    NoteUserNrChange();
    RunLifecycleTick(2, true);
    CHECK(worksets.empty() && retired_final_resources.empty());
    CHECK(released_workset_ids.size() == 1 && stream.slots[0].handle == nullptr);
    CHECK(nr_off_retirement_hold == 0 && quiesce_override_ticks >= 1);
    auto override_lines = [] {
      size_t count = 0;
      for (const auto& line : logs) count += line.find("NR quiesce override") != std::string::npos;
      return count;
    };
    CHECK(override_lines() == 1 && quiesce_override_logged == 1);
    RunLifecycleTick(3, true);
    CHECK(retired_nr_features.empty() && released_feature_handles.size() == 1);
    CHECK(override_lines() == 1);  // once per latch episode
    // A pending teardown holds regardless of the user.
    Reset();
    FeatureState& leaving = features[&handle_a];
    Eval(leaving, &handle_a, &key1, 1280, 720);
    fake_releasable = true;
    fake_now_ns = 100'000'000'000;
    shutdown_quiesce_latched.store(true);
    teardown_pending.store(true);
    enabled.store(false);
    NoteUserNrChange();
    RunLifecycleTick(1, true);
    CHECK(worksets.size() == 1 && retired_final_resources.empty());
    CHECK(nr_off_retirement_hold == 2 && quiesce_override_ticks == 0);
    // NR on, a recent setting change (RecreateFeatures): the replacement
    // generation releases under the latch; allocation stays held.
    Reset();
    FeatureState& changed = features[&handle_a];
    queue_tracking_active.store(true);
    Eval(changed, &handle_a, &key1, 1280, 720);
    fake_releasable = true;
    fake_now_ns = 100'000'000'000;
    NoteUserNrChange();
    worksets.begin()->second.replacement = true;
    RetireWorkset({&handle_a, &key1});
    shutdown_quiesce_latched.store(true);
    RunLifecycleTick(1, true);
    CHECK(retired_final_resources.empty() && released_workset_ids.size() == 1);
    CHECK(quiesce_override_ticks >= 1 && QuiesceHoldsAllocation());
    CHECK(Eval(changed, &handle_a, &key1, 1280, 720).quiesce_hold && worksets.empty());
    // The same change a minute later no longer overrides while NR stays on.
    shutdown_quiesce_latched.store(false);
    fake_now_ns += kQuiesceUserOverrideWindowNs;
    Eval(changed, &handle_a, &key1, 1280, 720);
    CHECK(worksets.size() == 1);
    worksets.begin()->second.replacement = true;
    RetireWorkset({&handle_a, &key1});
    shutdown_quiesce_latched.store(true);
    const uint64_t ticks_before = quiesce_override_ticks;
    RunLifecycleTick(2, true);
    CHECK(retired_final_resources.size() == 1 && quiesce_override_ticks == ticks_before);
    // ... but NR switched off by the user does, however old the change.
    enabled.store(false);
    RunLifecycleTick(3, true);
    CHECK(retired_final_resources.empty() && quiesce_override_ticks > ticks_before);
  }

  for (bool nr_enabled : {false, true}) {
    for (bool tracker : {false, true}) {
      Reset();
      enabled.store(nr_enabled);
      queue_tracking_active.store(tracker);
      next_lease = PendingLease();
      FeatureState& owner = features[&handle_a];
      RetireReplayPair(owner, &handle_b, &key1, 101);
      CHECK(!retired_final_resources.front().replay_hold_noted
            && !retired_nr_features.front().replay_hold_noted);
      fake_replay_held_uses = {&key1, &handle_b};
      DrainRetiredResources(true);
      const bool note = !nr_enabled && tracker;
      CHECK(replay_hold_events == (note ? 2u : 0u));
      CHECK(logs.size() == (note ? 2u : 0u));
      CHECK(fake_replay_queries.empty() == !note);
      CHECK(retired_final_resources.front().replay_hold_noted == note
            && retired_nr_features.front().replay_hold_noted == note);
      CHECK(released_workset_ids.empty() && released_feature_handles.empty());
      if (note) {
        std::ostringstream workset_message, feature_message;
        workset_message << "NR retirement held: workset ws101 (owner=" << &owner
                        << ") has a completed recording still replayable; waiting for command-list Reset/destroy";
        feature_message << "NR retirement held: feature " << &handle_b
                        << " (owner=" << &owner
                        << ") has a completed recording still replayable; waiting for command-list Reset/destroy";
        CHECK(logs[0] == workset_message.str());
        CHECK(logs[1] == feature_message.str());
      }
      const size_t query_count = fake_replay_queries.size();
      DrainRetiredResources(true);
      CHECK(replay_hold_events == (note ? 2u : 0u));
      CHECK(fake_replay_queries.size() == query_count);
    }
  }

  {
    Reset();
    enabled.store(false);
    queue_tracking_active.store(true);
    FeatureState& owner = features[&handle_a];
    RetireReplayPair(owner, &handle_b, &key1, 102);
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 0 && logs.empty());
    CHECK(!retired_final_resources.front().replay_hold_noted
          && !retired_nr_features.front().replay_hold_noted);
    CHECK(fake_replay_queries.size() == 4);
    fake_replay_held_uses = {&key1, &handle_b};
    fake_replay_queries.clear();
    DrainRetiredResources(false);
    CHECK(replay_hold_events == 1 && logs.size() == 1);
    CHECK(retired_final_resources.front().replay_hold_noted
          && !retired_nr_features.front().replay_hold_noted);
    CHECK(fake_replay_queries == std::vector<const void*>{&key1});
    DrainRetiredResources(false);
    CHECK(replay_hold_events == 1 && logs.size() == 1);
    CHECK(fake_replay_queries.size() == 1);
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 2 && logs.size() == 2);
    CHECK(retired_nr_features.front().replay_hold_noted);
    CHECK(released_workset_ids.empty() && released_feature_handles.empty());
    enabled.store(true);
    DrainRetiredResources(true);
    enabled.store(false);
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 2 && logs.size() == 2);
    CHECK(fake_replay_queries.size() == 2);
    fake_replay_held_uses.clear();
    fake_replay_queries.clear();
    fake_releasable = true;
    DrainRetiredResources(false);
    CHECK(retired_final_resources.empty() && released_workset_ids.size() == 1);
    CHECK(retired_nr_features.size() == 1 && released_feature_handles.empty());
    CHECK(nr_retirement_pending.load() && fake_replay_queries.empty());
    DrainRetiredResources(true);
    CHECK(retired_nr_features.empty() && released_feature_handles.size() == 1);
    CHECK(!nr_retirement_pending.load() && fake_replay_queries.empty());
    CHECK(replay_hold_events == 2 && ReplayLogCount() == 2);
  }

  for (auto member : {
           &FinalResources::original, &FinalResources::proxy,
           &FinalResources::nr_output, &FinalResources::decoded,
           &FinalResources::decoded_alt, &FinalResources::pre_sr_color,
           &FinalResources::pre_sr_source, &FinalResources::block_mean,
           &FinalResources::work0, &FinalResources::work_a, &FinalResources::work_b,
           &FinalResources::norm_scale, &FinalResources::descriptors,
           &FinalResources::look_output, &FinalResources::look_band_a,
           &FinalResources::look_band_b, &FinalResources::look_band_max_a,
           &FinalResources::look_band_max_b, &FinalResources::look_up_proxy,
           &FinalResources::look_up_neural, &FinalResources::look_trace_histogram,
           &FinalResources::look_trace_blocks}) {
    Reset();
    enabled.store(false);
    RetiredFinalResources retired;
    retired.resources.id = 103;
    retired.resources.original = &key1;
    retired.resources.*member = &key2;
    retired.tracker_proof = true;
    retired_final_resources.push_back(retired);
    fake_replay_held_uses = {&key2};
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 1 && logs.size() == 1);
    CHECK(retired_final_resources.size() == 1
          && retired_final_resources.front().replay_hold_noted);
    CHECK(std::find(fake_replay_queries.begin(), fake_replay_queries.end(), &key2)
          != fake_replay_queries.end());
    CHECK(released_workset_ids.empty());
    const size_t query_count = fake_replay_queries.size();
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 1 && logs.size() == 1);
    CHECK(fake_replay_queries.size() == query_count);
  }

  {
    Reset();
    enabled.store(false);
    RetiredFinalResources retired;
    retired.resources.original = &key1;
    retired.resources.norm_commit = &key2;
    retired.tracker_proof = true;
    retired_final_resources.push_back(retired);
    fake_replay_held_uses = {&key2};
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 0 && logs.empty());
    CHECK(fake_replay_queries == std::vector<const void*>{&key1});
    CHECK(!retired_final_resources.front().replay_hold_noted);
  }

  for (int held = 0; held < 4; ++held) {
    Reset();
    enabled.store(false);
    RetiredNrFeature retired;
    retired.handle = held == 3 ? nullptr : &handle_b;
    retired.look_history[0] = &key1;
    retired.look_history[1] = &key2;
    retired.tracker_proof = true;
    retired_nr_features.push_back(retired);
    const void* use = held == 0 ? static_cast<const void*>(&handle_b)
                               : held == 1 ? &key1 : &key2;
    fake_replay_held_uses = {use};
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 1 && logs.size() == 1);
    CHECK(retired_nr_features.size() == 1
          && retired_nr_features.front().replay_hold_noted);
    CHECK(std::find(fake_replay_queries.begin(), fake_replay_queries.end(), use)
          != fake_replay_queries.end());
    CHECK(released_feature_handles.empty());
    const size_t query_count = fake_replay_queries.size();
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 1 && logs.size() == 1);
    CHECK(fake_replay_queries.size() == query_count);
    fake_releasable = true;
    fake_replay_held_uses.clear();
    retired_nr_features.front().lease = PendingLease();
    DrainRetiredResources(true);
    CHECK(retired_nr_features.size() == 1 && released_feature_handles.empty());
    CHECK(replay_hold_events == 1 && fake_replay_queries.size() == query_count);
    retired_nr_features.front().lease.state = GpuLeaseState::kCompleted;
    DrainRetiredResources(false);
    CHECK(retired_nr_features.size() == 1 && released_feature_handles.empty());
    DrainRetiredResources(true);
    CHECK(retired_nr_features.empty() && released_feature_handles.size() == 1);
    CHECK(replay_hold_events == 1 && ReplayLogCount() == 1);
  }

  {
    Reset();
    enabled.store(false);
    queue_tracking_active.store(true);
    FeatureState& owner = features[&handle_a];
    NVSDK_NGX_Handle handles[34];
    ID3D12Resource resources[34];
    for (uint32_t i = 0; i < 32; ++i) {
      RetireReplayPair(owner, &handles[i], &resources[i], 200 + i);
      fake_replay_held_uses.insert(&handles[i]);
      fake_replay_held_uses.insert(&resources[i]);
    }
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 64 && logs.size() == 64);
    CHECK(released_workset_ids.empty() && released_feature_handles.empty());
    RetireReplayPair(owner, &handles[32], &resources[32], 232);
    fake_replay_held_uses.insert(&handles[32]);
    fake_replay_held_uses.insert(&resources[32]);
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 66 && logs.size() == 64);
    CHECK(retired_final_resources.size() == 33 && retired_nr_features.size() == 33);
    for (const auto& entry : retired_final_resources) CHECK(entry.replay_hold_noted);
    for (const auto& entry : retired_nr_features) CHECK(entry.replay_hold_noted);
    const size_t query_count = fake_replay_queries.size();
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 66 && logs.size() == 64);
    CHECK(fake_replay_queries.size() == query_count);
    fake_releasable = true;
    fake_replay_held_uses.clear();
    DrainRetiredResources(true);
    CHECK(retired_final_resources.empty() && retired_nr_features.empty());
    CHECK(released_workset_ids.size() == 33 && released_feature_handles.size() == 33);
    CHECK(replay_hold_events == 66 && ReplayLogCount() == 64);
    fake_releasable = false;
    RetireReplayPair(owner, &handles[33], &resources[33], 233);
    CHECK(!retired_final_resources.front().replay_hold_noted
          && !retired_nr_features.front().replay_hold_noted);
    fake_replay_held_uses = {&handles[33], &resources[33]};
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 68 && ReplayLogCount() == 64);
    CHECK(retired_final_resources.front().replay_hold_noted
          && retired_nr_features.front().replay_hold_noted);
  }

  {
    Reset();
    enabled.store(false);
    queue_tracking_active.store(true);
    FeatureState& owner = features[&handle_a];
    RetireReplayPair(owner, &handle_b, &key1, 104);
    fake_releasable = true;
    fake_replay_held_uses = {&key1, &handle_b};
    DrainRetiredResources(true);
    CHECK(replay_hold_events == 0 && ReplayLogCount() == 0);
    CHECK(fake_replay_queries.empty());
    CHECK(retired_final_resources.empty() && retired_nr_features.empty());
    CHECK(released_workset_ids.size() == 1 && released_feature_handles.size() == 1);
  }

  std::printf("nr-replacement-serial: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
'''

    # The include path has non-ASCII characters; copy the real header next to
    # the fixture so cl finds it without a /I it cannot parse.
    import shutil
    shutil.copyfile(DLSS5 / 'declines.hpp', OUT / 'declines.hpp')
    (OUT / 'nr-replacement-fixture.cpp').write_text(fixture, encoding='utf-8')
    runner = OUT / 'run-replacement-fixture.cmd'
    runner.write_text('@echo off\nsetlocal\nset "VSCMD_SKIP_SENDTELEMETRY=1"\n'
                      'call "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64 >nul\n'
                      'if errorlevel 1 exit /b %errorlevel%\n'
                      'cl /nologo /W4 /O2 /MT /EHsc /std:c++20 /utf-8 /wd4505 nr-replacement-fixture.cpp /Fe:nr-replacement-fixture.exe\n'
                      'if errorlevel 1 exit /b %errorlevel%\n'
                      'nr-replacement-fixture.exe\nexit /b %errorlevel%\n', encoding='ascii')
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
    print('Limit: CPU fixture of extracted gate/drain/retire/collect code with fake '
          'submission queries; not a real tracker test. No D3D12 or NGX calls run.')


if __name__ == '__main__':
    main()
