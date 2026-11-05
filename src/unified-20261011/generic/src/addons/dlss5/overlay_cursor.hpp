/*
 * Copyright (C) 2026
 * SPDX-License-Identifier: MIT
 */

// Keeps the mouse inside the game window while the ReShade overlay is open,
// and gives the game its own cursor lock back when the overlay closes.
//
// ReShade 6.8 (source/input_windows.cpp, block_mouse_cursor_warping) calls
// ClipCursor(nullptr) on every frame the overlay is open, so a game that had
// locked the cursor (Endfield: first/third-person camera) loses the lock and
// the cursor can leave for another monitor.  On close ReShade only restores
// the last rectangle its ClipCursor hook saw; when that is stale the lock
// stays off until the game is deactivated and reactivated.
//
// What this does, only when the game had the cursor clipped at the moment the
// overlay opened and the game window is in the foreground:
//   - every presented frame while the overlay is open, after ReShade has
//     drawn (reshade_present runs after draw_gui), the cursor is confined to
//     the game window's client area.  The call goes to win32u!NtUserClipCursor
//     because ReShade's ClipCursor hook replaces any rectangle with nullptr
//     while the overlay is open; without win32u (pre-1607 Windows) this part
//     is skipped.
//   - on close, the rectangle in force before opening is handed to the hooked
//     ClipCursor, so ReShade records it and restores exactly that rectangle in
//     the same frame.
// When the game had no clip (menus, windowed play) nothing changes.
// Ini kill switch: [DLSS5] NROverlayCursorConfine=0.

#pragma once

#include <windows.h>

#include <atomic>
#include <mutex>

#include <include/reshade.hpp>

namespace renodx::addons::dlss5::overlay_cursor {

inline std::atomic_bool enabled{true};

namespace internal {

using ClipFn = BOOL(WINAPI*)(const RECT*);

inline ClipFn RawClip() {
  static const ClipFn fn = [] {
    HMODULE win32u = GetModuleHandleW(L"win32u.dll");
    return win32u != nullptr
               ? reinterpret_cast<ClipFn>(GetProcAddress(win32u, "NtUserClipCursor"))
               : nullptr;
  }();
  return fn;
}

inline std::mutex mutex;
inline bool open = false;
inline bool had_clip = false;
inline bool confining = false;
inline RECT before{};

inline HWND GameRoot(reshade::api::effect_runtime* runtime) {
  if (runtime == nullptr) return nullptr;
  HWND hwnd = static_cast<HWND>(runtime->get_hwnd());
  return hwnd != nullptr ? GetAncestor(hwnd, GA_ROOT) : nullptr;
}

inline bool IsForeground(HWND root) {
  return root != nullptr && GetForegroundWindow() == root && !IsIconic(root);
}

// GetClipCursor reports the whole virtual desktop when nothing is clipped.
inline bool IsRealClip(const RECT& r) {
  const LONG vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const LONG vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const LONG vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const LONG vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (r.right <= r.left || r.bottom <= r.top) return false;
  return r.left > vx || r.top > vy || r.right < vx + vw || r.bottom < vy + vh;
}

inline bool ClientRectOnScreen(HWND root, RECT* out) {
  RECT client{};
  if (!GetClientRect(root, &client) || client.right <= client.left
      || client.bottom <= client.top) {
    return false;
  }
  POINT top_left{client.left, client.top};
  POINT bottom_right{client.right, client.bottom};
  if (!ClientToScreen(root, &top_left) || !ClientToScreen(root, &bottom_right)) return false;
  *out = RECT{top_left.x, top_left.y, bottom_right.x, bottom_right.y};
  return out->right > out->left && out->bottom > out->top;
}

inline void ReleaseLocked() {
  if (!confining) return;
  if (const ClipFn clip = RawClip()) clip(nullptr);
  confining = false;
}

}  // namespace internal

// reshade_open_overlay: fires inside ReShade's open_overlay, before this
// frame's input blocking is updated, so GetClipCursor still shows the game's
// own clip on open.
inline void OnOpenOverlay(reshade::api::effect_runtime* runtime, bool open) {
  using namespace internal;
  const std::lock_guard<std::mutex> lock(mutex);
  if (open) {
    internal::open = true;
    had_clip = false;
    if (!enabled.load(std::memory_order_relaxed)) return;
    const HWND root = GameRoot(runtime);
    RECT current{};
    if (IsForeground(root) && GetClipCursor(&current) && IsRealClip(current)) {
      before = current;
      had_clip = true;
    }
    return;
  }
  internal::open = false;
  ReleaseLocked();
  if (had_clip && IsForeground(GameRoot(runtime))) {
    // Hooked ClipCursor: ReShade stores the rectangle and re-applies it when
    // it stops blocking the cursor later in this frame.
    ClipCursor(&before);
  }
  had_clip = false;
}

// reshade_present: runs after ReShade drew the overlay and cleared the clip.
inline void OnPresent(reshade::api::effect_runtime* runtime) {
  using namespace internal;
  const std::lock_guard<std::mutex> lock(mutex);
  if (!internal::open || !had_clip) return;
  const ClipFn clip = RawClip();
  if (clip == nullptr) return;
  const HWND root = GameRoot(runtime);
  RECT target{};
  if (!IsForeground(root) || !ClientRectOnScreen(root, &target)) {
    ReleaseLocked();
    return;
  }
  // ReShade unclips every frame, so this re-applies every frame.
  if (clip(&target)) confining = true;
}

}  // namespace renodx::addons::dlss5::overlay_cursor
