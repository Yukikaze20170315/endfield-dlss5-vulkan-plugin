/*
 * Copyright (C) 2025 Carlos Lopez
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <mutex>
#include <ranges>
#include <tuple>

#include <detours.h>

#include "./log.hpp"

namespace renodx::utils::vtable {

// Shared across translation units in this addon, like its linked Detours state.
// Hold this lock through commit/abort, including direct Generic transactions.
// Other modules using their own Detours copy have independent transaction state.
inline std::mutex& TransactionMutex() {
  static std::mutex mutex;
  return mutex;
}

inline std::atomic<uint64_t> transaction_contended{0};

// Caller holds TransactionMutex(). Do not wait for non-cooperating owners:
// the caller must be able to defer/retry installation without a lock inversion.
[[nodiscard]] inline bool BeginTransaction() {
  const LONG error = DetourTransactionBegin();
  if (error == NO_ERROR) return true;
  if (error == ERROR_INVALID_OPERATION) {
    transaction_contended.fetch_add(1, std::memory_order_relaxed);
  } else {
    // Begin owns the transaction before making trampoline pages writable;
    // failure at that step still requires abort to release its ownership.
    DetourTransactionAbort();
  }
  log::e("vtable::BeginTransaction(Detour transaction begin failed: ", error, ")");
  return false;
}

// Function name, real function pointer, hook function pointer
using HookItem = std::tuple<const char*, void**, void*>;

template <std::ranges::range Hooks>
  requires std::convertible_to<std::ranges::range_value_t<Hooks>, HookItem>
static bool Hook(HMODULE module, const Hooks& hooks) {
  if (module == nullptr) {
    log::e("vtable::Hook(Module not loaded.)");
    return false;
  }

  std::lock_guard<std::mutex> transaction(TransactionMutex());
  // Commit each export independently: a failed attach must not invalidate
  // earlier successes or leave an unhooked export masquerading as a trampoline.
  bool any_hooked = false;
  for (const auto& [function_name, real, hook] : hooks) {
    FARPROC proc = GetProcAddress(module, function_name);
    if (proc == nullptr) {
      log::e("vtable::Hook(Failed to find ", function_name, ")");
      continue;
    }
    if (real == nullptr || hook == nullptr || *real != nullptr) {
      log::e("vtable::Hook(Invalid or already populated hook for ", function_name, ")");
      continue;
    }
    if (!BeginTransaction()) continue;
    LONG error = DetourUpdateThread(GetCurrentThread());
    if (error == NO_ERROR) {
      *real = reinterpret_cast<void*>(proc);
      error = DetourAttach(real, hook);
    }
    if (error != NO_ERROR) {
      DetourTransactionAbort();
      *real = nullptr;
      log::e("vtable::Hook(Detour update/attach failed for ", function_name, ": ", error, ")");
      continue;
    }
    error = DetourTransactionCommit();
    if (error != NO_ERROR) {
      // Detours aborts a failed commit; no trampoline was published.
      *real = nullptr;
      log::e("vtable::Hook(Detour commit failed for ", function_name, ": ", error, ")");
      continue;
    }
    any_hooked = true;
    log::d("vtable::Hook(", function_name, " hooked successfully)");
  }
  return any_hooked;
}

template <std::ranges::range Hooks>
  requires std::convertible_to<std::ranges::range_value_t<Hooks>, HookItem>
static void Unhook(HMODULE module, const Hooks& hooks) {
  if (module == nullptr) {
    log::w("vtable::Unhook(DLL already loaded)");
  }

  std::lock_guard<std::mutex> transaction(TransactionMutex());
  for (const auto& [function_name, real, hook] : hooks) {
    if (real == nullptr || hook == nullptr) {
      log::w("vtable::Unhook(Invalid hook tuple for ", function_name, ")");
      continue;
    }
    if (*real == nullptr) {
      log::w("vtable::Unhook(", function_name, " not hooked, skipping)");
      continue;
    }
    if (!BeginTransaction()) continue;
    if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR) {
      DetourTransactionAbort();
      log::e("vtable::Unhook(Detour update thread failed for ", function_name, ")");
      continue;
    }
    if (DetourDetach(reinterpret_cast<void**>(real), hook) != NO_ERROR) {
      DetourTransactionAbort();
      log::e("vtable::Unhook(Detour detach failed for ", function_name, ")");
      continue;
    }
    if (DetourTransactionCommit() != NO_ERROR) {
      log::e("vtable::Unhook(Detour transaction commit failed for ", function_name, ")");
      continue;
    }
    *real = nullptr;
    log::i("vtable::Unhook(", function_name, " unhooked successfully)");
  }
}

}  // namespace renodx::utils::vtable