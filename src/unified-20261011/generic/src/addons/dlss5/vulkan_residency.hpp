/*
 * Copyright (C) 2026
 * SPDX-License-Identifier: MIT
 */

// Vulkan video-memory residency (2026-10-30).  Included by vulkan_path.hpp
// inside namespace vk, after the layer hooks and before HookedLayerCreateDevice.
//
// Problem (8 GB laptop, two NR layers, 2026-10-29/30 field data): after an NR
// working-resolution change the video memory manager demotes a few of the
// game's own device-local blocks (32 MiB pool blocks, normal priority) to
// system memory and never promotes them back, although the process stays
// under its budget by ~500 MiB afterwards.  A block the game touches every
// frame then costs a system-memory round trip per frame: the real frame rate
// drops to 5-8 per second and frame generation turns that into slow motion.
// The game's device enables neither VK_EXT_memory_priority nor
// VK_EXT_pageable_device_local_memory, so the driver has no priority
// information and no mandate to page the allocation back (NVIDIA on
// vkd3d-proton #1543: "Without VK_EXT_pageable_device_local_memory you can
// end up with rendertargets ... spilled to a system heap and stay there
// forever ... With [it] and some half-decent priority hints, at least
// important stuff can make it back to a device-local heap once there's room
// again").
//
// What this file does, all on the game's own Vulkan device:
//   1. vkCreateDevice (HookedLayerCreateDevice): when the driver enumerates
//      both extensions and reports both features, append the two names and
//      chain the two feature structures (enabled) to the caller's create
//      info.  Nothing is added when the caller already carries them, when the
//      driver lacks them, or when NRVkPageableMemory=0.  A create that fails
//      with the additions restores the loader's layer link and is retried
//      once with the caller's own create info, so this add-on cannot be the
//      reason a game's device creation fails; the additions then stay off
//      for the process.
//   2. Allocation observation: the loader's VkLayerDeviceLink the ReShade
//      layer consumes is replaced by a copy whose pfnNextGetDeviceProcAddr
//      is NextGdpaDetour.  ReShade builds its own dispatch table through it
//      (its VMA allocations: this add-on's images) and forwards every
//      device-level name it does not hook itself through it (the loader's
//      dispatch: the game's, NGX's and Streamline's allocations), so one
//      wrapper sees every vkAllocateMemory / vkFreeMemory of the device.
//      Each live VkDeviceMemory is recorded with its size, memory type and
//      priority.
//   3. Priority, so that a shortage takes this add-on's memory before the
//      game's: VkMemoryPriorityAllocateInfoEXT on device-local allocations by
//      who makes them (thread-local scopes):
//        - OwnScope (this add-on's NR at a working resolution: the NR
//          runtime's create/evaluate, worksets, look history): the lowest
//          class, NRVkOwnMemoryPriority (0.10).  An NR resolution or pass
//          change releases and recreates exactly these, so what a shortage
//          moved to system memory goes with the player's next lower setting
//          instead of waiting for the driver to bring it back.
//        - FixedScope (this add-on's images that do not follow the NR
//          resolution: the pre-FG stream's images, the NR runtime's Init):
//          NRVkOwnFixedMemoryPriority (0.35), below the game's own memory and
//          above OwnScope, so a shortage reaches them only after every
//          NR-resolution allocation.
//        - HotScope without OwnScope (the game's NGX features: DLSS and
//          DLSS-G create/evaluate): NRVkHotMemoryPriority (0.70, the value
//          vkd3d-proton uses for D3D12_RESIDENCY_PRIORITY_HIGH), the HIGH
//          class the 10-27 WPR rundown recorded for NGX/Streamline images
//          without the extensions.
//        - Everything else (the game's own allocations) keeps the default
//          0.50.
//      A priority belongs to a VkDeviceMemory: an image ReShade's allocator
//      places in a block made earlier under another scope has that block's
//      class.
//      Field basis (8 GB laptop, 2026-10-30, NR quality/performance switches):
//      with this add-on and NGX tagged 0.70 the kernel demoted only the
//      game's normal class; with both tagged 0.10 only the lowest class.  The
//      kernel's demoted breakdown by class (item 4) says which group a
//      shortage took.
//   4. Demoted: D3DKMTQueryStatistics(PROCESS_SEGMENT_GROUP, LOCAL) once per
//      second gives the kernel's Budget / Requested / Usage / Demoted for
//      this process (the numbers query-segstats.exe -p reported from outside
//      on 10-29/30; DXGI's CurrentUsage does not show demotion).  A class
//      whose demoted amount turns non-zero or back to zero is a log line, and
//      the total feeds the vram_demoted health row: the panel and HUD tell
//      the player that Windows moved memory to system memory.  Nothing here
//      changes NR on its own; lowering the NR setting is the player's call.
//      The keys of an earlier automatic remedy (NRVkDemotedSelfHeal,
//      NRVkSelfHeal*) are not read; one left in the ini is named once.
//   5. Rebuild window: around every NR lifecycle event (create/release) the
//      250 ms VRAM samples are summarized once the window settles - local
//      before / peak / after, over-budget samples, demoted before / peak /
//      after - so whether a working-resolution change still produces a
//      transient above the new steady state is a line in the log, not an
//      inference.
//   6. Recovery account: that window settles 3 s after the last event, but
//      on 10-30 (8 GB laptop, quality/performance switches) the kernel's
//      requested memory stayed above the new steady state for 10-16 s after
//      it, by about the replaced features' size, and the game stayed slow
//      until about then.  From each rebuild's last lifecycle event the next
//      30 s of the 250 ms sampler are kept (the process's local and
//      non-local usage, the kernel's requested and demoted at their
//      once-per-second sample, the present count) and summarized in one
//      line: each series' value at the event, its peak, its end and the time
//      from which it stayed within 32 MiB of the end; the present rate
//      before the rebuild, its end rate, its slowest second and the span in
//      which the trailing one-second rate sat below 80% of the end rate -
//      the memory fall-back and the slow segment on one time axis.  An event
//      within 3 s of the previous one belongs to the same rebuild and
//      restarts the 30 s; a later one closes the account early and the line
//      says so.
//   7. Retired NR runtime memory: the NR runtime's allocations are attributed
//      to the feature whose create / evaluate made them (FeatureScope, on the
//      calling thread).  At the feature's release the ones still alive are
//      its retired set; one line per set says what the feature's calls
//      allocated, what its ReleaseFeature freed itself, what outlived the
//      release and when vkFreeMemory took that (or that it was still alive
//      30 s on), so whether the runtime holds a released feature's memory
//      through the slow segment is measured rather than inferred.
//      NRVkRetiredMemoryPriority=P (0-100; absent or above 100 = off, the
//      default) also lowers the set's device-local allocations to P/100 by
//      vkSetDeviceMemoryPriorityEXT right after the release, never raising
//      one (vkd3d-proton's Evict model: a shortage takes memory no feature
//      uses before anything a live feature does).  An experiment, off by
//      default: the set already sits at NRVkOwnMemoryPriority (0.10, the
//      kernel's min class in the 10-30 data) and whether the kernel orders
//      0.00 below it is unmeasured; and an NR runtime that kept a released
//      allocation for its next feature would hand that feature the lowered
//      priority (the set's line shows whether the set is freed or kept).
//
// Every function here that touches add-on state is called under
// runtime_mutex except the two Vulkan entry points (their own mutexes) and
// the device-creation path (serialized by the loader per device).

namespace residency {

// ---------------------------------------------------------------------------
// Vulkan declarations this file uses (vulkan_core.h / vk_layer.h subset)
// ---------------------------------------------------------------------------

constexpr VkStructureType kStDeviceCreateInfo = 3;
constexpr VkStructureType kStMemoryAllocateInfo = 5;
constexpr VkStructureType kStLoaderDeviceCreateInfo = 48;
constexpr VkStructureType kStPhysicalDeviceFeatures2 = 1000059000;
constexpr VkStructureType kStMemoryPriorityFeatures = 1000238000;
constexpr VkStructureType kStMemoryPriorityAllocateInfo = 1000238001;
constexpr VkStructureType kStPageableDeviceLocalMemoryFeatures = 1000412000;
constexpr uint32_t kLayerLinkInfo = 0;  // VkLayerFunction::VK_LAYER_LINK_INFO
constexpr VkResult kSuccess = 0;
constexpr VkResult kErrorInitializationFailed = -3;
constexpr uint32_t kMemoryHeapDeviceLocalBit = 0x1;
constexpr const char kMemoryPriorityExtension[] = "VK_EXT_memory_priority";
constexpr const char kPageableExtension[] = "VK_EXT_pageable_device_local_memory";
// A caller with more extensions than this is passed through unmodified (the
// appended list lives on the calling thread's stack, as the bridge's does).
constexpr uint32_t kMaxCallerExtensions = 256;

struct BaseStructMin {
  VkStructureType sType;
  const void* pNext;
};
struct DeviceCreateInfoMin {
  VkStructureType sType;
  const void* pNext;
  VkFlags flags;
  uint32_t queueCreateInfoCount;
  const void* pQueueCreateInfos;
  uint32_t enabledLayerCount;
  const char* const* ppEnabledLayerNames;
  uint32_t enabledExtensionCount;
  const char* const* ppEnabledExtensionNames;
  const void* pEnabledFeatures;
};
struct LayerDeviceLinkMin {
  LayerDeviceLinkMin* pNext;
  PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr;
  PFN_vkGetDeviceProcAddr pfnNextGetDeviceProcAddr;
};
struct LayerDeviceCreateInfoMin {
  VkStructureType sType;
  const void* pNext;
  uint32_t function;
  union {
    LayerDeviceLinkMin* pLayerInfo;
    void* opaque[2];
  } u;
};
struct MemoryPriorityFeaturesMin {
  VkStructureType sType;
  void* pNext;
  VkBool32 memoryPriority;
};
struct PageableDeviceLocalMemoryFeaturesMin {
  VkStructureType sType;
  void* pNext;
  VkBool32 pageableDeviceLocalMemory;
};
struct PhysicalDeviceFeatures2Min {
  VkStructureType sType;
  void* pNext;
  VkBool32 features[55];  // VkPhysicalDeviceFeatures (55 VkBool32, Vulkan 1.0)
};
struct MemoryAllocateInfoMin {
  VkStructureType sType;
  const void* pNext;
  uint64_t allocationSize;
  uint32_t memoryTypeIndex;
};
struct MemoryPriorityAllocateInfoMin {
  VkStructureType sType;
  const void* pNext;
  float priority;
};
using EnumerateDeviceExtensionPropertiesFn =
    VkResult(VKAPI_PTR*)(VkPhysicalDevice, const char*, uint32_t*, VkExtensionProperties*);
using GetPhysicalDeviceFeatures2Fn = void(VKAPI_PTR*)(VkPhysicalDevice, PhysicalDeviceFeatures2Min*);
using AllocateMemoryFn =
    VkResult(VKAPI_PTR*)(VkDevice, const MemoryAllocateInfoMin*, const void*, VkDeviceMemory*);
using FreeMemoryFn = void(VKAPI_PTR*)(VkDevice, VkDeviceMemory, const void*);
using SetDeviceMemoryPriorityFn = void(VKAPI_PTR*)(VkDevice, VkDeviceMemory, float);
// LayerCreateDeviceFn, GetPhysicalDeviceMemoryPropertiesFn and
// VkPhysicalDeviceMemoryPropertiesMin are the enclosing namespace's.

template <typename T>
inline const T* FindInChain(const void* chain, VkStructureType type) {
  for (auto* node = static_cast<const BaseStructMin*>(chain); node != nullptr;
       node = static_cast<const BaseStructMin*>(node->pNext)) {
    if (node->sType == type) return reinterpret_cast<const T*>(node);
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Configuration ([RenoDX.DLSS5], ini-only like NRVkBudgetReserveMiB)
// ---------------------------------------------------------------------------

struct Config {
  bool inject = true;                // NRVkPageableMemory
  float own_priority = 0.10f;        // NRVkOwnMemoryPriority (percent; 50 = no tag)
  float own_fixed_priority = 0.35f;  // NRVkOwnFixedMemoryPriority (percent)
  float hot_priority = 0.70f;        // NRVkHotMemoryPriority (percent)
  bool lower_retired = false;        // NRVkRetiredMemoryPriority present and at most 100 (item 7)
  float retired_priority = 0.f;      // its value (percent)
};
inline Config config;
inline std::once_flag config_loaded;
inline const Config& Settings() {
  std::call_once(config_loaded, [] {
    uint32_t value = 0;
    const auto percent = [](uint32_t v) { return static_cast<float>(std::min<uint32_t>(v, 100)) / 100.f; };
    if (reshade::get_config_value(nullptr, kConfigSection, "NRVkPageableMemory", value)) config.inject = value != 0;
    if (reshade::get_config_value(nullptr, kConfigSection, "NRVkOwnMemoryPriority", value)) config.own_priority = percent(value);
    if (reshade::get_config_value(nullptr, kConfigSection, "NRVkOwnFixedMemoryPriority", value)) config.own_fixed_priority = percent(value);
    if (reshade::get_config_value(nullptr, kConfigSection, "NRVkHotMemoryPriority", value)) config.hot_priority = percent(value);
    if (reshade::get_config_value(nullptr, kConfigSection, "NRVkRetiredMemoryPriority", value) && value <= 100) {
      config.lower_retired = true;
      config.retired_priority = percent(value);
    }
    char retired_text[16] = "off";
    if (config.lower_retired) snprintf(retired_text, sizeof(retired_text), "%.2f", config.retired_priority);
    char text[384];
    snprintf(text, sizeof(text),
             "Vulkan residency: NRVkPageableMemory=%u NRVkOwnMemoryPriority=%.2f NRVkOwnFixedMemoryPriority=%.2f"
             " NRVkHotMemoryPriority=%.2f NRVkRetiredMemoryPriority=%s",
             config.inject ? 1u : 0u, config.own_priority, config.own_fixed_priority, config.hot_priority,
             retired_text);
    Log(reshade::log::level::info, text);
    // Keys of an earlier automatic remedy (a priority nudge, then an NR
    // off/on cycle, on a demoted-MiB threshold): an ini that still carries
    // one is told once, so its value is not mistaken for an active setting.
    std::string retired;
    for (const char* key : {"NRVkDemotedSelfHeal", "NRVkSelfHealDemotedMiB", "NRVkSelfHealHoldSeconds",
                            "NRVkSelfHealNudgePriority", "NRVkSelfHealMaxSequences"}) {
      if (reshade::get_config_value(nullptr, kConfigSection, key, value)) retired += std::string(" ") + key;
    }
    if (!retired.empty()) {
      Log(reshade::log::level::info,
          "Vulkan residency: the ini still carries" + retired
              + "; this build has no demoted self-heal and does not read them (safe to delete)");
    }
  });
  return config;
}

// ---------------------------------------------------------------------------
// Allocation scopes (thread-local): whose allocation a vkAllocateMemory is
// ---------------------------------------------------------------------------

inline thread_local uint32_t own_depth = 0;    // this add-on's NR at a working resolution
inline thread_local uint32_t fixed_depth = 0;  // this add-on's images that do not follow it
inline thread_local uint32_t hot_depth = 0;    // any NGX create/evaluate (the game's included)
struct OwnScope {
  OwnScope() { ++own_depth; }
  ~OwnScope() { --own_depth; }
  OwnScope(const OwnScope&) = delete;
  OwnScope& operator=(const OwnScope&) = delete;
};
// Inside or outside an OwnScope: FixedScope wins.
struct FixedScope {
  FixedScope() { ++fixed_depth; }
  ~FixedScope() { --fixed_depth; }
  FixedScope(const FixedScope&) = delete;
  FixedScope& operator=(const FixedScope&) = delete;
};
struct HotScope {
  HotScope() { ++hot_depth; }
  ~HotScope() { --hot_depth; }
  HotScope(const HotScope&) = delete;
  HotScope& operator=(const HotScope&) = delete;
};
// Item 7: the NR feature whose runtime call this thread is in.  Inside its
// create / evaluate (releasing false) the runtime's OwnScope allocations are
// attributed to it; inside its release (true) the frees of its allocations
// are counted as its ReleaseFeature's own.  Token 0 = no feature.
inline thread_local uint64_t feature_token = 0;
inline thread_local bool feature_releasing = false;
inline std::atomic_uint64_t next_feature_token{1};
inline uint64_t NewFeatureToken() { return next_feature_token.fetch_add(1, std::memory_order_relaxed); }
struct FeatureScope {
  FeatureScope(uint64_t token, bool releasing) : saved_token(feature_token), saved_releasing(feature_releasing) {
    feature_token = token;
    feature_releasing = releasing;
  }
  ~FeatureScope() {
    feature_token = saved_token;
    feature_releasing = saved_releasing;
  }
  FeatureScope(const FeatureScope&) = delete;
  FeatureScope& operator=(const FeatureScope&) = delete;
  uint64_t saved_token;
  bool saved_releasing;
};

// ---------------------------------------------------------------------------
// Per-device state and the allocation registry
// ---------------------------------------------------------------------------

struct DeviceState {
  VkDevice device = nullptr;
  AllocateMemoryFn allocate = nullptr;
  FreeMemoryFn free = nullptr;
  uint32_t local_type_mask = 0;  // memory types whose heap is DEVICE_LOCAL
  bool memory_priority = false;  // VK_EXT_memory_priority enabled by this add-on
  bool pageable = false;         // VK_EXT_pageable_device_local_memory enabled by this add-on
  bool observed = false;         // allocations of this device pass through the wrapper
  SetDeviceMemoryPriorityFn set_priority = nullptr;  // vkSetDeviceMemoryPriorityEXT below the layer (item 7)
};
inline std::mutex state_mutex;
inline std::unordered_map<VkDevice, DeviceState> devices;
inline std::atomic<AllocateMemoryFn> last_allocate{nullptr};
inline std::atomic<FreeMemoryFn> last_free{nullptr};
inline std::atomic<PFN_vkGetDeviceProcAddr> real_next_gdpa{nullptr};
inline std::atomic_bool injection_declined{false};  // a failed create: additions off for the process
inline std::atomic_bool any_pageable_device{false};

// Whose a device memory is (the scopes above): own = OwnScope or FixedScope,
// fixed = FixedScope, hot = HotScope outside both (the game's NGX features).
// token = the FeatureScope's feature for an own, non-fixed allocation;
// retired_ns = when that feature was released, while the allocation outlives it.
struct Allocation {
  VkDevice device = nullptr;
  uint64_t bytes = 0;
  uint32_t type = 0;
  float priority = 0.5f;
  bool local = false;
  bool own = false;
  bool fixed = false;
  bool hot = false;
  uint64_t token = 0;
  int64_t retired_ns = 0;
};
inline std::mutex registry_mutex;
inline std::unordered_map<VkDeviceMemory, Allocation> registry;
// Counters under registry_mutex (the telemetry reads them under it too).
// Non-local (system-memory heap) bytes are kept for this add-on and the
// game's NGX features too: an NR create that the process counters show as
// "nonlocal +N MiB" is then attributable from the telemetry line.
struct Counters {
  uint64_t allocations = 0, frees = 0, own = 0, hot = 0, tagged = 0;
  uint64_t local_bytes = 0, own_local_bytes = 0, fixed_local_bytes = 0, hot_local_bytes = 0;
  uint64_t own_nonlocal_bytes = 0, hot_nonlocal_bytes = 0;
  // Item 7: feature releases with a retired set, with nothing attributed, and
  // with a set no follow slot was free for; allocations lowered at a release
  // (and their bytes); releases whose set could not be lowered (no
  // vkSetDeviceMemoryPriorityEXT); retired allocations still alive (and bytes).
  uint64_t retired_sets = 0, retired_unattributed = 0, retired_untracked = 0;
  uint64_t retired_lowered = 0, retired_lowered_bytes = 0, retired_lower_unavailable = 0;
  uint64_t retired_alive = 0, retired_alive_bytes = 0;
};
inline Counters counters;

// One allocation into (sign +1) or out of (-1) the byte counters; caller
// holds registry_mutex.
inline void Count(const Allocation& a, int sign) {
  const auto apply = [&](uint64_t& counter) {
    counter = sign > 0 ? counter + a.bytes : counter - std::min(counter, a.bytes);
  };
  if (a.local) {
    apply(counters.local_bytes);
    if (a.own) apply(counters.own_local_bytes);
    if (a.fixed) apply(counters.fixed_local_bytes);
    if (a.hot) apply(counters.hot_local_bytes);
  } else {
    if (a.own) apply(counters.own_nonlocal_bytes);
    if (a.hot) apply(counters.hot_nonlocal_bytes);
  }
}

// Item 7.  One live NR feature's runtime allocations (its create / evaluate
// under its FeatureScope): counts and bytes made, freed while it lived, freed
// by its ReleaseFeature.
struct FeatureMemory {
  VkDevice device = nullptr;
  uint32_t made = 0, freed_live = 0, freed_by_release = 0;
  uint64_t made_local_bytes = 0, made_nonlocal_bytes = 0, freed_live_bytes = 0, freed_by_release_bytes = 0;
};
inline std::unordered_map<uint64_t, FeatureMemory> feature_memory;  // by token, under registry_mutex
// A released feature's allocations that outlived its release, followed until
// vkFreeMemory has taken them all or for kRetiredFollowNs (token 0 = a free slot).
struct RetiredSet {
  uint64_t token = 0, serial = 0;
  VkDevice device = nullptr;
  int64_t released_ns = 0, first_free_ns = 0, last_free_ns = 0;
  FeatureMemory feature;  // the feature's account up to its release
  uint32_t alive = 0, freed = 0, lowered = 0;
  uint64_t alive_local_bytes = 0, alive_nonlocal_bytes = 0, freed_bytes = 0, lowered_bytes = 0;
  bool lower_unavailable = false;
};
inline constexpr uint32_t kRetiredSetSlots = 16;
inline constexpr int64_t kRetiredFollowNs = 30'000'000'000;
inline RetiredSet retired_sets[kRetiredSetSlots];  // under registry_mutex

// A freed allocation's share of item 7; caller holds registry_mutex.
inline void NoteTaggedFree(const Allocation& a) {
  if (a.token == 0) return;
  if (a.retired_ns != 0) {
    for (RetiredSet& account : retired_sets) {
      if (account.token != a.token) continue;
      const int64_t now_ns = SteadyNowNs();
      if (account.freed == 0) account.first_free_ns = now_ns;
      account.last_free_ns = now_ns;
      ++account.freed;
      account.freed_bytes += a.bytes;
      break;
    }
    counters.retired_alive -= std::min<uint64_t>(counters.retired_alive, 1);
    counters.retired_alive_bytes -= std::min(counters.retired_alive_bytes, a.bytes);
    return;
  }
  const auto found = feature_memory.find(a.token);
  if (found == feature_memory.end()) return;
  FeatureMemory& feature = found->second;
  if (feature_releasing && feature_token == a.token) {
    ++feature.freed_by_release;
    feature.freed_by_release_bytes += a.bytes;
  } else {
    ++feature.freed_live;
    feature.freed_live_bytes += a.bytes;
  }
}

inline DeviceState SnapshotDevice(VkDevice device) {
  std::lock_guard lock(state_mutex);
  const auto found = devices.find(device);
  return found != devices.end() ? found->second : DeviceState{};
}

inline VkResult VKAPI_CALL HookedAllocateMemory(VkDevice device, const MemoryAllocateInfoMin* info,
                                                const void* allocator, VkDeviceMemory* out) {
  const DeviceState state = SnapshotDevice(device);
  AllocateMemoryFn allocate = state.allocate != nullptr ? state.allocate : last_allocate.load();
  if (allocate == nullptr) return kErrorInitializationFailed;
  if (info == nullptr || info->sType != kStMemoryAllocateInfo) return allocate(device, info, allocator, out);
  const Config& cfg = Settings();
  const bool fixed = fixed_depth > 0;
  const bool own = fixed || own_depth > 0;
  const bool hot = !own && hot_depth > 0;
  const bool local = info->memoryTypeIndex < 32 && ((state.local_type_mask >> info->memoryTypeIndex) & 1u) != 0;
  const float target = fixed ? cfg.own_fixed_priority : own ? cfg.own_priority : hot ? cfg.hot_priority : 0.5f;
  float priority = 0.5f;
  bool tagged = false;
  MemoryAllocateInfoMin copy;
  MemoryPriorityAllocateInfoMin priority_info;
  const MemoryAllocateInfoMin* use = info;
  if (const auto* existing = FindInChain<MemoryPriorityAllocateInfoMin>(info->pNext, kStMemoryPriorityAllocateInfo);
      existing != nullptr) {
    priority = existing->priority;
  } else if (local && state.memory_priority && target != 0.5f) {
    copy = *info;
    priority_info = {kStMemoryPriorityAllocateInfo, info->pNext, target};
    copy.pNext = &priority_info;
    use = &copy;
    priority = target;
    tagged = true;
  }
  const VkResult result = allocate(device, use, allocator, out);
  if (result == kSuccess && out != nullptr && *out != nullptr) {
    const Allocation a = {device, info->allocationSize, info->memoryTypeIndex, priority, local, own, fixed, hot,
                          (own && !fixed) ? feature_token : uint64_t{0}};
    std::lock_guard lock(registry_mutex);
    registry[*out] = a;
    ++counters.allocations;
    if (own) ++counters.own;
    if (hot) ++counters.hot;
    if (tagged) ++counters.tagged;
    Count(a, +1);
    if (a.token != 0) {
      FeatureMemory& feature = feature_memory[a.token];
      feature.device = device;
      ++feature.made;
      (local ? feature.made_local_bytes : feature.made_nonlocal_bytes) += a.bytes;
    }
  }
  return result;
}

inline void VKAPI_CALL HookedFreeMemory(VkDevice device, VkDeviceMemory memory, const void* allocator) {
  const DeviceState state = SnapshotDevice(device);
  FreeMemoryFn free_memory = state.free != nullptr ? state.free : last_free.load();
  if (memory != nullptr) {
    std::lock_guard lock(registry_mutex);
    if (const auto found = registry.find(memory); found != registry.end()) {
      NoteTaggedFree(found->second);
      Count(found->second, -1);
      registry.erase(found);
      ++counters.frees;
    }
  }
  if (free_memory != nullptr) free_memory(device, memory, allocator);
}

// The pfnNextGetDeviceProcAddr the ReShade layer sees for a device created
// through CreateDevice below: the real answer for every name, except that
// vkAllocateMemory / vkFreeMemory resolve to the two observers above (their
// real entry points are kept per device).
inline PFN_vkVoidFunction VKAPI_CALL NextGdpaDetour(VkDevice device, const char* name) {
  const PFN_vkGetDeviceProcAddr real = real_next_gdpa.load(std::memory_order_acquire);
  const PFN_vkVoidFunction function = real != nullptr ? real(device, name) : nullptr;
  if (function == nullptr || name == nullptr || device == nullptr) return function;
  if (std::strcmp(name, "vkAllocateMemory") == 0) {
    std::lock_guard lock(state_mutex);
    DeviceState& state = devices[device];
    state.device = device;
    state.allocate = reinterpret_cast<AllocateMemoryFn>(function);
    state.observed = true;
    last_allocate.store(state.allocate);
    return reinterpret_cast<PFN_vkVoidFunction>(&HookedAllocateMemory);
  }
  if (std::strcmp(name, "vkFreeMemory") == 0) {
    std::lock_guard lock(state_mutex);
    DeviceState& state = devices[device];
    state.device = device;
    state.free = reinterpret_cast<FreeMemoryFn>(function);
    state.observed = true;
    last_free.store(state.free);
    return reinterpret_cast<PFN_vkVoidFunction>(&HookedFreeMemory);
  }
  return function;
}

// The device is gone (OnDestroyDeviceContext): its entries with it, and its
// features' item 7 accounts (dropped unlogged: the device took the memory).
inline void ForgetDevice(VkDevice device) {
  {
    std::lock_guard lock(state_mutex);
    devices.erase(device);
  }
  std::lock_guard lock(registry_mutex);
  for (auto it = registry.begin(); it != registry.end();) {
    if (it->second.device != device) {
      ++it;
      continue;
    }
    if (it->second.retired_ns != 0) {
      counters.retired_alive -= std::min<uint64_t>(counters.retired_alive, 1);
      counters.retired_alive_bytes -= std::min(counters.retired_alive_bytes, it->second.bytes);
    }
    Count(it->second, -1);
    it = registry.erase(it);
  }
  for (auto it = feature_memory.begin(); it != feature_memory.end();) {
    if (it->second.device == device) {
      it = feature_memory.erase(it);
    } else {
      ++it;
    }
  }
  for (RetiredSet& account : retired_sets) {
    if (account.token != 0 && account.device == device) account = {};
  }
}

// ---------------------------------------------------------------------------
// Device creation with the two extensions
// ---------------------------------------------------------------------------

inline LayerDeviceLinkMin link_ring[8];
inline std::atomic_uint32_t link_next{0};
inline uint32_t create_logged = 0;

inline bool HasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
  return std::any_of(extensions.begin(), extensions.end(), [&](const VkExtensionProperties& e) {
    return std::strncmp(e.extensionName, name, VK_MAX_EXTENSION_NAME_SIZE) == 0;
  });
}

inline int32_t CreateDevice(VkInstance instance, VkPhysicalDevice physical, const void* info,
                            const void* allocator, VkDevice* out, LayerCreateDeviceFn next) {
  const Config& cfg = Settings();
  const auto* caller = static_cast<const DeviceCreateInfoMin*>(info);
  const auto plain = [&] { return next(physical, info, allocator, out); };
  const auto note = [&](reshade::log::level level, const std::string& message) {
    if (create_logged < 16) {
      ++create_logged;
      Log(level, "Vulkan residency: " + message);
    }
  };
  if (!cfg.inject) return plain();
  if (injection_declined.load(std::memory_order_relaxed)) {
    note(reshade::log::level::info, "a device is created without the residency extensions: an earlier create with them failed in this process");
    return plain();
  }
  if (instance == nullptr || physical == nullptr || real_layer_gipa == nullptr || caller == nullptr
      || caller->sType != kStDeviceCreateInfo || caller->enabledExtensionCount > kMaxCallerExtensions
      || (caller->enabledExtensionCount != 0 && caller->ppEnabledExtensionNames == nullptr)) {
    note(reshade::log::level::info, "a device is created without the residency extensions: no instance context for its physical device or an unusual create info");
    return plain();
  }
  const auto enumerate = reinterpret_cast<EnumerateDeviceExtensionPropertiesFn>(
      real_layer_gipa(instance, "vkEnumerateDeviceExtensionProperties"));
  auto features2 = reinterpret_cast<GetPhysicalDeviceFeatures2Fn>(
      real_layer_gipa(instance, "vkGetPhysicalDeviceFeatures2"));
  if (features2 == nullptr) {
    features2 = reinterpret_cast<GetPhysicalDeviceFeatures2Fn>(
        real_layer_gipa(instance, "vkGetPhysicalDeviceFeatures2KHR"));
  }
  const auto memory_properties = reinterpret_cast<GetPhysicalDeviceMemoryPropertiesFn>(
      real_layer_gipa(instance, "vkGetPhysicalDeviceMemoryProperties"));
  if (enumerate == nullptr || features2 == nullptr || memory_properties == nullptr) {
    note(reshade::log::level::info, "a device is created without the residency extensions: the layer offers no extension / feature query");
    return plain();
  }
  uint32_t count = 0;
  if (enumerate(physical, nullptr, &count, nullptr) != kSuccess) return plain();
  std::vector<VkExtensionProperties> extensions(count);
  if (count != 0 && enumerate(physical, nullptr, &count, extensions.data()) < 0) return plain();
  extensions.resize(count);
  const bool has_priority = HasExtension(extensions, kMemoryPriorityExtension);
  const bool has_pageable = HasExtension(extensions, kPageableExtension);
  MemoryPriorityFeaturesMin priority_features = {kStMemoryPriorityFeatures, nullptr, 0};
  PageableDeviceLocalMemoryFeaturesMin pageable_features = {kStPageableDeviceLocalMemoryFeatures, nullptr, 0};
  if (has_priority) {
    PhysicalDeviceFeatures2Min features = {kStPhysicalDeviceFeatures2, &priority_features, {}};
    if (has_pageable) priority_features.pNext = &pageable_features;
    features2(physical, &features);
  }
  if (!has_priority || !has_pageable || priority_features.memoryPriority == 0
      || pageable_features.pageableDeviceLocalMemory == 0) {
    char text[256];
    snprintf(text, sizeof(text),
             "this driver does not offer the residency extensions (memory_priority ext=%u feature=%u,"
             " pageable_device_local_memory ext=%u feature=%u); the device is created as the game asked"
             " (NVIDIA Windows drivers from 472.02 on Turing and later offer both)",
             has_priority ? 1u : 0u, priority_features.memoryPriority, has_pageable ? 1u : 0u,
             pageable_features.pageableDeviceLocalMemory);
    note(reshade::log::level::info, text);
    return plain();
  }
  // The caller's own list and chain.
  bool app_priority_name = false, app_pageable_name = false;
  for (uint32_t i = 0; i < caller->enabledExtensionCount; ++i) {
    const char* name = caller->ppEnabledExtensionNames[i];
    if (name == nullptr) continue;
    app_priority_name = app_priority_name || std::strcmp(name, kMemoryPriorityExtension) == 0;
    app_pageable_name = app_pageable_name || std::strcmp(name, kPageableExtension) == 0;
  }
  const auto* app_priority_features = FindInChain<MemoryPriorityFeaturesMin>(caller->pNext, kStMemoryPriorityFeatures);
  const auto* app_pageable_features =
      FindInChain<PageableDeviceLocalMemoryFeaturesMin>(caller->pNext, kStPageableDeviceLocalMemoryFeatures);
  LayerDeviceCreateInfoMin* link_info = nullptr;
  for (auto* node = static_cast<const BaseStructMin*>(caller->pNext); node != nullptr;
       node = static_cast<const BaseStructMin*>(node->pNext)) {
    if (node->sType != kStLoaderDeviceCreateInfo) continue;
    auto* candidate = const_cast<LayerDeviceCreateInfoMin*>(reinterpret_cast<const LayerDeviceCreateInfoMin*>(node));
    if (candidate->function == kLayerLinkInfo) {
      link_info = candidate;
      break;
    }
  }
  if (app_priority_name && app_pageable_name && app_priority_features != nullptr && app_pageable_features != nullptr) {
    char text[200];
    snprintf(text, sizeof(text),
             "the game's own create info already carries both residency extensions (memoryPriority=%u"
             " pageableDeviceLocalMemory=%u); nothing is added and allocations are not observed",
             app_priority_features->memoryPriority, app_pageable_features->pageableDeviceLocalMemory);
    note(reshade::log::level::info, text);
    return plain();
  }
  // The extended create info: the caller's names plus the two, the caller's
  // chain with the two feature structures in front.
  const char* names[kMaxCallerExtensions + 2];
  uint32_t name_count = caller->enabledExtensionCount;
  for (uint32_t i = 0; i < name_count; ++i) names[i] = caller->ppEnabledExtensionNames[i];
  if (!app_priority_name) names[name_count++] = kMemoryPriorityExtension;
  if (!app_pageable_name) names[name_count++] = kPageableExtension;
  MemoryPriorityFeaturesMin our_priority = {kStMemoryPriorityFeatures, nullptr, 1};
  PageableDeviceLocalMemoryFeaturesMin our_pageable = {kStPageableDeviceLocalMemoryFeatures, nullptr, 1};
  DeviceCreateInfoMin extended = *caller;
  extended.enabledExtensionCount = name_count;
  extended.ppEnabledExtensionNames = names;
  const void* chain = caller->pNext;
  if (app_priority_features == nullptr) {
    our_priority.pNext = const_cast<void*>(chain);
    chain = &our_priority;
  }
  if (app_pageable_features == nullptr) {
    our_pageable.pNext = const_cast<void*>(chain);
    chain = &our_pageable;
  }
  extended.pNext = chain;
  // The layer link: the ReShade layer reads pfnNextGetDeviceProcAddr from
  // the link at the head and advances the head to the link's pNext; a copy
  // with the wrapper in that slot is what it reads instead.  The real
  // next-GDPA is the loader's device terminator, one per process; a
  // different one (never seen) leaves that device unobserved.
  LayerDeviceLinkMin* const original_link = link_info != nullptr ? link_info->u.pLayerInfo : nullptr;
  LayerDeviceLinkMin* our_link = nullptr;
  if (original_link != nullptr && original_link->pfnNextGetDeviceProcAddr != nullptr) {
    PFN_vkGetDeviceProcAddr expected = nullptr;
    if (real_next_gdpa.compare_exchange_strong(expected, original_link->pfnNextGetDeviceProcAddr)
        || expected == original_link->pfnNextGetDeviceProcAddr) {
      our_link = &link_ring[link_next.fetch_add(1, std::memory_order_relaxed) % std::size(link_ring)];
      *our_link = {original_link->pNext, original_link->pfnNextGetInstanceProcAddr, &NextGdpaDetour};
      link_info->u.pLayerInfo = our_link;
    }
  }
  const int32_t result = next(physical, &extended, allocator, out);
  if (result < 0) {
    injection_declined.store(true, std::memory_order_relaxed);
    if (link_info != nullptr) link_info->u.pLayerInfo = original_link;
    if (out != nullptr) *out = nullptr;
    const int32_t retry = next(physical, info, allocator, out);
    char text[320];
    snprintf(text, sizeof(text),
             "vkCreateDevice with VK_EXT_memory_priority + VK_EXT_pageable_device_local_memory added returned"
             " %d although the driver reported both; the loader's layer link was restored and the caller's"
             " own create info retried: %d. The residency extensions stay off for this process",
             result, retry);
    Log(reshade::log::level::warning, text);
    return retry;
  }
  if (out == nullptr || *out == nullptr) return result;
  // Device-local memory types of this physical device (the real query: the
  // reserve experiment's hooks only shrink sizes).
  uint32_t local_mask = 0;
  {
    VkPhysicalDeviceMemoryPropertiesMin properties = {};
    memory_properties(physical, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount && i < 32; ++i) {
      const uint32_t heap = properties.memoryTypes[i].heapIndex;
      if (heap < 16 && (properties.memoryHeaps[heap].flags & kMemoryHeapDeviceLocalBit) != 0) local_mask |= 1u << i;
    }
  }
  bool observed = false;
  // vkSetDeviceMemoryPriorityEXT (item 7) from the same next-GDPA the
  // wrapper resolves through; only where this device's link is the wrapped one.
  SetDeviceMemoryPriorityFn set_priority = nullptr;
  if (const PFN_vkGetDeviceProcAddr real = real_next_gdpa.load(std::memory_order_acquire);
      our_link != nullptr && real != nullptr) {
    set_priority = reinterpret_cast<SetDeviceMemoryPriorityFn>(real(*out, "vkSetDeviceMemoryPriorityEXT"));
  }
  {
    std::lock_guard lock(state_mutex);
    DeviceState& state = devices[*out];
    state.device = *out;
    state.memory_priority = true;
    state.pageable = true;
    state.local_type_mask = local_mask;
    state.set_priority = set_priority;
    observed = state.observed;
  }
  any_pageable_device.store(true, std::memory_order_relaxed);
  char text[640];
  snprintf(text, sizeof(text),
           "device %p created with VK_EXT_memory_priority%s and VK_EXT_pageable_device_local_memory%s"
           " (driver reports both features; the game asked for %u extension(s), neither of these);"
           " allocations observed through the layer link=%u, device-local memory types=0x%X; device-local"
           " priorities: this add-on's NR-resolution memory (NR runtime create/evaluate, worksets, look"
           " history) %.2f, its fixed images (pre-FG stream, NR runtime Init) %.2f, the game's NGX features"
           " (DLSS, DLSS-G create/evaluate) %.2f, everything else 0.50 - a shortage takes the lowest first",
           static_cast<void*>(*out), app_priority_name ? " (already requested)" : "",
           app_pageable_name ? " (already requested)" : "", caller->enabledExtensionCount,
           observed ? 1u : 0u, local_mask, cfg.own_priority, cfg.own_fixed_priority, cfg.hot_priority);
  std::string retired_policy = "; a released NR feature's runtime memory that outlives the release keeps its priority"
                               " (NRVkRetiredMemoryPriority off)";
  if (cfg.lower_retired) {
    char value[16];
    snprintf(value, sizeof(value), "%.2f", cfg.retired_priority);
    retired_policy = set_priority != nullptr
                         ? "; a released NR feature's runtime memory that outlives the release drops to "
                               + std::string(value) + " (NRVkRetiredMemoryPriority)"
                         : "; NRVkRetiredMemoryPriority is set but vkSetDeviceMemoryPriorityEXT did not resolve"
                           " on this device: released NR features' memory keeps its priority";
  }
  Log(reshade::log::level::info, text + retired_policy);
  return result;
}

// ---------------------------------------------------------------------------
// The kernel's per-process local segment group: Budget / Requested / Usage /
// Demoted (by eviction-priority class), D3DKMTQueryStatistics type 9
// ---------------------------------------------------------------------------

struct SegmentGroupSample {
  bool valid = false;
  uint64_t budget = 0, requested = 0, usage = 0, demoted = 0;
  uint64_t demoted_class[5] = {};
};
inline SegmentGroupSample QueryLocalSegmentGroup(LUID luid) {
  SegmentGroupSample sample;
  D3DKMT_QUERYSTATISTICS query = {};
  query.Type = D3DKMT_QUERYSTATISTICS_PROCESS_SEGMENT_GROUP;
  query.AdapterLuid = luid;
  query.hProcess = GetCurrentProcess();
  query.QueryProcessSegmentGroup = D3DKMT_MEMORY_SEGMENT_GROUP_LOCAL;
  if (D3DKMTQueryStatistics(&query) != 0) return sample;
  const auto& group = query.QueryResult.ProcessSegmentGroupInformation;
  sample.valid = true;
  sample.budget = group.Budget;
  sample.requested = group.Requested;
  sample.usage = group.Usage;
  for (int i = 0; i < 5; ++i) {
    sample.demoted_class[i] = group.Demoted[i];
    sample.demoted += group.Demoted[i];
  }
  return sample;
}
inline bool CurrentAdapterLuid(LUID* out) {
  const uint64_t packed = telemetry::fallback_luid.load(std::memory_order_relaxed);
  if (packed == 0) return false;
  out->LowPart = static_cast<DWORD>(packed & 0xFFFFFFFFu);
  out->HighPart = static_cast<LONG>(packed >> 32);
  return true;
}

// ---------------------------------------------------------------------------
// Demoted by class, and the rebuild window (release-before-create check)
// ---------------------------------------------------------------------------

inline constexpr const char* kClassNames[5] = {"min", "low", "normal", "high", "max"};
// Class transition lines per session (then counted only).
inline constexpr uint32_t kTransitionLogCap = 128;

struct Monitor {
  // Sampling.
  int64_t last_query_ns = 0;
  SegmentGroupSample last;
  bool logged_first = false;
  // Classes whose demoted amount was non-zero (whole MiB) at the last sample,
  // one bit per class; a change is a line, no amount threshold.
  uint32_t demoted_mask = 0;
  uint32_t transitions_logged = 0, transitions_unlogged = 0;
  // Lifecycle settling.
  uint64_t last_events = 0;
  int64_t last_event_ns = 0;
  // Rebuild window.
  bool window_open = false;
  int64_t window_opened_ns = 0;
  uint64_t window_before_mib = 0, window_peak_mib = 0, window_budget_mib = 0;
  uint64_t window_demoted_before = 0, window_demoted_peak = 0;
  uint32_t window_samples = 0, window_over = 0, window_events = 0;
  uint64_t previous_local_mib = 0;
  bool previous_valid = false;
  uint32_t window_logged = 0;
  uint32_t windows = 0, windows_with_spike = 0;
};
inline Monitor monitor;

inline std::string Mib(uint64_t bytes) { return std::to_string(bytes >> 20); }
inline std::string ClassBreakdown(const SegmentGroupSample& s) {
  std::string text;
  for (int i = 0; i < 5; ++i) text += (i != 0 ? "/" : "") + Mib(s.demoted_class[i]);
  return text;
}
inline uint32_t DemotedMask(const SegmentGroupSample& s) {
  uint32_t mask = 0;
  for (int i = 0; i < 5; ++i) {
    if ((s.demoted_class[i] >> 20) != 0) mask |= 1u << i;
  }
  return mask;
}

inline std::string Decimal(double v, int digits) {
  char text[32];
  snprintf(text, sizeof(text), "%.*f", digits, v);
  return text;
}
inline std::string Seconds(int64_t ns) { return Decimal(static_cast<double>(ns) / 1e9, 1); }

// ---------------------------------------------------------------------------
// Item 7: retired NR runtime memory
// ---------------------------------------------------------------------------

// Set lines per session (later sets are closed unlogged; the telemetry
// group counts every set).  Under runtime_mutex (the tick).
inline uint32_t retired_logged = 0;

// The feature `token` has just been released (its runtime.release returned;
// the caller holds runtime_mutex): its attributed allocations still alive
// become its retired set, which ServiceRetiredSets follows; with
// NRVkRetiredMemoryPriority on, the set's device-local allocations above
// that priority are lowered to it here.  A feature with nothing attributed
// and nothing alive is only counted.
inline void NoteFeatureReleased(uint64_t token) {
  if (token == 0) return;
  const Config& cfg = Settings();
  const int64_t now_ns = SteadyNowNs();
  // The devices' setters, copied out first: state_mutex is never taken under
  // registry_mutex (the Vulkan entry points take the two one after the other).
  std::vector<std::pair<VkDevice, SetDeviceMemoryPriorityFn>> setters;
  if (cfg.lower_retired) {
    std::lock_guard state_lock(state_mutex);
    for (const auto& entry : devices) {
      if (entry.second.set_priority != nullptr) setters.emplace_back(entry.first, entry.second.set_priority);
    }
  }
  RetiredSet account;
  account.token = token;
  account.released_ns = now_ns;
  std::lock_guard registry_lock(registry_mutex);
  if (const auto found = feature_memory.find(token); found != feature_memory.end()) {
    account.feature = found->second;
    feature_memory.erase(found);
  }
  account.device = account.feature.device;
  for (auto& entry : registry) {
    Allocation& a = entry.second;
    if (a.token != token || a.retired_ns != 0) continue;
    a.retired_ns = now_ns;
    if (account.device == nullptr) account.device = a.device;
    ++account.alive;
    (a.local ? account.alive_local_bytes : account.alive_nonlocal_bytes) += a.bytes;
    ++counters.retired_alive;
    counters.retired_alive_bytes += a.bytes;
    if (!cfg.lower_retired || !a.local || a.priority <= cfg.retired_priority) continue;
    SetDeviceMemoryPriorityFn setter = nullptr;
    for (const auto& candidate : setters) {
      if (candidate.first == a.device) setter = candidate.second;
    }
    if (setter == nullptr) {
      account.lower_unavailable = true;
      continue;
    }
    // Under registry_mutex on purpose: HookedFreeMemory erases the entry
    // under it before the real vkFreeMemory, so the handle is live here.
    setter(a.device, entry.first, cfg.retired_priority);
    a.priority = cfg.retired_priority;
    ++account.lowered;
    account.lowered_bytes += a.bytes;
  }
  counters.retired_lowered += account.lowered;
  counters.retired_lowered_bytes += account.lowered_bytes;
  if (account.lower_unavailable) ++counters.retired_lower_unavailable;
  if (account.feature.made == 0 && account.alive == 0) {
    ++counters.retired_unattributed;
    return;
  }
  account.serial = ++counters.retired_sets;
  for (RetiredSet& slot : retired_sets) {
    if (slot.token != 0) continue;
    slot = account;
    return;
  }
  ++counters.retired_untracked;
}

// A feature that is not released through NoteFeatureReleased (its create
// failed, or its handle is dropped with its device): its account goes.
inline void ForgetFeature(uint64_t token) {
  if (token == 0) return;
  std::lock_guard registry_lock(registry_mutex);
  feature_memory.erase(token);
}

inline std::string RetiredSetLine(const RetiredSet& account, int64_t now_ns) {
  const FeatureMemory& feature = account.feature;
  std::ostringstream text;
  text << "Vulkan NR retired runtime memory, set " << account.serial << " (released "
       << Seconds(now_ns - account.released_ns) << " s before this line): the feature's create/evaluate allocated "
       << feature.made << " (" << Mib(feature.made_local_bytes) << " MiB local, "
       << Mib(feature.made_nonlocal_bytes) << " MiB system), freed " << feature.freed_live << " ("
       << Mib(feature.freed_live_bytes) << " MiB) while it lived and " << feature.freed_by_release << " ("
       << Mib(feature.freed_by_release_bytes) << " MiB) inside its ReleaseFeature; ";
  if (account.alive == 0) {
    text << "none outlived the release, so memory the kernel still counts after it is not what this feature's"
            " create/evaluate allocated on the calling thread (a runtime worker thread's allocations are not"
            " attributed)";
    return text.str();
  }
  text << account.alive << " outlived it (" << Mib(account.alive_local_bytes) << " MiB local, "
       << Mib(account.alive_nonlocal_bytes) << " MiB system)";
  if (account.lowered != 0) {
    text << ", " << account.lowered << " (" << Mib(account.lowered_bytes) << " MiB) lowered to priority "
         << Decimal(Settings().retired_priority, 2);
  }
  if (account.lower_unavailable) text << ", not lowered: no vkSetDeviceMemoryPriorityEXT on its device";
  if (account.freed >= account.alive) {
    text << "; vkFreeMemory took them +" << Seconds(account.first_free_ns - account.released_ns) << " to +"
         << Seconds(account.last_free_ns - account.released_ns) << " s after the release";
  } else {
    text << "; " << (account.alive - account.freed) << " still alive +" << Seconds(now_ns - account.released_ns)
         << " s after the release";
    if (account.freed != 0) {
      text << " (" << account.freed << " freed +" << Seconds(account.first_free_ns - account.released_ns)
           << " to +" << Seconds(account.last_free_ns - account.released_ns) << " s)";
    }
  }
  return text.str();
}

// Every tick (under runtime_mutex): a set whose allocations vkFreeMemory has
// all taken, or that has been followed for kRetiredFollowNs, is a line.
inline void ServiceRetiredSets(int64_t now_ns) {
  RetiredSet done[kRetiredSetSlots];
  uint32_t done_count = 0;
  {
    std::lock_guard registry_lock(registry_mutex);
    for (RetiredSet& account : retired_sets) {
      if (account.token == 0) continue;
      if (account.freed < account.alive && now_ns - account.released_ns < kRetiredFollowNs) continue;
      done[done_count++] = account;
      account = {};
    }
  }
  for (uint32_t i = 0; i < done_count && retired_logged < kLifecycleLogCap; ++i) {
    ++retired_logged;
    Log(reshade::log::level::info, RetiredSetLine(done[i], now_ns));
  }
}

// ---------------------------------------------------------------------------
// Item 6: the recovery account after each rebuild
// ---------------------------------------------------------------------------

struct RecoverySample {
  int64_t at_ns = 0;
  uint64_t presents = 0;
  uint32_t local_mib = 0, nonlocal_mib = 0;     // the process (DXGI), when vram
  uint32_t requested_mib = 0, demoted_mib = 0;  // the kernel, when kernel
  bool vram = false;    // the 250 ms VRAM sample was valid
  bool kernel = false;  // the once-per-second kernel sample was taken at this tick
};
struct PresentMark {
  int64_t at_ns = 0;
  uint64_t presents = 0;
};
inline constexpr uint32_t kRecoverySamples = 160;              // 30 s of 250 ms ticks, with room
inline constexpr int64_t kRecoveryHorizonNs = 30'000'000'000;  // followed this long after the last event
inline constexpr int64_t kRecoveryBurstGapNs = 3'000'000'000;  // a later event within this: the same rebuild
inline constexpr uint32_t kRecoverySettleMib = 32;             // settled = within this of the end
inline constexpr uint32_t kPresentHistory = 9;                 // ~2 s of ticks before a rebuild
struct Recovery {
  bool active = false;
  int64_t first_event_ns = 0, anchor_ns = 0;  // the rebuild's first and last event tick
  uint32_t events = 0;
  double rate_before = -1.0;  // presents/s over the ticks before the first event (-1: under 1 s of them)
  uint32_t demoted_before_mib = 0;
  bool demoted_before_valid = false;
  uint32_t count = 0;  // samples from the anchor on
  RecoverySample samples[kRecoverySamples];
  PresentMark history[kPresentHistory];
  uint32_t history_count = 0, history_next = 0;
  uint32_t logged = 0, accounts = 0, cut_short = 0;
};
inline Recovery recovery;  // under runtime_mutex (the tick)

// Presents/s across the history ring; -1 when it spans under a second.
inline double HistoryRate(const Recovery& r) {
  if (r.history_count < 2) return -1.0;
  const PresentMark& oldest = r.history[(r.history_next + kPresentHistory - r.history_count) % kPresentHistory];
  const PresentMark& newest = r.history[(r.history_next + kPresentHistory - 1) % kPresentHistory];
  const int64_t span = newest.at_ns - oldest.at_ns;
  if (span < 1'000'000'000 || newest.presents < oldest.presents) return -1.0;
  return static_cast<double>(newest.presents - oldest.presents) * 1e9 / static_cast<double>(span);
}

// Presents/s over the samples j..i, j the latest sample at least span_ns
// before sample i; -1 when there is none.
inline double TrailingRate(const Recovery& r, uint32_t i, int64_t span_ns) {
  for (uint32_t j = i; j-- > 0;) {
    const int64_t span = r.samples[i].at_ns - r.samples[j].at_ns;
    if (span < span_ns) continue;
    if (r.samples[i].presents < r.samples[j].presents) return -1.0;
    return static_cast<double>(r.samples[i].presents - r.samples[j].presents) * 1e9 / static_cast<double>(span);
  }
  return -1.0;
}

struct SeriesSummary {
  bool valid = false;
  uint32_t first = 0, peak = 0, end = 0;
  int64_t settled_ns = 0;  // from the anchor: every later sample within kRecoverySettleMib of the end
};
inline SeriesSummary SummarizeSeries(const Recovery& r, uint32_t RecoverySample::*field,
                                     bool RecoverySample::*has) {
  SeriesSummary out;
  for (uint32_t i = 0; i < r.count; ++i) {
    const RecoverySample& sample = r.samples[i];
    if (!(sample.*has)) continue;
    const uint32_t value = sample.*field;
    if (!out.valid) {
      out.valid = true;
      out.first = value;
      out.peak = value;
    }
    out.peak = std::max(out.peak, value);
    out.end = value;
  }
  if (!out.valid) return out;
  for (uint32_t i = r.count; i-- > 0;) {
    const RecoverySample& sample = r.samples[i];
    if (!(sample.*has)) continue;
    const uint32_t value = sample.*field;
    if ((value > out.end ? value - out.end : out.end - value) > kRecoverySettleMib) break;
    out.settled_ns = sample.at_ns - r.anchor_ns;
  }
  return out;
}

inline std::string RecoveryLine(const Recovery& r, int64_t now_ns, bool rebuilding, bool cut_short) {
  std::ostringstream text;
  text << "Vulkan NR recovery " << r.accounts << ": " << r.events << " lifecycle event sample(s) over "
       << Seconds(r.anchor_ns - r.first_event_ns) << " s, the last " << Seconds(now_ns - r.anchor_ns)
       << " s before this line; series from the last event (MiB; +s after it):";
  const auto series = [&](const std::string& label, uint32_t RecoverySample::*field, bool RecoverySample::*has) {
    const SeriesSummary summarized = SummarizeSeries(r, field, has);
    text << ' ' << label;
    if (!summarized.valid) {
      text << " n/a;";
      return;
    }
    text << " first=" << summarized.first << " peak=" << summarized.peak << " end=" << summarized.end
         << ", within " << kRecoverySettleMib << " of the end from +" << Seconds(summarized.settled_ns) << ';';
  };
  series("kernel requested", &RecoverySample::requested_mib, &RecoverySample::kernel);
  series("process local", &RecoverySample::local_mib, &RecoverySample::vram);
  series("nonlocal", &RecoverySample::nonlocal_mib, &RecoverySample::vram);
  series(std::string("demoted (before the rebuild ")
             + (r.demoted_before_valid ? std::to_string(r.demoted_before_mib) : std::string("n/a")) + ')',
         &RecoverySample::demoted_mib, &RecoverySample::kernel);
  // Presents: this add-on's present handler, frame generation's included.
  double end_rate = -1.0;
  bool end_whole = false;
  if (r.count != 0) end_rate = TrailingRate(r, r.count - 1, 5'000'000'000);
  if (end_rate < 0.0 && r.count >= 2) {
    const RecoverySample& first = r.samples[0];
    const RecoverySample& last = r.samples[r.count - 1];
    if (last.at_ns > first.at_ns && last.presents >= first.presents) {
      end_rate = static_cast<double>(last.presents - first.presents) * 1e9
                 / static_cast<double>(last.at_ns - first.at_ns);
      end_whole = true;
    }
  }
  double slowest = -1.0;
  int64_t slowest_ns = 0, below_first_ns = -1, below_last_ns = -1;
  for (uint32_t i = 0; i < r.count; ++i) {
    const double rate = TrailingRate(r, i, 1'000'000'000);
    if (rate < 0.0) continue;
    const int64_t at = r.samples[i].at_ns - r.anchor_ns;
    if (slowest < 0.0 || rate < slowest) {
      slowest = rate;
      slowest_ns = at;
    }
    if (end_rate > 0.0 && rate < 0.8 * end_rate) {
      if (below_first_ns < 0) below_first_ns = at;
      below_last_ns = at;
    }
  }
  const auto per_second = [](double value) { return value < 0.0 ? std::string("n/a") : Decimal(value, 1); };
  text << " presents/s (frame generation's included) before the rebuild " << per_second(r.rate_before) << ", end "
       << per_second(end_rate) << (end_whole ? " (whole account: under 5 s)" : " (last 5 s)") << ", slowest 1 s "
       << per_second(slowest);
  if (slowest >= 0.0) text << " at +" << Seconds(slowest_ns);
  if (end_rate > 0.0) {
    if (below_first_ns >= 0) {
      text << ", the trailing 1 s below 80% of the end from +" << Seconds(below_first_ns) << " to +"
           << Seconds(below_last_ns);
    } else {
      text << ", the trailing 1 s never below 80% of the end";
    }
  }
  if (cut_short) text << " - cut short by a later rebuild";
  if (rebuilding) text << "; NR retirement still pending at the close";
  return text.str();
}

inline void CloseRecovery(int64_t now_ns, bool rebuilding, bool cut_short) {
  Recovery& r = recovery;
  r.active = false;
  ++r.accounts;
  if (cut_short) ++r.cut_short;
  if (r.count != 0 && r.logged < kLifecycleLogCap) {
    ++r.logged;
    Log(reshade::log::level::info, RecoveryLine(r, now_ns, rebuilding, cut_short));
  }
}

// Every tick, after the kernel sample and the rebuild window.  event_now: a
// lifecycle event since the previous tick; kernel_fresh: the kernel sample
// was taken at this tick.
inline void TrackRecovery(int64_t now_ns, const telemetry::VideoMemory& vram, bool event_now,
                          uint64_t present_count, bool kernel_fresh, bool rebuilding) {
  Recovery& r = recovery;
  if (event_now) {
    if (r.active
        && (now_ns - r.anchor_ns >= kRecoveryBurstGapNs || now_ns - r.first_event_ns >= kRecoveryHorizonNs)) {
      CloseRecovery(now_ns, rebuilding, true);
    }
    if (!r.active) {
      r.active = true;
      r.first_event_ns = now_ns;
      r.events = 0;
      r.rate_before = HistoryRate(r);
      r.demoted_before_valid = monitor.last.valid;
      r.demoted_before_mib = static_cast<uint32_t>(monitor.last.demoted >> 20);
    }
    ++r.events;
    r.anchor_ns = now_ns;
    r.count = 0;
  }
  r.history[r.history_next] = {now_ns, present_count};
  r.history_next = (r.history_next + 1) % kPresentHistory;
  if (r.history_count < kPresentHistory) ++r.history_count;
  if (!r.active) return;
  if (r.count < kRecoverySamples) {
    RecoverySample& sample = r.samples[r.count++];
    sample = {};
    sample.at_ns = now_ns;
    sample.presents = present_count;
    sample.vram = vram.valid;
    if (vram.valid) {
      sample.local_mib = static_cast<uint32_t>(vram.local_usage >> 20);
      sample.nonlocal_mib = static_cast<uint32_t>(vram.nonlocal_usage >> 20);
    }
    sample.kernel = kernel_fresh;
    if (kernel_fresh) {
      sample.requested_mib = static_cast<uint32_t>(monitor.last.requested >> 20);
      sample.demoted_mib = static_cast<uint32_t>(monitor.last.demoted >> 20);
    }
  }
  if (now_ns - r.anchor_ns >= kRecoveryHorizonNs) CloseRecovery(now_ns, rebuilding, false);
}

// Once per lifecycle tick under runtime_mutex, after the 250 ms VRAM sample
// (`vram` may be invalid).  `events` is the NR lifecycle counter sum the
// VRAM-jump attribution uses; `rebuilding` the Vulkan retirement flag;
// `nr_on` the user's NR switch; `present_count` the add-on's present count
// (presents_observed, frame generation's presents included) for item 6.
// Item 7's sets are serviced first: they need no kernel sample.
inline void Tick(int64_t now_ns, const telemetry::VideoMemory& vram, uint64_t events, bool rebuilding,
                 bool nr_on, uint64_t present_count) {
  ServiceRetiredSets(now_ns);
  LUID luid;
  if (!CurrentAdapterLuid(&luid)) return;
  Monitor& m = monitor;
  if (events != m.last_events) {
    m.last_events = events;
    m.last_event_ns = now_ns;
  }
  const bool event_now = m.last_event_ns == now_ns;
  bool kernel_fresh = false;
  // --- the per-second kernel sample ---
  if (now_ns - m.last_query_ns >= 1'000'000'000) {
    m.last_query_ns = now_ns;
    const SegmentGroupSample sample = QueryLocalSegmentGroup(luid);
    if (sample.valid) {
      kernel_fresh = true;
      const SegmentGroupSample previous = m.last;
      m.last = sample;
      if (!m.logged_first) {
        m.logged_first = true;
        Log(reshade::log::level::info,
            "Vulkan residency: kernel local segment group of this process: budget " + Mib(sample.budget)
                + " requested " + Mib(sample.requested) + " usage " + Mib(sample.usage) + " demoted "
                + Mib(sample.demoted) + " MiB (min/low/normal/high/max " + ClassBreakdown(sample)
                + "); sampled once per second from here on, a class turning non-zero or back to zero is"
                  " logged");
      }
      const uint32_t mask = DemotedMask(sample);
      if (mask != m.demoted_mask) {
        if (m.transitions_logged < kTransitionLogCap) {
          ++m.transitions_logged;
          std::string changed;
          for (int i = 0; i < 5; ++i) {
            const uint32_t bit = 1u << i;
            if (((mask ^ m.demoted_mask) & bit) == 0) continue;
            changed += std::string(changed.empty() ? "" : ", ") + kClassNames[i]
                       + ((mask & bit) != 0 ? " now demoted" : " back to 0");
          }
          Log(reshade::log::level::info,
              "Vulkan residency: demoted " + changed + ": classes min/low/normal/high/max "
                  + ClassBreakdown(previous) + " -> " + ClassBreakdown(sample) + " MiB (budget "
                  + Mib(sample.budget) + " usage " + Mib(sample.usage) + " requested " + Mib(sample.requested)
                  + " MiB; NR " + (nr_on ? "on" : "off") + ", rebuild " + (rebuilding ? "in flight" : "settled")
                  + ", " + std::to_string((now_ns - m.last_event_ns) / 1'000'000)
                  + " ms since the last NR lifecycle event)");
        } else {
          ++m.transitions_unlogged;
        }
        m.demoted_mask = mask;
      }
    }
  }
  // --- the rebuild window (every lifecycle event opens or extends it) ---
  if (vram.valid) {
    const uint64_t local_mib = vram.local_usage >> 20;
    if (!m.window_open && (event_now || rebuilding)) {
      m.window_open = true;
      m.window_opened_ns = now_ns;
      m.window_before_mib = m.previous_valid ? m.previous_local_mib : local_mib;
      m.window_peak_mib = local_mib;
      m.window_budget_mib = vram.local_budget >> 20;
      m.window_demoted_before = m.last.demoted >> 20;
      m.window_demoted_peak = m.window_demoted_before;
      m.window_samples = 0;
      m.window_over = 0;
      m.window_events = 0;
    }
    if (m.window_open) {
      ++m.window_samples;
      if (event_now) ++m.window_events;
      m.window_peak_mib = std::max(m.window_peak_mib, local_mib);
      m.window_demoted_peak = std::max(m.window_demoted_peak, m.last.demoted >> 20);
      if (vram.local_usage > vram.local_budget) ++m.window_over;
      if (!rebuilding && now_ns - m.last_event_ns >= 3'000'000'000) {
        m.window_open = false;
        ++m.windows;
        const uint64_t after_mib = local_mib;
        const uint64_t steady_max = std::max(m.window_before_mib, after_mib);
        const bool spike = m.window_peak_mib > steady_max + 32;
        if (spike) ++m.windows_with_spike;
        if (m.window_logged < kLifecycleLogCap) {
          ++m.window_logged;
          const int64_t delta = static_cast<int64_t>(m.window_peak_mib) - static_cast<int64_t>(steady_max);
          std::ostringstream text;
          text << "Vulkan NR rebuild window " << m.windows << ": process local VRAM before=" << m.window_before_mib
               << " peak=" << m.window_peak_mib << " after=" << after_mib << " MiB (budget " << m.window_budget_mib
               << "); the peak is " << (delta >= 0 ? "+" : "") << delta
               << " MiB against the higher steady state, so "
               << (spike ? "a transient above both steady states occurred (release-before-create did not cover it)"
                         : "no transient above either steady state (release before create held)")
               << "; over-budget samples " << m.window_over << '/' << m.window_samples << ", demoted before="
               << m.window_demoted_before << " peak=" << m.window_demoted_peak << " after=" << (m.last.demoted >> 20)
               << " MiB (classes " << ClassBreakdown(m.last) << "), " << m.window_events
               << " lifecycle event sample(s) in "
               << (m.last_event_ns - m.window_opened_ns) / 1'000'000 << " ms";
          Log(reshade::log::level::info, text.str());
        }
      }
    }
    m.previous_local_mib = local_mib;
    m.previous_valid = true;
  }
  TrackRecovery(now_ns, vram, event_now, present_count, kernel_fresh, rebuilding);
}

// The health ledger's view (CollectHealthSample, under runtime_mutex like
// the tick): whether the kernel's per-process account is being sampled, and
// its demoted total in whole MiB, every class.
inline bool KernelSampleValid() { return monitor.last.valid; }
inline double DemotedMib() {
  return monitor.last.valid ? static_cast<double>(monitor.last.demoted >> 20) : 0.0;
}

inline void NoteDeviceGone() {
  monitor.window_open = false;
  monitor.previous_valid = false;
  monitor.last = {};
  monitor.demoted_mask = 0;
  // An open recovery account is dropped unlogged (its samples straddle the
  // device), and the present history restarts with the next device.
  recovery.active = false;
  recovery.count = 0;
  recovery.history_count = 0;
  recovery.history_next = 0;
}

// The telemetry group (one per 30 s line).  own_mib is this add-on's
// device-local memory (fixed_mib of it in FixedScope); hot_mib the game's NGX
// features' (DLSS, DLSS-G) alone - builds up to pageable1 counted own_mib in
// hot_mib too, so compare logs across that boundary by sum.  The
// *_nonlocal_mib pair is the same owners' system-heap memory.  retired[] is
// item 7 (sets / unattributed / untracked releases, allocations lowered and
// their MiB, releases that could not lower, retired allocations still alive
// and their MiB), recovery[] item 6 (accounts closed, of them cut short).
inline std::string Telemetry() {
  if (!any_pageable_device.load(std::memory_order_relaxed) && !monitor.last.valid) return {};
  std::ostringstream out;
  {
    std::lock_guard lock(registry_mutex);
    out << " vkmem[pageable=" << (any_pageable_device.load(std::memory_order_relaxed) ? 1 : 0)
        << " live=" << registry.size() << " allocs=" << counters.allocations << " frees=" << counters.frees
        << " local_mib=" << (counters.local_bytes >> 20) << " own_mib=" << (counters.own_local_bytes >> 20)
        << " fixed_mib=" << (counters.fixed_local_bytes >> 20) << " hot_mib=" << (counters.hot_local_bytes >> 20)
        << " own_nonlocal_mib=" << (counters.own_nonlocal_bytes >> 20)
        << " hot_nonlocal_mib=" << (counters.hot_nonlocal_bytes >> 20) << " tagged=" << counters.tagged << ']';
    out << " retired[sets=" << counters.retired_sets << " unattributed=" << counters.retired_unattributed
        << " untracked=" << counters.retired_untracked << " lowered=" << counters.retired_lowered
        << " lowered_mib=" << (counters.retired_lowered_bytes >> 20)
        << " lower_unavailable=" << counters.retired_lower_unavailable << " alive=" << counters.retired_alive
        << " alive_mib=" << (counters.retired_alive_bytes >> 20) << ']';
  }
  if (monitor.last.valid) {
    out << " kmt_local[budget=" << (monitor.last.budget >> 20) << " requested=" << (monitor.last.requested >> 20)
        << " usage=" << (monitor.last.usage >> 20) << " demoted=" << (monitor.last.demoted >> 20) << " classes="
        << ClassBreakdown(monitor.last) << ']';
  }
  out << " rebuild[windows=" << monitor.windows << " spikes=" << monitor.windows_with_spike
      << " demoted_transitions=" << (monitor.transitions_logged + monitor.transitions_unlogged) << ']';
  out << " recovery[accounts=" << recovery.accounts << " cut_short=" << recovery.cut_short << ']';
  return out.str();
}

}  // namespace residency
