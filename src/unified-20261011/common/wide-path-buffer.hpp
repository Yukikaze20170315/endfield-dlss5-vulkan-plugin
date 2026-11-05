#pragma once
#include <windows.h>

namespace dlss5 {
// Windows' maximum UTF-16 path must not become a 64 KiB stack frame in
// callbacks entered on driver-owned threads. Each call owns its buffer;
// there is no shared scratch, truncation, or exception on allocation failure.
class WidePathBuffer {
 public:
  static constexpr DWORD capacity = 32768;
  WidePathBuffer() noexcept
      : value_(static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                              capacity * sizeof(wchar_t)))) {}
  ~WidePathBuffer() noexcept {
    const DWORD error = GetLastError();
    if (value_) HeapFree(GetProcessHeap(), 0, value_);
    SetLastError(error);
  }
  WidePathBuffer(const WidePathBuffer&) = delete;
  WidePathBuffer& operator=(const WidePathBuffer&) = delete;
  wchar_t* data() const noexcept { return value_; }
  explicit operator bool() const noexcept { return value_ != nullptr; }
  bool Module(HMODULE module) noexcept {
    if (!value_) return false;
    const DWORD n = GetModuleFileNameW(module, value_, capacity);
    if (n != 0 && n < capacity) return true;
    value_[0] = 0;
    return false;
  }
 private:
  wchar_t* value_;
};
} // namespace dlss5
