#pragma once
// Chain of unhandled-exception filters around the bridge's crash filter.
//
// The bridge installs its filter on top of the original one (G). Before the
// health point it also takes the top back when another component replaced it
// (X1, X2, ...), so every replacement must stay reachable:
//
//   links[0] = G, links[k] = the filter found on top at the k-th retake.
//
// A crash enters the bridge filter first. It writes the report and calls the
// newest replacement links[count]. That replacement usually chains to the
// filter it saw on top when it installed itself - the bridge filter - so the
// bridge filter is re-entered on the same thread and calls the next lower
// link. Each link therefore runs once and G runs last, exactly as if the
// retakes had never happened. A link that installed itself again after its
// retake sits above the bridge filter and is called by the system before it;
// its saved predecessor is the bridge filter, so it is marked as already run
// on the first entry (see Top and MarkCalled) and is not called a second time
// from the chain. A second thread that crashes while the first
// is still inside the chain gets EXCEPTION_CONTINUE_SEARCH and is not
// chained, so no link is ever run concurrently with itself.
//
// A replacement can belong to a module that is unloaded later while the
// bridge stays loaded (ReShade frees its add-ons when the last device goes
// away; the bridge is pinned). Each link therefore remembers the image it
// lives in - base address, link time stamp and size - and is called only
// while that same image is still mapped there; a link whose image is gone
// is skipped and the next lower one runs in its place. Some protectors make
// a module's header page inaccessible or erase the headers; the headers are
// therefore read only when readable and under an exception guard, and a
// link whose headers cannot be read is matched by its base address alone,
// so the original filter is never dropped for that reason.
#include <windows.h>

namespace dlss5::crash_chain {

inline constexpr LONG kMaxRetakes = 8;

// The image a filter lives in. A filter outside any image (a thunk in
// allocated memory) has base == nullptr and only needs executable memory.
// stamp and size are valid only when identity is set (the PE headers were
// readable when the link was recorded).
struct Image {
  const void* base = nullptr;
  DWORD stamp = 0;
  DWORD size = 0;
  bool identity = false;
};

struct Chain {
  LPTOP_LEVEL_EXCEPTION_FILTER links[kMaxRetakes + 1]{};
  Image images[kMaxRetakes + 1]{};
  volatile LONG count = 0;   // retakes recorded; links[count] is called first
  volatile LONG owner = 0;   // thread inside the chain, 0 when idle
  LONG depth = 0;            // links consumed during the current crash (owner only)
  DWORD called = 0;          // links the system already ran before the bridge filter (owner only)
};

enum class Entry { First, Reentry, Busy };

inline bool Executable(const MEMORY_BASIC_INFORMATION& m) {
  constexpr DWORD kExec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  return m.State == MEM_COMMIT && (m.Protect & kExec) != 0 && (m.Protect & PAGE_GUARD) == 0;
}

inline bool Readable(const MEMORY_BASIC_INFORMATION& m) {
  constexpr DWORD kRead = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                          PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  return m.State == MEM_COMMIT && (m.Protect & kRead) != 0 && (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0;
}

inline int HeaderFault(DWORD code) {
  return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR || code == STATUS_GUARD_PAGE_VIOLATION
             ? EXCEPTION_EXECUTE_HANDLER
             : EXCEPTION_CONTINUE_SEARCH;
}

// Reads the link time stamp and size from the PE headers of a mapped image.
// Only a readable header page that covers the headers is read, and the reads
// are guarded against a concurrent protection change, so hidden or erased
// headers yield false instead of a fault.
inline bool ReadIdentity(const void* base, DWORD& stamp, DWORD& size) {
  MEMORY_BASIC_INFORMATION m{};
  if (base == nullptr || VirtualQuery(base, &m, sizeof(m)) != sizeof(m) || m.Type != MEM_IMAGE || !Readable(m))
    return false;
  const auto* bytes = static_cast<const BYTE*>(base);
  const SIZE_T available = static_cast<SIZE_T>(static_cast<const BYTE*>(m.BaseAddress) + m.RegionSize - bytes);
  if (available < sizeof(IMAGE_DOS_HEADER)) return false;
  __try {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<SIZE_T>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS) > available)
      return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(bytes + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    stamp = nt->FileHeader.TimeDateStamp;
    size = nt->OptionalHeader.SizeOfImage;
    return true;
  } __except (HeaderFault(GetExceptionCode())) {
    return false;
  }
}

inline Image Locate(LPTOP_LEVEL_EXCEPTION_FILTER filter) {
  Image image{};
  MEMORY_BASIC_INFORMATION m{};
  if (filter == nullptr || VirtualQuery(reinterpret_cast<const void*>(filter), &m, sizeof(m)) != sizeof(m)) return image;
  if (m.Type != MEM_IMAGE) return image;
  image.base = m.AllocationBase;
  image.identity = ReadIdentity(image.base, image.stamp, image.size);
  return image;
}

// True while links[index] still points into the image it was recorded in.
inline bool Callable(const Chain& chain, LONG index) {
  const LPTOP_LEVEL_EXCEPTION_FILTER filter = chain.links[index];
  if (filter == nullptr) return false;
  MEMORY_BASIC_INFORMATION m{};
  if (VirtualQuery(reinterpret_cast<const void*>(filter), &m, sizeof(m)) != sizeof(m) || !Executable(m)) return false;
  const Image& recorded = chain.images[index];
  if (recorded.base == nullptr) return m.Type != MEM_IMAGE;
  if (m.Type != MEM_IMAGE || m.AllocationBase != recorded.base) return false;
  if (!recorded.identity) return true;
  DWORD stamp = 0, size = 0;
  // Headers hidden or erased since the link was recorded: only the base address can be compared.
  if (!ReadIdentity(recorded.base, stamp, size)) return true;
  return stamp == recorded.stamp && size == recorded.size;
}

inline void Install(Chain& chain, LPTOP_LEVEL_EXCEPTION_FILTER original) {
  for (auto& link : chain.links) link = nullptr;
  for (auto& image : chain.images) image = Image{};
  chain.links[0] = original;
  chain.images[0] = Locate(original);
  InterlockedExchange(&chain.count, 0);
}

// Records the filter found on top at a retake. A filter that is already in
// the chain re-installed itself and keeps a single place: calling it twice
// would run it twice per crash.
inline bool Append(Chain& chain, LPTOP_LEVEL_EXCEPTION_FILTER top) {
  const LONG count = chain.count;
  for (LONG i = 0; i <= count; ++i)
    if (chain.links[i] == top) return false;
  if (count >= kMaxRetakes) return false;
  chain.images[count + 1] = Locate(top);
  chain.links[count + 1] = top;
  InterlockedExchange(&chain.count, count + 1);
  return true;
}

// The filter that should be on top once the bridge filter is removed: the
// newest replacement whose image is still loaded, else G (nullptr when G's
// own image is gone).
inline LPTOP_LEVEL_EXCEPTION_FILTER Successor(const Chain& chain) {
  for (LONG i = chain.count; i > 0; --i)
    if (Callable(chain, i)) return chain.links[i];
  return Callable(chain, 0) ? chain.links[0] : nullptr;
}

inline Entry Enter(Chain& chain) {
  const LONG self = static_cast<LONG>(GetCurrentThreadId());
  const LONG previous = InterlockedCompareExchange(&chain.owner, self, 0);
  if (previous == 0) { chain.depth = 0; chain.called = 0; return Entry::First; }
  return previous == self ? Entry::Reentry : Entry::Busy;
}

// The filter currently on top, i.e. the one the system called first for this
// crash. There is no getter, so it is read by a swap and put back unchanged.
inline LPTOP_LEVEL_EXCEPTION_FILTER Top(LPTOP_LEVEL_EXCEPTION_FILTER self) {
  const LPTOP_LEVEL_EXCEPTION_FILTER top = SetUnhandledExceptionFilter(self);
  if (top != self) SetUnhandledExceptionFilter(top);
  return top;
}

// First entry only: a link found on top has already run and chained here.
// Append keeps every filter at a single place, so at most one link matches.
inline void MarkCalled(Chain& chain, LPTOP_LEVEL_EXCEPTION_FILTER top) {
  if (top == nullptr) return;
  for (LONG i = 0; i <= chain.count; ++i)
    if (chain.links[i] == top) chain.called |= 1u << i;
}

// Calls the next unconsumed link that is still loaded. The depth only grows
// during one crash, so no link is called twice even if a filter calls its
// predecessor more than once, and a skipped link is consumed as well.
inline LONG CallNext(Chain& chain, EXCEPTION_POINTERS* ep) {
  for (LONG index = chain.count - chain.depth; index >= 0; index = chain.count - chain.depth) {
    ++chain.depth;
    if ((chain.called & (1u << index)) == 0 && Callable(chain, index)) return chain.links[index](ep);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

// First entry: call the newest link and release the chain afterwards.
inline LONG CallFirst(Chain& chain, EXCEPTION_POINTERS* ep) {
  const LONG verdict = CallNext(chain, ep);
  chain.depth = 0;
  chain.called = 0;
  InterlockedExchange(&chain.owner, 0);
  return verdict;
}

// Re-entry from a link that chained back: move one link further down.
inline LONG CallReentry(Chain& chain, EXCEPTION_POINTERS* ep) {
  return CallNext(chain, ep);
}

}  // namespace dlss5::crash_chain
