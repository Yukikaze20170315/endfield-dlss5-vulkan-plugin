/*
 * Copyright (C) 2026
 * SPDX-License-Identifier: MIT
 */

// Runtime telemetry ([RenoDX.DLSS5] NRTelemetrySeconds).
//
// Field reports of "performance drops after ~20 s or after changing a
// setting" cannot be triaged from the contract log: nothing in it says how
// long frames take, how much video memory the process holds, or whether
// retired NR features and worksets are ever released.  This module keeps the
// two pieces of that picture that do not depend on addon internals - present
// pacing and the process's DXGI video-memory accounting - and dlssnr.hpp
// folds them into one throttled, self-describing log line together with its
// own pool and retirement counters.
//
// Diagnostic only: nothing computed here feeds the image path.  Every
// function requires the caller to hold runtime_mutex.

#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

#include "../../utils/directx.hpp"

namespace renodx::addons::dlss5::telemetry {

// Present-interval histogram: 0.25 ms bins up to 100 ms plus one overflow
// bin, enough resolution for a p99 that separates a 60 Hz cadence from its
// hitches without keeping per-frame samples.
constexpr uint32_t kPacingBins = 401;
constexpr double kPacingBinMs = 0.25;

struct Pacing {
  uint64_t count = 0;
  double sum_ms = 0.0;
  double max_ms = 0.0;
  std::array<uint32_t, kPacingBins> bins{};

  void Add(double ms) {
    ++count;
    sum_ms += ms;
    max_ms = std::max(max_ms, ms);
    const auto bin = static_cast<uint32_t>(std::max(ms, 0.0) / kPacingBinMs);
    ++bins[std::min(bin, kPacingBins - 1)];
  }

  // Upper edge of the bin holding the p-quantile (0 when empty).
  double Percentile(double p) const {
    if (count == 0) return 0.0;
    const auto rank = static_cast<uint64_t>(p * static_cast<double>(count - 1));
    uint64_t seen = 0;
    for (uint32_t bin = 0; bin < kPacingBins; ++bin) {
      seen += bins[bin];
      if (seen > rank) return (bin + 1) * kPacingBinMs;
    }
    return max_ms;
  }
};

struct VideoMemory {
  bool valid = false;
  uint64_t local_usage = 0;
  uint64_t local_budget = 0;
  uint64_t nonlocal_usage = 0;
};

inline IDXGIAdapter3* adapter = nullptr;
inline LUID adapter_luid{};

inline void ReleaseAdapter() {
  if (adapter != nullptr) adapter->Release();
  adapter = nullptr;
  adapter_luid = {};
}

// The adapter to read when no D3D12 device of the addon's own exists
// (v9): a Vulkan session publishes the game's physical device LUID here
// (vulkan_path.hpp NoteAdapterLuid), so the telemetry line's vram_mib and
// every lifecycle line's "vram=" no longer depend on the bridge's private
// D3D12 device.  HighPart in the upper half; 0 = none.
inline std::atomic_uint64_t fallback_luid{0};
inline void SetFallbackAdapterLuid(LUID luid) {
  fallback_luid.store((static_cast<uint64_t>(static_cast<uint32_t>(luid.HighPart)) << 32)
                          | luid.LowPart,
                      std::memory_order_relaxed);
}

// This process's usage and OS budget of the adapter's local (video) and
// non-local segment groups.  The adapter is resolved once per LUID through
// the repository's DXGI loader; a failure leaves `valid` false and is
// retried on the next call.
inline VideoMemory QueryVideoMemoryByLuid(LUID luid) {
  VideoMemory memory;
  if (adapter == nullptr || luid.LowPart != adapter_luid.LowPart
      || luid.HighPart != adapter_luid.HighPart) {
    ReleaseAdapter();
    NoteFirstDirectxInitialize();
    if (!renodx::utils::directx::Initialize()
        || renodx::utils::directx::pCreateDXGIFactory1 == nullptr) {
      return memory;
    }
    IDXGIFactory4* factory = nullptr;
    if (FAILED(renodx::utils::directx::pCreateDXGIFactory1(
            IID_PPV_ARGS(&factory)))
        || factory == nullptr) {
      return memory;
    }
    if (FAILED(factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter)))) {
      adapter = nullptr;
    }
    factory->Release();
    if (adapter == nullptr) return memory;
    adapter_luid = luid;
  }
  DXGI_QUERY_VIDEO_MEMORY_INFO local{};
  DXGI_QUERY_VIDEO_MEMORY_INFO nonlocal{};
  if (FAILED(adapter->QueryVideoMemoryInfo(
          0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local))
      || FAILED(adapter->QueryVideoMemoryInfo(
          0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonlocal))) {
    return memory;
  }
  memory.valid = true;
  memory.local_usage = local.CurrentUsage;
  memory.local_budget = local.Budget;
  memory.nonlocal_usage = nonlocal.CurrentUsage;
  return memory;
}

// The device's adapter, or the published fallback adapter when the addon
// has no D3D12 device (a Vulkan session).
inline VideoMemory QueryVideoMemory(ID3D12Device* device) {
  if (device != nullptr) return QueryVideoMemoryByLuid(device->GetAdapterLuid());
  const uint64_t packed = fallback_luid.load(std::memory_order_relaxed);
  if (packed == 0) return {};
  LUID luid;
  luid.LowPart = static_cast<DWORD>(packed & 0xFFFFFFFFu);
  luid.HighPart = static_cast<LONG>(packed >> 32);
  return QueryVideoMemoryByLuid(luid);
}

}  // namespace renodx::addons::dlss5::telemetry
